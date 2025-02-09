/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "btreeidx.hh"
#include "chunkedstorage.hh"
#include "decompress.hh"
#include "folding.hh"
#include "ftshelpers.hh"
#include "htmlescape.hh"
#include "langcoder.hh"
#include "sdict.hh"
#include "text.hh"
#include <map>
#include <QAtomicInt>
#include <QDir>
#include <QRegularExpression>
#include <QString>
#include <set>
#include <string>


namespace Sdict {

using std::map;
using std::multimap;
using std::pair;
using std::set;
using std::string;

using BtreeIndexing::WordArticleLink;
using BtreeIndexing::IndexedWords;
using BtreeIndexing::IndexInfo;

namespace {

DEF_EX_STR( exNotDctFile, "Not an Sdictionary file", Dictionary::Ex )
using Dictionary::exCantReadFile;
DEF_EX_STR( exWordIsTooLarge, "Enountered a word that is too large:", Dictionary::Ex )
DEF_EX_STR( exSuddenEndOfFile, "Sudden end of file", Dictionary::Ex )

#pragma pack( push, 1 )

/// DCT file header
struct DCT_header
{
  char signature[ 4 ];
  char inputLang[ 3 ];
  char outputLang[ 3 ];
  uint8_t compression;
  uint32_t wordCount;
  uint32_t shortIndexLength;
  uint32_t titleOffset;
  uint32_t copyrightOffset;
  uint32_t versionOffset;
  uint32_t shortIndexOffset;
  uint32_t fullIndexOffset;
  uint32_t articlesOffset;
};
static_assert( alignof( DCT_header ) == 1 );

struct IndexElement
{
  uint16_t nextWord;
  uint16_t previousWord;
  uint32_t articleOffset;
};
static_assert( alignof( IndexElement ) == 1 );

enum {
  Signature            = 0x43494453, // SDIC on little-endian, CIDS on big-endian
  CurrentFormatVersion = 1 + BtreeIndexing::FormatVersion + Folding::Version
};

struct IdxHeader
{
  uint32_t signature;             // First comes the signature, SDIC
  uint32_t formatVersion;         // File format version (CurrentFormatVersion)
  uint32_t chunksOffset;          // The offset to chunks' storage
  uint32_t indexBtreeMaxElements; // Two fields from IndexInfo
  uint32_t indexRootOffset;
  uint32_t wordCount;
  uint32_t articleCount;
  uint32_t compressionType; // Data compression in file. 0 - no compression, 1 - zip, 2 - bzip2
  uint32_t langFrom;        // Source language
  uint32_t langTo;          // Target language
};
static_assert( alignof( IdxHeader ) == 1 );
#pragma pack( pop )

bool indexIsOldOrBad( string const & indexFile )
{
  File::Index idx( indexFile, QIODevice::ReadOnly );

  IdxHeader header;

  return idx.readRecords( &header, sizeof( header ), 1 ) != 1 || header.signature != Signature
    || header.formatVersion != CurrentFormatVersion;
}

class SdictDictionary: public BtreeIndexing::BtreeDictionary
{
  QMutex idxMutex, sdictMutex;
  File::Index idx;
  IdxHeader idxHeader;
  ChunkedStorage::Reader chunks;
  File::Index df; // Not an index, uses this type for legacy reasons.

public:

  SdictDictionary( string const & id, string const & indexFile, vector< string > const & dictionaryFiles );

  ~SdictDictionary();


  unsigned long getArticleCount() noexcept override
  {
    return idxHeader.articleCount;
  }

  unsigned long getWordCount() noexcept override
  {
    return idxHeader.wordCount;
  }

  inline quint32 getLangFrom() const override
  {
    return idxHeader.langFrom;
  }

  inline quint32 getLangTo() const override
  {
    return idxHeader.langTo;
  }

  sptr< Dictionary::DataRequest > getArticle( std::u32string const &,
                                              vector< std::u32string > const & alts,
                                              std::u32string const &,
                                              bool ignoreDiacritics ) override;

  QString const & getDescription() override;

