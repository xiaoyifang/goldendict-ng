/* This file is (c) 2008-2009 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "xdxf.hh"
#include "btreeidx.hh"
#include "folding.hh"
#include "text.hh"
#include "chunkedstorage.hh"
#include "dictzip.hh"
#include "htmlescape.hh"
#include <map>
#include <set>
#include <string>
#include <vector>
#include <list>
#include <wctype.h>
#include <stdlib.h>
#include "xdxf2html.hh"
#include "ufile.hh"
#include "langcoder.hh"
#include "indexedzip.hh"
#include "filetype.hh"
#include "tiff.hh"
#include "ftshelpers.hh"
#include <QIODevice>
#include <QXmlStreamReader>
#include <QFileInfo>
#include <QDir>
#include <QPainter>
#include <QRegularExpression>
#include <QAtomicInt>

#include "utils.hh"

namespace Xdxf {

using std::map;
using std::multimap;
using std::pair;
using std::set;
using std::string;
using std::vector;
using std::list;

using BtreeIndexing::WordArticleLink;
using BtreeIndexing::IndexedWords;
using BtreeIndexing::IndexInfo;

quint32 getLanguageId( const QString & lang )
{
  QString lstr = lang.left( 3 );

  if ( lstr.endsWith( QChar( '-' ) ) ) {
    lstr.chop( 1 );
  }

  switch ( lstr.size() ) {
    case 2:
      return LangCoder::code2toInt( lstr.toLatin1().data() );
    case 3:
      return LangCoder::findIdForLanguageCode3( lstr.toStdString() );
  }

  return 0;
}

namespace {

using Dictionary::exCantReadFile;
DEF_EX_STR( exNotXdxfFile, "The file is not an XDXF file:", Dictionary::Ex )
DEF_EX_STR( exDictzipError, "DICTZIP error", Dictionary::Ex )

enum {
  Signature            = 0x46584458, // XDXF on little-endian, FXDX on big-endian
  CurrentFormatVersion = 6 + BtreeIndexing::FormatVersion + Folding::Version + BtreeIndexing::ZipParseLogicVersion
};

enum ArticleFormat {
  Default = 0,
  Visual  = 1,
  Logical = 2
};

#pragma pack( push, 1 )
struct IdxHeader
{
  uint32_t signature;             // First comes the signature, XDXF
  uint32_t formatVersion;         // File format version (CurrentFormatVersion)
  uint32_t articleFormat;         // ArticleFormat value, except that 0 = bad file
  uint32_t langFrom;              // Source language
  uint32_t langTo;                // Target language
  uint32_t articleCount;          // Total number of articles
  uint32_t wordCount;             // Total number of words
  uint32_t nameAddress;           // Address of an utf8 name string, in chunks
  uint32_t nameSize;              // And its size
  uint32_t descriptionAddress;    // Address of an utf8 description string, in chunks
  uint32_t descriptionSize;       // And its size
  uint32_t hasAbrv;               // Non-zero means file has abrvs at abrvAddress
  uint32_t abrvAddress;           // Address of abrv map in the chunked storage
  uint32_t chunksOffset;          // The offset to chunks' storage
  uint32_t indexBtreeMaxElements; // Two fields from IndexInfo
  uint32_t indexRootOffset;
  uint32_t hasZipFile;               // Non-zero means there's a zip file with resources
                                     // present
  uint32_t zipIndexBtreeMaxElements; // Two fields from IndexInfo of the zip
                                     // resource index.
  uint32_t zipIndexRootOffset;
  uint32_t revisionNumber; // Format revision
};
static_assert( alignof( IdxHeader ) == 1 );
#pragma pack( pop )

bool indexIsOldOrBad( string const & indexFile )
{
  File::Index idx( indexFile, QIODevice::ReadOnly );

  IdxHeader header;

  return idx.readRecords( &header, sizeof( header ), 1 ) != 1 || header.signature != Signature
    || header.formatVersion != CurrentFormatVersion || !header.articleFormat;
}


class XdxfDictionary: public BtreeIndexing::BtreeDictionary
{
  QMutex idxMutex;
  File::Index idx;
  IdxHeader idxHeader;
  sptr< ChunkedStorage::Reader > chunks;
  QMutex dzMutex;
  dictData * dz;
  QMutex resourceZipMutex;
  IndexedZip resourceZip;
  map< string, string > abrv;

public:

  XdxfDictionary( string const & id, string const & indexFile, vector< string > const & dictionaryFiles );

  ~XdxfDictionary();

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

  sptr< Dictionary::DataRequest > getResource( string const & name ) override;

  QString const & getDescription() override;

  QString getMainFilename() override;

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
      can_FTS = fts.enabled && !fts.disabledTypes.contains( "XDXF", Qt::CaseInsensitive )
        && ( fts.maxDictionarySize == 0 || getArticleCount() <= fts.maxDictionarySize );
    }
  }

  uint32_t getFtsIndexVersion() override
  {
    return 1;
  }

protected:

  void loadIcon() noexcept override;

private:

  // Loads the article, storing its headword and formatting article's data into an html.
  void loadArticle( uint32_t address, string & articleText, QString * headword = 0 );

  friend class XdxfArticleRequest;
  friend class XdxfResourceRequest;
};

XdxfDictionary::XdxfDictionary( string const & id, string const & indexFile, vector< string > const & dictionaryFiles ):
  BtreeDictionary( id, dictionaryFiles ),
  idx( indexFile, QIODevice::ReadOnly ),
  idxHeader( idx.read< IdxHeader >() )
{
  // Read the dictionary name

  chunks = std::shared_ptr< ChunkedStorage::Reader >( new ChunkedStorage::Reader( idx, idxHeader.chunksOffset ) );

  if ( idxHeader.nameSize ) {
    vector< char > chunk;

    dictionaryName = string( chunks->getBlock( idxHeader.nameAddress, chunk ), idxHeader.nameSize );
  }

  // Open the file

  DZ_ERRORS error;
  dz = dict_data_open( dictionaryFiles[ 0 ].c_str(), &error, 0 );

  if ( !dz ) {
    throw exDictzipError( string( dz_error_str( error ) ) + "(" + dictionaryFiles[ 0 ] + ")" );
  }

  // Read the abrv, if any

  if ( idxHeader.hasAbrv ) {
    vector< char > chunk;

    char * abrvBlock = chunks->getBlock( idxHeader.abrvAddress, chunk );

    uint32_t total;
    memcpy( &total, abrvBlock, sizeof( uint32_t ) );
    abrvBlock += sizeof( uint32_t );

    while ( total-- ) {
      uint32_t keySz;
      memcpy( &keySz, abrvBlock, sizeof( uint32_t ) );
      abrvBlock += sizeof( uint32_t );

      char * key = abrvBlock;

      abrvBlock += keySz;

      uint32_t valueSz;
      memcpy( &valueSz, abrvBlock, sizeof( uint32_t ) );
      abrvBlock += sizeof( uint32_t );

      abrv[ string( key, keySz ) ] = string( abrvBlock, valueSz );

      abrvBlock += valueSz;
    }

    // Open a resource zip file, if there's one

    if ( idxHeader.hasZipFile && ( idxHeader.zipIndexBtreeMaxElements || idxHeader.zipIndexRootOffset ) ) {
      resourceZip.openIndex( IndexInfo( idxHeader.zipIndexBtreeMaxElements, idxHeader.zipIndexRootOffset ),
                             idx,
                             idxMutex );

      QString zipName = QDir::fromNativeSeparators( getDictionaryFilenames().back().c_str() );

      if ( zipName.endsWith( ".zip", Qt::CaseInsensitive ) ) { // Sanity check
        resourceZip.openZipFile( zipName );
      }
    }
  }

  // Initialize the index

  openIndex( IndexInfo( idxHeader.indexBtreeMaxElements, idxHeader.indexRootOffset ), idx, idxMutex );

  // Full-text search parameters

  ftsIdxName = indexFile + Dictionary::getFtsSuffix();
}

XdxfDictionary::~XdxfDictionary()
{
  if ( dz ) {
    dict_data_close( dz );
  }
}

void XdxfDictionary::loadIcon() noexcept
{
  if ( dictionaryIconLoaded ) {
    return;
  }

  QString fileName = QDir::fromNativeSeparators( getDictionaryFilenames()[ 0 ].c_str() );

  QFileInfo baseInfo( fileName );

  fileName = baseInfo.absoluteDir().absoluteFilePath( "icon32.png" );
  QFileInfo info( fileName );

  if ( !info.isFile() ) {
    fileName = baseInfo.absoluteDir().absoluteFilePath( "icon16.png" );
    info     = QFileInfo( fileName );
  }

  if ( !info.isFile() ) {
    fileName = baseInfo.absoluteDir().absoluteFilePath( "dict.bmp" );
    info     = QFileInfo( fileName );
  }

  if ( info.isFile() ) {
    loadIconFromFile( fileName, true );
  }

  if ( dictionaryIcon.isNull() ) {
    // Load failed -- use default icons

    dictionaryIcon = QIcon( ":/icons/icon32_xdxf.png" );
  }

  dictionaryIconLoaded = true;
}

QString const & XdxfDictionary::getDescription()
{
  if ( !dictionaryDescription.isEmpty() ) {
    return dictionaryDescription;
  }

  if ( idxHeader.descriptionAddress == 0 ) {
    dictionaryDescription = "NONE";
  }
  else {
    try {
      vector< char > chunk;
      char * descr;
      {
        QMutexLocker _( &idxMutex );
        descr = chunks->getBlock( idxHeader.descriptionAddress, chunk );
      }
      dictionaryDescription = QString::fromUtf8( descr, idxHeader.descriptionSize );
    }
    catch ( ... ) {
    }
  }
  return dictionaryDescription;
}

QString XdxfDictionary::getMainFilename()
{
  return getDictionaryFilenames()[ 0 ].c_str();
}

void XdxfDictionary::makeFTSIndex( QAtomicInt & isCancelled )
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


  qDebug( "Xdxf: Building the full-text index for dictionary: %s", getName().c_str() );

  try {
    FtsHelpers::makeFTSIndex( this, isCancelled );
    FTS_index_completed.ref();
  }
  catch ( std::exception & ex ) {
    qWarning( "Xdxf: Failed building full-text search index for \"%s\", reason: %s", getName().c_str(), ex.what() );
    QFile::remove( ftsIdxName.c_str() );
  }
}

void XdxfDictionary::getArticleText( uint32_t articleAddress, QString & headword, QString & text )
{
  try {
    string articleStr;
    loadArticle( articleAddress, articleStr, &headword );

    text = Html::unescape( QString::fromStdString( articleStr ) );
  }
  catch ( std::exception & ex ) {
    qWarning( "Xdxf: Failed retrieving article from \"%s\", reason: %s", getName().c_str(), ex.what() );
  }
}

sptr< Dictionary::DataRequest >
XdxfDictionary::getSearchResults( QString const & searchString, int searchMode, bool matchCase, bool ignoreDiacritics )
{
  return std::make_shared< FtsHelpers::FTSResultsRequest >( *this,
                                                            searchString,
                                                            searchMode,
                                                            matchCase,
                                                            ignoreDiacritics );
}

/// XdxfDictionary::getArticle()


class XdxfArticleRequest: public Dictionary::DataRequest
{

  std::u32string word;
  vector< std::u32string > alts;
  XdxfDictionary & dict;
  bool ignoreDiacritics;

  QAtomicInt isCancelled;
  QFuture< void > f;

public:

  XdxfArticleRequest( std::u32string const & word_,
                      vector< std::u32string > const & alts_,
                      XdxfDictionary & dict_,
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

  ~XdxfArticleRequest()
  {
    isCancelled.ref();
    f.waitForFinished();
  }
};


void XdxfArticleRequest::run()
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

  for ( const auto & x : chain ) {
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
      qWarning( "XDXF: Failed loading article from \"%s\", reason: %s", dict.getName().c_str(), ex.what() );
    }
  }

  if ( mainArticles.empty() && alternateArticles.empty() ) {
    // No such word
    finish();
    return;
  }

  string result;

  multimap< std::u32string, pair< string, string > >::const_iterator i;

  string cleaner = Utils::Html::getHtmlCleaner();

  for ( i = mainArticles.begin(); i != mainArticles.end(); ++i ) {
    //      result += "<h3>";
    //      result += i->second.first;
    //      result += "</h3>";
    result += i->second.second;
    result += cleaner;
  }

  for ( i = alternateArticles.begin(); i != alternateArticles.end(); ++i ) {
    //      result += "<h3>";
    //      result += i->second.first;
    //      result += "</h3>";
    result += i->second.second;
    result += cleaner;
  }

  appendString( result );

  hasAnyData = true;

  finish();
}

sptr< Dictionary::DataRequest > XdxfDictionary::getArticle( std::u32string const & word,
                                                            vector< std::u32string > const & alts,
                                                            std::u32string const &,
                                                            bool ignoreDiacritics )

{
  return std::make_shared< XdxfArticleRequest >( word, alts, *this, ignoreDiacritics );
}

void XdxfDictionary::loadArticle( uint32_t address, string & articleText, QString * headword )
{
  // Read the properties

  vector< char > chunk;

  char * propertiesData;

  {
    QMutexLocker _( &idxMutex );

    propertiesData = chunks->getBlock( address, chunk );
  }

  if ( &chunk.front() + chunk.size() - propertiesData < 9 ) {
    articleText = string( "<div class=\"xdxf\">Index seems corrupted</div>" );
    return;
  }

  unsigned char fType = (unsigned char)*propertiesData;

  uint32_t articleOffset, articleSize;

  memcpy( &articleOffset, propertiesData + 1, sizeof( uint32_t ) );
  memcpy( &articleSize, propertiesData + 5, sizeof( uint32_t ) );

  // Load the article

  char * articleBody;

  {
    QMutexLocker _( &dzMutex );

    // Note that the function always zero-pads the result.
    articleBody = dict_data_read_( dz, articleOffset, articleSize, 0, 0 );
  }

  if ( !articleBody ) {
    //    throw exCantReadFile( getDictionaryFilenames()[ 0 ] );
    articleText = string( "<div class=\"xdxf\">DICTZIP error: " ) + dict_error_str( dz ) + "</div>";
    return;
  }

  articleText = Xdxf2Html::convert( string( articleBody ),
                                    Xdxf2Html::XDXF,
                                    idxHeader.hasAbrv ? &abrv : NULL,
                                    this,
                                    fType == Logical,
                                    idxHeader.revisionNumber,
                                    headword );

  free( articleBody );
}

class GzippedFile: public QIODevice
{
  gzFile gz;

public:

  GzippedFile( char const * fileName );

  ~GzippedFile();

  //  size_t gzTell();

  char * readDataArray( unsigned long startPos, unsigned long size );

protected:

  dictData * dz;

  bool isSequential() const override
  {
    return false;
  } // Which is a lie, but else pos() won't work

  bool waitForReadyRead( int ) override
  {
    return !gzeof( gz );
  }

  qint64 bytesAvailable() const override
  {
    return ( gzeof( gz ) ? 0 : 1 ) + QIODevice::bytesAvailable();
  }

  qint64 readData( char * data, qint64 maxSize ) override;

  bool atEnd() const override;

  qint64 writeData( const char * /*data*/, qint64 /*maxSize*/ ) override
  {
    return -1;
  }
};

