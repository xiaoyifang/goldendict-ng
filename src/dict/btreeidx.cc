/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "btreeidx.hh"
#include "folding.hh"
#include "text.hh"
#include <string.h>
#include "utils.hh"

#include <QRegularExpression>
#include "wildcard.hh"
#include "globalbroadcaster.hh"

#include <QtConcurrentRun>
#include <QDir>
#include <QFileInfo>

namespace BtreeIndexing {

using std::pair;

// Forward declarations
static QByteArray serializeLinks(const vector<WordArticleLink>& links);
static vector<WordArticleLink> deserializeLinks(const QByteArray& data);

// Apply standard text folding for word matching
static std::u32string applyStandardFolding( const std::u32string & word, bool ignoreDiacritics )
{
  std::u32string result = Folding::applySimpleCaseOnly( Text::normalize( word ) );
  if ( ignoreDiacritics ) {
    result = Folding::applyDiacriticsOnly( result );
  }
  if ( GlobalBroadcaster::instance()->getPreference()->ignorePunctuation ) {
    result = Folding::trimWhitespaceOrPunct( result );
  }
  return result;
}

BtreeIndex::BtreeIndex():
  idxFile( nullptr ),
  env( nullptr ),
  dbi( 0 )
{
}

void BtreeIndex::closeLmdb()
{
  if (env) {
    if (dbi) {
      mdb_dbi_close(env, dbi);
      dbi = 0;
    }
    mdb_env_close(env);
    env = nullptr;
  }
}

BtreeDictionary::BtreeDictionary( const string & id, const vector< string > & dictionaryFiles ):
  Dictionary::Class( id, dictionaryFiles )
{
}

const string & BtreeDictionary::ensureInitDone()
{
  static string empty;
  return empty;
}

void BtreeIndex::openIndex( const IndexInfo & indexInfo, File::Index & file, QMutex & mutex )
{
  idxFile      = &file;
  idxFileMutex = &mutex;
  
  closeLmdb();
  
  mdb_env_create(&env);
  if (!env) {
      qWarning() << "LMDB env create failed";
      return;
  }
  
  string lmdbPath = file.file().fileName().toStdString() + indexInfo.suffix + ".lmdb";
  qDebug() << "LMDB opening index at:" << QString::fromStdString(lmdbPath);

  int rc = mdb_env_open( env, lmdbPath.c_str(), MDB_RDONLY | MDB_NOSUBDIR, 0664 );
  if ( rc != MDB_SUCCESS ) {
    qWarning() << "LMDB open failed for" << QString::fromStdString( lmdbPath ) << ": " << mdb_strerror( rc );
    mdb_env_close( env );
    env = nullptr;
    return;
  }

  MDB_txn *txn;
  rc = mdb_txn_begin(env, nullptr, MDB_RDONLY, &txn);
  if (rc != MDB_SUCCESS) {
      qWarning() << "LMDB txn begin failed: " << mdb_strerror(rc);
      mdb_env_close(env);
      env = nullptr;
      return;
  }
  
  rc = mdb_dbi_open(txn, nullptr, 0, &dbi);
  if (rc != MDB_SUCCESS) {
      qWarning() << "LMDB dbi open failed: " << mdb_strerror(rc);
      mdb_txn_abort(txn);
      mdb_env_close(env);
      env = nullptr;
      return;
  }
  
  // Check if database is empty
  MDB_stat stat;
  mdb_stat(txn, dbi, &stat);
  qDebug() << "LMDB database opened, entries:" << stat.ms_entries;
  
  mdb_txn_commit(txn);
}

vector< WordArticleLink >
BtreeIndex::findArticles( const std::u32string & search_word, bool ignoreDiacritics, uint32_t maxMatchCount )
{
  vector< WordArticleLink > result;
  if (!env || !dbi) return result;
  
  std::u32string word = Text::removeTrailingZero( search_word );
  std::u32string folded = Folding::apply( word );
  if ( folded.empty() ) {
    folded = Folding::applyWhitespaceOnly( word );
  }
  
  string key_str = Text::toUtf8(folded);
  
  MDB_txn *txn;
  if (mdb_txn_begin(env, nullptr, MDB_RDONLY, &txn) != MDB_SUCCESS) return result;
  
  MDB_cursor *cursor;
  mdb_cursor_open(txn, dbi, &cursor);
  
  MDB_val key, data;
  key.mv_size = key_str.size();
  key.mv_data = (void*)key_str.c_str();
  
  int rc = mdb_cursor_get(cursor, &key, &data, MDB_SET);
  if (rc == MDB_SUCCESS) {
      string current_key((char*)key.mv_data, key.mv_size);
      if (current_key == key_str) {
          QByteArray ba((const char*)data.mv_data, data.mv_size);
          vector<WordArticleLink> links = deserializeLinks(ba);
          
          // Apply standard folding to search word
          std::u32string caseFolded = applyStandardFolding( word, ignoreDiacritics );
          
          // Stop early once enough matches are found
          uint32_t found = 0;
          for ( auto & link : links ) {
              // Apply standard folding to current entry
              std::u32string entry = applyStandardFolding( Text::toUtf32( link.word ), ignoreDiacritics );
              
              if ( entry == caseFolded ) {
                  result.push_back( std::move( link ) );
                  if ( maxMatchCount != (uint32_t)-1 && ++found >= maxMatchCount ) {
                      break;  // Exit early after finding enough matches
                  }
              }
          }
      }
  }
  
  mdb_cursor_close(cursor);
  mdb_txn_commit(txn);
  
  return result;
}

void BtreeIndex::findAllArticleLinks( QList< WordArticleLink > & articleLinks ) {
  findArticleLinks(&articleLinks, nullptr, nullptr, nullptr);
}
void BtreeIndex::getAllHeadwords( QSet< QString > & headwords ) {
  findArticleLinks(nullptr, nullptr, &headwords, nullptr);
}

void BtreeDictionary::findHeadWordsWithLenth( QString & lastWord, QSet< QString > * headwords, uint32_t length ) {
  if (!env || !dbi || !headwords) return;

  MDB_txn *txn;
  if (mdb_txn_begin(env, nullptr, MDB_RDONLY, &txn) != MDB_SUCCESS) return;
  MDB_cursor *cursor;
  mdb_cursor_open(txn, dbi, &cursor);

  MDB_val key, data;
  int rc;
  
  if (lastWord.isEmpty()) {
    rc = mdb_cursor_get(cursor, &key, &data, MDB_FIRST);
  } else {
    string lastWordStr = lastWord.toStdString();
    key.mv_size = lastWordStr.size();
    key.mv_data = (void*)lastWordStr.c_str();
    rc = mdb_cursor_get(cursor, &key, &data, MDB_SET_RANGE);
    if (rc == MDB_SUCCESS) {
      string current_key((char*)key.mv_data, key.mv_size);
      if (current_key == lastWordStr) {
        rc = mdb_cursor_get(cursor, &key, &data, MDB_NEXT);
      }
    }
  }

  uint32_t count = 0;
  while (rc == MDB_SUCCESS && count < length) {
    QByteArray ba((const char*)data.mv_data, data.mv_size);
    vector<WordArticleLink> links = deserializeLinks(ba);
    for (auto & l : links) {
      headwords->insert(QString::fromStdString(l.word));
      
      // Check if we have enough headwords
      if (headwords->size() >= length) {
        mdb_cursor_close(cursor);
        mdb_txn_commit(txn);
        return;
      }
    }
    lastWord = QString::fromUtf8((char*)key.mv_data, key.mv_size);
    count++;
    rc = mdb_cursor_get(cursor, &key, &data, MDB_NEXT);
  }

  mdb_cursor_close(cursor);
  mdb_txn_commit(txn);
}

void BtreeIndex::findArticleLinks( QList< WordArticleLink > * articleLinks, QSet< uint32_t > * offsets, QSet< QString > * headwords, QAtomicInt * isCancelled )
{
  if (!env || !dbi) return;
  MDB_txn *txn;
  if (mdb_txn_begin(env, nullptr, MDB_RDONLY, &txn) != MDB_SUCCESS) return;
  MDB_cursor *cursor;
  mdb_cursor_open(txn, dbi, &cursor);
  
  MDB_val key, data;
  int rc = mdb_cursor_get(cursor, &key, &data, MDB_FIRST);
  
  while (rc == MDB_SUCCESS) {
      if ( isCancelled && Utils::AtomicInt::loadAcquire( *isCancelled ) ) break;
      
      QByteArray ba((const char*)data.mv_data, data.mv_size);
      vector<WordArticleLink> links = deserializeLinks(ba);
      for (auto & l : links) {
          if (articleLinks) articleLinks->append(l);
          if (offsets) offsets->insert(l.articleOffset);
          if (headwords) headwords->insert(QString::fromStdString(l.word));
      }
      rc = mdb_cursor_get(cursor, &key, &data, MDB_NEXT);
  }
  
  mdb_cursor_close(cursor);
  mdb_txn_commit(txn);
}



void BtreeIndex::getHeadwordsFromOffsets( QList< uint32_t > & offsets, QList< QString > & headwords, QAtomicInt * isCancelled )
{
  if (!env || !dbi) return;
  MDB_txn *txn;
  if (mdb_txn_begin(env, nullptr, MDB_RDONLY, &txn) != MDB_SUCCESS) return;
  
  MDB_cursor *cursor;
  mdb_cursor_open(txn, dbi, &cursor);
  
  MDB_val key, data;
  int rc = mdb_cursor_get(cursor, &key, &data, MDB_FIRST);
  
  QSet<uint32_t> targetOffsets(offsets.begin(), offsets.end());
  
  while (rc == MDB_SUCCESS && !targetOffsets.isEmpty()) {
      if ( isCancelled && Utils::AtomicInt::loadAcquire( *isCancelled ) ) break;
      
      QByteArray ba((const char*)data.mv_data, data.mv_size);
      vector<WordArticleLink> links = deserializeLinks(ba);
      for (auto & l : links) {
          if (targetOffsets.contains(l.articleOffset)) {
              headwords.append(QString::fromStdString(l.word));
          }
      }
      rc = mdb_cursor_get(cursor, &key, &data, MDB_NEXT);
  }
  
  mdb_cursor_close(cursor);
  mdb_txn_commit(txn);
}

BtreeWordSearchRequest::BtreeWordSearchRequest( BtreeDictionary & dict_, const std::u32string & str_, unsigned minLength_, int maxSuffixVariation_, bool allowMiddleMatches_, unsigned long maxResults_, bool startRunnable ):
  dict( dict_ ), str( str_ ), maxResults( maxResults_ ), minLength( minLength_ ), maxSuffixVariation( maxSuffixVariation_ ), allowMiddleMatches( allowMiddleMatches_ )
{
  if ( startRunnable ) { f = QtConcurrent::run( [ this ]() { this->run(); } ); }
}

void BtreeWordSearchRequest::findMatches()
{
  if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) { finish(); return; }
  if ( dict.ensureInitDone().size() ) { setErrorString( QString::fromUtf8( dict.ensureInitDone().c_str() ) ); finish(); return; }
  if ( !dict.env || !dict.dbi ) { finish(); return; }

