/* This file is (c) 2012 Abs62
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#ifdef MAKE_ZIM_SUPPORT

  #include "zim.hh"
  #include "btreeidx.hh"

  #include "folding.hh"
  #include "gddebug.hh"
  #include "utf8.hh"
  #include "langcoder.hh"
  #include "filetype.hh"
  #include "file.hh"
  #include "utils.hh"
  #include "tiff.hh"
  #include "ftshelpers.hh"
  #include "htmlescape.hh"

  #ifdef _MSC_VER
    #include <stub_msvc.h>
  #endif

  #include <QByteArray>
  #include <QFile>
  #include <QString>
  #include <QAtomicInt>
  #include <QImage>
  #include <QDir>

  #include <QRegularExpression>

  #include <string>
  #include <set>
  #include <map>
  #include <algorithm>
  #include <QtConcurrent>
  #include <utility>
  #include "globalregex.hh"
  #include <zim/zim.h>
  #include <zim/archive.h>
  #include <zim/entry.h>
  #include <zim/item.h>
  #include <zim/error.h>

namespace Zim {

using std::string;
using std::map;
using std::vector;
using std::multimap;
using std::pair;
using std::set;
using gd::wstring;

using BtreeIndexing::WordArticleLink;
using BtreeIndexing::IndexedWords;
using BtreeIndexing::IndexInfo;

DEF_EX_STR( exNotZimFile, "Not an Zim file", Dictionary::Ex )
using Dictionary::exCantReadFile;
DEF_EX_STR( exInvalidZimHeader, "Invalid Zim header", Dictionary::Ex )
DEF_EX( exUserAbort, "User abort", Dictionary::Ex )


using ZimFile = zim::Archive;


  #pragma pack( push, 1 )

enum {
  Signature            = 0x584D495A, // ZIMX on little-endian, XMIZ on big-endian
  CurrentFormatVersion = 4 + BtreeIndexing::FormatVersion + Folding::Version
};

struct IdxHeader
{
  quint32 signature;             // First comes the signature, ZIMX
  quint32 formatVersion;         // File format version (CurrentFormatVersion)
  quint32 indexBtreeMaxElements; // Two fields from IndexInfo
  quint32 indexRootOffset;
  quint32 resourceIndexBtreeMaxElements; // Two fields from IndexInfo
  quint32 resourceIndexRootOffset;
  quint32 wordCount;
  quint32 articleCount;
  quint32 namePtr;
  quint32 descriptionPtr;
  quint32 langFrom; // Source language
  quint32 langTo;   // Target language
}
  #ifndef _MSC_VER
__attribute__( ( packed ) )
  #endif
;

  #pragma pack( pop )

// Some supporting functions
bool indexIsOldOrBad( string const & indexFile )
{
  File::Index idx( indexFile, "rb" );

  IdxHeader header;

  return idx.readRecords( &header, sizeof( header ), 1 ) != 1 || header.signature != Signature
    || header.formatVersion != CurrentFormatVersion;
}

quint32 getArticleCluster( ZimFile const & file, quint32 articleNumber )
{
  try {
    auto entry = file.getEntryByPath( articleNumber );

    auto item = entry.getItem( true );

    return item.getIndex();
  }
  catch ( std::exception & e ) {
    qDebug() << e.what();
    return 0xFFFFFFFF;
  }
}

bool isArticleMime( const string & mime_type )
{
  return mime_type == "text/html" /*|| mime_type.compare( "text/plain" ) == 0*/;
}

quint32 readArticle( ZimFile const & file, quint32 articleNumber, string & result )
{
  try {
    auto entry = file.getEntryByPath( articleNumber );

    auto item = entry.getItem( true );
    result    = string( item.getData( 0 ).data(), item.getData( 0 ).size() );
    return item.getIndex();
  }
  catch ( std::exception & e ) {
    qDebug() << e.what();
    return 0xFFFFFFFF;
  }
}