GzippedFile::GzippedFile( char const * fileName )
{
  gz = gd_gzopen( fileName );
  if ( !gz ) {
    throw exCantReadFile( fileName );
  }

  DZ_ERRORS error;
  dz = dict_data_open( fileName, &error, 0 );
}

GzippedFile::~GzippedFile()
{
  gzclose( gz );
  if ( dz ) {
    dict_data_close( dz );
  }
}

bool GzippedFile::atEnd() const
{
  return gzeof( gz );
}

/*
size_t GzippedFile::gzTell()
{
  return gztell( gz );
}
*/

qint64 GzippedFile::readData( char * data, qint64 maxSize )
{
  if ( maxSize > 1 ) {
    maxSize = 1;
  }

  // The returning value translates directly to QIODevice semantics
  int n = gzread( gz, data, maxSize );

  // With QT 5.x QXmlStreamReader ask one byte instead of one UTF-8 char.
  // We read and return all bytes for char.

  if ( n == 1 ) {
    char ch      = *data;
    int addBytes = 0;
    if ( ch & 0x80 ) {
      if ( ( ch & 0xF8 ) == 0xF0 ) {
        addBytes = 3;
      }
      else if ( ( ch & 0xF0 ) == 0xE0 ) {
        addBytes = 2;
      }
      else if ( ( ch & 0xE0 ) == 0xC0 ) {
        addBytes = 1;
      }
    }

    if ( addBytes ) {
      n += gzread( gz, data + 1, addBytes );
    }
  }

  return n;
}