  QRegularExpression regexp;
  bool useWildcards = false;
  if ( allowMiddleMatches ) {
    useWildcards = ( str.find( '*' ) != std::u32string::npos || str.find( '?' ) != std::u32string::npos
                     || str.find( '[' ) != std::u32string::npos || str.find( ']' ) != std::u32string::npos );
  }

  std::u32string folded = Folding::apply( str );
  int minMatchLength = 0;

  if ( useWildcards ) {
    regexp.setPattern( wildcardsToRegexp(
      QString::fromStdU32String( Folding::applyDiacriticsOnly( Folding::applySimpleCaseOnly( str ) ) ) ) );
    if ( !regexp.isValid() ) {
      regexp.setPattern( QRegularExpression::escape( regexp.pattern() ) );
    }
    regexp.setPatternOptions( QRegularExpression::CaseInsensitiveOption );

    bool bNoLetters = folded.empty();
    std::u32string foldedWithWildcards;
    if (bNoLetters) {
      foldedWithWildcards = Folding::applyWhitespaceOnly( str );
    } else {
      foldedWithWildcards = Folding::apply( str, useWildcards );
    }

    bool insideSet = false;
    bool escaped   = false;
    for ( char32_t ch : foldedWithWildcards ) {
      if ( ch == L'\\' && !escaped ) { escaped = true; continue; }
      if ( ch == L']' && !escaped ) { insideSet = false; continue; }
      if ( insideSet ) { escaped = false; continue; }
      if ( ch == L'[' && !escaped ) { minMatchLength += 1; insideSet = true; continue; }
      if ( ch == L'*' && !escaped ) { continue; }
      escaped = false;
      minMatchLength += 1;
    }

    folded.clear();
    folded.reserve( foldedWithWildcards.size() );
    escaped = false;
    for ( char32_t ch : foldedWithWildcards ) {
      if ( escaped ) {
        if ( bNoLetters || ( ch != L'*' && ch != L'?' && ch != L'[' && ch != L']' ) ) { folded.push_back( ch ); }
        escaped = false; continue;
      }
      if ( ch == L'\\' ) {
        if ( bNoLetters || folded.empty() ) { escaped = true; continue; } else { break; }
      }
      if ( ch == '*' || ch == '?' || ch == '[' || ch == ']' ) { break; }
      folded.push_back( ch );
    }
  } else {
    if ( folded.empty() ) { folded = Folding::applyWhitespaceOnly( str ); }
  }