  sptr< Dictionary::DataRequest >
  getSearchResults( QString const & searchString, int searchMode, bool matchCase, bool ignoreDiacritics ) override;
  void getArticleText( uint32_t articleAddress, QString & headword, QString & text ) override;

  void makeFTSIndex( QAtomicInt & isCancelled ) override;

  void setFTSParameters( Config::FullTextSearch const & fts ) override
  {
    if ( metadata_enable_fts.has_value() ) {
      can_FTS = fts.enabled && metadata_enable_fts.value();
    }
    else {
      can_FTS = fts.enabled && !fts.disabledTypes.contains( "SDICT", Qt::CaseInsensitive )
        && ( fts.maxDictionarySize == 0 || getArticleCount() <= fts.maxDictionarySize );
    }
  }

protected:

  void loadIcon() noexcept override;

private:

  /// Loads the article.
  void loadArticle( uint32_t address, string & articleText );
  string convert( string const & in_data );

  friend class SdictArticleRequest;
};

SdictDictionary::SdictDictionary( string const & id,
                                  string const & indexFile,
                                  vector< string > const & dictionaryFiles ):
  BtreeDictionary( id, dictionaryFiles ),
  idx( indexFile, QIODevice::ReadOnly ),
  idxHeader( idx.read< IdxHeader >() ),
  chunks( idx, idxHeader.chunksOffset ),
  df( dictionaryFiles[ 0 ], QIODevice::ReadOnly )
{
  // Read dictionary name

  idx.seek( sizeof( idxHeader ) );
  idx.readU32SizeAndData<>( dictionaryName );

  // Initialize the index

  openIndex( IndexInfo( idxHeader.indexBtreeMaxElements, idxHeader.indexRootOffset ), idx, idxMutex );

  // Full-text search parameters

  ftsIdxName = indexFile + Dictionary::getFtsSuffix();
}

SdictDictionary::~SdictDictionary()
{
  df.close();
}

void SdictDictionary::loadIcon() noexcept
{
  if ( dictionaryIconLoaded ) {
    return;
  }

  QString fileName = QDir::fromNativeSeparators( getDictionaryFilenames()[ 0 ].c_str() );

  if ( !loadIconFromFileName( fileName ) ) {
    // Load failed -- use default icons
    dictionaryIcon = QIcon( ":/icons/icon32_sdict.png" );
  }

  dictionaryIconLoaded = true;
}

string SdictDictionary::convert( string const & in )
{
  //    qDebug( "Source>>>>>>>>>>: %s\n\n", in.c_str() );

  string inConverted;

  inConverted.reserve( in.size() );

  bool afterEol = false;

  for ( char i : in ) {
    switch ( i ) {
      case '\n':
        afterEol = true;
        inConverted.append( "<br/>" );
        break;

      case ' ':
        if ( afterEol ) {
          inConverted.append( "&nbsp;" );
          break;
        }
        // Fall-through

      default:
        inConverted.push_back( i );
        afterEol = false;
    }
  }

  QString result = QString::fromUtf8( inConverted.c_str(), inConverted.size() );

  result.replace( QRegularExpression( "<\\s*(p|br)\\s*>", QRegularExpression::CaseInsensitiveOption ), "<br/>" );
  result.remove( QRegularExpression( "<\\s*/p\\s*>", QRegularExpression::CaseInsensitiveOption ) );

  result.replace( QRegularExpression( "<\\s*t\\s*>", QRegularExpression::CaseInsensitiveOption ),
                  R"(<span class="sdict_tr" dir="ltr">)" );
  result.replace( QRegularExpression( "<\\s*f\\s*>", QRegularExpression::CaseInsensitiveOption ),
                  "<span class=\"sdict_forms\">" );
  result.replace( QRegularExpression( "<\\s*/(t|f)\\s*>", QRegularExpression::CaseInsensitiveOption ), "</span>" );

  result.replace( QRegularExpression( "<\\s*l\\s*>", QRegularExpression::CaseInsensitiveOption ), "<ul>" );
  result.replace( QRegularExpression( "<\\s*/l\\s*>", QRegularExpression::CaseInsensitiveOption ), "</ul>" );


  // Links handling

  qsizetype n = 0;
  for ( ;; ) {
    QRegularExpression start_link_tag( "<\\s*r\\s*>", QRegularExpression::CaseInsensitiveOption );
    QRegularExpression end_link_tag( "<\\s*/r\\s*>", QRegularExpression::CaseInsensitiveOption );

    n = result.indexOf( start_link_tag, n );
    if ( n < 0 ) {
      break;
    }

    auto end = result.indexOf( end_link_tag, n );
    if ( end < 0 ) {
      break;
    }

    QRegularExpressionMatch m = start_link_tag.match( result, 0, QRegularExpression::PartialPreferFirstMatch );
    auto tag_len              = m.captured().length();
    QString link_text         = result.mid( n + tag_len, end - n - tag_len );

    m = end_link_tag.match( result, 0, QRegularExpression::PartialPreferFirstMatch );
    result.replace( end, m.captured().length(), "</a>" );
    result.replace( n, tag_len, QString( R"(<a class="sdict_wordref" href="bword:)" ) + link_text + "\">" );
  }

  // Adjust text direction for lines

  n      = 0;
  bool b = true;
  while ( b ) {
    auto next = result.indexOf( "<br/>", n );
    if ( next < 0 ) {
      next = result.length();
      b    = false;
    }

    if ( !result.mid( n, next - n ).contains( '<' ) ) {
      if ( Html::unescape( result.mid( n, next - n ) ).isRightToLeft() != isToLanguageRTL() ) {
        result.insert( next, "</span>" );
        result.insert( n, QString( "<span dir = \"" ) + ( isToLanguageRTL() ? "ltr" : "rtl" ) + "\">" );
        next = result.indexOf( "<br/>", n );
      }
    }

    n = next + 5;
  }

  return result.toUtf8().data();
}

void SdictDictionary::loadArticle( uint32_t address, string & articleText )
{
  uint32_t articleOffset = address;
  uint32_t articleSize;

  vector< char > articleBody;

  {
    QMutexLocker _( &sdictMutex );
    df.seek( articleOffset );
    df.read( &articleSize, sizeof( articleSize ) );
    articleBody.resize( articleSize );
    df.read( &articleBody.front(), articleSize );
  }

  if ( articleBody.empty() ) {
    throw exCantReadFile( getDictionaryFilenames()[ 0 ] );
  }

  if ( idxHeader.compressionType == 1 ) {
    articleText = decompressZlib( articleBody.data(), articleSize );
  }
  else if ( idxHeader.compressionType == 2 ) {
    articleText = decompressBzip2( articleBody.data(), articleSize );
  }
  else {
    articleText = string( articleBody.data(), articleSize );
  }

  articleText = convert( articleText );

  string div = "<div class=\"sdict\"";
  if ( isToLanguageRTL() ) {
    div += " dir=\"rtl\"";
  }
  div += ">";

  articleText.insert( 0, div );
  articleText.append( "</div>" );
}

void SdictDictionary::makeFTSIndex( QAtomicInt & isCancelled )
{
  if ( !( Dictionary::needToRebuildIndex( getDictionaryFilenames(), ftsIdxName )
          || FtsHelpers::ftsIndexIsOldOrBad( this ) ) ) {
    FTS_index_completed.ref();
  }

  if ( haveFTSIndex() ) {
    return;
  }

  if ( ensureInitDone().size() ) {
    return;
  }


  qDebug( "SDict: Building the full-text index for dictionary: %s", getName().c_str() );

  try {
    FtsHelpers::makeFTSIndex( this, isCancelled );
    FTS_index_completed.ref();
  }
  catch ( std::exception & ex ) {
    qWarning( "SDict: Failed building full-text search index for \"%s\", reason: %s", getName().c_str(), ex.what() );
    QFile::remove( ftsIdxName.c_str() );
  }
}

void SdictDictionary::getArticleText( uint32_t articleAddress, QString & headword, QString & text )
{
  try {
    string articleStr;
    headword.clear();
    text.clear();

    loadArticle( articleAddress, articleStr );

    try {
      text = Html::unescape( QString::fromStdString( articleStr ) );
    }
    catch ( std::exception & ) {
    }
  }
  catch ( std::exception & ex ) {
    qWarning( "SDict: Failed retrieving article from \"%s\", reason: %s", getName().c_str(), ex.what() );
  }
}

sptr< Dictionary::DataRequest >
SdictDictionary::getSearchResults( QString const & searchString, int searchMode, bool matchCase, bool ignoreDiacritics )
{
  return std::make_shared< FtsHelpers::FTSResultsRequest >( *this,
                                                            searchString,
                                                            searchMode,
                                                            matchCase,
                                                            ignoreDiacritics );
}

/// SdictDictionary::getArticle()


class SdictArticleRequest: public Dictionary::DataRequest
{