char * GzippedFile::readDataArray( unsigned long startPos, unsigned long size )
{
  if ( dz == NULL ) {
    return NULL;
  }
  return dict_data_read_( dz, startPos, size, 0, 0 );
}

QString readXhtmlData( QXmlStreamReader & stream )
{
  QString result;

  while ( !stream.atEnd() ) {
    stream.readNext();

    if ( stream.isStartElement() ) {
      QString name = stream.name().toString();

      result += "<" + Utils::escape( name ) + " ";

      QXmlStreamAttributes attrs = stream.attributes();

      for ( const auto & attr : attrs ) {
        result += Utils::escape( attr.name().toString() );
        result += "=\"" + Utils::escape( attr.value().toString() ) + "\"";
      }

      result += ">";

      result += readXhtmlData( stream );

      result += "</" + Utils::escape( name ) + ">";
    }
    else if ( stream.isCharacters() || stream.isWhitespace() || stream.isCDATA() ) {
      result += stream.text();
    }
    else if ( stream.isEndElement() ) {
      break;
    }
  }

  return result;
}

namespace {

/// Deal with Qt 4.5 incompatibility
QString readElementText( QXmlStreamReader & stream )
{
  return stream.readElementText( QXmlStreamReader::SkipChildElements );
}

} // namespace