  int initialFoldedSize = folded.size();
  int charsLeftToChop = 0;
  if ( maxSuffixVariation >= 0 ) {
    charsLeftToChop = initialFoldedSize - (int)minLength;
    if ( charsLeftToChop < 0 ) { charsLeftToChop = 0; }
    else if ( charsLeftToChop > maxSuffixVariation ) { charsLeftToChop = maxSuffixVariation; }
  }

  try {
    for ( ;; ) {
      if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) break;

      string search_prefix = Text::toUtf8(folded);

      MDB_txn *txn;
      if (mdb_txn_begin(dict.env, nullptr, MDB_RDONLY, &txn) == MDB_SUCCESS) {
        MDB_cursor *cursor;
        mdb_cursor_open(txn, dict.dbi, &cursor);

        MDB_val key, data;
        key.mv_size = search_prefix.size();
        key.mv_data = (void*)search_prefix.c_str();

        int rc = mdb_cursor_get(cursor, &key, &data, MDB_SET_RANGE);

        while (rc == MDB_SUCCESS) {
          if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) break;

          string current_key((char*)key.mv_data, key.mv_size);
          std::u32string resultFolded = Text::toUtf32(current_key);

          if ( ( useWildcards && folded.empty() )
               || ( resultFolded.size() >= folded.size() && !resultFolded.compare( 0, folded.size(), folded ) ) ) {
            
            QByteArray ba((const char*)data.mv_data, data.mv_size);
            vector<WordArticleLink> chain = deserializeLinks(ba);

            for ( auto & x : chain ) {
              if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) break;
              if ( useWildcards ) {
                std::u32string word   = Text::toUtf32( x.word );
                std::u32string result = Folding::applyDiacriticsOnly( word );
                if ( result.size() >= (std::u32string::size_type)minMatchLength ) {
                  QRegularExpressionMatch match = regexp.match( QString::fromStdU32String( result ) );
                  if ( match.hasMatch() ) {
                    addMatch( word );
                  }
                }
              }
              else {
                if ( ( allowMiddleMatches || Folding::apply( Text::toUtf32( x.prefix ) ).empty() )
                     && ( maxSuffixVariation < 0
                          || (int)resultFolded.size() - initialFoldedSize <= maxSuffixVariation ) ) {
                  addMatch( Text::toUtf32( x.word ) );
                }
              }
              if ( matches.size() >= maxResults ) break;
            }
          } else {
            break; 
          }
          if ( matches.size() >= maxResults ) break;
          rc = mdb_cursor_get(cursor, &key, &data, MDB_NEXT);
        }