  std::u32string word;
  vector< std::u32string > alts;
  SdictDictionary & dict;
  bool ignoreDiacritics;

  QAtomicInt isCancelled;

  QFuture< void > f;

public:

  SdictArticleRequest( std::u32string const & word_,
                       vector< std::u32string > const & alts_,
                       SdictDictionary & dict_,
                       bool ignoreDiacritics_ ):
    word( word_ ),
    alts( alts_ ),
    dict( dict_ ),
    ignoreDiacritics( ignoreDiacritics_ )
  {
    f = QtConcurrent::run( [ this ]() {
      this->run();
    } );
  }

  void run();

  void cancel() override
  {
    isCancelled.ref();
  }

  ~SdictArticleRequest()
  {
    isCancelled.ref();
    f.waitForFinished();
  }
};

void SdictArticleRequest::run()
{
  if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
    finish();
    return;
  }

  vector< WordArticleLink > chain = dict.findArticles( word, ignoreDiacritics );

  for ( const auto & alt : alts ) {
    /// Make an additional query for each alt

    vector< WordArticleLink > altChain = dict.findArticles( alt, ignoreDiacritics );

    chain.insert( chain.end(), altChain.begin(), altChain.end() );
  }

  multimap< std::u32string, pair< string, string > > mainArticles, alternateArticles;