void addAllKeyTags( QXmlStreamReader & stream, list< QString > & words )
{
  // todo implement support for tag <srt>, that overrides the article sorting order
  if ( stream.name() == u"k" ) {
    words.push_back( readElementText( stream ) );
    return;
  }

  while ( !stream.atEnd() ) {
    stream.readNext();

    if ( stream.isStartElement() ) {
      addAllKeyTags( stream, words );
    }
    else if ( stream.isEndElement() ) {
      return;
    }
  }
}

void checkArticlePosition( GzippedFile & gzFile, uint32_t * pOffset, uint32_t * pSize )
{
  char * data = gzFile.readDataArray( *pOffset, *pSize );
  if ( data == NULL ) {
    return;
  }
  QString s = QString::fromUtf8( data );
  free( data );
  int n = s.lastIndexOf( "</ar" );
  if ( n > 0 ) {
    *pSize -= s.size() - n;
  }
  if ( s.at( 0 ) == '>' ) {
    *pOffset += 1;
    *pSize -= 1;
  }
}

void indexArticle( GzippedFile & gzFile,
                   QXmlStreamReader & stream,
                   IndexedWords & indexedWords,
                   ChunkedStorage::Writer & chunks,
                   unsigned & articleCount,
                   unsigned & wordCount,
                   ArticleFormat defaultFormat )
{
  ArticleFormat format( Default );

  QStringView formatValue = stream.attributes().value( "f" );

  if ( formatValue == u"v" ) {
    format = Visual;
  }
  else if ( formatValue == u"l" ) {
    format = Logical;
  }
  if ( format == Default ) {
    format = defaultFormat;
  }
  size_t articleOffset = gzFile.pos() - 1; // stream.characterOffset() is loony

  // uint32_t lineNumber = stream.lineNumber();
  // uint32_t columnNumber = stream.columnNumber();

  list< QString > words;

  while ( !stream.atEnd() ) {
    stream.readNext();

    // Find any <k> tags and index them
    if ( stream.isEndElement() ) {
      // End of the <ar> tag

      if ( words.empty() ) {
        // Nothing to index, this article didn't have any tags
        qWarning( "No <k> tags found in an article at offset 0x%x, article skipped.", (unsigned)articleOffset );
      }
      else {
        // Add an entry

        uint32_t offset = chunks.startNewBlock();

        uint32_t offs = articleOffset;
        uint32_t size = gzFile.pos() - 1 - articleOffset;

        checkArticlePosition( gzFile, &offs, &size );

        unsigned char f = format;
        chunks.addToBlock( &f, 1 );
        chunks.addToBlock( &offs, sizeof( offs ) );
        chunks.addToBlock( &size, sizeof( size ) );

        // Add also first header - it's needed for full-text search
        chunks.addToBlock( words.begin()->toUtf8().data(), words.begin()->toUtf8().length() + 1 );

        //        qDebug( "%x: %s", articleOffset, words.begin()->toUtf8().data() );

        // Add words to index

        for ( const auto & word : words ) {
          indexedWords.addWord( word.toStdU32String(), offset );
        }

        ++articleCount;

        wordCount += words.size();
      }

      return;
    }
    else if ( stream.isStartElement() ) {
      addAllKeyTags( stream, words );
    }
  }
}