quint32 readArticleByPath( ZimFile const & file, const string & path, string & result )
{
  try {
    auto entry = file.getEntryByPath( path );

    auto item = entry.getItem( true );
    result    = item.getData();
    return item.getIndex();
  }
  catch ( std::exception & e ) {
    qDebug() << e.what();
    return 0xFFFFFFFF;
  }
}

// ZimDictionary

class ZimDictionary: public BtreeIndexing::BtreeDictionary
{
  QMutex idxMutex;
  QMutex zimMutex;
  File::Index idx;
  IdxHeader idxHeader;
  ZimFile df;
  set< quint32 > articlesIndexedForFTS;

public:

  ZimDictionary( string const & id, string const & indexFile, vector< string > const & dictionaryFiles );

  ~ZimDictionary() = default;

  string getName() noexcept override
  {
    return dictionaryName;
  }

  map< Dictionary::Property, string > getProperties() noexcept override
  {
    return {};
  }

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

  sptr< Dictionary::DataRequest >
  getArticle( wstring const &, vector< wstring > const & alts, wstring const &, bool ignoreDiacritics ) override;

  sptr< Dictionary::DataRequest > getResource( string const & name ) override;

  QString const & getDescription() override;

  /// Loads the resource.
  void loadResource( std::string & resourceName, string & data );

  sptr< Dictionary::DataRequest >
  getSearchResults( QString const & searchString, int searchMode, bool matchCase, bool ignoreDiacritics ) override;
  void getArticleText( uint32_t articleAddress, QString & headword, QString & text ) override;

  void makeFTSIndex( QAtomicInt & isCancelled ) override;

  void setFTSParameters( Config::FullTextSearch const & fts ) override
  {
    if ( metadata_enable_fts.has_value() ) {
      can_FTS = fts.enabled && metadata_enable_fts.value();
    }
    else
      can_FTS = fts.enabled && !fts.disabledTypes.contains( "ZIM", Qt::CaseInsensitive )
        && ( fts.maxDictionarySize == 0 || getArticleCount() <= fts.maxDictionarySize );
  }

protected:

  void loadIcon() noexcept override;

private:

  /// Loads the article.
  quint32 loadArticle( quint32 address, string & articleText, bool rawText = false );

  string convert( string const & in_data );
  friend class ZimArticleRequest;
  friend class ZimResourceRequest;
};

ZimDictionary::ZimDictionary( string const & id, string const & indexFile, vector< string > const & dictionaryFiles ):
  BtreeDictionary( id, dictionaryFiles ),
  idx( indexFile, "rb" ),
  idxHeader( idx.read< IdxHeader >() ),
  df( dictionaryFiles[ 0 ] )
{
  // Initialize the indexes

  openIndex( IndexInfo( idxHeader.indexBtreeMaxElements, idxHeader.indexRootOffset ), idx, idxMutex );

  // Read dictionary name

  dictionaryName = df.getMetadata( "Title" );
  if ( dictionaryName.empty() ) {
    QString name   = QDir::fromNativeSeparators( dictionaryFiles[ 0 ].c_str() );
    int n          = name.lastIndexOf( '/' );
    dictionaryName = name.mid( n + 1 ).toStdString();
  }

  // Full-text search parameters

  ftsIdxName = indexFile + Dictionary::getFtsSuffix();
}

void ZimDictionary::loadIcon() noexcept
{
  if ( dictionaryIconLoaded )
    return;

  // Try to load Original GD's user provided icon
  QString fileName = QDir::fromNativeSeparators( getDictionaryFilenames()[ 0 ].c_str() );
  // Remove the extension
  fileName.chop( 3 );
  if ( loadIconFromFile( fileName ) ) {
    dictionaryIconLoaded = true;
    return;
  }

  // Try to load zim's illustration, which is usually 48x48 png
  try {
    auto illustration = df.getIllustrationItem( 48 ).getData();
    QImage img = QImage::fromData( reinterpret_cast< const uchar * >( illustration.data() ), illustration.size() );

    if ( img.isNull() ) {
      // Fallback to default icon
      dictionaryIcon = QIcon( ":/icons/icon32_zim.png" );
    }
    else {
      dictionaryIcon = QIcon( QPixmap::fromImage( img ) );
    }

    dictionaryIconLoaded = true;
    return;
  }
  catch ( zim::EntryNotFound & e ) {
    gdDebug( "ZIM icon not loaded for: %s", dictionaryName.c_str() );
  }
}