        mdb_cursor_close(cursor);
        mdb_txn_commit(txn);
      }

      if ( matches.size() >= maxResults ) break;

      if ( charsLeftToChop && !Utils::AtomicInt::loadAcquire( isCancelled ) ) {
        --charsLeftToChop;
        folded.resize( folded.size() - 1 );
      } else {
        break;
      }
    }
  }
  catch ( std::exception & e ) {
    qWarning( "Index searching failed: \"%s\", error: %s", dict.getName().c_str(), e.what() );
  }
  catch ( ... ) {
    qWarning( "Index searching failed: \"%s\"", dict.getName().c_str() );
  }
}
void BtreeWordSearchRequest::run()
{
  if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
    finish();
    return;
  }

  if ( dict.ensureInitDone().size() ) {
    setErrorString( QString::fromUtf8( dict.ensureInitDone().c_str() ) );
    finish();
    return;
  }

  findMatches();

  finish();
}
BtreeWordSearchRequest::~BtreeWordSearchRequest() { isCancelled.ref(); f.waitForFinished(); }

sptr< Dictionary::WordSearchRequest > BtreeDictionary::prefixMatch( const std::u32string & str, unsigned long maxResults )
{
  return std::make_shared< BtreeWordSearchRequest >( *this, str, 0, -1, true, maxResults );
}