//// XdxfDictionary::getResource()

class XdxfResourceRequest: public Dictionary::DataRequest
{

  XdxfDictionary & dict;

  string resourceName;

  QAtomicInt isCancelled;
  QFuture< void > f;

public:

  XdxfResourceRequest( XdxfDictionary & dict_, string const & resourceName_ ):
    dict( dict_ ),
    resourceName( resourceName_ )
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

  ~XdxfResourceRequest()
  {
    isCancelled.ref();
    f.waitForFinished();
  }
};


void XdxfResourceRequest::run()
{
  // Some runnables linger enough that they are cancelled before they start
  if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
    finish();
    return;
  }

  if ( dict.ensureInitDone().size() ) {
    setErrorString( QString::fromUtf8( dict.ensureInitDone().c_str() ) );
    finish();
    return;
  }

  string n = dict.getContainingFolder().toStdString() + Utils::Fs::separator() + resourceName;

  qDebug( "xdxf resource name is %s", n.c_str() );

  try {
    try {
      QMutexLocker _( &dataMutex );

      File::loadFromFile( n, data );
    }
    catch ( File::exCantOpen & ) {
      n = dict.getDictionaryFilenames()[ 0 ] + ".files" + Utils::Fs::separator() + resourceName;

      try {
        QMutexLocker _( &dataMutex );

        File::loadFromFile( n, data );
      }
      catch ( File::exCantOpen & ) {
        // Try reading from zip file

        if ( dict.resourceZip.isOpen() ) {
          QMutexLocker _( &dataMutex );

          if ( !dict.resourceZip.loadFile( Text::toUtf32( resourceName ), data ) ) {
            throw; // Make it fail since we couldn't read the archive
          }
        }
        else {
          throw;
        }
      }
    }

    if ( Filetype::isNameOfTiff( resourceName ) ) {
      // Convert it
      QMutexLocker _( &dataMutex );
      GdTiff::tiff2img( data );
    }

    QMutexLocker _( &dataMutex );

    hasAnyData = true;
  }
  catch ( std::exception & ex ) {
    qWarning( "XDXF: Failed loading resource \"%s\" for \"%s\", reason: %s",
              resourceName.c_str(),
              dict.getName().c_str(),
              ex.what() );
    // Resource not loaded -- we don't set the hasAnyData flag then
  }

  finish();
}

