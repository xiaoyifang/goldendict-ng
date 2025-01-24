/* This file is (c) 2013 Timon Wong <timon86.wang AT gmail DOT com>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "mdx.hh"
#include "btreeidx.hh"
#include "folding.hh"
#include "text.hh"
#include "dictfile.hh"
#include "text.hh"
#include "chunkedstorage.hh"
#include "langcoder.hh"
#include "audiolink.hh"
#include "ex.hh"
#include "mdictparser.hh"
#include "filetype.hh"
#include "ftshelpers.hh"
#include "htmlescape.hh"
#include <algorithm>
#include <map>
#include <set>
#include <list>
#include "globalregex.hh"
#include "tiff.hh"
#include "utils.hh"
#include <QAtomicInt>
#include <QCryptographicHash>
#include <QDir>
#include <QRegularExpression>
#include <QString>
#include <QStringBuilder>
#include <QThreadPool>
#include <QtConcurrentRun>

namespace Mdx {

using std::map;
using std::multimap;
using std::set;
using std::list;
using std::pair;
using std::string;

using BtreeIndexing::WordArticleLink;
using BtreeIndexing::IndexedWords;
using BtreeIndexing::IndexInfo;

using namespace Mdict;

enum {
  kSignature            = 0x4349444d, // MDIC
  kCurrentFormatVersion = 11 + BtreeIndexing::FormatVersion + Folding::Version
};

DEF_EX( exCorruptDictionary, "dictionary file was tampered or corrupted", std::exception )

#pragma pack( push, 1 )

struct IdxHeader
{
  uint32_t signature;     // First comes the signature, MDIC
  uint32_t formatVersion; // File format version, currently 1.
  uint32_t parserVersion; // Version of the parser used to parse the MDIC file.
  // Version of the folding algorithm used when building
  // index. If it's different from the current one,
  // the file is to be rebuilt.
  uint32_t foldingVersion;

  uint32_t articleCount; // Total number of articles, for informative purposes only
  uint32_t wordCount;    // Total number of words, for informative purposes only

  uint32_t isRightToLeft; // Right to left
  uint32_t chunksOffset;  // The offset to chunks' storage

  uint32_t descriptionAddress; // Address of the dictionary description in the chunks' storage
  uint32_t descriptionSize;    // Size of the description in the chunks' storage, 0 = no description

  uint32_t styleSheetAddress;
  uint32_t styleSheetCount;

  uint32_t indexBtreeMaxElements; // Two fields from IndexInfo
  uint32_t indexRootOffset;

  uint32_t langFrom; // Source language
  uint32_t langTo;   // Target language

  uint32_t mddIndexInfosOffset; // address of IndexInfos for resource files (.mdd)
  uint32_t mddIndexInfosCount;  // count of IndexInfos for resource files
};
static_assert( alignof( IdxHeader ) == 1 );
#pragma pack( pop )

// A helper method to read resources from .mdd file
class IndexedMdd: public BtreeIndexing::BtreeIndex
{
  QMutex & idxMutex;
  QMutex fileMutex;
  ChunkedStorage::Reader & chunks;
  QFile mddFile;
  bool isFileOpen;

public:

  IndexedMdd( QMutex & idxMutex, ChunkedStorage::Reader & chunks ):
    idxMutex( idxMutex ),
    chunks( chunks ),
    isFileOpen( false )
  {
  }

  /// Opens the index. The values are those previously returned by buildIndex().
  using BtreeIndexing::BtreeIndex::openIndex;

  /// Opens the mdd file itself. Returns true if succeeded, false otherwise.
  bool open( const char * fileName )
  {
    mddFile.setFileName( QString::fromUtf8( fileName ) );
    isFileOpen = mddFile.open( QFile::ReadOnly );
    return isFileOpen;
  }

  /// Returns true if the mdd is open, false otherwise.
  inline bool isOpen() const
  {
    return isFileOpen;
  }

  /// Checks whether the given file exists in the mdd file or not.
  /// Note that this function is thread-safe, since it does not access mdd file.
  bool hasFile( std::u32string const & name )
  {
    if ( !isFileOpen ) {
      return false;
    }
    vector< WordArticleLink > links = findArticles( name );
    return !links.empty();
  }

  /// Attempts loading the given file into the given vector. Returns true on
  /// success, false otherwise.
  bool loadFile( std::u32string const & name, std::vector< char > & result )
  {
    if ( !isFileOpen ) {
      return false;
    }

    vector< WordArticleLink > links = findArticles( name );
    if ( links.empty() ) {
      return false;
    }

    MdictParser::RecordInfo indexEntry{};
    vector< char > chunk;
    // QMutexLocker _( &idxMutex );
    const char * indexEntryPtr = chunks.getBlock( links[ 0 ].articleOffset, chunk );
    memcpy( &indexEntry, indexEntryPtr, sizeof( indexEntry ) );

    //corrupted file or broken entry.
    if ( indexEntry.decompressedBlockSize < indexEntry.recordOffset + indexEntry.recordSize ) {
      return false;
    }

    QByteArray decompressed;

    {
      QMutexLocker _( &idxMutex );
      ScopedMemMap compressed( mddFile, indexEntry.compressedBlockPos, indexEntry.compressedBlockSize );
      if ( !compressed.startAddress() ) {
        return false;
      }

      if ( !MdictParser::parseCompressedBlock( indexEntry.compressedBlockSize,
                                               (char *)compressed.startAddress(),
                                               indexEntry.decompressedBlockSize,
                                               decompressed ) ) {
        return false;
      }
    }

    result.resize( indexEntry.recordSize );
    memcpy( &result.front(), decompressed.constData() + indexEntry.recordOffset, indexEntry.recordSize );
    return true;
  }
};

class MdxDictionary: public BtreeIndexing::BtreeDictionary
{
  QMutex idxMutex;
  File::Index idx;
  string idxFileName;
  IdxHeader idxHeader;
  string encoding;
  ChunkedStorage::Reader chunks;
  QFile dictFile;
  vector< sptr< IndexedMdd > > mddResources;
  MdictParser::StyleSheets styleSheets;

  QAtomicInt deferredInitDone;
  QMutex deferredInitMutex;
  bool deferredInitRunnableStarted;

  string initError;
  QString cacheDirName;

public:

  MdxDictionary( string const & id, string const & indexFile, vector< string > const & dictionaryFiles );

  ~MdxDictionary() override;

  void deferredInit() override;

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

  sptr< Dictionary::DataRequest > getArticle( std::u32string const & word,
                                              vector< std::u32string > const & alts,
                                              std::u32string const &,
                                              bool ignoreDiacritics ) override;
  sptr< Dictionary::DataRequest > getResource( string const & name ) override;
  QString const & getDescription() override;

  sptr< Dictionary::DataRequest >
  getSearchResults( QString const & searchString, int searchMode, bool matchCase, bool ignoreDiacritics ) override;
  void getArticleText( uint32_t articleAddress, QString & headword, QString & text ) override;

  void makeFTSIndex( QAtomicInt & isCancelled ) override;

  void setFTSParameters( Config::FullTextSearch const & fts ) override
  {
    if ( !ensureInitDone().empty() ) {
      return;
    }
    if ( metadata_enable_fts.has_value() ) {
      can_FTS = fts.enabled && metadata_enable_fts.value();
    }
    else {
      can_FTS = fts.enabled && !fts.disabledTypes.contains( "MDICT", Qt::CaseInsensitive )
        && ( fts.maxDictionarySize == 0 || getArticleCount() <= fts.maxDictionarySize );
    }
  }

  QString getCachedFileName( QString name );

protected:

  void loadIcon() noexcept override;

private:

  string const & ensureInitDone() override;
  void doDeferredInit();

  /// Loads an article with the given offset, filling the given strings.
  void loadArticle( uint32_t offset, string & articleText, bool noFilter = false );

  /// Process resource links (images, audios, etc)
  QString & filterResource( QString & article );

  void replaceLinks( QString & id, QString & article );
  //@font-face
  void replaceStyleInHtml( QString & id, QString & article );
  void replaceFontLinks( QString & id, QString & article );

  friend class MdxArticleRequest;
  friend class MddResourceRequest;
  void loadResourceFile( const std::u32string & resourceName, vector< char > & data );
};

MdxDictionary::MdxDictionary( string const & id, string const & indexFile, vector< string > const & dictionaryFiles ):
  BtreeDictionary( id, dictionaryFiles ),
  idx( indexFile, QIODevice::ReadOnly ),
  idxFileName( indexFile ),
  idxHeader( idx.read< IdxHeader >() ),
  chunks( idx, idxHeader.chunksOffset ),
  deferredInitRunnableStarted( false )
{
  // Read the dictionary's name
  idx.seek( sizeof( idxHeader ) );
  idx.readU32SizeAndData<>( dictionaryName );

  //fallback, use filename as dictionary name
  if ( dictionaryName.empty() ) {
    QFileInfo f( QString::fromUtf8( dictionaryFiles[ 0 ].c_str() ) );
    dictionaryName = f.baseName().toStdString();
  }

  // then read the dictionary's encoding
  idx.readU32SizeAndData<>( encoding );

  dictFile.setFileName( QString::fromUtf8( dictionaryFiles[ 0 ].c_str() ) );
  dictFile.open( QIODevice::ReadOnly );

  // Full-text search parameters

  ftsIdxName = indexFile + Dictionary::getFtsSuffix();

  cacheDirName = QDir::tempPath() + QDir::separator() + QString::fromUtf8( getId().c_str() ) + ".cache";
}

MdxDictionary::~MdxDictionary()
{
  QMutexLocker _( &deferredInitMutex );

  dictFile.close();

  Utils::Fs::removeDirectory( cacheDirName );
}

//////// MdxDictionary::deferredInit()

void MdxDictionary::deferredInit()
{
  if ( !Utils::AtomicInt::loadAcquire( deferredInitDone ) ) {
    QMutexLocker _( &deferredInitMutex );

    if ( Utils::AtomicInt::loadAcquire( deferredInitDone ) ) {
      return;
    }

    if ( !deferredInitRunnableStarted ) {
      QThreadPool::globalInstance()->start(
        [ this ]() {
          this->doDeferredInit();
        },
        -1000 );
      deferredInitRunnableStarted = true;
    }
  }
}

string const & MdxDictionary::ensureInitDone()
{
  doDeferredInit();
  return initError;
}

void MdxDictionary::doDeferredInit()
{
  if ( !Utils::AtomicInt::loadAcquire( deferredInitDone ) ) {
    QMutexLocker _( &deferredInitMutex );

    if ( Utils::AtomicInt::loadAcquire( deferredInitDone ) ) {
      return;
    }

    // Do deferred init

    try {
      // Retrieve stylesheets
      idx.seek( idxHeader.styleSheetAddress );
      for ( uint32_t i = 0; i < idxHeader.styleSheetCount; i++ ) {
        qint32 key = idx.read< qint32 >();
        vector< char > buf;
        quint32 sz;

        sz = idx.read< quint32 >();
        buf.resize( sz );
        idx.read( &buf.front(), sz );
        QString styleBegin = QString::fromUtf8( buf.data() );

        sz = idx.read< quint32 >();
        buf.resize( sz );
        idx.read( &buf.front(), sz );
        QString styleEnd = QString::fromUtf8( buf.data() );

        styleSheets[ key ] = pair< QString, QString >( styleBegin, styleEnd );
      }

      // Initialize the index
      openIndex( IndexInfo( idxHeader.indexBtreeMaxElements, idxHeader.indexRootOffset ), idx, idxMutex );

      vector< string > mddFileNames;
      vector< IndexInfo > mddIndexInfos;
      idx.seek( idxHeader.mddIndexInfosOffset );
      for ( uint32_t i = 0; i < idxHeader.mddIndexInfosCount; i++ ) {
        quint32 sz = idx.read< quint32 >();
        vector< char > buf( sz );
        idx.read( &buf.front(), sz );
        uint32_t btreeMaxElements = idx.read< uint32_t >();
        uint32_t rootOffset       = idx.read< uint32_t >();
        mddFileNames.emplace_back( &buf.front() );
        mddIndexInfos.emplace_back( btreeMaxElements, rootOffset );
      }

      vector< string > const dictFiles = getDictionaryFilenames();
      for ( uint32_t i = 1; i < dictFiles.size() && i < mddFileNames.size() + 1; i++ ) {
        QFileInfo fi( QString::fromUtf8( dictFiles[ i ].c_str() ) );
        QString mddFileName = QString::fromUtf8( mddFileNames[ i - 1 ].c_str() );

        if ( fi.fileName() != mddFileName || !fi.exists() ) {
          continue;
        }

        sptr< IndexedMdd > mdd = std::make_shared< IndexedMdd >( idxMutex, chunks );
        mdd->openIndex( mddIndexInfos[ i - 1 ], idx, idxMutex );
        mdd->open( dictFiles[ i ].c_str() );
        mddResources.push_back( mdd );
      }
    }
    catch ( std::exception & e ) {
      initError = e.what();
    }
    catch ( ... ) {
      initError = "Unknown error";
    }

    deferredInitDone.ref();
  }
}

void MdxDictionary::makeFTSIndex( QAtomicInt & isCancelled )
{
  if ( !( Dictionary::needToRebuildIndex( getDictionaryFilenames(), ftsIdxName )
          || FtsHelpers::ftsIndexIsOldOrBad( this ) ) ) {
    FTS_index_completed.ref();
  }

  if ( haveFTSIndex() ) {
    return;
  }

  //  if( !ensureInitDone().empty() )
  //    return;


  qDebug( "MDict: Building the full-text index for dictionary: %s", getName().c_str() );

  try {
    auto _dict = std::make_shared< MdxDictionary >( this->getId(), idxFileName, this->getDictionaryFilenames() );
    if ( !_dict->ensureInitDone().empty() ) {
      return;
    }
    FtsHelpers::makeFTSIndex( _dict.get(), isCancelled );
    FTS_index_completed.ref();
  }
  catch ( std::exception & ex ) {
    qWarning( "MDict: Failed building full-text search index for \"%s\", reason: %s", getName().c_str(), ex.what() );
    QFile::remove( ftsIdxName.c_str() );
  }
}

void MdxDictionary::getArticleText( uint32_t articleAddress, QString & headword, QString & text )
{
  try {
    headword.clear();
    string articleText;

    loadArticle( articleAddress, articleText, true );
    text = Html::unescape( QString::fromUtf8( articleText.data(), articleText.size() ) );
  }
  catch ( std::exception & ex ) {
    qWarning( "MDict: Failed retrieving article from \"%s\", reason: %s", getName().c_str(), ex.what() );
  }
}

sptr< Dictionary::DataRequest > MdxDictionary::getSearchResults( QString const & searchString,
                                                                 int searchMode,
                                                                 bool matchCase,

                                                                 bool ignoreDiacritics )
{
  return std::make_shared< FtsHelpers::FTSResultsRequest >( *this,
                                                            searchString,
                                                            searchMode,
                                                            matchCase,
                                                            ignoreDiacritics );
}

/// MdxDictionary::getArticle

class MdxArticleRequest: public Dictionary::DataRequest
{
  std::u32string word;
  vector< std::u32string > alts;
  MdxDictionary & dict;
  bool ignoreDiacritics;

  QAtomicInt isCancelled;
  QFuture< void > f;

public:

  MdxArticleRequest( std::u32string const & word_,
                     vector< std::u32string > const & alts_,
                     MdxDictionary & dict_,
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

  ~MdxArticleRequest() override
  {
    isCancelled.ref();
    f.waitForFinished();
  }
};


void MdxArticleRequest::run()
{
  if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
    finish();
    return;
  }

  if ( !dict.ensureInitDone().empty() ) {
    setErrorString( QString::fromUtf8( dict.ensureInitDone().c_str() ) );
    finish();
    return;
  }

  vector< WordArticleLink > chain = dict.findArticles( word, ignoreDiacritics );

  for ( const auto & alt : alts ) {
    /// Make an additional query for each alt
    vector< WordArticleLink > altChain = dict.findArticles( alt, ignoreDiacritics );
    chain.insert( chain.end(), altChain.begin(), altChain.end() );
  }

  // Some synonims make it that the articles appear several times. We combat this
  // by only allowing them to appear once.
  set< uint32_t > articlesIncluded;
  // Sometimes the articles are physically duplicated. We store hashes of
  // the bodies to account for this.
  set< QByteArray > articleBodiesIncluded;
  string articleText;

  for ( unsigned x = 0; x < chain.size(); ++x ) {
    if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
      finish();
      return;
    }

    if ( articlesIncluded.find( chain[ x ].articleOffset ) != articlesIncluded.end() ) {
      continue; // We already have this article in the body.
    }

    // Grab that article
    string articleBody;
    bool hasError = false;
    QString errorMessage;

    try {
      dict.loadArticle( chain[ x ].articleOffset, articleBody );
    }
    catch ( exCorruptDictionary & ) {
      errorMessage = tr( "Dictionary file was tampered or corrupted" );
      hasError     = true;
    }
    catch ( std::exception & e ) {
      errorMessage = e.what();
      hasError     = true;
    }

    if ( hasError ) {
      setErrorString( tr( "Failed loading article from %1, reason: %2" )
                        .arg( QString::fromUtf8( dict.getDictionaryFilenames()[ 0 ].c_str() ), errorMessage ) );
      finish();
      return;
    }

    if ( articlesIncluded.find( chain[ x ].articleOffset ) != articlesIncluded.end() ) {
      continue; // We already have this article in the body.
    }

    QCryptographicHash hash( QCryptographicHash::Md5 );
    hash.addData( { articleBody.data(), static_cast< qsizetype >( articleBody.length() ) } );
    if ( !articleBodiesIncluded.insert( hash.result() ).second ) {
      continue; // Already had this body
    }

    // Handle internal redirects
    if ( strncmp( articleBody.c_str(), "@@@LINK=", 8 ) == 0 ) {
      std::u32string target = Text::toUtf32( articleBody.c_str() + 8 );
      target                = Folding::trimWhitespace( target );
      // Make an additional query for this redirection
      vector< WordArticleLink > altChain = dict.findArticles( target );
      chain.insert( chain.end(), altChain.begin(), altChain.end() );
      continue;
    }

    // See Issue #271: A mechanism to clean-up invalid HTML cards.
    string cleaner = Utils::Html::getHtmlCleaner();
    articleText += "<div class=\"mdict\">" + articleBody + cleaner + "</div>\n";
  }

  if ( !articleText.empty() ) {
    articleText += "</div></div></div></div></div></div></div></div></div>";

    QMutexLocker _( &dataMutex );
    data.insert( data.end(), articleText.begin(), articleText.end() );
    hasAnyData = true;
  }

  finish();
}

sptr< Dictionary::DataRequest > MdxDictionary::getArticle( const std::u32string & word,
                                                           const vector< std::u32string > & alts,
                                                           const std::u32string &,
                                                           bool ignoreDiacritics )
{
  return std::make_shared< MdxArticleRequest >( word, alts, *this, ignoreDiacritics );
}

/// MdxDictionary::getResource
class MddResourceRequest: public Dictionary::DataRequest
{
  MdxDictionary & dict;
  std::u32string resourceName;
  QAtomicInt isCancelled;
  QFuture< void > f;

public:

  MddResourceRequest( MdxDictionary & dict_, string const & resourceName_ ):
    Dictionary::DataRequest( &dict_ ),
    dict( dict_ ),
    resourceName( Text::toUtf32( resourceName_ ) )
  {
    f = QtConcurrent::run( [ this ]() {
      this->run();
    } );
  }

  QByteArray isolate_css();
  void run();

  void cancel() override
  {
    isCancelled.ref();
  }

  ~MddResourceRequest()
  {
    isCancelled.ref();
    f.waitForFinished();
  }
};

QByteArray MddResourceRequest::isolate_css()
{

  const QString id = QString::fromUtf8( dict.getId().c_str() );

  QString css = QString::fromUtf8( data.data(), data.size() );

  int pos = 0;

  QString newCSS;
  QRegularExpressionMatchIterator it = RX::Mdx::links.globalMatch( css );
  while ( it.hasNext() ) {
    QRegularExpressionMatch match = it.next();
    newCSS += css.mid( pos, match.capturedStart() - pos );
    pos         = match.capturedEnd();
    QString url = match.captured( 2 );


    if ( url.indexOf( ":/" ) >= 0 || url.indexOf( "data:" ) >= 0 ) {
      // External link or base64-encoded data
      newCSS += match.captured();

      continue;
    }

    QString newUrl = QString( "url(" ) + match.captured( 1 ) + "bres://" + id + "/" + url + match.captured( 3 ) + ")";
    newCSS += newUrl;
  }
  if ( pos ) {
    newCSS += css.mid( pos );
    css = newCSS;
    newCSS.clear();
  }
  dict.isolateCSS( css, ".mdict" );
  auto bytes = css.toUtf8();

  return bytes;
}

void MddResourceRequest::run()
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

  // In order to prevent recursive internal redirection...
  set< std::u32string, std::less<> > resourceIncluded;

  for ( ;; ) {
    // Some runnables linger enough that they are cancelled before they start
    if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
      finish();
      return;
    }
    string u8ResourceName = Text::toUtf8( resourceName );
    if ( !resourceIncluded.insert( resourceName ).second ) {
      finish();
      return;
    }

    QMutexLocker _( &dataMutex );
    data.clear();

    //check the global cache
    //generate resource unique key
    const QString id = QString::fromUtf8( dict.getId().c_str() );

    const QString unique_key = id + QString::fromStdString( u8ResourceName );

    dict.loadResourceFile( resourceName, data );

    // Check if this file has a redirection
    // Always encoded in UTF16-LE
    // L"@@@LINK="
    static const char pattern[ 16 ] =
      { '@', '\0', '@', '\0', '@', '\0', 'L', '\0', 'I', '\0', 'N', '\0', 'K', '\0', '=', '\0' };

    if ( data.size() > sizeof( pattern ) ) {
      if ( memcmp( &data.front(), pattern, sizeof( pattern ) ) == 0 ) {
        data.push_back( '\0' );
        data.push_back( '\0' );
        QString target =
          MdictParser::toUtf16( "UTF-16LE", &data.front() + sizeof( pattern ), data.size() - sizeof( pattern ) );
        resourceName = target.trimmed().toStdU32String();
        continue;
      }
    }

    if ( data.size() > 0 ) {
      hasAnyData = true;

      if ( Filetype::isNameOfCSS( u8ResourceName ) ) {
        QByteArray bytes = isolate_css();

        data.resize( bytes.size() );
        memcpy( &data.front(), bytes.constData(), bytes.size() );
      }
      if ( Filetype::isNameOfTiff( u8ResourceName ) ) {
        // Convert it
        GdTiff::tiff2img( data );
      }
    }
    break;
  }

  finish();
}

sptr< Dictionary::DataRequest > MdxDictionary::getResource( const string & name )
{
  return std::make_shared< MddResourceRequest >( *this, name );
}

const QString & MdxDictionary::getDescription()
{
  if ( !dictionaryDescription.isEmpty() ) {
    return dictionaryDescription;
  }

  if ( idxHeader.descriptionSize == 0 ) {
    dictionaryDescription = "NONE";
  }
  else {
    // QMutexLocker _( &idxMutex );
    vector< char > chunk;
    char * dictDescription = chunks.getBlock( idxHeader.descriptionAddress, chunk );
    string str( dictDescription );
    dictionaryDescription = QString::fromUtf8( str.c_str(), str.size() );
  }

  return dictionaryDescription;
}

void MdxDictionary::loadIcon() noexcept
{
  if ( dictionaryIconLoaded ) {
    return;
  }

  QString fileName = QDir::fromNativeSeparators( getDictionaryFilenames()[ 0 ].c_str() );

  // Remove the extension
  fileName.chop( 3 );
  QString text = QString::fromStdString( dictionaryName );

  if ( !loadIconFromFile( fileName ) && !loadIconFromText( ":/icons/mdict-bg.png", text ) ) {
    // Use default icons
    dictionaryIcon = QIcon( ":/icons/mdict.png" );
  }

  dictionaryIconLoaded = true;
}

void MdxDictionary::loadArticle( uint32_t offset, string & articleText, bool noFilter )
{
  vector< char > chunk;
  // QMutexLocker _( &idxMutex );

  // Load record info from index
  MdictParser::RecordInfo recordInfo;
  const char * pRecordInfo = chunks.getBlock( offset, chunk );
  memcpy( &recordInfo, pRecordInfo, sizeof( recordInfo ) );

  QByteArray decompressed;

  {
    QMutexLocker _( &idxMutex );
    ScopedMemMap compressed( dictFile, recordInfo.compressedBlockPos, recordInfo.compressedBlockSize );
    if ( !compressed.startAddress() ) {
      throw exCorruptDictionary();
    }

    if ( !MdictParser::parseCompressedBlock( recordInfo.compressedBlockSize,
                                             (char *)compressed.startAddress(),
                                             recordInfo.decompressedBlockSize,
                                             decompressed ) ) {
      throw exCorruptDictionary();
    }
  }

  QString article =
    MdictParser::toUtf16( encoding.c_str(), decompressed.constData() + recordInfo.recordOffset, recordInfo.recordSize );

  if ( !noFilter ) {
    article = MdictParser::substituteStylesheet( article, styleSheets );
    article = filterResource( article );
  }

  articleText = Utils::c_string( article );
}

QString & MdxDictionary::filterResource( QString & article )
{
  QString id = QString::fromStdString( getId() );
  replaceLinks( id, article );
  replaceStyleInHtml( id, article );
  return article;
}

void MdxDictionary::replaceLinks( QString & id, QString & article )
{
  QString articleNewText;
  int linkPos                        = 0;
  QRegularExpressionMatchIterator it = RX::Mdx::allLinksRe.globalMatch( article );
  while ( it.hasNext() ) {
    QRegularExpressionMatch allLinksMatch = it.next();

    if ( allLinksMatch.capturedEnd() < linkPos ) {
      continue;
    }

    articleNewText += article.mid( linkPos, allLinksMatch.capturedStart() - linkPos );
    linkPos = allLinksMatch.capturedEnd();

    QString linkTxt  = allLinksMatch.captured();
    QString linkType = allLinksMatch.captured( 1 ).toLower();
    QString newLink;

    if ( linkType.compare( "a" ) == 0 || linkType.compare( "area" ) == 0 ) {
      newLink = linkTxt;

      QRegularExpressionMatch match = RX::Mdx::audioRe.match( newLink );
      if ( match.hasMatch() ) {
        // sounds and audio link script
        QString newTxt = match.captured( 1 ) + match.captured( 2 ) + "gdau://" + id + "/" + match.captured( 3 )
          + match.captured( 2 ) + R"( onclick="return false;" )";
        newLink = QString::fromUtf8(
                    addAudioLink( "gdau://" + getId() + "/" + match.captured( 3 ).toUtf8().data(), getId() ).c_str() )
          + newLink.replace( match.capturedStart(), match.capturedLength(), newTxt );
      }

      match = RX::Mdx::wordCrossLink.match( newLink );
      if ( match.hasMatch() ) {
        if ( !match.captured( 3 ).isEmpty() ) {
          QString newTxt = match.captured( 1 ) + match.captured( 2 ) + "gdlookup://localhost/" + match.captured( 3 );

          if ( match.lastCapturedIndex() >= 4 && !match.captured( 4 ).isEmpty() ) {
            newTxt += QString( "?gdanchor=" ) + match.captured( 4 ).mid( 1 );
          }

          newTxt += match.captured( 2 );
          newLink.replace( match.capturedStart(), match.capturedLength(), newTxt );
        }
        else {
          //links like entry://#abc,just remove the prefix entry://
          QString newTxt = match.captured( 1 ) + match.captured( 2 );

          if ( match.lastCapturedIndex() >= 4 && !match.captured( 4 ).isEmpty() ) {
            newTxt += match.captured( 4 );
          }

          newTxt += match.captured( 2 );
          newLink.replace( match.capturedStart(), match.capturedLength(), newTxt );
        }
      }
    }
    else if ( linkType.compare( "link" ) == 0 ) {
      // stylesheets
      QRegularExpressionMatch match = RX::Mdx::stylesRe.match( linkTxt );
      if ( match.hasMatch() ) {
        QString newText =
          match.captured( 1 ) + match.captured( 2 ) + "bres://" + id + "/" + match.captured( 3 ) + match.captured( 2 );
        newLink = linkTxt.replace( match.capturedStart(), match.capturedLength(), newText );
      }
      else {
        newLink = linkTxt.replace( RX::Mdx::stylesRe2, R"(\1"bres://)" + id + R"(/\2")" );
      }
    }
    else {
      //linkType in ("script","img","source","audio","video")
      // javascripts and images
      QRegularExpressionMatch match = RX::Mdx::inlineScriptRe.match( linkTxt );
      // "script" tag
      if ( linkType.compare( "script" ) == 0 && match.hasMatch() && match.capturedLength() == linkTxt.length() ) {
        // skip inline scripts
        articleNewText += linkTxt;
        match = RX::Mdx::closeScriptTagRe.match( article, linkPos );
        if ( match.hasMatch() ) {
          articleNewText += article.mid( linkPos, match.capturedEnd() - linkPos );
          linkPos = match.capturedEnd();
        }
        continue;
      }
      else {
        //audio ,video ,html5 tags fall here.
        match = RX::Mdx::srcRe.match( linkTxt );
        if ( match.hasMatch() ) {
          QString newText;
          QString scheme;
          // "source" tag
          if ( linkType.compare( "source" ) == 0 ) {
            scheme = "gdvideo://";
          }
          else {
            scheme = "bres://";
          }
          newText =
            match.captured( 1 ) + match.captured( 2 ) + scheme + id + "/" + match.captured( 3 ) + match.captured( 2 );

          newLink = linkTxt.replace( match.capturedStart(), match.capturedLength(), newText );
        }
        else {
          newLink = linkTxt.replace( RX::Mdx::srcRe2, R"(\1"bres://)" + id + R"(/\2")" );
        }
      }

      // convert <img src="bres://{id}/a.png" srcset="a-1x.png 1x, b-2x.png 2x, c.png">
      // into    <img src="bres://{id}/a.png" srcset="bres://{id}/a-1x.png 1x,bres://{id}/b-2x.png 2x,bres://{id}/c.png">

      if ( linkType.compare( "img" ) == 0 ) {
        match = RX::Mdx::srcset.match( newLink ); // have to use newLink since linkTxt may already be modified
        if ( match.hasMatch() ) {
          auto srcsetOriginalText   = match.captured( "text" );
          QStringList srcsetNewText = {};

          auto ImageList = srcsetOriginalText.split( u',', Qt::SkipEmptyParts );

          for ( auto & img : ImageList ) {
            auto imgPair = img.split( RX::whiteSpace );

            if ( !imgPair.empty() && !imgPair.at( 0 ).contains( "//" ) ) {
              if ( imgPair.length() == 1 ) {
                srcsetNewText.append( QString( R"(bres://%1/%2)" ).arg( id, imgPair.at( 0 ) ) );
              }
              else if ( imgPair.length() == 2 ) {
                srcsetNewText.append( QString( R"(bres://%1/%2 %3)" ).arg( id, imgPair.at( 0 ), imgPair.at( 1 ) ) );
              }
            }
          }

          newLink.replace( match.capturedStart(),
                           match.capturedLength(),
                           match.captured( "before" ) % srcsetNewText.join( ',' ) % match.captured( "after" ) );
        }
      }

      if ( linkType.compare( "object" ) == 0 ) {
        match = RX::Mdx::objectdata.match( newLink );
        if ( match.hasMatch() ) {
          auto srcsetOriginalText = match.captured( "text" );
          QString srcsetNewText;
          if ( !srcsetOriginalText.contains( "//" ) ) {
            srcsetNewText = QString( R"(bres://%1/%2)" ).arg( id, srcsetOriginalText );
          }

          newLink.replace( match.capturedStart(),
                           match.capturedLength(),
                           match.captured( "before" ) % srcsetNewText % match.captured( "after" ) );
        }
      }
    }

    if ( !newLink.isEmpty() ) {
      articleNewText += newLink;
    }
    else {
      articleNewText += allLinksMatch.captured();
    }
  }
  if ( linkPos ) {
    articleNewText += article.mid( linkPos );
    article = articleNewText;
  }
}

void MdxDictionary::replaceStyleInHtml( QString & id, QString & article )
{
  //article = article.replace( RX::Mdx::fontFace, "src:url(\"bres://" + id + "/" + "\\1\")" );
  QString articleNewText;
  int linkPos                        = 0;
  QRegularExpressionMatchIterator it = RX::Mdx::styleElement.globalMatch( article );
  while ( it.hasNext() ) {
    QRegularExpressionMatch allLinksMatch = it.next();

    if ( allLinksMatch.capturedEnd() < linkPos ) {
      continue;
    }

    articleNewText += article.mid( linkPos, allLinksMatch.capturedStart() - linkPos );
    linkPos = allLinksMatch.capturedEnd();

    articleNewText += allLinksMatch.captured( 1 );

    // the style
    auto style = allLinksMatch.captured( 2 );
    replaceFontLinks( id, style );
    articleNewText += style;
    articleNewText += allLinksMatch.captured( 3 );
  }
  if ( linkPos ) {
    articleNewText += article.mid( linkPos );
    article = articleNewText;
  }
}

void MdxDictionary::replaceFontLinks( QString & id, QString & article )
{
  //article = article.replace( RX::Mdx::fontFace, "src:url(\"bres://" + id + "/" + "\\1\")" );
  QString articleNewText;
  int linkPos                        = 0;
  QRegularExpressionMatchIterator it = RX::Mdx::fontFace.globalMatch( article );
  while ( it.hasNext() ) {
    QRegularExpressionMatch allLinksMatch = it.next();

    if ( allLinksMatch.capturedEnd() < linkPos ) {
      continue;
    }

    articleNewText += article.mid( linkPos, allLinksMatch.capturedStart() - linkPos );
    linkPos          = allLinksMatch.capturedEnd();
    QString linkTxt  = allLinksMatch.captured();
    QString linkType = allLinksMatch.captured( 1 );
    QString newLink  = linkTxt;

    //skip remote url
    if ( !linkType.contains( ":" ) ) {
      newLink = QString( "url(\"bres://%1/%2\")" ).arg( id, linkType );
    }
    articleNewText += newLink;
  }
  if ( linkPos ) {
    articleNewText += article.mid( linkPos );
    article = articleNewText;
  }
}


QString MdxDictionary::getCachedFileName( QString filename )
{
  QDir dir;
  QFileInfo info( cacheDirName );
  if ( !info.exists() || !info.isDir() ) {
    if ( !dir.mkdir( cacheDirName ) ) {
      qWarning( "Mdx: can't create cache directory \"%s\"", cacheDirName.toUtf8().data() );
      return QString();
    }
  }

  // Create subfolders if needed

  QString name = filename;
  name.replace( '/', '\\' );
  QStringList list = name.split( '\\' );
  int subFolders   = list.size() - 1;
  if ( subFolders > 0 ) {
    QString dirName = cacheDirName;
    for ( int i = 0; i < subFolders; i++ ) {
      dirName += QDir::separator() + list.at( i );
      QFileInfo dirInfo( dirName );
      if ( !dirInfo.exists() ) {
        if ( !dir.mkdir( dirName ) ) {
          qWarning( "Mdx: can't create cache directory \"%s\"", dirName.toUtf8().data() );
          return QString();
        }
      }
    }
  }

  QString fullName = cacheDirName + QDir::separator() + filename;

  info.setFile( fullName );
  if ( info.exists() ) {
    return fullName;
  }
  QFile f( fullName );
  if ( !f.open( QFile::WriteOnly ) ) {
    qWarning( R"(Mdx: file "%s" creating error: "%s")", fullName.toUtf8().data(), f.errorString().toUtf8().data() );
    return QString();
  }
  std::u32string resourceName = filename.toStdU32String();
  vector< char > data;

  // In order to prevent recursive internal redirection...
  set< std::u32string, std::less<> > resourceIncluded;

  for ( ;; ) {
    if ( !resourceIncluded.insert( resourceName ).second ) {
      continue;
    }

    loadResourceFile( resourceName, data );

    // Check if this file has a redirection
    // Always encoded in UTF16-LE
    // L"@@@LINK="
    if ( static const char pattern[ 16 ] =
           { '@', '\0', '@', '\0', '@', '\0', 'L', '\0', 'I', '\0', 'N', '\0', 'K', '\0', '=', '\0' };
         data.size() > sizeof( pattern ) && memcmp( &data.front(), pattern, sizeof( pattern ) ) == 0 ) {
      data.push_back( '\0' );
      data.push_back( '\0' );
      QString target =
        MdictParser::toUtf16( "UTF-16LE", &data.front() + sizeof( pattern ), data.size() - sizeof( pattern ) );
      resourceName = target.trimmed().toStdU32String();
      continue;
    }
    break;
  }

  qint64 n = 0;
  if ( !data.empty() ) {
    n = f.write( data.data(), data.size() );
  }

  f.close();

  if ( n < (qint64)data.size() ) {
    qWarning( R"(Mdx: file "%s" writing error: "%s")", fullName.toUtf8().data(), f.errorString().toUtf8().data() );
    return QString();
  }
  return fullName;
}

void MdxDictionary::loadResourceFile( const std::u32string & resourceName, vector< char > & data )
{
  std::u32string newResourceName = resourceName;
  string u8ResourceName          = Text::toUtf8( resourceName );

  // Convert to the Windows separator
  std::replace( newResourceName.begin(), newResourceName.end(), '/', '\\' );
  if ( newResourceName[ 0 ] == '.' ) {
    newResourceName.erase( 0, 1 );
  }
  if ( newResourceName[ 0 ] != '\\' ) {
    newResourceName.insert( 0, 1, '\\' );
  }
  // local file takes precedence
  if ( string fn = getContainingFolder().toStdString() + Utils::Fs::separator() + u8ResourceName;
       Utils::Fs::exists( fn ) ) {
    File::loadFromFile( fn, data );
    return;
  }
  for ( const auto & mddResource : mddResources ) {
    if ( mddResource->loadFile( newResourceName, data ) ) {
      break;
    }
  }
}

static void addEntryToIndex( QString const & word, uint32_t offset, IndexedWords & indexedWords )
{
  // Strip any leading or trailing whitespaces
  QString wordTrimmed = word.trimmed();
  indexedWords.addWord( wordTrimmed.toStdU32String(), offset );
}

static void addEntryToIndexSingle( QString const & word, uint32_t offset, IndexedWords & indexedWords )
{
  // Strip any leading or trailing whitespaces
  QString wordTrimmed = word.trimmed();
  indexedWords.addSingleWord( wordTrimmed.toStdU32String(), offset );
}

class ArticleHandler: public MdictParser::RecordHandler
{
public:
  ArticleHandler( ChunkedStorage::Writer & chunks, IndexedWords & indexedWords ):
    chunks( chunks ),
    indexedWords( indexedWords )
  {
  }

  void handleRecord( QString const & headWord, MdictParser::RecordInfo const & recordInfo ) override
  {
    // Save the article's record info
    uint32_t articleAddress = chunks.startNewBlock();
    chunks.addToBlock( &recordInfo, sizeof( recordInfo ) );
    // Add entries to the index
    addEntryToIndex( headWord, articleAddress, indexedWords );
  }

private:
  ChunkedStorage::Writer & chunks;
  IndexedWords & indexedWords;
};

class ResourceHandler: public MdictParser::RecordHandler
{
public:
  ResourceHandler( ChunkedStorage::Writer & chunks, IndexedWords & indexedWords ):
    chunks( chunks ),
    indexedWords( indexedWords )
  {
  }

  void handleRecord( QString const & fileName, MdictParser::RecordInfo const & recordInfo ) override
  {
    uint32_t resourceInfoAddress = chunks.startNewBlock();
    chunks.addToBlock( &recordInfo, sizeof( recordInfo ) );
    // Add entries to the index
    addEntryToIndexSingle( fileName, resourceInfoAddress, indexedWords );
  }

private:
  ChunkedStorage::Writer & chunks;
  IndexedWords & indexedWords;
};


static bool indexIsOldOrBad( vector< string > const & dictFiles, string const & indexFile )
{
  File::Index idx( indexFile, QIODevice::ReadOnly );
  IdxHeader header;

  return idx.readRecords( &header, sizeof( header ), 1 ) != 1 || header.signature != kSignature
    || header.formatVersion != kCurrentFormatVersion || header.parserVersion != MdictParser::kParserVersion
    || header.foldingVersion != Folding::Version || header.mddIndexInfosCount != dictFiles.size() - 1;
}

static void findResourceFiles( string const & mdx, vector< string > & dictFiles )
{
  string base( mdx, 0, mdx.size() - 4 );
  // Check if there' is any file end with .mdd, which is the resource file for the dictionary
  string resFile;
  if ( File::tryPossibleName( base + ".mdd", resFile ) ) {
    dictFiles.push_back( resFile );
    // Find complementary .mdd file (volumes), like follows:
    //   demo.mdx   <- main dictionary file
    //   demo.mdd   <- main resource file ( 1st volume )
    //   demo.1.mdd <- 2nd volume
    //   ...
    //   demo.n.mdd <- nth volume
    QString baseU8 = QString::fromUtf8( base.c_str() );
    int vol        = 1;
    while ( File::tryPossibleName( string( QString( "%1.%2.mdd" ).arg( baseU8 ).arg( vol ).toUtf8().constBegin() ),
                                   resFile ) ) {
      dictFiles.push_back( resFile );
      vol++;
    }
  }
}

vector< sptr< Dictionary::Class > > makeDictionaries( vector< string > const & fileNames,
                                                      string const & indicesDir,
                                                      Dictionary::Initializing & initializing )
{
  vector< sptr< Dictionary::Class > > dictionaries;

  for ( const auto & fileName : fileNames ) {
    // Skip files with the extensions different to .mdx to speed up the
    // scanning
    if ( !Utils::endsWithIgnoreCase( fileName, ".mdx" ) ) {
      continue;
    }

    vector< string > dictFiles( 1, fileName );
    findResourceFiles( fileName, dictFiles );

    initializing.loadingDictionary( fileName );

    string dictId    = Dictionary::makeDictionaryId( dictFiles );
    string indexFile = indicesDir + dictId;

    if ( Dictionary::needToRebuildIndex( dictFiles, indexFile ) || indexIsOldOrBad( dictFiles, indexFile ) ) {
      // Building the index

      qDebug( "MDict: Building the index for dictionary: %s", fileName.c_str() );

      MdictParser parser;
      list< sptr< MdictParser > > mddParsers;

      if ( !parser.open( fileName.c_str() ) ) {
        continue;
      }

      string title = parser.title().toStdString();
      initializing.indexingDictionary( title );

      for ( vector< string >::const_iterator mddIter = dictFiles.begin() + 1; mddIter != dictFiles.end(); ++mddIter ) {
        if ( Utils::Fs::exists( *mddIter ) ) {
          sptr< MdictParser > mddParser = std::make_shared< MdictParser >();
          if ( !mddParser->open( mddIter->c_str() ) ) {
            qWarning( "Broken mdd (resource) file: %s", mddIter->c_str() );
            continue;
          }
          mddParsers.push_back( mddParser );
        }
      }

      File::Index idx( indexFile, QIODevice::WriteOnly );
      IdxHeader idxHeader;
      memset( &idxHeader, 0, sizeof( idxHeader ) );
      // We write a dummy header first. At the end of the process the header
      // will be rewritten with the right values.
      idx.write( idxHeader );

      // Write the title first
      idx.write< uint32_t >( title.size() );
      idx.write( title.data(), title.size() );

      // then the encoding
      {
        string encoding = parser.encoding().toStdString();
        idx.write< uint32_t >( encoding.size() );
        idx.write( encoding.data(), encoding.size() );
      }

      // This is our index data that we accumulate during the loading process.
      // For each new word encountered, we emit the article's body to the file
      // immediately, inserting the word itself and its offset in this map.
      // This map maps folded words to the original words and the corresponding
      // articles' offsets.
      IndexedWords indexedWords;
      ChunkedStorage::Writer chunks( idx );

      idxHeader.isRightToLeft = parser.isRightToLeft();

      // Save dictionary description if there's one
      {
        string description           = parser.description().toStdString();
        idxHeader.descriptionAddress = chunks.startNewBlock();
        chunks.addToBlock( description.c_str(), description.size() + 1 );
        idxHeader.descriptionSize = description.size() + 1;
      }

      ArticleHandler articleHandler( chunks, indexedWords );
      MdictParser::HeadWordIndex headWordIndex;

      // enumerating word and its definition
      while ( parser.readNextHeadWordIndex( headWordIndex ) ) {
        parser.readRecordBlock( headWordIndex, articleHandler );
      }

      // enumerating resources if there's any
      vector< sptr< IndexedWords > > mddIndices;
      vector< string > mddFileNames;
      while ( !mddParsers.empty() ) {
        sptr< MdictParser > mddParser        = mddParsers.front();
        sptr< IndexedWords > mddIndexedWords = std::make_shared< IndexedWords >();
        MdictParser::HeadWordIndex resourcesIndex;
        ResourceHandler resourceHandler( chunks, *mddIndexedWords );

        while ( mddParser->readNextHeadWordIndex( headWordIndex ) ) {
          resourcesIndex.insert( resourcesIndex.end(), headWordIndex.begin(), headWordIndex.end() );
        }
        mddParser->readRecordBlock( resourcesIndex, resourceHandler );

        mddIndices.push_back( mddIndexedWords );
        // Save filename for .mdd files only
        QFileInfo fi( mddParser->filename() );
        mddFileNames.push_back( fi.fileName().toStdString() );
        mddParsers.pop_front();
      }

      // Finish with the chunks
      idxHeader.chunksOffset = chunks.finish();

      qDebug( "Writing index..." );

      // Good. Now build the index
      IndexInfo idxInfo               = BtreeIndexing::buildIndex( indexedWords, idx );
      idxHeader.indexBtreeMaxElements = idxInfo.btreeMaxElements;
      idxHeader.indexRootOffset       = idxInfo.rootOffset;

      // Save dictionary stylesheets
      {
        MdictParser::StyleSheets const & styleSheets = parser.styleSheets();
        idxHeader.styleSheetAddress                  = idx.tell();
        idxHeader.styleSheetCount                    = styleSheets.size();

        for ( auto const & [ key, value ] : styleSheets ) {
          string const styleBegin( value.first.toStdString() );
          string const styleEnd( value.second.toStdString() );

          // key
          idx.write< qint32 >( key );
          // styleBegin
          idx.write< quint32 >( (quint32)styleBegin.size() + 1 );
          idx.write( styleBegin.c_str(), styleBegin.size() + 1 );
          // styleEnd
          idx.write< quint32 >( (quint32)styleEnd.size() + 1 );
          idx.write( styleEnd.c_str(), styleEnd.size() + 1 );
        }
      }

      // read languages from dictioanry's file name
      auto langs = LangCoder::findLangIdPairFromPath( fileName );

      // if no languages found, try dictionary name
      if ( langs.first == 0 || langs.second == 0 ) {
        langs = LangCoder::findLangIdPairFromName( parser.title() );
      }

      idxHeader.langFrom = langs.first;
      idxHeader.langTo   = langs.second;

      // Build index info for each mdd file
      vector< IndexInfo > mddIndexInfos;
      for ( const auto & mddIndice : mddIndices ) {
        IndexInfo const resourceIdxInfo = BtreeIndexing::buildIndex( *mddIndice, idx );
        mddIndexInfos.push_back( resourceIdxInfo );
      }

      // Save address of IndexInfos for resource files
      idxHeader.mddIndexInfosOffset = idx.tell();
      idxHeader.mddIndexInfosCount  = mddIndexInfos.size();
      for ( uint32_t mi = 0; mi < mddIndexInfos.size(); mi++ ) {
        const string & mddfile = mddFileNames[ mi ];

        idx.write< quint32 >( (quint32)mddfile.size() + 1 );
        idx.write( mddfile.c_str(), mddfile.size() + 1 );
        idx.write< uint32_t >( mddIndexInfos[ mi ].btreeMaxElements );
        idx.write< uint32_t >( mddIndexInfos[ mi ].rootOffset );
      }

      // That concludes it. Update the header.
      idxHeader.signature      = kSignature;
      idxHeader.formatVersion  = kCurrentFormatVersion;
      idxHeader.parserVersion  = MdictParser::kParserVersion;
      idxHeader.foldingVersion = Folding::Version;
      idxHeader.articleCount   = parser.wordCount();
      idxHeader.wordCount      = parser.wordCount();

      idx.rewind();
      idx.write( &idxHeader, sizeof( idxHeader ) );
    }

    dictionaries.push_back( std::make_shared< MdxDictionary >( dictId, indexFile, dictFiles ) );
  }

  return dictionaries;
}

} // namespace Mdx