sptr< Dictionary::WordSearchRequest > BtreeDictionary::stemmedMatch( const std::u32string & str, unsigned minLength, unsigned maxSuffixVariation, unsigned long maxResults )
{
  return std::make_shared< BtreeWordSearchRequest >( *this, str, minLength, (int)maxSuffixVariation, false, maxResults );
}

bool BtreeDictionary::getHeadwords( QStringList & headwords )
{
  QSet<QString> set;
  getAllHeadwords(set);
  headwords = set.values();
  return true;
}

void BtreeDictionary::getArticleText( uint32_t articleAddress, QString & headword, QString & text ) {}

void IndexedWords::addWord( const std::u32string & index_word, uint32_t articleOffset, unsigned int maxHeadwordSize )
{
  std::u32string word = Text::removeTrailingZero( index_word );
  
  if ( word.empty() )
    return;

  if ( word.size() > maxHeadwordSize ) {
    auto nonSpacePos = word.find_last_not_of( ' ', maxHeadwordSize );
    if ( nonSpacePos != std::u32string::npos ) {
      word = word.substr( 0, nonSpacePos );
    } else {
      word = word.substr( 0, maxHeadwordSize );
    }
  }

  size_t wordSize = word.size();
  const char32_t * wordBegin = word.c_str();

  while ( wordSize > 0 && Folding::isWhitespace( wordBegin[ wordSize - 1 ] ) ) {
    --wordSize;
  }

  std::u32string fullWord( wordBegin, wordSize );
  std::u32string fullFolded = Folding::apply( fullWord );
  if ( fullFolded.empty() ) {
    fullFolded = Folding::applyWhitespaceOnly( fullWord );
  }
  if ( !fullFolded.empty() ) {
    string foldedUtf8 = Text::toUtf8( fullFolded );
    string wordUtf8 = Text::toUtf8( fullWord );
    (*this)[ foldedUtf8 ].emplace_back( wordUtf8, articleOffset, "" );
  }

  const char32_t * nextChar = wordBegin;
  int wordsAdded = 0;

  while ( nextChar < wordBegin + wordSize ) {
    while ( nextChar < wordBegin + wordSize && Folding::isWhitespace( *nextChar ) ) {
      ++nextChar;
    }

    if ( nextChar >= wordBegin + wordSize )
      break;

    const char32_t * wordStart = nextChar;
    while ( nextChar < wordBegin + wordSize && !Folding::isWhitespace( *nextChar ) ) {
      ++nextChar;
    }

    std::u32string currentWord( wordStart, nextChar - wordStart );
    std::u32string folded = Folding::apply( currentWord );
    
    if ( folded.empty() ) {
      folded = Folding::applyWhitespaceOnly( currentWord );
    }

    if ( !folded.empty() ) {
      string foldedUtf8 = Text::toUtf8( folded );
      string wordUtf8 = Text::toUtf8( word );
      string prefixUtf8 = Text::toUtf8( std::u32string( wordBegin, wordStart - wordBegin ) );
      
      auto & chain = (*this)[ foldedUtf8 ];
      chain.emplace_back( wordUtf8, articleOffset, prefixUtf8 );
      wordsAdded++;
    }
  }

  if ( wordsAdded == 0 ) {
    std::u32string folded = Folding::applyWhitespaceOnly( word.substr( 0, wordSize ) );
    if ( !folded.empty() ) {
      string foldedUtf8 = Text::toUtf8( folded );
      string wordUtf8 = Text::toUtf8( word.substr( 0, wordSize ) );
      (*this)[ foldedUtf8 ].emplace_back( wordUtf8, articleOffset, "" );
    }
  }
}