sptr< Dictionary::DataRequest > XdxfDictionary::getResource( string const & name )

{
  return std::make_shared< XdxfResourceRequest >( *this, name );
}

} // namespace
// anonymous namespace - this section of file is devoted to rebuilding of dictionary articles index

vector< sptr< Dictionary::Class > > makeDictionaries( vector< string > const & fileNames,
                                                      string const & indicesDir,
                                                      Dictionary::Initializing & initializing )

{
  vector< sptr< Dictionary::Class > > dictionaries;

  for ( const auto & fileName : fileNames ) {
    // Only allow .xdxf and .xdxf.dz suffixes

    if ( !Utils::endsWithIgnoreCase( fileName, ".xdxf" ) && !Utils::endsWithIgnoreCase( fileName, ".xdxf.dz" ) ) {
      continue;
    }

    try {
      vector< string > dictFiles( 1, fileName );

      string baseName = ( fileName[ fileName.size() - 5 ] == '.' ) ? string( fileName, 0, fileName.size() - 5 ) :
                                                                     string( fileName, 0, fileName.size() - 8 );

      // See if there's a zip file with resources present. If so, include it.

      string zipFileName;

      if ( File::tryPossibleZipName( baseName + ".xdxf.files.zip", zipFileName )
           || File::tryPossibleZipName( baseName + ".xdxf.dz.files.zip", zipFileName )
           || File::tryPossibleZipName( baseName + ".XDXF.FILES.ZIP", zipFileName )
           || File::tryPossibleZipName( baseName + ".XDXF.DZ.FILES.ZIP", zipFileName ) ) {
        dictFiles.push_back( zipFileName );
      }

      string dictId = Dictionary::makeDictionaryId( dictFiles );

      string indexFile = indicesDir + dictId;

      if ( Dictionary::needToRebuildIndex( dictFiles, indexFile ) || indexIsOldOrBad( indexFile ) ) {
        // Building the index

        qDebug( "Xdxf: Building the index for dictionary: %s", fileName.c_str() );

        //initializing.indexingDictionary( nameFromFileName( dictFiles[ 0 ] ) );

        File::Index idx( indexFile, QIODevice::WriteOnly );

        IdxHeader idxHeader;
        map< string, string > abrv;

        memset( &idxHeader, 0, sizeof( idxHeader ) );

        // We write a dummy header first. At the end of the process the header
        // will be rewritten with the right values.

        idx.write( idxHeader );

        IndexedWords indexedWords;

        GzippedFile gzFile( dictFiles[ 0 ].c_str() );

        if ( !gzFile.open( QIODevice::ReadOnly ) ) {
          throw exCantReadFile( dictFiles[ 0 ] );
        }

        QXmlStreamReader stream( &gzFile );

        QString dictionaryName, dictionaryDescription;

        ChunkedStorage::Writer chunks( idx );

        // Wait for the first element, which must be xdxf

        bool hadXdxf = false;

        while ( !stream.atEnd() ) {
          stream.readNext();

          if ( stream.isStartElement() ) {
            if ( stream.name() != u"xdxf" ) {
              throw exNotXdxfFile( dictFiles[ 0 ] );
            }
            else {
              // Read the xdxf

              string str = stream.attributes().value( "lang_from" ).toString().toLatin1().data();
              if ( !str.empty() ) {
                idxHeader.langFrom = getLanguageId( str.c_str() );
              }

              str = stream.attributes().value( "lang_to" ).toString().toLatin1().data();
              if ( !str.empty() ) {
                idxHeader.langTo = getLanguageId( str.c_str() );
              }

              QRegularExpression regNum( "\\d+" );
              auto match               = regNum.match( stream.attributes().value( "revision" ).toString() );
              idxHeader.revisionNumber = match.captured().toUInt();

              bool isLogical =
                ( stream.attributes().value( "format" ) == u"logical" || idxHeader.revisionNumber >= 34 );

              idxHeader.articleFormat = isLogical ? Logical : Visual;

              unsigned articleCount = 0, wordCount = 0;

              while ( !stream.atEnd() ) {
                stream.readNext();

                if ( stream.isStartElement() ) {
                  // todo implement using short <title> for denoting the dictionary in settings or dict list toolbar
                  if ( stream.name() == u"full_name" || stream.name() == u"full_title" ) {
                    // That's our name

                    QString name = stream.readElementText();

                    if ( dictionaryName.isEmpty() ) {
                      dictionaryName = name;

                      initializing.indexingDictionary( dictionaryName.toUtf8().data() );

                      idxHeader.nameAddress = chunks.startNewBlock();

                      QByteArray n = dictionaryName.toUtf8();

                      idxHeader.nameSize = n.size();

                      chunks.addToBlock( n.data(), n.size() );
                    }
                    else {
                      qDebug( "Warning: duplicate full_name in %s", dictFiles[ 0 ].c_str() );
                    }
                  }
                  else if ( stream.name() == u"description" ) {
                    // todo implement adding other information to the description like <publisher>, <authors>, <file_ver>, <creation_date>, <last_edited_date>, <dict_edition>, <publishing_date>, <dict_src_url>
                    QString desc = readXhtmlData( stream );

                    if ( isLogical ) {
                      desc = desc.simplified();
                      QRegularExpression br( "<br\\s*>\\s*</br>" );
                      desc.replace( br, QString( "\n" ) );
                    }

                    if ( dictionaryDescription.isEmpty() ) {
                      dictionaryDescription        = desc;
                      idxHeader.descriptionAddress = chunks.startNewBlock();

                      QByteArray n = dictionaryDescription.toUtf8();

                      idxHeader.descriptionSize = n.size();

                      chunks.addToBlock( n.data(), n.size() );
                    }
                    else {
                      qDebug( "Warning: duplicate description in %s", dictFiles[ 0 ].c_str() );
                    }
                  }
                  else if ( stream.name() == u"languages" ) {
                    while ( !( stream.isEndElement() && stream.name() == u"languages" ) && !stream.atEnd() ) {
                      if ( !stream.readNext() ) {
                        break;
                      }
                      if ( stream.isStartElement() ) {
                        if ( stream.name() == u"from" ) {
                          if ( idxHeader.langFrom == 0 ) {
                            QString lang       = stream.attributes().value( "xml:lang" ).toString();
                            idxHeader.langFrom = getLanguageId( lang );
                          }
                        }
                        else if ( stream.name() == u"to" ) {
                          if ( idxHeader.langTo == 0 ) {
                            QString lang     = stream.attributes().value( "xml:lang" ).toString();
                            idxHeader.langTo = getLanguageId( lang );
                          }
                        }
                      }
                      else if ( stream.isEndElement() && stream.name() == u"languages" ) {
                        break;
                      }
                    }
                  }
                  else if ( stream.name() == u"abbreviations" ) {
                    QString s;
                    string value;
                    list< std::u32string > keys;
                    while ( !( stream.isEndElement() && stream.name() == u"abbreviations" ) && !stream.atEnd() ) {
                      if ( !stream.readNextStartElement() ) {
                        break;
                      }
                      // abbreviations tag set switch at format revision = 30
                      if ( idxHeader.revisionNumber >= 30 ) {
                        while ( !( stream.isEndElement() && stream.name() == u"abbr_def" ) || !stream.atEnd() ) {
                          if ( stream.isStartElement() && stream.name() == u"abbr_k" ) {
                            s = readElementText( stream );
                            keys.push_back( s.toStdU32String() );
                          }
                          else if ( stream.isStartElement() && stream.name() == u"abbr_v" ) {
                            s     = readElementText( stream );
                            value = Folding::trimWhitespace( s ).toStdString();
                            for ( const auto & key : keys ) {
                              abrv[ Text::toUtf8( Folding::trimWhitespace( key ) ) ] = value;
                            }
                            keys.clear();
                          }
                          else if ( stream.isEndElement() && stream.name() == u"abbreviations" ) {
                            break;
                          }
                          stream.readNext();
                        }
                      }
                      else {
                        while ( !( stream.isEndElement() && stream.name() == u"abr_def" ) || !stream.atEnd() ) {
                          if ( stream.isStartElement() && stream.name() == u"k" ) {
                            s = readElementText( stream );
                            keys.push_back( s.toStdU32String() );
                          }
                          else if ( stream.isStartElement() && stream.name() == u"v" ) {
                            s     = readElementText( stream );
                            value = Folding::trimWhitespace( s ).toStdString();
                            for ( const auto & key : keys ) {
                              abrv[ Text::toUtf8( Folding::trimWhitespace( key ) ) ] = value;
                            }
                            keys.clear();
                          }
                          else if ( stream.isEndElement() && stream.name() == u"abbreviations" ) {
                            break;
                          }
                          stream.readNext();
                        }
                      }
                    }
                  }
                  else if ( stream.name() == u"ar" ) {
                    indexArticle( gzFile,
                                  stream,
                                  indexedWords,
                                  chunks,
                                  articleCount,
                                  wordCount,
                                  isLogical ? Logical : Visual );
                  }
                }
              }

              // Write abbreviations if presented

              if ( !abrv.empty() ) {
                idxHeader.hasAbrv     = 1;
                idxHeader.abrvAddress = chunks.startNewBlock();

                uint32_t sz = abrv.size();

                chunks.addToBlock( &sz, sizeof( uint32_t ) );

                for ( const auto & i : abrv ) {
                  sz = i.first.size();
                  chunks.addToBlock( &sz, sizeof( uint32_t ) );
                  chunks.addToBlock( i.first.data(), sz );
                  sz = i.second.size();
                  chunks.addToBlock( &sz, sizeof( uint32_t ) );
                  chunks.addToBlock( i.second.data(), sz );
                }
              }

              // Finish with the chunks

              idxHeader.chunksOffset = chunks.finish();

              // Build index

              IndexInfo idxInfo = BtreeIndexing::buildIndex( indexedWords, idx );

              idxHeader.indexBtreeMaxElements = idxInfo.btreeMaxElements;
              idxHeader.indexRootOffset       = idxInfo.rootOffset;

              indexedWords.clear(); // Release memory -- no need for this data

              // If there was a zip file, index it too

              if ( zipFileName.size() ) {
                qDebug( "Indexing zip file" );

                idxHeader.hasZipFile = 1;

                IndexedWords zipFileNames;
                IndexedZip zipFile;
                if ( zipFile.openZipFile( QDir::fromNativeSeparators( zipFileName.c_str() ) ) ) {
                  zipFile.indexFile( zipFileNames );
                }

                if ( !zipFileNames.empty() ) {
                  // Build the resulting zip file index

                  IndexInfo idxInfo = BtreeIndexing::buildIndex( zipFileNames, idx );

                  idxHeader.zipIndexBtreeMaxElements = idxInfo.btreeMaxElements;
                  idxHeader.zipIndexRootOffset       = idxInfo.rootOffset;
                }
                else {
                  // Bad zip file -- no index (though the mark that we have one
                  // remains)
                  idxHeader.zipIndexBtreeMaxElements = 0;
                  idxHeader.zipIndexRootOffset       = 0;
                }
              }
              else {
                idxHeader.hasZipFile = 0;
              }
              // That concludes it. Update the header.

              idxHeader.signature     = Signature;
              idxHeader.formatVersion = CurrentFormatVersion;

              idxHeader.articleCount = articleCount;
              idxHeader.wordCount    = wordCount;

              idx.rewind();

              idx.write( &idxHeader, sizeof( idxHeader ) );

              hadXdxf = true;
            }
            break;
          }
        }

        if ( !hadXdxf ) {
          throw exNotXdxfFile( dictFiles[ 0 ] );
        }

        if ( stream.hasError() ) {
          qWarning( "%s had a parse error %s at line %lu, and therefore was indexed only up to the point of error.",
                    dictFiles[ 0 ].c_str(),
                    stream.errorString().toUtf8().data(),
                    (unsigned long)stream.lineNumber() );
        }
      }

      dictionaries.push_back( std::make_shared< XdxfDictionary >( dictId, indexFile, dictFiles ) );
    }
    catch ( std::exception & e ) {
      qWarning( "Xdxf dictionary initializing failed: %s, error: %s", fileName.c_str(), e.what() );
    }
  }

  return dictionaries;
}

} // namespace Xdxf
