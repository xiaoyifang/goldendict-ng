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
  #include <QFileInfo>
  #include <QString>
  #include <QRunnable>
  #include <QSemaphore>
  #include <QAtomicInt>
  #include <QImage>
  #include <QDir>

  #include <QRegularExpression>

  #include <string>
  #include <set>
  #include <map>
  #include <algorithm>
  #include <QtConcurrent>
  #include "globalregex.hh"
  #include <zim/zim.h>
  #include <zim/archive.h>
  #include <zim/entry.h>
  #include <zim/item.h>
namespace Zim {

#define CACHE_SIZE 3

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
DEF_EX_STR( exCantReadFile, "Can't read file", Dictionary::Ex )
DEF_EX_STR( exInvalidZimHeader, "Invalid Zim header", Dictionary::Ex )
DEF_EX( exUserAbort, "User abort", Dictionary::Ex )


//namespace {

using ZimFile = zim::Archive;


  #pragma pack( push, 1 )

enum
{
  Signature = 0x584D495A, // ZIMX on little-endian, XMIZ on big-endian
  CurrentFormatVersion = 4 + BtreeIndexing::FormatVersion + Folding::Version
};

struct IdxHeader
{
  quint32 signature; // First comes the signature, ZIMX
  quint32 formatVersion; // File format version (CurrentFormatVersion)
  quint32 indexBtreeMaxElements; // Two fields from IndexInfo
  quint32 indexRootOffset;
  quint32 resourceIndexBtreeMaxElements; // Two fields from IndexInfo
  quint32 resourceIndexRootOffset;
  quint32 wordCount;
  quint32 articleCount;
  quint32 namePtr;
  quint32 descriptionPtr;
  quint32 langFrom;  // Source language
  quint32 langTo;    // Target language
}
#ifndef _MSC_VER
__attribute__((packed))
#endif
;

#pragma pack( pop )


// Some supporting functions

bool indexIsOldOrBad( string const & indexFile )
{
  File::Class idx( indexFile, "rb" );

  IdxHeader header;

  return idx.readRecords( &header, sizeof( header ), 1 ) != 1 ||
         header.signature != Signature ||
         header.formatVersion != CurrentFormatVersion;
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
  return mime_type.compare( "text/html" ) == 0 || mime_type.compare( "text/plain" ) == 0;
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

// ZimDictionary

class ZimDictionary: public BtreeIndexing::BtreeDictionary
{
    enum LINKS_TYPE { UNKNOWN, SLASH, NO_SLASH };

    Mutex idxMutex;
    Mutex zimMutex, idxResourceMutex;
    File::Class idx;
    BtreeIndex resourceIndex;
    IdxHeader idxHeader;
    ZimFile df;
    set< quint32 > articlesIndexedForFTS;

  public:

    ZimDictionary( string const & id, string const & indexFile,
                     vector< string > const & dictionaryFiles );

    ~ZimDictionary() = default;

    string getName() noexcept override
    { return dictionaryName; }

    map< Dictionary::Property, string > getProperties() noexcept override
    { return map< Dictionary::Property, string >(); }

    unsigned long getArticleCount() noexcept override
    { return idxHeader.articleCount; }

    unsigned long getWordCount() noexcept override
    { return idxHeader.wordCount; }

    inline quint32 getLangFrom() const override
    { return idxHeader.langFrom; }

    inline quint32 getLangTo() const override
    { return idxHeader.langTo; }

    sptr< Dictionary::DataRequest >
    getArticle( wstring const &, vector< wstring > const & alts, wstring const &, bool ignoreDiacritics ) override;

    sptr< Dictionary::DataRequest > getResource( string const & name ) override;

    QString const& getDescription() override;

    /// Loads the resource.
    void loadResource( std::string &resourceName, string & data );

    sptr< Dictionary::DataRequest > getSearchResults( QString const & searchString,
                                                              int searchMode, bool matchCase,
                                                              int distanceBetweenWords,
                                                              int maxResults,
                                                              bool ignoreWordsOrder,
                                                              bool ignoreDiacritics ) override;
    void getArticleText( uint32_t articleAddress, QString & headword, QString & text ) override;

    quint32 getArticleText( uint32_t articleAddress, QString & headword, QString & text,
                            set< quint32 > * loadedArticles );

    void makeFTSIndex(QAtomicInt & isCancelled, bool firstIteration ) override;

    void setFTSParameters( Config::FullTextSearch const & fts ) override
    {
      can_FTS = fts.enabled
                && !fts.disabledTypes.contains( "ZIM", Qt::CaseInsensitive )
                && ( fts.maxDictionarySize == 0 || getArticleCount() <= fts.maxDictionarySize );
    }

    void sortArticlesOffsetsForFTS( QVector< uint32_t > & offsets, QAtomicInt & isCancelled ) override;

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

  resourceIndex.openIndex( IndexInfo( idxHeader.resourceIndexBtreeMaxElements, idxHeader.resourceIndexRootOffset ),
                           idx,
                           idxResourceMutex );

  // Read dictionary name

  if ( idxHeader.namePtr == 0xFFFFFFFF ) {
    QString name   = QDir::fromNativeSeparators( dictionaryFiles[ 0 ].c_str() );
    int n          = name.lastIndexOf( '/' );
    dictionaryName = name.mid( n + 1 ).toStdString();
  }
  else {
    dictionaryName = df.getMetadata( "Title" );
  }

  // Full-text search parameters

  can_FTS = true;

  ftsIdxName = indexFile + Dictionary::getFtsSuffix();

  if ( !Dictionary::needToRebuildIndex( dictionaryFiles, ftsIdxName )
       && !FtsHelpers::ftsIndexIsOldOrBad( ftsIdxName, this ) )
    FTS_index_completed.ref();
}

void ZimDictionary::loadIcon() noexcept
{
  if ( dictionaryIconLoaded )
    return;

  QString fileName = QDir::fromNativeSeparators( getDictionaryFilenames()[ 0 ].c_str() );

  // Remove the extension
  fileName.chop( 3 );

  if( !loadIconFromFile( fileName ) )
  {
    // Load failed -- use default icons
    dictionaryNativeIcon = dictionaryIcon = QIcon(":/icons/icon32_zim.png");
  }

  dictionaryIconLoaded = true;
}

quint32 ZimDictionary::loadArticle( quint32 address, string & articleText, bool rawText )
{
  quint32 ret = 0;
  {
    Mutex::Lock _( zimMutex );
    ret = readArticle( df, address, articleText);
  }
  if( !rawText )
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

  QRegularExpression rxImgScript( R"(<\s*(img|script)\s+([^>]*)src=(")([^"]*)\3)" );
  QRegularExpressionMatchIterator it = rxImgScript.globalMatch( text );
  int pos = 0;
  QString newText;
  while( it.hasNext() )
  {
    QRegularExpressionMatch match = it.next();

    newText += text.mid( pos, match.capturedStart() - pos );
    pos = match.capturedEnd();

    QStringList list = match.capturedTexts();

    QString url = list[ 4 ]; // a url

    QString urlLink = match.captured();

    QString replacedLink = urlLink;
    if( !url.isEmpty() && !url.startsWith( "//" ) && !url.startsWith( "http://" ) && !url.startsWith( "https://" ) )
    {
      //<\\1 \\2src=\\3bres://%1/
      url.remove(QRegularExpression(R"(^\.*\/[A-Z]\/)", QRegularExpression::CaseInsensitiveOption));
      replacedLink =
        QString( "<%1 %2 src=\"bres://%3/%4\"" ).arg( list[ 1 ], list[ 2 ], QString::fromStdString( getId() ), url );
    }

    newText += replacedLink;
  }
  if( pos )
  {
    newText += text.mid( pos );
    text = newText;
  }
  newText.clear();


  // Fix links without '"'
  text.replace( QRegularExpression( R"(href=(\.\.|)/([^\s>]+))" ),
                QString( R"(href="\1/\2")" ) );

  // pattern <link... href="..." ...>
  text.replace( QRegularExpression( R"(<\s*link\s+([^>]*)href="(\.\.|)/)" ),
                QString( "<link \\1href=\"bres://%1/").arg( getId().c_str() ) );

  // localize the http://en.wiki***.com|org/wiki/<key> series links
  // excluding those keywords that have ":" in it
  QString urlWiki = "\"http(s|)://en\\.(wiki(pedia|books|news|quote|source|voyage|versity)|wiktionary)\\.(org|com)/wiki/([^:\"]*)\"";
  text.replace( QRegularExpression( R"(<\s*a\s+(class="external"\s+|)href=)" + urlWiki ),
                QString( R"(<a href="gdlookup://localhost/\6")" ) );

  // pattern <a href="..." ...>, excluding any known protocols such as http://, mailto:, #(comment)
  // these links will be translated into local definitions
  // <meta http-equiv="Refresh" content="0;url=../dsalsrv02.uchicago.edu/cgi-bin/0994.html">
  QRegularExpression rxLink( R"lit(<\s*(?:a|meta)\s+([^>]*)(?:href|url)="?(?!(?:\w+://|#|mailto:|tel:))()([^"]*)"\s*(title="[^"]*")?[^>]*>)lit" );
  it = rxLink.globalMatch( text );
  pos = 0;
  while( it.hasNext() )
  {
    QRegularExpressionMatch match = it.next();

    newText += text.mid( pos, match.capturedStart() - pos );
    pos = match.capturedEnd();

    QStringList list = match.capturedTexts();
    // Add empty strings for compatibility with QRegExp behaviour
    for( int i = list.size(); i < 5; i++ )
      list.append( QString() );

    QString formatTag;
    QString tag = list[ 3 ]; // a url, ex: Precambrian_Chaotian.html
    QString url = tag;
    if( !list[ 4 ].isEmpty() ) // a title, ex: title="Precambrian/Chaotian"
    {
      tag       = list[ 4 ];
      formatTag = tag.split( "\"" )[ 1 ];
    }
    else
    {
      //tag from list[3]
      formatTag = tag;
      formatTag.replace( RX::Zim::linkSpecialChar, "" );
    }

    QString urlLink = match.captured();

    QString replacedLink = urlLink;
    if( !url.isEmpty() && !url.startsWith( "//" ) )
    {
      replacedLink = urlLink.replace( url, "gdlookup://localhost/" + formatTag );
    }

    newText += replacedLink;
  }
  if( pos )
  {
    newText += text.mid( pos );
    text = newText;
  }
  newText.clear();

  // Occasionally words needs to be displayed in vertical, but <br/> were changed to <br\> somewhere
  // proper style: <a href="gdlookup://localhost/Neoptera" ... >N<br/>e<br/>o<br/>p<br/>t<br/>e<br/>r<br/>a</a>
  QRegularExpression rxBR( R"((<a href="gdlookup://localhost/[^"]*"\s*[^>]*>)\s*((\w\s*&lt;br(\\|/|)&gt;\s*)+\w)\s*</a>)",
                           QRegularExpression::UseUnicodePropertiesOption );
  pos = 0;
  QRegularExpressionMatchIterator it2 = rxBR.globalMatch( text );
  while( it2.hasNext() )
  {
    QRegularExpressionMatch match = it2.next();

    newText += text.mid( pos, match.capturedStart() - pos );
    pos = match.capturedEnd();

    QStringList list = match.capturedTexts();
    // Add empty strings for compatibility with QRegExp behaviour
    for( int i = match.lastCapturedIndex() + 1; i < 3; i++ )
      list.append( QString() );

    QString tag = list[2];
    tag.replace( QRegularExpression( "&lt;br( |)(\\\\|/|)&gt;", QRegularExpression::PatternOption::CaseInsensitiveOption ) , "<br/>" ).
        prepend( list[1] ).
        append( "</a>" );

    newText += tag;
  }
  if( pos )
  {
    newText += text.mid( pos );
    text = newText;
  }
  newText.clear();


  // // output all links in the page - only for analysis
  // QRegExp rxPrintAllLinks( "<\\s*a\\s+[^>]*href=\"[^\"]*\"[^>]*>",
  //                         Qt::CaseSensitive,
  //                         QRegExp::RegExp2 );
  // pos = 0;
  // while( (pos = rxPrintAllLinks.indexIn( text, pos )) >= 0 )
  // {
  //   QStringList list = rxPrintAllLinks.capturedTexts();
  //   qDebug() << "\n--Alllinks--" << list[0];
  //   pos += list[0].length() + 1;
  // }

  // Fix outstanding elements
  text += "<br style=\"clear:both;\" />";

  return text.toUtf8().data();
}

void ZimDictionary::loadResource( std::string & resourceName, string & data )
{
  vector< WordArticleLink > link;
  string resData;

  link = resourceIndex.findArticles( Utf8::decode( resourceName ) );

  if( link.empty() )
    return;

  {
    Mutex::Lock _( zimMutex );
    readArticle( df, link[ 0 ].articleOffset, data);
  }
}

QString const& ZimDictionary::getDescription()
{
    if( !dictionaryDescription.isEmpty() || idxHeader.descriptionPtr == 0xFFFFFFFF )
        return dictionaryDescription;

    dictionaryDescription = QString::fromStdString( df.getMetadata( "Description" ) );
    return dictionaryDescription;
}

void ZimDictionary::makeFTSIndex( QAtomicInt & isCancelled, bool firstIteration )
{
  if( !( Dictionary::needToRebuildIndex( getDictionaryFilenames(), ftsIdxName )
         || FtsHelpers::ftsIndexIsOldOrBad( ftsIdxName, this ) ) )
    FTS_index_completed.ref();

  if( haveFTSIndex() )
    return;

  if( ensureInitDone().size() )
    return;

  if( firstIteration )
    return;

  gdDebug( "Zim: Building the full-text index for dictionary: %s\n",
           getName().c_str() );
  try
  {
    return FtsHelpers::makeFTSIndexXapian(this,isCancelled);
  }
  catch( std::exception &ex )
  {
    gdWarning( "Zim: Failed building full-text search index for \"%s\", reason: %s\n", getName().c_str(), ex.what() );
    QFile::remove( ftsIdxName.c_str() );
  }
}

void ZimDictionary::sortArticlesOffsetsForFTS( QVector< uint32_t > & offsets,
                                               QAtomicInt & isCancelled )
{
  QVector< QPair< quint32, uint32_t > > offsetsWithClusters;
  offsetsWithClusters.reserve( offsets.size() );

  for( QVector< uint32_t >::ConstIterator it = offsets.constBegin();
       it != offsets.constEnd(); ++it )
  {
    if( Utils::AtomicInt::loadAcquire( isCancelled ) )
      return;

    Mutex::Lock _( zimMutex );
    offsetsWithClusters.append( QPair< uint32_t, quint32 >( getArticleCluster( df, *it ), *it ) );
  }

  std::sort( offsetsWithClusters.begin(), offsetsWithClusters.end() );

  for( int i = 0; i < offsetsWithClusters.size(); i++ )
    offsets[ i ] = offsetsWithClusters.at( i ).second;
}

void ZimDictionary::getArticleText( uint32_t articleAddress, QString & headword, QString & text )
{
  try
  {
    headword.clear();
    string articleText;

    loadArticle( articleAddress, articleText, true );
    text = Html::unescape( QString::fromUtf8( articleText.data(), articleText.size() ) );
  }
  catch( std::exception &ex )
  {
    gdWarning( "Zim: Failed retrieving article from \"%s\", reason: %s\n", getName().c_str(), ex.what() );
  }
}

quint32 ZimDictionary::getArticleText( uint32_t articleAddress, QString & headword, QString & text,
                                    set< quint32 > * loadedArticles )
{
  quint32 articleNumber = 0xFFFFFFFF;
  try
  {
    headword.clear();
    string articleText;

    articleNumber = loadArticle( articleAddress, articleText, true );
    text = Html::unescape( QString::fromUtf8( articleText.data(), articleText.size() ) );
  }
  catch( std::exception &ex )
  {
    gdWarning( "Zim: Failed retrieving article from \"%s\", reason: %s\n", getName().c_str(), ex.what() );
  }
  return articleNumber;
}

sptr< Dictionary::DataRequest > ZimDictionary::getSearchResults( QString const & searchString,
                                                                 int searchMode, bool matchCase,
                                                                 int distanceBetweenWords,
                                                                 int maxResults,
                                                                 bool ignoreWordsOrder,
                                                                 bool ignoreDiacritics )
{
  return std::make_shared<FtsHelpers::FTSResultsRequest>( *this, searchString,searchMode, matchCase, distanceBetweenWords, maxResults, ignoreWordsOrder, ignoreDiacritics );
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

  ZimArticleRequest( wstring const & word_,
                     vector< wstring > const & alts_,
                     ZimDictionary & dict_, bool ignoreDiacritics_ ):
    word( word_ ), alts( alts_ ), dict( dict_ ), ignoreDiacritics( ignoreDiacritics_ )
  {
    f = QtConcurrent::run( [ this ]() { this->run(); } );
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
  if ( Utils::AtomicInt::loadAcquire( isCancelled ) )
  {
    finish();
    return;
  }

  vector< WordArticleLink > chain = dict.findArticles( word, ignoreDiacritics );

  for( unsigned x = 0; x < alts.size(); ++x )
  {
    /// Make an additional query for each alt

    vector< WordArticleLink > altChain = dict.findArticles( alts[ x ], ignoreDiacritics );

    chain.insert( chain.end(), altChain.begin(), altChain.end() );
  }

  multimap< wstring, pair< string, string > > mainArticles, alternateArticles;

  set< quint32 > articlesIncluded; // Some synonims make it that the articles
                                    // appear several times. We combat this
                                    // by only allowing them to appear once.

  wstring wordCaseFolded = Folding::applySimpleCaseOnly( word );
  if( ignoreDiacritics )
    wordCaseFolded = Folding::applyDiacriticsOnly( wordCaseFolded );

  for( unsigned x = 0; x < chain.size(); ++x )
  {
    if ( Utils::AtomicInt::loadAcquire( isCancelled ) )
    {
      finish();
      return;
    }

    // Now grab that article

    string headword, articleText;

    headword = chain[ x ].word;

    quint32 articleNumber = 0xFFFFFFFF;
    try
    {
      articleNumber = dict.loadArticle( chain[ x ].articleOffset, articleText );
    }
    catch(...)
    {
    }

    if( articleNumber == 0xFFFFFFFF )
      continue; // No article loaded

    if ( articlesIncluded.find( articleNumber ) != articlesIncluded.end() )
      continue; // We already have this article in the body.

    // Ok. Now, does it go to main articles, or to alternate ones? We list
    // main ones first, and alternates after.

    // We do the case-folded comparison here.

    wstring headwordStripped =
      Folding::applySimpleCaseOnly( headword );
    if( ignoreDiacritics )
      headwordStripped = Folding::applyDiacriticsOnly( headwordStripped );

    multimap< wstring, pair< string, string > > & mapToUse =
      ( wordCaseFolded == headwordStripped ) ?
        mainArticles : alternateArticles;

    mapToUse.insert( pair(
      Folding::applySimpleCaseOnly( headword ),
      pair( headword, articleText ) ) );

    articlesIncluded.insert( articleNumber );
  }

  if ( mainArticles.empty() && alternateArticles.empty() )
  {
    // No such word
    finish();
    return;
  }

  string result;

  // See Issue #271: A mechanism to clean-up invalid HTML cards.
  string cleaner = Utils::Html::getHtmlCleaner();

  multimap< wstring, pair< string, string > >::const_iterator i;


  for( i = mainArticles.begin(); i != mainArticles.end(); ++i )
  {
      result += "<div class=\"zimdict\">";
      result += "<h2 class=\"zimdict_headword\">";
      result += i->second.first;
      result += "</h2>";
      result += i->second.second;
      result += cleaner + "</div>";
  }

  for( i = alternateArticles.begin(); i != alternateArticles.end(); ++i )
  {
      result += "<div class=\"zimdict\">";
      result += "<h2 class=\"zimdict_headword\">";
      result += i->second.first;
      result += "</h2>";
      result += i->second.second;
      result += cleaner + "</div>";
  }

  Mutex::Lock _( dataMutex );

  data.resize( result.size() );

  memcpy( &data.front(), result.data(), result.size() );

  hasAnyData = true;

  finish();
}

sptr< Dictionary::DataRequest > ZimDictionary::getArticle( wstring const & word,
                                                           vector< wstring > const & alts,
                                                           wstring const &,
                                                           bool ignoreDiacritics )
  
{
  return std::make_shared<ZimArticleRequest>( word, alts, *this, ignoreDiacritics );
}

//// ZimDictionary::getResource()

class ZimResourceRequest: public Dictionary::DataRequest
{
  ZimDictionary & dict;

  string resourceName;

  QAtomicInt isCancelled;
  QFuture< void > f;

public:
  ZimResourceRequest(ZimDictionary &dict_, string const &resourceName_)
      : dict(dict_), resourceName(resourceName_) {
    f = QtConcurrent::run( [ this ]() { this->run(); } );
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
  if ( Utils::AtomicInt::loadAcquire( isCancelled ) )
  {
    finish();
    return;
  }

  try
  {
    string resource;
    dict.loadResource( resourceName, resource );
    if( resource.empty() )
      throw File::Ex();

    if( Filetype::isNameOfCSS( resourceName ) )
    {
      QString css = QString::fromUtf8( resource.data(), resource.size() );
      dict.isolateCSS( css, ".zimdict" );
      QByteArray bytes = css.toUtf8();

      Mutex::Lock _( dataMutex );
      data.resize( bytes.size() );
      memcpy( &data.front(), bytes.constData(), bytes.size() );
    }
    else
    if ( Filetype::isNameOfTiff( resourceName ) )
    {
      // Convert it
      Mutex::Lock _( dataMutex );
      GdTiff::tiff2img( data );
    }
    else
    {
      Mutex::Lock _( dataMutex );
      data.resize( resource.size() );
      memcpy( &data.front(), resource.data(), data.size() );
    }

    Mutex::Lock _( dataMutex );
    hasAnyData = true;
  }
  catch( std::exception &ex )
  {
    gdWarning( "ZIM: Failed loading resource \"%s\" from \"%s\", reason: %s\n",
               resourceName.c_str(), dict.getName().c_str(), ex.what() );
    // Resource not loaded -- we don't set the hasAnyData flag then
  }

  finish();
}

sptr< Dictionary::DataRequest > ZimDictionary::getResource( string const & name )
{
  auto formatedName = QString::fromStdString(name).remove(QRegularExpression(R"(^\.*\/[A-Z]\/)", QRegularExpression::CaseInsensitiveOption));
  return std::make_shared<ZimResourceRequest>( *this, formatedName.toStdString() );
}

//} // anonymous namespace

vector< sptr< Dictionary::Class > > makeDictionaries(
                                      vector< string > const & fileNames,
                                      string const & indicesDir,
                                      Dictionary::Initializing & initializing,
                                      unsigned maxHeadwordsToExpand )
  
{
  vector< sptr< Dictionary::Class > > dictionaries;

  for( vector< string >::const_iterator i = fileNames.begin(); i != fileNames.end();
       ++i )
  {
      // Skip files with the extensions different to .zim to speed up the
      // scanning

    QString firstName = QDir::fromNativeSeparators( i->c_str() );
    if ( !firstName.endsWith( ".zim" ) || !firstName.endsWith( ".zimaa" ) ) {
      continue;
    }

      // Got the file -- check if we need to rebuid the index
    //fileName  is logical.
    if ( firstName.endsWith( ".zimaa" ) ) {
      //remove aa
      firstName.remove( firstName.length() - 2, 2 );
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
        gdDebug( "Zim: Building the index for dictionary: %s\n", i->c_str() );

        unsigned articleCount = df.getArticleCount();
        unsigned wordCount    = 0;

        {
          int n = firstName.lastIndexOf( '/' );
          initializing.indexingDictionary( firstName.mid( n + 1 ).toUtf8().constData() );
        }

        File::Class idx( indexFile, "wb" );
        IdxHeader idxHeader;
        memset( &idxHeader, 0, sizeof( idxHeader ) );
        idxHeader.namePtr        = 0xFFFFFFFF;
        idxHeader.descriptionPtr = 0xFFFFFFFF;

        auto lang = df.getMetadata( "Language" );
        if ( lang.size() == 2 )
          idxHeader.langFrom = LangCoder::code2toInt( lang );
        else if ( lang.size() == 3 )
          idxHeader.langFrom = LangCoder::findIdForLanguageCode3( lang );
        idxHeader.langTo = idxHeader.langFrom;
        // We write a dummy header first. At the end of the process the header
        // will be rewritten with the right values.

        idx.write( idxHeader );

        IndexedWords indexedWords, indexedResources;

        for ( unsigned n = 0; n < articleCount; n++ ) {
          try {
            auto entry    = df.getEntryByPath( n );
            auto item     = entry.getItem( true );
            auto mimeType = item.getMimetype();
            auto url   = item.getPath();
            auto title = item.getTitle();
            qDebug() << n << mimeType.c_str()<<url.c_str()<<title.c_str();
            // Read article url and title
            if ( !isArticleMime( mimeType ) ) {
              indexedResources.addSingleWord( Utf8::decode( url ), n );
              continue;
            }

            if ( maxHeadwordsToExpand && articleCount >= maxHeadwordsToExpand ) {
              if ( !title.empty() ) {
                wstring word = Utf8::decode( title );
                indexedWords.addSingleWord( word, n );
              }
              if ( !url.empty() ) {
                auto formatedUrl = QString::fromStdString( url ).replace( RX::Zim::linkSpecialChar, "" );
                indexedWords.addSingleWord( Utf8::decode( formatedUrl.toStdString() ), n );
              }
            }
            else {
              if ( !title.empty() ) {
                auto word = Utf8::decode( title );
                indexedWords.addWord( word, n );
              }
              if ( !url.empty() ) {
                auto formatedUrl = QString::fromStdString( url ).replace( RX::Zim::linkSpecialChar, "" );
                indexedWords.addWord( Utf8::decode( formatedUrl.toStdString() ), n );
              }
            }

            wordCount++;
          }
          catch ( std::exception & e ) {
            qWarning() << e.what();
            continue;
          }
        }

        // Build index
        {
          IndexInfo idxInfo = BtreeIndexing::buildIndex( indexedWords, idx );

          idxHeader.indexBtreeMaxElements = idxInfo.btreeMaxElements;
          idxHeader.indexRootOffset       = idxInfo.rootOffset;

          indexedWords.clear(); // Release memory -- no need for this data
        }

        {
          IndexInfo idxInfo = BtreeIndexing::buildIndex( indexedResources, idx );

          idxHeader.resourceIndexBtreeMaxElements = idxInfo.btreeMaxElements;
          idxHeader.resourceIndexRootOffset       = idxInfo.rootOffset;

          indexedResources.clear(); // Release memory -- no need for this data
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
      catch( std::exception & e )
      {
        gdWarning( "Zim dictionary initializing failed: %s, error: %s\n",
                   i->c_str(), e.what() );
        continue;
      }
      catch( ... )
      {
        qWarning( "Zim dictionary initializing failed\n" );
        continue;
      }
  }
  return dictionaries;
}

} // namespace Zim

#endif