quint32 ZimDictionary::loadArticle( quint32 address, string & articleText, bool rawText )
{
  quint32 ret = 0;
  {
    QMutexLocker _( &zimMutex );
    ret = readArticle( df, address, articleText );
  }
  if ( !rawText )
    articleText = convert( articleText );

  return ret;
}

string ZimDictionary::convert( const string & in )
{
  QString text = QString::fromUtf8( in.c_str() );

  // replace background
  text.replace( QRegularExpression( R"(<\s*body\s+([^>]*)(background(|-color)):([^;"]*(;|)))" ),
                QString( "<body \\1" ) );

  // pattern of img and script
  // text.replace( QRegularExpression( "<\\s*(img|script)\\s+([^>]*)src=(\")([^\"]*)\\3" ),
  //               QString( "<\\1 \\2src=\\3bres://%1/").arg( getId().c_str() ) );

  QRegularExpression rxImgScript( R"(<\s*(img|script|source)\s+([^>]*)src=(")([^"]*)\3)" );
  QRegularExpressionMatchIterator it = rxImgScript.globalMatch( text );
  int pos                            = 0;
  QString newText;
  while ( it.hasNext() ) {
    QRegularExpressionMatch match = it.next();

    newText += text.mid( pos, match.capturedStart() - pos );
    pos = match.capturedEnd();

    QStringList list = match.capturedTexts();

    QString url = list[ 4 ]; // a url

    QString urlLink = match.captured();

    QString replacedLink = urlLink;
    if ( !url.isEmpty() && !url.startsWith( "//" ) && !url.startsWith( "http://" ) && !url.startsWith( "https://" ) ) {
      //the pattern like : <\\1 \\2src=\\3bres://%1/

      //remove leading dot and slash
      url.remove( RX::Zim::leadingDotSlash );
      replacedLink =
        QString( "<%1 %2 src=\"bres://%3/%4\"" ).arg( list[ 1 ], list[ 2 ], QString::fromStdString( getId() ), url );
    }

    newText += replacedLink;
  }
  if ( pos ) {
    newText += text.mid( pos );
    text = newText;
  }
  newText.clear();


  // Fix links without '"'
  text.replace( QRegularExpression( R"(href=(\.\.|)/([^\s>]+))" ), QString( R"(href="\1/\2")" ) );

  // pattern <link... href="..." ...>
  text.replace( QRegularExpression( R"(<\s*link\s+([^>]*)href="(\.\.|)/)" ),
                QString( "<link \\1href=\"bres://%1/" ).arg( getId().c_str() ) );

  // localize the http://en.wiki***.com|org/wiki/<key> series links
  // excluding those keywords that have ":" in it
  QString urlWiki =
    "\"http(s|)://en\\.(wiki(pedia|books|news|quote|source|voyage|versity)|wiktionary)\\.(org|com)/wiki/([^:\"]*)\"";
  text.replace( QRegularExpression( R"(<\s*a\s+(class="external"\s+|)href=)" + urlWiki ),
                QString( R"(<a href="gdlookup://localhost/\6")" ) );

  // pattern <a href="..." ...>, excluding any known protocols such as http://, mailto:, #(comment)
  // these links will be translated into local definitions
  // <meta http-equiv="Refresh" content="0;url=../dsalsrv02.uchicago.edu/cgi-bin/0994.html">
  QRegularExpression rxLink(
    R"lit(<\s*(?:a|meta)\s+([^>]*)(?:href|url)="?(?!(?:\w+://|#|mailto:|tel:))()([^"]*)"\s*(title="[^"]*")?[^>]*>)lit" );
  it  = rxLink.globalMatch( text );
  pos = 0;
  while ( it.hasNext() ) {
    QRegularExpressionMatch match = it.next();

    newText += text.mid( pos, match.capturedStart() - pos );
    pos = match.capturedEnd();

    QStringList list = match.capturedTexts();
    // Add empty strings for compatibility with regex behaviour
    for ( int i = list.size(); i < 5; i++ )
      list.append( QString() );

    QString formatTag;
    QString tag = list[ 3 ]; // a url, ex: Precambrian_Chaotian.html
    QString url = tag;
    if ( !list[ 4 ].isEmpty() ) // a title, ex: title="Precambrian/Chaotian"
    {
      tag       = list[ 4 ];
      formatTag = tag.split( "\"" )[ 1 ];
    }
    else {
      //tag from list[3]
      formatTag = tag;
      formatTag.remove( RX::Zim::leadingDotSlash );
    }

    QString urlLink = match.captured();

    QString replacedLink = urlLink;
    if ( !url.isEmpty() && !url.startsWith( "//" ) ) {
      replacedLink = urlLink.replace( url, "gdlookup://localhost/" + formatTag );
    }

    newText += replacedLink;
  }
  if ( pos != 0 ) {
    newText += text.mid( pos );
    text = newText;
  }
  newText.clear();

  // Occasionally words needs to be displayed in vertical, but <br/> were changed to <br\> somewhere
  // proper style: <a href="gdlookup://localhost/Neoptera" ... >N<br/>e<br/>o<br/>p<br/>t<br/>e<br/>r<br/>a</a>
  QRegularExpression rxBR(
    R"((<a href="gdlookup://localhost/[^"]*"\s*[^>]*>)\s*((\w\s*&lt;br(\\|/|)&gt;\s*)+\w)\s*</a>)",
    QRegularExpression::UseUnicodePropertiesOption );
  pos                                 = 0;
  QRegularExpressionMatchIterator it2 = rxBR.globalMatch( text );
  while ( it2.hasNext() ) {
    QRegularExpressionMatch match = it2.next();

    newText += text.mid( pos, match.capturedStart() - pos );
    pos = match.capturedEnd();

    QStringList list = match.capturedTexts();
    // Add empty strings for compatibility with regex behaviour
    for ( int i = match.lastCapturedIndex() + 1; i < 3; i++ )
      list.append( QString() );

    QString tag = list[ 2 ];
    tag
      .replace(
        QRegularExpression( "&lt;br( |)(\\\\|/|)&gt;", QRegularExpression::PatternOption::CaseInsensitiveOption ),
        "<br/>" )
      .prepend( list[ 1 ] )
      .append( "</a>" );

    newText += tag;
  }
  if ( pos ) {
    newText += text.mid( pos );
    text = newText;
  }
  newText.clear();

  // Fix outstanding elements
  text += "<br style=\"clear:both;\" />";

  return text.toUtf8().data();
}

void ZimDictionary::loadResource( std::string & resourceName, string & data )
{
  if ( resourceName.empty() )
    return;
  QMutexLocker _( &zimMutex );
  readArticleByPath( df, resourceName, data );
}

QString const & ZimDictionary::getDescription()
{
  if ( !dictionaryDescription.isEmpty() )
    return dictionaryDescription;

  dictionaryDescription = QString::fromStdString( df.getMetadata( "Description" ) );
  return dictionaryDescription;
}

void ZimDictionary::makeFTSIndex( QAtomicInt & isCancelled )
{
  if ( !( Dictionary::needToRebuildIndex( getDictionaryFilenames(), ftsIdxName )
          || FtsHelpers::ftsIndexIsOldOrBad( this ) ) )
    FTS_index_completed.ref();

  if ( haveFTSIndex() )
    return;

  if ( !ensureInitDone().empty() )
    return;

  if ( firstIteration )
    return;

  gdDebug( "Zim: Building the full-text index for dictionary: %s\n", getName().c_str() );
  try {
    FtsHelpers::makeFTSIndex( this, isCancelled );
    FTS_index_completed.ref();
  }
  catch ( std::exception & ex ) {
    gdWarning( "Zim: Failed building full-text search index for \"%s\", reason: %s\n", getName().c_str(), ex.what() );
    QFile::remove( ftsIdxName.c_str() );
  }
}

void ZimDictionary::getArticleText( uint32_t articleAddress, QString & headword, QString & text )
{
  try {
    headword.clear();
    string articleText;

    loadArticle( articleAddress, articleText, true );
    text = Html::unescape( QString::fromUtf8( articleText.data(), articleText.size() ) );
  }
  catch ( std::exception & ex ) {
    gdWarning( "Zim: Failed retrieving article from \"%s\", reason: %s\n", getName().c_str(), ex.what() );
  }
}

sptr< Dictionary::DataRequest >
ZimDictionary::getSearchResults( QString const & searchString, int searchMode, bool matchCase, bool ignoreDiacritics )
{
  return std::make_shared< FtsHelpers::FTSResultsRequest >( *this,
                                                            searchString,
                                                            searchMode,
                                                            matchCase,
                                                            ignoreDiacritics );
}

/// ZimDictionary::getArticle()

class ZimArticleRequest: public Dictionary::DataRequest
{
  wstring word;
  vector< wstring > alts;
  ZimDictionary & dict;
  bool ignoreDiacritics;

  QAtomicInt isCancelled;
  QFuture< void > f;

public:

  ZimArticleRequest( wstring word_, vector< wstring > const & alts_, ZimDictionary & dict_, bool ignoreDiacritics_ ):
    word( std::move( word_ ) ),
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

  ~ZimArticleRequest()
  {
    isCancelled.ref();
    f.waitForFinished();
  }
};

void ZimArticleRequest::run()
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

  multimap< wstring, pair< string, string > > mainArticles, alternateArticles;

  set< quint32 > articlesIncluded; // Some synonyms make it that the articles
                                   // appear several times. We combat this
                                   // by only allowing them to appear once.

  wstring wordCaseFolded = Folding::applySimpleCaseOnly( word );
  if ( ignoreDiacritics )
    wordCaseFolded = Folding::applyDiacriticsOnly( wordCaseFolded );

  for ( auto & x : chain ) {
    if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
      finish();
      return;
    }

    // Now grab that article

    string headword, articleText;

    headword = x.word;

    quint32 articleNumber = 0xFFFFFFFF;
    try {
      articleNumber = dict.loadArticle( x.articleOffset, articleText );
    }
    catch ( ... ) {
    }

    if ( articleNumber == 0xFFFFFFFF )
      continue; // No article loaded

    if ( articlesIncluded.find( articleNumber ) != articlesIncluded.end() )
      continue; // We already have this article in the body.

    // Ok. Now, does it go to main articles, or to alternate ones? We list
    // main ones first, and alternates after.

    // We do the case-folded comparison here.

    wstring headwordStripped = Folding::applySimpleCaseOnly( headword );
    if ( ignoreDiacritics )
      headwordStripped = Folding::applyDiacriticsOnly( headwordStripped );

    multimap< wstring, pair< string, string > > & mapToUse =
      ( wordCaseFolded == headwordStripped ) ? mainArticles : alternateArticles;

    mapToUse.insert( pair( Folding::applySimpleCaseOnly( headword ), pair( headword, articleText ) ) );

    articlesIncluded.insert( articleNumber );
  }

  if ( mainArticles.empty() && alternateArticles.empty() ) {
    // No such word
    finish();
    return;
  }

  string result;

  // See Issue #271: A mechanism to clean-up invalid HTML cards.
  string cleaner = Utils::Html::getHtmlCleaner();

  multimap< wstring, pair< string, string > >::const_iterator i;


  for ( i = mainArticles.begin(); i != mainArticles.end(); ++i ) {
    result += "<div class=\"zimdict\">";
    result += "<h2 class=\"zimdict_headword\">";
    result += i->second.first;
    result += "</h2>";
    result += i->second.second;
    result += cleaner + "</div>";
  }

  for ( i = alternateArticles.begin(); i != alternateArticles.end(); ++i ) {
    result += "<div class=\"zimdict\">";
    result += "<h2 class=\"zimdict_headword\">";
    result += i->second.first;
    result += "</h2>";
    result += i->second.second;
    result += cleaner + "</div>";
  }

  appendString( result );

  hasAnyData = true;

  finish();
}

sptr< Dictionary::DataRequest > ZimDictionary::getArticle( wstring const & word,
                                                           vector< wstring > const & alts,
                                                           wstring const &,
                                                           bool ignoreDiacritics )

{
  return std::make_shared< ZimArticleRequest >( word, alts, *this, ignoreDiacritics );
}

//// ZimDictionary::getResource()

class ZimResourceRequest: public Dictionary::DataRequest
{
  //the dict will outlive this object, so the reference & used here is proper.
  ZimDictionary & dict;

  string resourceName;

  QAtomicInt isCancelled;
  QFuture< void > f;

public:
  ZimResourceRequest( ZimDictionary & dict_, string resourceName_ ):
    dict( dict_ ),
    resourceName( std::move( resourceName_ ) )
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

  ~ZimResourceRequest()
  {
    isCancelled.ref();
    f.waitForFinished();
  }
};

void ZimResourceRequest::run()
{
  // Some runnables linger enough that they are cancelled before they start
  if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
    finish();
    return;
  }

  try {
    string resource;
    dict.loadResource( resourceName, resource );
    if ( resource.empty() )
      throw File::Ex();

    if ( Filetype::isNameOfCSS( resourceName ) ) {
      QString css = QString::fromUtf8( resource.data(), resource.size() );
      dict.isolateCSS( css, ".zimdict" );
      QByteArray bytes = css.toUtf8();

      QMutexLocker _( &dataMutex );
      data.resize( bytes.size() );
      memcpy( &data.front(), bytes.constData(), bytes.size() );
    }
    else if ( Filetype::isNameOfTiff( resourceName ) ) {
      // Convert it
      QMutexLocker _( &dataMutex );
      GdTiff::tiff2img( data );
    }
    else {
      QMutexLocker _( &dataMutex );
      data.resize( resource.size() );
      memcpy( &data.front(), resource.data(), data.size() );
    }

    QMutexLocker _( &dataMutex );
    hasAnyData = true;
  }
  catch ( std::exception & ex ) {
    gdWarning( "ZIM: Failed loading resource \"%s\" from \"%s\", reason: %s\n",
               resourceName.c_str(),
               dict.getName().c_str(),
               ex.what() );
    // Resource not loaded -- we don't set the hasAnyData flag then
  }

  finish();
}

sptr< Dictionary::DataRequest > ZimDictionary::getResource( string const & name )
{
  auto noLeadingDot = QString::fromStdString( name ).remove( RX::Zim::leadingDotSlash );
  return std::make_shared< ZimResourceRequest >( *this, noLeadingDot.toStdString() );
}

wstring normalizeWord( const std::string & url );
vector< sptr< Dictionary::Class > > makeDictionaries( vector< string > const & fileNames,
                                                      string const & indicesDir,
                                                      Dictionary::Initializing & initializing,
                                                      unsigned maxHeadwordsToExpand )

{
  vector< sptr< Dictionary::Class > > dictionaries;

  for ( const auto & fileName : fileNames ) {
    // Skip files with the extensions different to .zim to speed up the
    // scanning

    QString firstName = QDir::fromNativeSeparators( fileName.c_str() );
    if ( !firstName.endsWith( ".zim" ) && !firstName.endsWith( ".zimaa" ) ) {
      continue;
    }

    // Got the file -- check if we need to rebuid the index
    //fileName  is logical.
    if ( firstName.endsWith( ".zimaa" ) ) {
      //remove aa
      firstName.chop( 2 );
    }

    ZimFile df( firstName.toStdString() );

    vector< string > dictFiles;
    dictFiles.push_back( firstName.toStdString() );

    string dictId = df.getChecksum();

    string indexFile = indicesDir + dictId;

    initializing.indexingDictionary( df.getFilename() );

    try {
      //only check zim file.
      if ( Dictionary::needToRebuildIndex( dictFiles, indexFile ) || indexIsOldOrBad( indexFile ) ) {
        gdDebug( "Zim: Building the index for dictionary: %s\n", fileName.c_str() );

        unsigned articleCount = df.getArticleCount();
        unsigned wordCount    = 0;

        {
          int n = firstName.lastIndexOf( '/' );
          initializing.indexingDictionary( firstName.mid( n + 1 ).toUtf8().constData() );
        }

        File::Index idx( indexFile, "wb" );
        IdxHeader idxHeader;
        memset( &idxHeader, 0, sizeof( idxHeader ) );
        idxHeader.namePtr        = 0xFFFFFFFF;
        idxHeader.descriptionPtr = 0xFFFFFFFF;

        auto lang = df.getMetadata( "Language" );
        if ( lang.size() == 2 )
          idxHeader.langFrom = LangCoder::code2toInt( lang.c_str() );
        else if ( lang.size() == 3 )
          idxHeader.langFrom = LangCoder::findIdForLanguageCode3( lang.c_str() );
        idxHeader.langTo = idxHeader.langFrom;
        // We write a dummy header first. At the end of the process the header
        // will be rewritten with the right values.
        idx.write( idxHeader );

        IndexedWords indexedWords;

        //only iterate the article
        for ( const auto & entry : df.iterByTitle() ) {
          auto item     = entry.getItem( true );
          auto mimeType = item.getMimetype();
          auto url      = item.getPath();
          auto title    = item.getTitle();
          auto index    = item.getIndex();
          // Read article url and title
          if ( !isArticleMime( mimeType ) ) {
            continue;
          }

          if ( maxHeadwordsToExpand > 0 && ( articleCount >= maxHeadwordsToExpand ) ) {
            if ( !title.empty() ) {
              wstring word = Utf8::decode( title );
              indexedWords.addSingleWord( word, index );
            }
            else if ( !url.empty() ) {
              indexedWords.addSingleWord( normalizeWord( url ), index );
            }
          }
          else {
            if ( !title.empty() ) {
              auto word = Utf8::decode( title );
              indexedWords.addWord( word, index );
              wordCount++;
            }
            else if ( !url.empty() ) {
              indexedWords.addWord( normalizeWord( url ), index );
              wordCount++;
            }
          }
        }

        // Build index
        {
          IndexInfo idxInfo = BtreeIndexing::buildIndex( indexedWords, idx );

          idxHeader.indexBtreeMaxElements = idxInfo.btreeMaxElements;
          idxHeader.indexRootOffset       = idxInfo.rootOffset;

          indexedWords.clear(); // Release memory -- no need for this data
        }

        idxHeader.signature     = Signature;
        idxHeader.formatVersion = CurrentFormatVersion;

        idxHeader.articleCount = articleCount;
        idxHeader.wordCount    = wordCount;

        idx.rewind();

        idx.write( &idxHeader, sizeof( idxHeader ) );
      }

      dictionaries.push_back( std::make_shared< ZimDictionary >( dictId, indexFile, dictFiles ) );
    }
    catch ( std::exception & e ) {
      gdWarning( "Zim dictionary initializing failed: %s, error: %s\n", fileName.c_str(), e.what() );
      continue;
    }
    catch ( ... ) {
      qWarning( "Zim dictionary initializing failed\n" );
      continue;
    }
  }
  return dictionaries;
}
wstring normalizeWord( const std::string & url )
{
  auto formattedUrl = QString::fromStdString( url ).remove( RX::Zim::leadingDotSlash );
  return formattedUrl.toStdU32String();
}

} // namespace Zim

#endif