  set< uint32_t > articlesIncluded; // Some synonims make it that the articles
                                    // appear several times. We combat this
                                    // by only allowing them to appear once.

  std::u32string wordCaseFolded = Folding::applySimpleCaseOnly( word );
  if ( ignoreDiacritics ) {
    wordCaseFolded = Folding::applyDiacriticsOnly( wordCaseFolded );
  }

  for ( auto & x : chain ) {
    if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
      finish();
      return;
    }

    if ( articlesIncluded.find( x.articleOffset ) != articlesIncluded.end() ) {
      continue; // We already have this article in the body.
    }

    // Now grab that article

    string headword, articleText;

    headword = x.word;

    try {
      dict.loadArticle( x.articleOffset, articleText );

      // Ok. Now, does it go to main articles, or to alternate ones? We list
      // main ones first, and alternates after.

      // We do the case-folded comparison here.

      std::u32string headwordStripped = Folding::applySimpleCaseOnly( headword );
      if ( ignoreDiacritics ) {
        headwordStripped = Folding::applyDiacriticsOnly( headwordStripped );
      }

      multimap< std::u32string, pair< string, string > > & mapToUse =
        ( wordCaseFolded == headwordStripped ) ? mainArticles : alternateArticles;

      mapToUse.insert( pair( Folding::applySimpleCaseOnly( headword ), pair( headword, articleText ) ) );

      articlesIncluded.insert( x.articleOffset );
    }
    catch ( std::exception & ex ) {
      qWarning( "SDict: Failed loading article from \"%s\", reason: %s", dict.getName().c_str(), ex.what() );
    }
  }

  if ( mainArticles.empty() && alternateArticles.empty() ) {
    // No such word
    finish();
    return;
  }

  string result;

  multimap< std::u32string, pair< string, string > >::const_iterator i;