void IndexedWords::addSingleWord( const std::u32string & index_word, uint32_t articleOffset )
{
  const std::u32string & word = Text::removeTrailingZero( index_word );
  std::u32string folded       = Folding::apply( word );
  if ( folded.empty() ) {
    folded = Folding::applyWhitespaceOnly( word );
  }
  operator[]( Text::toUtf8( folded ) ).emplace_back( Text::toUtf8( word ), articleOffset );
}

static QByteArray serializeLinks(const vector<WordArticleLink>& links) {
    QByteArray data;
    QDataStream out(&data, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << (quint32)links.size();
    for (const auto& link : links) {
        out << QString::fromStdString(link.word);
        out << QString::fromStdString(link.prefix);
        out << (quint32)link.articleOffset;
    }
    return data;
}

static vector<WordArticleLink> deserializeLinks(const QByteArray& data) {
    vector<WordArticleLink> links;
    QDataStream in(data);
    in.setVersion(QDataStream::Qt_6_0);
    quint32 size;
    in >> size;
    links.reserve(size);
    for (quint32 i = 0; i < size; ++i) {
        QString word, prefix;
        quint32 offset;
        in >> word >> prefix >> offset;
        links.emplace_back(word.toStdString(), offset, prefix.toStdString());
    }
    return links;
}

IndexInfo buildIndex( const IndexedWords & indexedWords, File::Index & file, const string & suffix )
{
    qDebug() << "LMDB buildIndex started, entries:" << indexedWords.size() << "suffix:" << QString::fromStdString(suffix);
    
    MDB_env *env = nullptr;
    mdb_env_create(&env);
    if (!env) {
        qWarning() << "LMDB env create failed";
        return IndexInfo(0, 0);
    }
    
    size_t entryCount = indexedWords.size();
    size_t estimatedSize = entryCount * 250;  // ~125 bytes per entry
    
    const size_t minSize = 1 * 1024 * 1024;    // Minimum 1MB
    const size_t maxSize = 1000 * 1024 * 1024;  // Maximum 100MB
    
    if (estimatedSize < minSize) {
        estimatedSize = minSize;
    } else if (estimatedSize > maxSize) {
        estimatedSize = maxSize;
    }
    
    mdb_env_set_mapsize(env, estimatedSize);
    
    string lmdbPath = file.file().fileName().toStdString() + suffix + ".lmdb";
    qDebug() << "LMDB building index at:" << QString::fromStdString(lmdbPath);
    
    int rc = mdb_env_open(env, lmdbPath.c_str(), MDB_NOSUBDIR, 0664);
    if (rc != MDB_SUCCESS) {
        qWarning() << "LMDB build open failed: " << mdb_strerror(rc);
        mdb_env_close(env);
        return IndexInfo(0, 0);
    }
    
    MDB_txn *txn = nullptr;
    rc = mdb_txn_begin(env, nullptr, 0, &txn);
    if (rc != MDB_SUCCESS) {
        qWarning() << "LMDB txn begin failed: " << mdb_strerror(rc);
        mdb_env_close(env);
        return IndexInfo(0, 0);
    }
    
    MDB_dbi dbi = 0;
    rc = mdb_dbi_open(txn, nullptr, MDB_CREATE, &dbi);
    if (rc != MDB_SUCCESS) {
        qWarning() << "LMDB dbi open failed: " << mdb_strerror(rc);
        mdb_txn_abort(txn);
        mdb_env_close(env);
        return IndexInfo(0, 0);
    }

    qDebug() << "LMDB writing" << indexedWords.size() << "entries to index";

    const int maxRetry = 3;
    bool writeSuccess = false;
    int retryCount = 0;
    
    do {
        writeSuccess = true;
        
        for (auto it = indexedWords.begin(); it != indexedWords.end() && writeSuccess; ++it) {
            QByteArray data = serializeLinks(it->second);
            MDB_val key, val;
            key.mv_size = it->first.size();
            key.mv_data = (void*)it->first.c_str();
            val.mv_size = data.size();
            val.mv_data = (void*)data.constData();
            
            int rcPut = mdb_put(txn, dbi, &key, &val, 0);
            if (rcPut != MDB_SUCCESS) {
                if (rcPut == MDB_MAP_FULL && retryCount < maxRetry) {
                    qWarning() << "LMDB mdb_put failed: MDB_MAP_FULL, expanding mapsize and retrying";
                    mdb_txn_abort(txn);
                    mdb_env_close(env);
                    
                    estimatedSize *= 2;
                    if (estimatedSize > maxSize) {
                        estimatedSize = maxSize;
                    }
                    
                    qDebug() << "LMDB expanding mapsize to" << estimatedSize;
                    
                    mdb_env_create(&env);
                    mdb_env_set_mapsize(env, estimatedSize);
                    
                    rc = mdb_env_open(env, lmdbPath.c_str(), MDB_NOSUBDIR, 0664);
                    if (rc != MDB_SUCCESS) {
                        qWarning() << "LMDB re-open failed after expansion: " << mdb_strerror(rc);
                        mdb_env_close(env);
                        QFile::remove(QString::fromStdString(lmdbPath));
                        return IndexInfo(0, 0, suffix);
                    }
                    
                    rc = mdb_txn_begin(env, nullptr, 0, &txn);
                    if (rc != MDB_SUCCESS) {
                        qWarning() << "LMDB txn begin failed after expansion: " << mdb_strerror(rc);
                        mdb_env_close(env);
                        QFile::remove(QString::fromStdString(lmdbPath));
                        return IndexInfo(0, 0, suffix);
                    }
                    
                    rc = mdb_dbi_open(txn, nullptr, MDB_CREATE, &dbi);
                    if (rc != MDB_SUCCESS) {
                        qWarning() << "LMDB dbi open failed after expansion: " << mdb_strerror(rc);
                        mdb_txn_abort(txn);
                        mdb_env_close(env);
                        QFile::remove(QString::fromStdString(lmdbPath));
                        return IndexInfo(0, 0, suffix);
                    }
                    
                    retryCount++;
                    writeSuccess = false;
                } else {
                    qWarning() << "LMDB mdb_put failed: " << mdb_strerror(rcPut);
                    mdb_txn_abort(txn);
                    mdb_env_close(env);
                    QFile::remove(QString::fromStdString(lmdbPath));
                    return IndexInfo(0, 0, suffix);
                }
            }
        }
    } while (!writeSuccess && retryCount <= maxRetry);

    qDebug() << "LMDB preparing to commit transaction";
    
    int rcCommit = mdb_txn_commit(txn);
    if (rcCommit != MDB_SUCCESS) {
        qWarning() << "LMDB txn commit failed: " << mdb_strerror(rcCommit);
        qWarning() << "LMDB error details:";
        qWarning() << "  - entries:" << indexedWords.size();
        qWarning() << "  - estimatedSize:" << estimatedSize;
        qWarning() << "  - lmdbPath:" << QString::fromStdString(lmdbPath);
        
        mdb_env_close(env);
        QFile::remove(QString::fromStdString(lmdbPath));
        return IndexInfo(0, 0, suffix);
    }
    
    qDebug() << "LMDB txn committed successfully";
    
    MDB_stat stat;
    mdb_env_stat(env, &stat);
    
    mdb_env_close(env);

    string tempPath = lmdbPath + "_compact";
    qDebug() << "LMDB compacting from" << QString::fromStdString(lmdbPath) << "to" << QString::fromStdString(tempPath);
    
    MDB_env *envCopy = nullptr;
    mdb_env_create(&envCopy);
    
    qDebug() << "LMDB compact - pagesize:" << stat.ms_psize 
             << ", entries:" << stat.ms_entries;
    
    int rcCopy = mdb_env_open(envCopy, lmdbPath.c_str(), MDB_RDONLY | MDB_NOSUBDIR, 0664);
    if (rcCopy == MDB_SUCCESS) {
        QFile::remove(QString::fromStdString(tempPath));
        
        rcCopy = mdb_env_copy2(envCopy, tempPath.c_str(), MDB_CP_COMPACT);
        mdb_env_close(envCopy);
        
        if (rcCopy == MDB_SUCCESS) {
            QFile::remove(QString::fromStdString(lmdbPath));
            QFile::rename(QString::fromStdString(tempPath), QString::fromStdString(lmdbPath));
            qDebug() << "LMDB compact succeeded";
        } else {
            qWarning() << "LMDB compact failed: " << mdb_strerror(rcCopy);
        }
    } else {
        qWarning() << "LMDB open for compact failed: " << mdb_strerror(rcCopy);
        mdb_env_close(envCopy);
    }
    
    return IndexInfo(0, 0, suffix);
}

} // namespace BtreeIndexing
