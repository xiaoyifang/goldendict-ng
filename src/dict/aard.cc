/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "aard.hh"
#include "btreeidx.hh"
#include "folding.hh"
#include "text.hh"
#include "chunkedstorage.hh"
#include "langcoder.hh"
#include "decompress.hh"
#include "ftshelpers.hh"
#include "htmlescape.hh"

#include <map>
#include <set>
#include <string>

#include <QDir>
#include <QString>
#include <QAtomicInt>
#include <QtEndian>
#include <QRegularExpression>
#include "utils.hh"

namespace Aard {

using std::map;
using std::multimap;
using std::pair;
using std::set;
using std::string;

using BtreeIndexing::WordArticleLink;
using BtreeIndexing::IndexedWords;
using BtreeIndexing::IndexInfo;

namespace {

#pragma pack( push, 1 )

/// AAR file header
struct AAR_header
{
  char signature[ 4 ];
  char checksum[ 40 ];
  quint16 version;
  char uuid[ 16 ];
  quint16 volume;
  quint16 totalVolumes;
  quint32 metaLength;
  quint32 wordsCount;
  quint32 articleOffset;
  char indexItemFormat[ 4 ];
  char keyLengthFormat[ 2 ];
  char articleLengthFormat[ 2 ];
};
static_assert( alignof( AAR_header ) == 1 );

struct IndexElement
{
  quint32 wordOffset;
  quint32 articleOffset;
};
static_assert( alignof( IndexElement ) == 1 );

struct IndexElement64
{
  quint32 wordOffset;
  quint64 articleOffset;
};
static_assert( alignof( IndexElement64 ) == 1 );


enum {
  Signature            = 0x58524141, // AARX on little-endian, XRAA on big-endian
  CurrentFormatVersion = 4 + BtreeIndexing::FormatVersion + Folding::Version
};

struct IdxHeader
{
  quint32 signature;             // First comes the signature, AARX
  quint32 formatVersion;         // File format version (CurrentFormatVersion)
  quint32 chunksOffset;          // The offset to chunks' storage
  quint32 indexBtreeMaxElements; // Two fields from IndexInfo
  quint32 indexRootOffset;
  quint32 wordCount;
  quint32 articleCount;
  quint32 langFrom; // Source language
  quint32 langTo;   // Target language
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

void readJSONValue( string const & source, string & str, string::size_type & pos )
{
  int level = 1;
  char endChar;
  str.push_back( source[ pos ] );
  if ( source[ pos ] == '{' ) {
    endChar = '}';
  }
  else if ( source[ pos ] == '[' ) {
    endChar = ']';
  }
  else if ( source[ pos ] == '\"' ) {
    str.clear();
    endChar = '\"';
  }
  else {
    endChar = ',';
  }

  pos++;
  char ch     = 0;
  char lastCh = 0;
  while ( !( ch == endChar && lastCh != '\\' && level == 0 ) && pos < source.size() ) {
    lastCh = ch;
    ch     = source[ pos++ ];
    if ( ( ch == '{' || ch == '[' ) && lastCh != '\\' ) {
      level++;
    }
    if ( ( ch == '}' || ch == ']' ) && lastCh != '\\' ) {
      level--;
    }

    if ( ch == endChar && ( ( ch == '\"' && lastCh != '\\' ) || ch == ',' ) && level == 1 ) {
      break;
    }
    str.push_back( ch );
  }
}

map< string, string > parseMetaData( string const & metaData )
{
  // Parsing JSON string
  map< string, string > data;
  string name, value;
  string::size_type n = 0;

  while ( n < metaData.length() && metaData[ n ] != '{' ) {
    n++;
  }
  while ( n < metaData.length() ) {
    // Skip to '"'
    while ( n < metaData.length() && metaData[ n ] != '\"' ) {
      n++;
    }
    if ( ++n >= metaData.length() ) {
      break;
    }

    // Read name
    while ( n < metaData.length()
            && !( ( metaData[ n ] == '\"' || metaData[ n ] == '{' ) && metaData[ n - 1 ] != '\\' ) ) {
      name.push_back( metaData[ n++ ] );
    }

    // Skip to ':'
    if ( ++n >= metaData.length() ) {
      break;
    }
    while ( n < metaData.length() && metaData[ n ] != ':' ) {
      n++;
    }
    if ( ++n >= metaData.length() ) {
      break;
    }

    // Find value start after ':'
    while ( n < metaData.length()
            && !( ( metaData[ n ] == '\"' || metaData[ n ] == '{' || metaData[ n ] == '['
                    || ( metaData[ n ] >= '0' && metaData[ n ] <= '9' ) )
                  && metaData[ n - 1 ] != '\\' ) ) {
      n++;
    }
    if ( n >= metaData.length() ) {
      break;
    }

    readJSONValue( metaData, value, n );

    data[ name ] = value;

    name.clear();
    value.clear();
    if ( ++n >= metaData.length() ) {
      break;
    }
  }
  return data;
}

class AardDictionary: public BtreeIndexing::BtreeDictionary
{
  QMutex idxMutex;
  QMutex aardMutex;
  File::Index idx;
  IdxHeader idxHeader;
  ChunkedStorage::Reader chunks;
  File::Index df;

public:

  AardDictionary( string const & id, string const & indexFile, vector< string > const & dictionaryFiles );

  ~AardDictionary();

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
      can_FTS = fts.enabled && !fts.disabledTypes.contains( "AARD", Qt::CaseInsensitive )
        && ( fts.maxDictionarySize == 0 || getArticleCount() <= fts.maxDictionarySize );
    }
  }

protected:

  void loadIcon() noexcept override;

private:

  /// Loads the article.
  void loadArticle( quint32 address, string & articleText, bool rawText = false );
  string convert( string const & in_data );

  friend class AardArticleRequest;
};

AardDictionary::AardDictionary( string const & id, string const & indexFile, vector< string > const & dictionaryFiles ):
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

AardDictionary::~AardDictionary()
{
  df.close();
}

void AardDictionary::loadIcon() noexcept
{
  if ( dictionaryIconLoaded ) {
    return;
  }

  QString fileName = QDir::fromNativeSeparators( QString::fromStdString( getDictionaryFilenames()[ 0 ] ) );

  if ( !loadIconFromFileName( fileName ) ) {
    // Load failed -- use default icons
    dictionaryIcon = QIcon( ":/icons/icon32_aard.png" );
  }

  dictionaryIconLoaded = true;
}

string AardDictionary::convert( const string & in )
{
  string inConverted;
  char inCh, lastCh = 0;
  bool afterEol = false;

  for ( char i : in ) {
    inCh = i;
    if ( lastCh == '\\' ) {
      inConverted.erase( inConverted.size() - 1 );
      lastCh = 0;
      if ( inCh == 'n' ) {
        inConverted.append( "<br/>" );
        afterEol = true;
        continue;
      }
      else if ( inCh == 'r' ) {
        continue;
      }
    }
    else if ( inCh == ' ' && afterEol ) {
      inConverted.append( "&nbsp;" );
      continue;
    }
    else {
      lastCh = inCh;
    }
    afterEol = false;
    inConverted.push_back( inCh );
  }

  QString text = QString::fromUtf8( inConverted.c_str() );

  text.replace( QRegularExpression( R"(<\s*a\s+href\s*=\s*\"(w:|s:){0,1}([^#](?!ttp://)[^\"]*)(.))",
                                    QRegularExpression::DotMatchesEverythingOption ),
                R"(<a href="bword:\2")" );
  text.replace( QRegularExpression( R"(<\s*a\s+href\s*=\s*'(w:|s:){0,1}([^#](?!ttp://)[^']*)(.))",
                                    QRegularExpression::DotMatchesEverythingOption ),
                R"(<a href="bword:\2")" );

  // Anchors
  text.replace( QRegularExpression( R"(<a\s+href="bword:([^#"]+)#([^"]+))" ),
                R"(<a href="gdlookup://localhost/\1?gdanchor=\2)" );

  static QRegularExpression self_closing_divs( "(<div\\s+[^>]*)/>",
                                               QRegularExpression::CaseInsensitiveOption ); // <div ... />
  text.replace( self_closing_divs, "\\1></div>" );


  // Fix outstanding elements
  text += "<br style=\"clear:both;\" />";

  return text.toUtf8().data();
}

void AardDictionary::loadArticle( quint32 address, string & articleText, bool rawText )
{
  quint32 articleOffset = address;
  quint32 articleSize;
  quint32 size;

  vector< char > articleBody;

  articleText.clear();


  while ( 1 ) {
    articleText = QObject::tr( "Article loading error" ).toStdString();
    try {
      QMutexLocker _( &aardMutex );
      df.seek( articleOffset );
      df.read( &size, sizeof( size ) );
      articleSize = qFromBigEndian( size );

      // Don't try to read and decode too big articles,
      // it is most likely error in dictionary
      if ( articleSize > 1048576 ) {
        break;
      }

      articleBody.resize( articleSize );
      df.read( &articleBody.front(), articleSize );
    }
    catch ( std::exception & ex ) {
      qWarning( "AARD: Failed loading article from \"%s\", reason: %s", getName().c_str(), ex.what() );
      break;
    }
    catch ( ... ) {
      break;
    }

    if ( articleBody.empty() ) {
      break;
    }

    articleText.clear();

    string text = decompressBzip2( articleBody.data(), articleSize );
    if ( text.empty() ) {
      text = decompressZlib( articleBody.data(), articleSize );
    }
    if ( text.empty() ) {
      text = string( articleBody.data(), articleSize );
    }

    if ( text.empty() || text[ 0 ] != '[' ) {
      break;
    }

    string::size_type n = text.find( '\"' );
    if ( n == string::npos ) {
      break;
    }

    readJSONValue( text, articleText, n );

    if ( articleText.empty() ) {
      n = text.find( "\"r\"" );
      if ( n != string::npos && n + 3 < text.size() ) {
        n = text.find( '\"', n + 3 );
        if ( n == string::npos ) {
          break;
        }

        string link;
        readJSONValue( text, link, n );
        if ( !link.empty() ) {
          string encodedLink;
          encodedLink.reserve( link.size() );
          bool prev = false;
          for ( char i : link ) {
            if ( i == '\\' ) {
              if ( !prev ) {
                prev = true;
                continue;
              }
            }
            encodedLink.push_back( i );
            prev = false;
          }
          encodedLink =
            string( QUrl( QString::fromUtf8( encodedLink.data(), encodedLink.size() ) ).toEncoded().data() );
          articleText = "<a href=\"" + encodedLink + "\">" + link + "</a>";
        }
      }
    }

    break;
  }

  if ( !articleText.empty() ) {
    if ( rawText ) {
      return;
    }

    articleText = convert( articleText );
  }
  else {
    articleText = QObject::tr( "Article decoding error" ).toStdString();
  }

  // See Issue #271: A mechanism to clean-up invalid HTML cards.
  const string cleaner = Utils::Html::getHtmlCleaner();

  string prefix( "<div class=\"aard\"" );
  if ( isToLanguageRTL() ) {
    prefix += " dir=\"rtl\"";
  }
  prefix += ">";

  articleText = prefix + articleText + cleaner + "</div>";
}

QString const & AardDictionary::getDescription()
{
  if ( !dictionaryDescription.isEmpty() ) {
    return dictionaryDescription;
  }

  AAR_header dictHeader;
  quint32 size;
  vector< char > data;

  {
    QMutexLocker _( &aardMutex );
    df.seek( 0 );
    df.read( &dictHeader, sizeof( dictHeader ) );
    size = qFromBigEndian( dictHeader.metaLength );
    data.resize( size );
    df.read( &data.front(), size );
  }

  string metaStr = decompressBzip2( data.data(), size );
  if ( metaStr.empty() ) {
    metaStr = decompressZlib( data.data(), size );
  }

  map< string, string > meta = parseMetaData( metaStr );

  if ( !meta.empty() ) {
    map< string, string >::const_iterator iter = meta.find( "copyright" );
    if ( iter != meta.end() ) {
      dictionaryDescription =
        QObject::tr( "Copyright: %1%2" ).arg( QString::fromUtf8( iter->second.c_str() ) ).arg( "\n\n" );
    }

    iter = meta.find( "version" );
    if ( iter != meta.end() ) {
      dictionaryDescription =
        QObject::tr( "Version: %1%2" ).arg( QString::fromUtf8( iter->second.c_str() ) ).arg( "\n\n" );
    }

    iter = meta.find( "description" );
    if ( iter != meta.end() ) {
      QString desc = QString::fromUtf8( iter->second.c_str() );
      desc.replace( "\\n", "\n" );
      desc.replace( "\\t", "\t" );
      dictionaryDescription += desc;
    }
  }

  if ( dictionaryDescription.isEmpty() ) {
    dictionaryDescription = "NONE";
  }

  return dictionaryDescription;
}

void AardDictionary::makeFTSIndex( QAtomicInt & isCancelled )
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


  qDebug( "Aard: Building the full-text index for dictionary: %s", getName().c_str() );

  try {
    FtsHelpers::makeFTSIndex( this, isCancelled );
    FTS_index_completed.ref();
  }
  catch ( std::exception & ex ) {
    qWarning( "Aard: Failed building full-text search index for \"%s\", reason: %s", getName().c_str(), ex.what() );
    QFile::remove( QString::fromStdString( ftsIdxName ) );
  }
}

void AardDictionary::getArticleText( uint32_t articleAddress, QString & headword, QString & text )
{
  try {
    headword.clear();
    string articleText;

    loadArticle( articleAddress, articleText );

    text = Html::unescape( QString::fromUtf8( articleText.data(), articleText.size() ) );
  }
  catch ( std::exception & ex ) {
    qWarning( "Aard: Failed retrieving article from \"%s\", reason: %s", getName().c_str(), ex.what() );
  }
}

sptr< Dictionary::DataRequest >
AardDictionary::getSearchResults( QString const & searchString, int searchMode, bool matchCase, bool ignoreDiacritics )
{
  return std::make_shared< FtsHelpers::FTSResultsRequest >( *this,
                                                            searchString,
                                                            searchMode,
                                                            matchCase,
                                                            ignoreDiacritics );
}

/// AardDictionary::getArticle()

class AardArticleRequest: public Dictionary::DataRequest
{
  std::u32string word;
  vector< std::u32string > alts;
  AardDictionary & dict;
  bool ignoreDiacritics;

  QAtomicInt isCancelled;
  QFuture< void > f;

public:

  AardArticleRequest( std::u32string const & word_,
                      vector< std::u32string > const & alts_,
                      AardDictionary & dict_,
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

  ~AardArticleRequest()
  {
    isCancelled.ref();
    f.waitForFinished();
  }
};

void AardArticleRequest::run()
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

  set< quint32 > articlesIncluded; // Some synonims make it that the articles
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
    }
    catch ( ... ) {
    }

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

  if ( mainArticles.empty() && alternateArticles.empty() ) {
    // No such word
    finish();
    return;
  }

  string result;

  multimap< std::u32string, pair< string, string > >::const_iterator i;

  for ( i = mainArticles.begin(); i != mainArticles.end(); ++i ) {
    result += "<h3>";
    result += i->second.first;
    result += "</h3>";
    result += i->second.second;
  }

  for ( i = alternateArticles.begin(); i != alternateArticles.end(); ++i ) {
    result += "<h3>";
    result += i->second.first;
    result += "</h3>";
    result += i->second.second;
  }

  appendString( result );

  hasAnyData = true;

  finish();
}

sptr< Dictionary::DataRequest > AardDictionary::getArticle( std::u32string const & word,
                                                            vector< std::u32string > const & alts,
                                                            std::u32string const &,
                                                            bool ignoreDiacritics )

{
  return std::make_shared< AardArticleRequest >( word, alts, *this, ignoreDiacritics );
}

} // anonymous namespace

vector< sptr< Dictionary::Class > > makeDictionaries( vector< string > const & fileNames,
                                                      string const & indicesDir,
                                                      Dictionary::Initializing & initializing,
                                                      unsigned maxHeadwordsToExpand )

{
  vector< sptr< Dictionary::Class > > dictionaries;

  for ( const auto & fileName : fileNames ) {
    // Skip files with the extensions different to .aar to speed up the
    // scanning
    if ( !Utils::endsWithIgnoreCase( fileName, ".aar" ) ) {
      continue;
    }

    // Got the file -- check if we need to rebuid the index

    vector< string > dictFiles( 1, fileName );
    initializing.loadingDictionary( fileName );
    string dictId = Dictionary::makeDictionaryId( dictFiles );

    string indexFile = indicesDir + dictId;

    if ( Dictionary::needToRebuildIndex( dictFiles, indexFile ) || indexIsOldOrBad( indexFile ) ) {
      try {

        qDebug( "Aard: Building the index for dictionary: %s", fileName.c_str() );

        {
          QFileInfo info( QString::fromUtf8( fileName.c_str() ) );
          if ( static_cast< quint64 >( info.size() ) > ULONG_MAX ) {
            qWarning( "File %s is too large", fileName.c_str() );
            continue;
          }
        }

        File::Index df( fileName, QIODevice::ReadOnly );

        AAR_header dictHeader;

        df.read( &dictHeader, sizeof( dictHeader ) );
        bool has64bitIndex = !strncmp( dictHeader.indexItemFormat, ">LQ", 4 );
        if ( strncmp( dictHeader.signature, "aard", 4 )
             || ( !has64bitIndex && strncmp( dictHeader.indexItemFormat, ">LL", 4 ) )
             || strncmp( dictHeader.keyLengthFormat, ">H", 2 ) || strncmp( dictHeader.articleLengthFormat, ">L", 2 ) ) {
          qWarning( "File %s is not in supported aard format", fileName.c_str() );
          continue;
        }

        vector< char > data;
        quint32 size = qFromBigEndian( dictHeader.metaLength );

        if ( size == 0 ) {
          qWarning( "File %s has invalid metadata", fileName.c_str() );
          continue;
        }

        data.resize( size );
        df.read( &data.front(), size );
        string metaStr = decompressBzip2( data.data(), size );
        if ( metaStr.empty() ) {
          metaStr = decompressZlib( data.data(), size );
        }

        map< string, string > meta = parseMetaData( metaStr );

        if ( meta.empty() ) {
          qWarning( "File %s has invalid metadata", fileName.c_str() );
          continue;
        }

        string dictName;
        map< string, string >::const_iterator iter = meta.find( "title" );
        if ( iter != meta.end() ) {
          dictName = iter->second;
        }

        string langFrom;
        iter = meta.find( "index_language" );
        if ( iter != meta.end() ) {
          langFrom = iter->second;
        }

        string langTo;
        iter = meta.find( "article_language" );
        if ( iter != meta.end() ) {
          langTo = iter->second;
        }

        if ( ( dictName.compare( "Wikipedia" ) == 0 || dictName.compare( "Wikiquote" ) == 0
               || dictName.compare( "Wiktionary" ) == 0 )
             && !langTo.empty() ) {
          string capitalized = langTo.c_str();
          capitalized[ 0 ]   = toupper( capitalized[ 0 ] );
          dictName           = dictName + " (" + capitalized + ")";
        }

        quint16 volumes = qFromBigEndian( dictHeader.totalVolumes );
        if ( volumes > 1 ) {
          QString ss = QString( " (%1/%2)" ).arg( qFromBigEndian( dictHeader.volume ), volumes );
          dictName += ss.toLocal8Bit().data();
        }

        initializing.indexingDictionary( dictName );

        File::Index idx( indexFile, QIODevice::WriteOnly );
        IdxHeader idxHeader;
        memset( &idxHeader, 0, sizeof( idxHeader ) );

        // We write a dummy header first. At the end of the process the header
        // will be rewritten with the right values.

        idx.write( idxHeader );

        idx.write( (quint32)dictName.size() );
        if ( !dictName.empty() ) {
          idx.write( dictName.data(), dictName.size() );
        }

        IndexedWords indexedWords;

        ChunkedStorage::Writer chunks( idx );

        quint32 wordCount = qFromBigEndian( dictHeader.wordsCount );
        set< quint32 > articleOffsets;

        quint32 pos          = df.tell();
        quint32 wordsBase    = pos + wordCount * ( has64bitIndex ? sizeof( IndexElement64 ) : sizeof( IndexElement ) );
        quint32 articlesBase = qFromBigEndian( dictHeader.articleOffset );

        data.clear();
        for ( quint32 j = 0; j < wordCount; j++ ) {
          quint32 articleOffset;
          quint32 wordOffset;

          if ( has64bitIndex ) {
            IndexElement64 el64;

            df.seek( pos );
            df.read( &el64, sizeof( el64 ) );
            articleOffset = articlesBase + qFromBigEndian( el64.articleOffset );
            wordOffset    = wordsBase + qFromBigEndian( el64.wordOffset );
          }
          else {
            IndexElement el;

            df.seek( pos );
            df.read( &el, sizeof( el ) );
            articleOffset = articlesBase + qFromBigEndian( el.articleOffset );
            wordOffset    = wordsBase + qFromBigEndian( el.wordOffset );
          }

          df.seek( wordOffset );

          quint16 sizeBE;
          df.read( &sizeBE, sizeof( sizeBE ) );
          quint16 wordSize = qFromBigEndian( sizeBE );
          if ( data.size() < wordSize ) {
            data.resize( wordSize );
          }
          df.read( &data.front(), wordSize );

          if ( articleOffsets.find( articleOffset ) == articleOffsets.end() ) {
            articleOffsets.insert( articleOffset );
          }

          // Insert new entry
          std::u32string word = Text::toUtf32( string( data.data(), wordSize ) );
          if ( maxHeadwordsToExpand && dictHeader.wordsCount >= maxHeadwordsToExpand ) {
            indexedWords.addSingleWord( word, articleOffset );
          }
          else {
            indexedWords.addWord( word, articleOffset );
          }

          pos += has64bitIndex ? sizeof( IndexElement64 ) : sizeof( IndexElement );
        }
        data.clear();

        idxHeader.articleCount = articleOffsets.size();
        articleOffsets.clear();

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

        idxHeader.wordCount = wordCount;

        if ( langFrom.size() == 3 ) {
          idxHeader.langFrom = LangCoder::findIdForLanguageCode3( langFrom );
        }
        else if ( langFrom.size() == 2 ) {
          idxHeader.langFrom = LangCoder::code2toInt( langFrom.c_str() );
        }

        if ( langTo.size() == 3 ) {
          idxHeader.langTo = LangCoder::findIdForLanguageCode3( langTo );
        }
        else if ( langTo.size() == 2 ) {
          idxHeader.langTo = LangCoder::code2toInt( langTo.c_str() );
        }

        idx.rewind();

        idx.write( &idxHeader, sizeof( idxHeader ) );
      }
      catch ( std::exception & e ) {
        qWarning( "Aard dictionary indexing failed: %s, error: %s", fileName.c_str(), e.what() );
        continue;
      }
      catch ( ... ) {
        qWarning( "Aard dictionary indexing failed" );
        continue;
      }
    } // if need to rebuild
    try {
      dictionaries.push_back( std::make_shared< AardDictionary >( dictId, indexFile, dictFiles ) );
    }
    catch ( std::exception & e ) {
      qWarning( "Aard dictionary initializing failed: %s, error: %s", fileName.c_str(), e.what() );
      continue;
    }
  }
  return dictionaries;
}

} // namespace Aard