  for ( i = mainArticles.begin(); i != mainArticles.end(); ++i ) {
    result += dict.isFromLanguageRTL() ? "<h3 dir=\"rtl\">" : "<h3>";
    result += i->second.first;
    result += "</h3>";
    result += i->second.second;
  }

  for ( i = alternateArticles.begin(); i != alternateArticles.end(); ++i ) {
    result += dict.isFromLanguageRTL() ? "<h3 dir=\"rtl\">" : "<h3>";
    result += i->second.first;
    result += "</h3>";
    if ( dict.isToLanguageRTL() ) {
      result += "<span dir=\"rtl\">";
    }
    result += i->second.second;
    if ( dict.isToLanguageRTL() ) {
      result += "</span>";
    }
  }

  appendString( result );

  hasAnyData = true;

  finish();
}

sptr< Dictionary::DataRequest > SdictDictionary::getArticle( std::u32string const & word,
                                                             vector< std::u32string > const & alts,
                                                             std::u32string const &,
                                                             bool ignoreDiacritics )

{
  return std::make_shared< SdictArticleRequest >( word, alts, *this, ignoreDiacritics );
}

QString const & SdictDictionary::getDescription()
{
  if ( !dictionaryDescription.isEmpty() ) {
    return dictionaryDescription;
  }

  dictionaryDescription = QObject::tr( "Title: %1%2" ).arg( QString::fromUtf8( getName().c_str() ) ).arg( "\n\n" );

  try {
    QMutexLocker _( &sdictMutex );

    DCT_header dictHeader;

    df.seek( 0 );
    if ( df.readRecords( &dictHeader, sizeof( dictHeader ), 1 ) != 1 ) {
      throw exCantReadFile( getDictionaryFilenames()[ 0 ] );
    }

    int compression = dictHeader.compression & 0x0F;

    vector< char > data;
    uint32_t size;
    string str;

    df.seek( dictHeader.copyrightOffset );
    df.read( &size, sizeof( size ) );
    data.resize( size );
    df.read( &data.front(), size );

    if ( compression == 1 ) {
      str = decompressZlib( data.data(), size );
    }
    else if ( compression == 2 ) {
      str = decompressBzip2( data.data(), size );
    }
    else {
      str = string( data.data(), size );
    }

    dictionaryDescription +=
      QObject::tr( "Copyright: %1%2" ).arg( QString::fromUtf8( str.c_str(), str.size() ) ).arg( "\n\n" );

    df.seek( dictHeader.versionOffset );
    df.read( &size, sizeof( size ) );
    data.resize( size );
    df.read( &data.front(), size );

    if ( compression == 1 ) {
      str = decompressZlib( data.data(), size );
    }
    else if ( compression == 2 ) {
      str = decompressBzip2( data.data(), size );
    }
    else {
      str = string( data.data(), size );
    }

    dictionaryDescription +=
      QObject::tr( "Version: %1%2" ).arg( QString::fromUtf8( str.c_str(), str.size() ) ).arg( "\n\n" );
  }
  catch ( std::exception & ex ) {
    qWarning( "SDict: Failed description reading for \"%s\", reason: %s", getName().c_str(), ex.what() );
  }

  if ( dictionaryDescription.isEmpty() ) {
    dictionaryDescription = "NONE";
  }

  return dictionaryDescription;
}

} // anonymous namespace

vector< sptr< Dictionary::Class > > makeDictionaries( vector< string > const & fileNames,
                                                      string const & indicesDir,
                                                      Dictionary::Initializing & initializing )

{
  vector< sptr< Dictionary::Class > > dictionaries;

  for ( const auto & fileName : fileNames ) {
    // Skip files with the extensions different to .dct to speed up the
    // scanning
    if ( !Utils::endsWithIgnoreCase( fileName, ".dct" ) ) {
      continue;
    }

    // Got the file -- check if we need to rebuid the index

    vector< string > dictFiles( 1, fileName );

    string dictId = Dictionary::makeDictionaryId( dictFiles );

    string indexFile = indicesDir + dictId;

    if ( Dictionary::needToRebuildIndex( dictFiles, indexFile ) || indexIsOldOrBad( indexFile ) ) {
      try {
        qDebug( "SDict: Building the index for dictionary: %s", fileName.c_str() );

        File::Index df( fileName, QIODevice::ReadOnly );

        DCT_header dictHeader;

        df.read( &dictHeader, sizeof( dictHeader ) );
        if ( strncmp( dictHeader.signature, "sdct", 4 ) ) {
          qWarning( "File \"%s\" is not valid SDictionary file", fileName.c_str() );
          continue;
        }
        int compression = dictHeader.compression & 0x0F;

        vector< char > data;
        uint32_t size;

        df.seek( dictHeader.titleOffset );
        df.read( &size, sizeof( size ) );
        data.resize( size );
        df.read( &data.front(), size );

        string dictName;

        if ( compression == 1 ) {
          dictName = decompressZlib( data.data(), size );
        }
        else if ( compression == 2 ) {
          dictName = decompressBzip2( data.data(), size );
        }
        else {
          dictName = string( data.data(), size );
        }

        initializing.indexingDictionary( dictName );

        File::Index idx( indexFile, QIODevice::WriteOnly );
        IdxHeader idxHeader;
        memset( &idxHeader, 0, sizeof( idxHeader ) );

        // We write a dummy header first. At the end of the process the header
        // will be rewritten with the right values.

        idx.write( idxHeader );

        idx.write( (uint32_t)dictName.size() );
        idx.write( dictName.data(), dictName.size() );

        IndexedWords indexedWords;

        ChunkedStorage::Writer chunks( idx );

        uint32_t wordCount = 0;
        set< uint32_t > articleOffsets;
        uint32_t pos = dictHeader.fullIndexOffset;

        for ( uint32_t j = 0; j < dictHeader.wordCount; j++ ) {
          IndexElement el;
          df.seek( pos );
          df.read( &el, sizeof( el ) );
          uint32_t articleOffset = dictHeader.articlesOffset + el.articleOffset;
          size                   = el.nextWord - sizeof( el );
          if ( el.nextWord < sizeof( el ) ) {
            break;
          }
          wordCount++;
          data.resize( size );
          df.read( &data.front(), size );

          if ( articleOffsets.find( articleOffset ) == articleOffsets.end() ) {
            articleOffsets.insert( articleOffset );
          }

          // Insert new entry

          indexedWords.addWord( Text::toUtf32( string( data.data(), size ) ), articleOffset );

          pos += el.nextWord;
        }
        // Finish with the chunks

        idxHeader.chunksOffset = chunks.finish();

        // Build index

        IndexInfo idxInfo = BtreeIndexing::buildIndex( indexedWords, idx );

        idxHeader.indexBtreeMaxElements = idxInfo.btreeMaxElements;
        idxHeader.indexRootOffset       = idxInfo.rootOffset;

        indexedWords.clear(); // Release memory -- no need for this data

        // That concludes it. Update the header.

        idxHeader.signature     = Signature;
        idxHeader.formatVersion = CurrentFormatVersion;

        idxHeader.articleCount = articleOffsets.size();
        idxHeader.wordCount    = wordCount;

        idxHeader.langFrom        = LangCoder::code2toInt( dictHeader.inputLang );
        idxHeader.langTo          = LangCoder::code2toInt( dictHeader.outputLang );
        idxHeader.compressionType = compression;

        idx.rewind();

        idx.write( &idxHeader, sizeof( idxHeader ) );
      }
      catch ( std::exception & e ) {
        qWarning( "Sdictionary dictionary indexing failed: %s, error: %s", fileName.c_str(), e.what() );
        continue;
      }
      catch ( ... ) {
        qWarning( "Sdictionary dictionary indexing failed" );
        continue;
      }
    } // if need to rebuild
    try {
      dictionaries.push_back( std::make_shared< SdictDictionary >( dictId, indexFile, dictFiles ) );
    }
    catch ( std::exception & e ) {
      qWarning( "Sdictionary dictionary initializing failed: %s, error: %s", fileName.c_str(), e.what() );
    }
  }
  return dictionaries;
}

} // namespace Sdict
