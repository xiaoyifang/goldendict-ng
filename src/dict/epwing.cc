/* This file is (c) 2014 Abs62
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */
#ifndef NO_EPWING_SUPPORT

  #include "epwing_book.hh"
  #include "epwing.hh"

  #include <QByteArray>
  #include <QDir>
  #include <QRunnable>
  #include <QSemaphore>

  #include <map>
  #include <QtConcurrent>
  #include <set>
  #include <string>

  #include "btreeidx.hh"
  #include "folding.hh"
  #include "gddebug.hh"

  #include "chunkedstorage.hh"
  #include "wstring_qt.hh"
  #include "filetype.hh"
  #include "ftshelpers.hh"
  #include "globalregex.hh"
  #include "sptr.hh"

namespace Epwing {

using BtreeIndexing::WordArticleLink;
using BtreeIndexing::IndexedWords;
using BtreeIndexing::IndexInfo;

using std::map;
using std::multimap;
using std::vector;
using std::set;
using std::pair;
using gd::wstring;

namespace {

#pragma pack( push, 1 )

enum {
  Signature            = 0x58575045, // EPWX on little-endian, XWPE on big-endian
  CurrentFormatVersion = 7 + BtreeIndexing::FormatVersion + Folding::Version
};

struct IdxHeader
{
  quint32 signature;             // First comes the signature, EPWX
  quint32 formatVersion;         // File format version (CurrentFormatVersion)
  quint32 chunksOffset;          // The offset to chunks' storage
  quint32 indexBtreeMaxElements; // Two fields from IndexInfo
  quint32 indexRootOffset;
  quint32 wordCount;
  quint32 articleCount;
  quint32 nameSize;
  quint32 langFrom; // Source language
  quint32 langTo;   // Target language
}
#ifndef _MSC_VER
__attribute__( ( packed ) )
#endif
;

#pragma pack( pop )

bool indexIsOldOrBad( string const & indexFile )
{
  File::Class idx( indexFile, "rb" );

  IdxHeader header;

  return idx.readRecords( &header, sizeof( header ), 1 ) != 1 || header.signature != Signature
    || header.formatVersion != CurrentFormatVersion;
}
class EpwingHeadwordsRequest;

class EpwingDictionary: public BtreeIndexing::BtreeDictionary
{
  Q_DECLARE_TR_FUNCTIONS( Epwing::EpwingDictionary )

  QMutex idxMutex;
  File::Class idx;
  IdxHeader idxHeader;
  string bookName;
  ChunkedStorage::Reader chunks;
  Epwing::Book::EpwingBook eBook;
  QString cacheDirectory;

public:

  EpwingDictionary( string const & id,
                    string const & indexFile,
                    vector< string > const & dictionaryFiles,
                    int subBook );

  ~EpwingDictionary();

  string getName() noexcept override
  {
    return bookName;
  }

  void setName( string _name ) noexcept override
  {
    bookName = _name;
  }

  map< Dictionary::Property, string > getProperties() noexcept override
  {
    return map< Dictionary::Property, string >();
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

  QString const & getDescription() override;

  void getHeadwordPos( wstring const & word_, QVector< int > & pg, QVector< int > & off );

  sptr< Dictionary::DataRequest >
  getArticle( wstring const &, vector< wstring > const & alts, wstring const &, bool ignoreDiacritics ) override;

  sptr< Dictionary::DataRequest > getResource( string const & name ) override;

  sptr< Dictionary::DataRequest >
  getSearchResults( QString const & searchString, int searchMode, bool matchCase, bool ignoreDiacritics ) override;
  void getArticleText( uint32_t articleAddress, QString & headword, QString & text ) override;

  void makeFTSIndex( QAtomicInt & isCancelled, bool firstIteration ) override;

  void setFTSParameters( Config::FullTextSearch const & fts ) override
  {
    if ( ensureInitDone().size() )
      return;

    can_FTS = enable_FTS && fts.enabled && !fts.disabledTypes.contains( "EPWING", Qt::CaseInsensitive )
      && ( fts.maxDictionarySize == 0 || getArticleCount() <= fts.maxDictionarySize );
  }

  static int japaneseWriting( gd::wchar ch );

  static bool isSign( gd::wchar ch );

  static bool isJapanesePunctiation( gd::wchar ch );

  sptr< Dictionary::WordSearchRequest > prefixMatch( wstring const &, unsigned long ) override;

  sptr< Dictionary::WordSearchRequest >
  stemmedMatch( wstring const &, unsigned minLength, unsigned maxSuffixVariation, unsigned long maxResults ) override;

protected:

  void loadIcon() noexcept override;

private:

  /// Loads the article.
  void loadArticle(
    quint32 address, string & articleHeadword, string & articleText, int & articlePage, int & articleOffset );


  sptr< Dictionary::WordSearchRequest > findHeadwordsForSynonym( wstring const & word ) override;

  void loadArticleNextPage( string & articleHeadword, string & articleText, int & articlePage, int & articleOffset );
  void
  loadArticlePreviousPage( string & articleHeadword, string & articleText, int & articlePage, int & articleOffset );

  void loadArticle( int articlePage, int articleOffset, string & articleHeadword, string & articleText );

  QString const & getImagesCacheDir()
  {
    return eBook.getImagesCacheDir();
  }

  QString const & getSoundsCacheDir()
  {
    return eBook.getSoundsCacheDir();
  }

  QString const & getMoviesCacheDir()
  {
    return eBook.getMoviesCacheDir();
  }

  friend class EpwingArticleRequest;
  friend class EpwingResourceRequest;
  friend class EpwingWordSearchRequest;

  friend class EpwingHeadwordsRequest;
  string epwing_previous_button( const int & articleOffset, const int & articlePage );
  string epwing_next_button( const int & articleOffset, const int & articlePage );
  bool readHeadword( const EB_Position & pos, QString & headword );
};


EpwingDictionary::EpwingDictionary( string const & id,
                                    string const & indexFile,
                                    vector< string > const & dictionaryFiles,
                                    int subBook ):
  BtreeDictionary( id, dictionaryFiles ),
  idx( indexFile, "rb" ),
  idxHeader( idx.read< IdxHeader >() ),
  chunks( idx, idxHeader.chunksOffset )
{
  vector< char > data( idxHeader.nameSize );
  idx.seek( sizeof( idxHeader ) );
  if ( data.size() > 0 ) {
    idx.read( &data.front(), idxHeader.nameSize );
    bookName = string( &data.front(), idxHeader.nameSize );
  }

  // Initialize eBook

  eBook.setBook( dictionaryFiles[ 0 ] );
  eBook.setSubBook( subBook );

  // Initialize the index

  openIndex( IndexInfo( idxHeader.indexBtreeMaxElements, idxHeader.indexRootOffset ), idx, idxMutex );

  eBook.setDictID( getId() );

  cacheDirectory = QDir::tempPath() + QDir::separator() + QString::fromUtf8( getId().c_str() ) + ".cache";
  eBook.setCacheDirectory( cacheDirectory );

  // Full-text search parameters

  ftsIdxName = indexFile + Dictionary::getFtsSuffix();

}

EpwingDictionary::~EpwingDictionary()
{
  Utils::Fs::removeDirectory( cacheDirectory );
}

void EpwingDictionary::loadIcon() noexcept
{
  if ( dictionaryIconLoaded )
    return;

  QString fileName = QString::fromStdString( getDictionaryFilenames()[ 0 ] ) + QDir::separator()
    + eBook.getCurrentSubBookDirectory() + ".";

  if ( !fileName.isEmpty() )
    loadIconFromFile( fileName );

  if ( dictionaryIcon.isNull() ) {
    // Load failed -- use default icons
    dictionaryIcon = QIcon( ":/icons/icon32_epwing.png" );
  }

  dictionaryIconLoaded = true;
}

void EpwingDictionary::loadArticle(
  quint32 address, string & articleHeadword, string & articleText, int & articlePage, int & articleOffset )
{
  vector< char > chunk;

  char * articleProps;

  {
    QMutexLocker _( &idxMutex );
    articleProps = chunks.getBlock( address, chunk );
  }

  memcpy( &articlePage, articleProps, sizeof( articlePage ) );
  memcpy( &articleOffset, articleProps + sizeof( articlePage ), sizeof( articleOffset ) );

  QString headword, text;

  try {
    QMutexLocker _( &eBook.getLibMutex() );
    eBook.getArticle( headword, text, articlePage, articleOffset, false );
  }
  catch ( std::exception & e ) {
    text = QString( "Article reading error: %1" ).arg( QString::fromUtf8( e.what() ) );
  }

  articleHeadword = string( headword.toUtf8().data() );
  articleText     = string( text.toUtf8().data() );

  string prefix( "<div class=\"epwing_text\">" );

  articleText = prefix + articleText + "</div>";
}

string Epwing::EpwingDictionary::epwing_previous_button( const int & articlePage, const int & articleOffset )
{
  QString previousButton = QString( "p%1At%2" ).arg( articlePage ).arg( articleOffset );
  string previousLink    = R"(<p><a class="epwing_previous_page" href="gdlookup://localhost/)"
    + previousButton.toStdString() + "\">" + tr( "Previous Page" ).toStdString() + "</a></p>";

  return previousLink;
}

void EpwingDictionary::loadArticleNextPage( string & articleHeadword,
                                            string & articleText,
                                            int & articlePage,
                                            int & articleOffset )
{
  QString headword, text;
  EB_Position pos;
  try {
    QMutexLocker _( &eBook.getLibMutex() );
    pos = eBook.getArticleNextPage( headword, text, articlePage, articleOffset, false );
  }
  catch ( std::exception & e ) {
    qWarning() << QString( "Article reading error: %1" ).arg( QString::fromUtf8( e.what() ) );
    return;
  }

  articleHeadword = string( headword.toUtf8().data() );
  articleText     = string( text.toUtf8().data() );

  string prefix( "<div class=\"epwing_text\">" );
  string previousLink = epwing_previous_button( articlePage, articleOffset );

  articleText     = prefix + previousLink + articleText;
  string nextLink = epwing_next_button( pos.page, pos.offset );
  articleText     = articleText + nextLink;
  articleText     = articleText + "</div>";
}

string Epwing::EpwingDictionary::epwing_next_button( const int & articlePage, const int & articleOffset )
{
  QString refLink = QString( "r%1At%2" ).arg( articlePage ).arg( articleOffset );
  string nextLink = R"(<p><a class="epwing_next_page" href="gdlookup://localhost/)" + refLink.toStdString() + "\">"
    + tr( "Next Page" ).toStdString() + "</a></p>";

  return nextLink;
}

void EpwingDictionary::loadArticlePreviousPage( string & articleHeadword,
                                                string & articleText,
                                                int & articlePage,
                                                int & articleOffset )
{
  QString headword, text;
  EB_Position pos;
  try {
    QMutexLocker _( &eBook.getLibMutex() );
    pos = eBook.getArticlePreviousPage( headword, text, articlePage, articleOffset, false );
  }
  catch ( std::exception & e ) {
    qDebug() << QString( "Article reading error: %1" ).arg( QString::fromUtf8( e.what() ) );
    return;
  }

  articleHeadword = string( headword.toUtf8().data() );
  articleText     = string( text.toUtf8().data() );

  string prefix( "<div class=\"epwing_text\">" );

  string previousLink = epwing_previous_button( pos.page, pos.offset );
  articleText         = prefix + previousLink + articleText;
  string nextLink     = epwing_next_button( articlePage, articleOffset );
  articleText         = articleText + nextLink;
  articleText         = articleText + "</div>";
}

void EpwingDictionary::loadArticle( int articlePage, int articleOffset, string & articleHeadword, string & articleText )
{
  QString headword, text;

  try {
    QMutexLocker _( &eBook.getLibMutex() );
    eBook.getArticle( headword, text, articlePage, articleOffset, false );
  }
  catch ( std::exception & e ) {
    text = QString( "Article reading error: %1" ).arg( QString::fromUtf8( e.what() ) );
  }

  articleHeadword = string( headword.toUtf8().data() );
  articleText     = string( text.toUtf8().data() );

  string prefix( "<div class=\"epwing_text\">" );

  articleText = prefix + articleText + "</div>";
}

QString const & EpwingDictionary::getDescription()
{
  if ( !dictionaryDescription.isEmpty() )
    return dictionaryDescription;

  dictionaryDescription = "NONE";

  QString str;
  {
    QMutexLocker _( &eBook.getLibMutex() );
    str = eBook.copyright();
  }

  if ( !str.isEmpty() )
    dictionaryDescription = str;

  return dictionaryDescription;
}

void EpwingDictionary::makeFTSIndex( QAtomicInt & isCancelled, bool firstIteration )
{
  if ( !( Dictionary::needToRebuildIndex( getDictionaryFilenames(), ftsIdxName )
          || FtsHelpers::ftsIndexIsOldOrBad( this ) ) )
    FTS_index_completed.ref();


  if ( haveFTSIndex() )
    return;

  if ( firstIteration && getArticleCount() > FTS::MaxDictionarySizeForFastSearch )
    return;

  gdDebug( "Epwing: Building the full-text index for dictionary: %s\n", getName().c_str() );

  try {
    FtsHelpers::makeFTSIndex( this, isCancelled );
    FTS_index_completed.ref();
  }
  catch ( std::exception & ex ) {
    gdWarning( "Epwing: Failed building full-text search index for \"%s\", reason: %s\n",
               getName().c_str(),
               ex.what() );
    QFile::remove( QString::fromStdString( ftsIdxName ) );
  }
}

void EpwingDictionary::getArticleText( uint32_t articleAddress, QString & headword, QString & text )
{
  headword.clear();
  text.clear();

  vector< char > chunk;
  char * articleProps;

  {
    QMutexLocker _( &idxMutex );
    articleProps = chunks.getBlock( articleAddress, chunk );
  }

  uint32_t articlePage, articleOffset;

  memcpy( &articlePage, articleProps, sizeof( articlePage ) );
  memcpy( &articleOffset, articleProps + sizeof( articlePage ), sizeof( articleOffset ) );

  try {
    QMutexLocker _( &eBook.getLibMutex() );
    eBook.getArticle( headword, text, articlePage, articleOffset, true );
  }
  catch ( std::exception & e ) {
    text = QString( "Article reading error: %1" ).arg( QString::fromUtf8( e.what() ) );
  }
}


class EpwingHeadwordsRequest: public Dictionary::WordSearchRequest
{
  wstring str;
  EpwingDictionary & dict;

  QAtomicInt isCancelled;
  QFuture< void > f;

public:

  EpwingHeadwordsRequest( wstring const & word_, EpwingDictionary & dict_ ):
    str( word_ ),
    dict( dict_ )
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

  ~EpwingHeadwordsRequest()
  {
    isCancelled.ref();
    f.waitForFinished();
  }
};


void EpwingHeadwordsRequest::run()
{
  if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
    finish();
    return;
  }

  QRegularExpressionMatch m = RX::Epwing::refWord.match( QString::fromStdU32String( str ) );
  if ( !m.hasMatch() ) {
    finish();
    return;
  }
  int articlePage   = m.captured( 1 ).toInt();
  int articleOffset = m.captured( 2 ).toInt();
  EB_Position pos;
  pos.offset = articleOffset;
  pos.page   = articlePage;
  QString headword;
  dict.readHeadword( pos, headword );
  if ( headword.isEmpty() ) {
    finish();
    return;
  }

  auto parts = headword.split( ' ', Qt::SkipEmptyParts );
  if ( parts.empty() ) {
    finish();
    return;
  }


  QVector< int > pg;
  QVector< int > off;
  dict.getHeadwordPos( parts[ 0 ].toStdU32String(), pg, off );

  for ( unsigned i = 0; i < pg.size(); ++i ) {
    if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
      finish();
      return;
    }

    if ( pg.at( i ) == articlePage && off.at( i ) == articleOffset ) {

      QMutexLocker _( &dataMutex );

      matches.emplace_back( parts[ 0 ].toStdU32String() );
      break;
    }
  }

  finish();
}
sptr< Dictionary::WordSearchRequest > EpwingDictionary::findHeadwordsForSynonym( wstring const & word )
{
  return synonymSearchEnabled ? std::make_shared< EpwingHeadwordsRequest >( word, *this ) :
                                Class::findHeadwordsForSynonym( word );
}
/// EpwingDictionary::getArticle()

class EpwingArticleRequest: public Dictionary::DataRequest
{
  wstring word;
  vector< wstring > alts;
  EpwingDictionary & dict;
  bool ignoreDiacritics;

  QAtomicInt isCancelled;
  QFuture< void > f;

public:

  EpwingArticleRequest( wstring const & word_,
                        vector< wstring > const & alts_,
                        EpwingDictionary & dict_,
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

  void getBuiltInArticle( wstring const & word_,
                          QVector< int > & pages,
                          QVector< int > & offsets,
                          multimap< wstring, pair< string, string > > & mainArticles );

  void cancel() override
  {
    isCancelled.ref();
  }

  ~EpwingArticleRequest()
  {
    isCancelled.ref();
    f.waitForFinished();
  }
};

void EpwingArticleRequest::run()
{
  if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
    finish();
    return;
  }

  vector< WordArticleLink > chain = dict.findArticles( word, ignoreDiacritics );

  for ( auto & alt : alts ) {
    /// Make an additional query for each alt
    vector< WordArticleLink > altChain = dict.findArticles( alt, ignoreDiacritics );

    chain.insert( chain.end(), altChain.begin(), altChain.end() );
  }

  multimap< wstring, pair< string, string > > mainArticles, alternateArticles;

  set< quint32 > articlesIncluded; // Some synonims make it that the articles
                                   // appear several times. We combat this
                                   // by only allowing them to appear once.

  wstring wordCaseFolded = Folding::applySimpleCaseOnly( word );
  if ( ignoreDiacritics )
    wordCaseFolded = Folding::applyDiacriticsOnly( wordCaseFolded );

  QVector< int > pages, offsets;

  for ( auto & x : chain ) {
    if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
      finish();
      return;
    }

    if ( articlesIncluded.find( x.articleOffset ) != articlesIncluded.end() )
      continue; // We already have this article in the body.

    // Now grab that article

    string headword, articleText;
    int articlePage, articleOffset;

    try {
      dict.loadArticle( x.articleOffset, headword, articleText, articlePage, articleOffset );
    }
    catch ( ... ) {
    }

    pages.append( articlePage );
    offsets.append( articleOffset );

    // Ok. Now, does it go to main articles, or to alternate ones? We list
    // main ones first, and alternates after.

    // We do the case-folded comparison here.

    wstring headwordStripped = Folding::applySimpleCaseOnly( headword );
    if ( ignoreDiacritics )
      headwordStripped = Folding::applyDiacriticsOnly( headwordStripped );

    multimap< wstring, pair< string, string > > & mapToUse =
      ( wordCaseFolded == headwordStripped ) ? mainArticles : alternateArticles;

    mapToUse.insert( pair( Folding::applySimpleCaseOnly( headword ), pair( headword, articleText ) ) );

    articlesIncluded.insert( x.articleOffset );
  }

  QRegularExpressionMatch m = RX::Epwing::refWord.match( QString::fromStdU32String( word ) );
  bool ref                  = m.hasMatch();

  // Also try to find word in the built-in dictionary index
  getBuiltInArticle( word, pages, offsets, mainArticles );
  for ( auto & alt : alts ) {
    getBuiltInArticle( alt, pages, offsets, alternateArticles );
  }

  if ( mainArticles.empty() && alternateArticles.empty() && !ref ) {
    // No such word
    finish();
    return;
  }

  string result = "<div class=\"epwing_article\">";

  multimap< wstring, pair< string, string > >::const_iterator i;

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

  //only load the next/previous page when not hitted.
  if ( mainArticles.empty() && alternateArticles.empty() && ref ) {
    string headword, articleText;
    int articlePage   = m.captured( 1 ).toInt();
    int articleOffset = m.captured( 2 ).toInt();
    if ( word[ 0 ] == 'r' )
      dict.loadArticleNextPage( headword, articleText, articlePage, articleOffset );
    else {
      //starts with p
      dict.loadArticlePreviousPage( headword, articleText, articlePage, articleOffset );
    }

    //the reference may not contain valid text. at this point ,should return directly.
    if ( articleText.empty() ) {
      //clear result.
      result.clear();
      // No such word
      finish();
      return;
    }

    result += articleText;
  }

  result += "</div>";

  appendString( result );

  hasAnyData = true;

  finish();
}

void EpwingArticleRequest::getBuiltInArticle( wstring const & word_,
                                              QVector< int > & pages,
                                              QVector< int > & offsets,
                                              multimap< wstring, pair< string, string > > & mainArticles )
{
  try {
    string headword, articleText;

    QVector< int > pg, off;
    {
      QMutexLocker _( &dict.eBook.getLibMutex() );
      dict.eBook.getArticlePos( QString::fromStdU32String( word_ ), pg, off );
    }

    for ( int i = 0; i < pg.size(); i++ ) {
      bool already = false;
      for ( int n = 0; n < pages.size(); n++ ) {
        if ( pages.at( n ) == pg.at( i ) && abs( offsets.at( n ) - off.at( i ) ) <= 4 ) {
          already = true;
          break;
        }
      }

      if ( !already ) {
        dict.loadArticle( pg.at( i ), off.at( i ), headword, articleText );

        mainArticles.insert( pair( Folding::applySimpleCaseOnly( headword ), pair( headword, articleText ) ) );

        pages.append( pg.at( i ) );
        offsets.append( off.at( i ) );
      }
    }
  }
  catch ( ... ) {
  }
}

void EpwingDictionary::getHeadwordPos( wstring const & word_, QVector< int > & pg, QVector< int > & off )
{
  try {
    QMutexLocker _( &eBook.getLibMutex() );
    eBook.getArticlePos( QString::fromStdU32String( word_ ), pg, off );
  }
  catch ( ... ) {
    //ignore
  }
}

sptr< Dictionary::DataRequest > EpwingDictionary::getArticle( wstring const & word,
                                                              vector< wstring > const & alts,
                                                              wstring const &,
                                                              bool ignoreDiacritics )

{
  return std::make_shared< EpwingArticleRequest >( word, alts, *this, ignoreDiacritics );
}

//// EpwingDictionary::getResource()

class EpwingResourceRequest: public Dictionary::DataRequest
{
  EpwingDictionary & dict;

  string resourceName;

  QAtomicInt isCancelled;
  QFuture< void > f;

public:

  EpwingResourceRequest( EpwingDictionary & dict_, string const & resourceName_ ):
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

  ~EpwingResourceRequest()
  {
    isCancelled.ref();
    f.waitForFinished();
  }
};

void EpwingResourceRequest::run()
{
  // Some runnables linger enough that they are cancelled before they start
  if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
    finish();
    return;
  }

  QString cacheDir;
  {
    QMutexLocker _( &dict.eBook.getLibMutex() );
    if ( Filetype::isNameOfPicture( resourceName ) )
      cacheDir = dict.getImagesCacheDir();
    else if ( Filetype::isNameOfSound( resourceName ) )
      cacheDir = dict.getSoundsCacheDir();
    else if ( Filetype::isNameOfVideo( resourceName ) )
      cacheDir = dict.getMoviesCacheDir();
  }

  try {
    if ( cacheDir.isEmpty() ) {
      finish();
      return;
    }

    QString fullName = cacheDir + QDir::separator() + QString::fromStdString( resourceName );

    QFile f( fullName );
    if ( f.open( QFile::ReadOnly ) ) {
      QByteArray buffer = f.readAll();

      QMutexLocker _( &dataMutex );

      data.resize( buffer.size() );

      memcpy( &data.front(), buffer.data(), data.size() );

      hasAnyData = true;
    }
  }
  catch ( std::exception & ex ) {
    gdWarning( "Epwing: Failed loading resource \"%s\" for \"%s\", reason: %s\n",
               resourceName.c_str(),
               dict.getName().c_str(),
               ex.what() );
    // Resource not loaded -- we don't set the hasAnyData flag then
  }

  finish();
}

sptr< Dictionary::DataRequest > EpwingDictionary::getResource( string const & name )

{
  return std::make_shared< EpwingResourceRequest >( *this, name );
}


sptr< Dictionary::DataRequest > EpwingDictionary::getSearchResults( QString const & searchString,
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

int EpwingDictionary::japaneseWriting( gd::wchar ch )
{
  if ( ( ch >= 0x30A0 && ch <= 0x30FF ) || ( ch >= 0x31F0 && ch <= 0x31FF ) || ( ch >= 0x3200 && ch <= 0x32FF )
       || ( ch >= 0xFF00 && ch <= 0xFFEF ) || ( ch == 0x1B000 ) )
    return 1; // Katakana
  else if ( ( ch >= 0x3040 && ch <= 0x309F ) || ( ch == 0x1B001 ) )
    return 2; // Hiragana
  else if ( ( ch >= 0x4E00 && ch <= 0x9FAF ) || ( ch >= 0x3400 && ch <= 0x4DBF ) )
    return 3; // Kanji

  return 0;
}

bool EpwingDictionary::isSign( gd::wchar ch )
{
  switch ( ch ) {
    case 0x002B: // PLUS SIGN
    case 0x003C: // LESS-THAN SIGN
    case 0x003D: // EQUALS SIGN
    case 0x003E: // GREATER-THAN SIGN
    case 0x00AC: // NOT SIGN
    case 0xFF0B: // FULLWIDTH PLUS SIGN
    case 0xFF1C: // FULLWIDTH LESS-THAN SIGN
    case 0xFF1D: // FULLWIDTH EQUALS SIGN
    case 0xFF1E: // FULLWIDTH GREATER-THAN SIGN
    case 0xFFE2: // FULLWIDTH NOT SIGN
      return true;

    default:
      return false;
  }
}

bool EpwingDictionary::isJapanesePunctiation( gd::wchar ch )
{
  return ch >= 0x3000 && ch <= 0x303F;
}


class EpwingWordSearchRequest: public BtreeIndexing::BtreeWordSearchRequest
{

  EpwingDictionary & edict;

public:

  EpwingWordSearchRequest( EpwingDictionary & dict_,
                           wstring const & str_,
                           unsigned minLength_,
                           int maxSuffixVariation_,
                           bool allowMiddleMatches_,
                           unsigned long maxResults_ ):
    BtreeWordSearchRequest( dict_, str_, minLength_, maxSuffixVariation_, allowMiddleMatches_, maxResults_, false ),
    edict( dict_ )
  {
    f = QtConcurrent::run( [ this ]() {
      this->run();
    } );
  }

  void findMatches() override;
};


void EpwingWordSearchRequest::findMatches()
{
  BtreeWordSearchRequest::findMatches();
  if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
    finish();
    return;
  }

  while ( matches.size() < maxResults ) {
    QVector< QString > headwords;
    {
      QMutexLocker _( &edict.eBook.getLibMutex() );
      if ( Utils::AtomicInt::loadAcquire( isCancelled ) )
        break;

      if ( !edict.eBook.getMatches( QString::fromStdU32String( str ), headwords ) )
        break;
    }

    QMutexLocker _( &dataMutex );

    for ( const auto & headword : headwords )
      addMatch( gd::toWString( headword ) );

    break;
  }

  finish();
}

sptr< Dictionary::WordSearchRequest > EpwingDictionary::prefixMatch( wstring const & str, unsigned long maxResults )

{
  return std::make_shared< EpwingWordSearchRequest >( *this, str, 0, -1, true, maxResults );
}

sptr< Dictionary::WordSearchRequest > EpwingDictionary::stemmedMatch( wstring const & str,
                                                                      unsigned minLength,
                                                                      unsigned maxSuffixVariation,
                                                                      unsigned long maxResults )

{
  return std::make_shared< EpwingWordSearchRequest >( *this,
                                                      str,
                                                      minLength,
                                                      (int)maxSuffixVariation,
                                                      false,
                                                      maxResults );
}
bool Epwing::EpwingDictionary::readHeadword( const EB_Position & pos, QString & headword )
{
  try {
    QMutexLocker _( &eBook.getLibMutex() );
    eBook.readHeadword( pos, headword, true );
    eBook.fixHeadword( headword );
    return eBook.isHeadwordCorrect( headword );
  }
  catch ( std::exception & ) {
    return false;
  }
}

} // anonymous namespace

void addWordToChunks( Epwing::Book::EpwingHeadword & head,
                      ChunkedStorage::Writer & chunks,
                      BtreeIndexing::IndexedWords & indexedWords,
                      int & wordCount,
                      int & articleCount )
{
  if ( !head.headword.isEmpty() ) {
    uint32_t offset = chunks.startNewBlock();
    chunks.addToBlock( &head.page, sizeof( head.page ) );
    chunks.addToBlock( &head.offset, sizeof( head.offset ) );

    wstring hw = gd::toWString( head.headword );

    indexedWords.addWord( hw, offset );
    wordCount++;
    articleCount++;

    vector< wstring > words;

    // Parse combined kanji/katakana/hiragana headwords

    int w_prev = 0;
    wstring word;
    for ( wstring::size_type n = 0; n < hw.size(); n++ ) {
      gd::wchar ch = hw[ n ];

      if ( Folding::isPunct( ch ) || Folding::isWhitespace( ch ) || EpwingDictionary::isSign( ch )
           || EpwingDictionary::isJapanesePunctiation( ch ) )
        continue;

      int w = EpwingDictionary::japaneseWriting( ch );

      if ( w > 0 ) {
        // Store only separated words
        gd::wchar ch_prev = 0;
        if ( n )
          ch_prev = hw[ n - 1 ];
        bool needStore = ( n == 0 || Folding::isPunct( ch_prev ) || Folding::isWhitespace( ch_prev )
                           || EpwingDictionary::isJapanesePunctiation( ch ) );

        word.push_back( ch );
        w_prev = w;
        wstring::size_type i;
        for ( i = n + 1; i < hw.size(); i++ ) {
          ch = hw[ i ];
          if ( Folding::isPunct( ch ) || Folding::isWhitespace( ch ) || EpwingDictionary::isJapanesePunctiation( ch ) )
            break;
          w = EpwingDictionary::japaneseWriting( ch );
          if ( w != w_prev )
            break;
          word.push_back( ch );
        }

        if ( needStore ) {
          if ( i >= hw.size() || Folding::isPunct( ch ) || Folding::isWhitespace( ch )
               || EpwingDictionary::isJapanesePunctiation( ch ) )
            words.push_back( word );
        }
        word.clear();

        if ( i < hw.size() )
          n = i;
        else
          break;
      }
    }

    if ( words.size() > 1 ) {
      // Allow only one word in every charset

      size_t n;
      int writings[ 4 ];
      memset( writings, 0, sizeof( writings ) );

      for ( n = 0; n < words.size(); n++ ) {
        int w = EpwingDictionary::japaneseWriting( words[ n ][ 0 ] );
        if ( writings[ w ] )
          break;
        else
          writings[ w ] = 1;
      }

      if ( n >= words.size() ) {
        for ( n = 0; n < words.size(); n++ ) {
          indexedWords.addWord( words[ n ], offset );
          wordCount++;
        }
      }
    }
  }
}

vector< sptr< Dictionary::Class > > makeDictionaries( vector< string > const & fileNames,
                                                      string const & indicesDir,
                                                      Dictionary::Initializing & initializing )

{
  vector< sptr< Dictionary::Class > > dictionaries;

  vector< string > dictFiles;
  QByteArray catName = QString( "%1catalogs" ).arg( QDir::separator() ).toUtf8();

  for ( const auto & fileName : fileNames ) {
    // Skip files other than "catalogs" to speed up the scanning

    if ( fileName.size() < (unsigned)catName.size()
         || strcasecmp( fileName.c_str() + ( fileName.size() - catName.size() ), catName.data() ) != 0 )
      continue;

    int ndir = fileName.size() - catName.size();
    if ( ndir < 1 )
      ndir = 1;

    string mainDirectory = fileName.substr( 0, ndir );

    Epwing::Book::EpwingBook dict;
    int subBooksNumber = 0;
    try {
      subBooksNumber = dict.setBook( mainDirectory );
    }
    catch ( std::exception & e ) {
      gdWarning( "Epwing dictionary initializing failed: %s, error: %s\n", mainDirectory.c_str(), e.what() );
      continue;
    }

    for ( int sb = 0; sb < subBooksNumber; sb++ ) {
      QString dir;

      try {
        dictFiles.clear();
        dictFiles.push_back( mainDirectory );
        dictFiles.push_back( fileName );

        dict.setSubBook( sb );

        dir = QString::fromStdString( mainDirectory ) + Utils::Fs::separator() + dict.getCurrentSubBookDirectory();
        QDir _dir( dir );
        if ( !_dir.exists() ) {
          continue;
        }

        Epwing::Book::EpwingBook::collectFilenames( dir, dictFiles );

        QString fontSubName =
          QString::fromStdString( mainDirectory ) + QDir::separator() + "afonts_" + QString::number( sb );
        QFileInfo info( fontSubName );
        if ( info.exists() && info.isFile() )
          dictFiles.push_back( fontSubName.toStdString() );

        // Check if we need to rebuild the index

        string dictId = Dictionary::makeDictionaryId( dictFiles );

        string indexFile = indicesDir + dictId;

        if ( Dictionary::needToRebuildIndex( dictFiles, indexFile ) || indexIsOldOrBad( indexFile ) ) {
          gdDebug( "Epwing: Building the index for dictionary in directory %s\n", dir.toUtf8().data() );

          QString str         = dict.title();
          QByteArray nameData = str.toUtf8();
          initializing.indexingDictionary( nameData.data() );

          File::Class idx( indexFile, "wb" );

          IdxHeader idxHeader{};

          memset( &idxHeader, 0, sizeof( idxHeader ) );

          // We write a dummy header first. At the end of the process the header
          // will be rewritten with the right values.

          idx.write( idxHeader );

          idx.write( nameData.data(), nameData.size() );
          idxHeader.nameSize = nameData.size();

          IndexedWords indexedWords;

          ChunkedStorage::Writer chunks( idx );

          Epwing::Book::EpwingHeadword head;
          int wordCount    = 0;
          int articleCount = 0;
          if ( dict.getFirstHeadword( head ) ) {
            for ( ;; ) {
              addWordToChunks( head, chunks, indexedWords, wordCount, articleCount );
              if ( !dict.getNextHeadword( head ) )
                break;
            }
          }
          else {
            //the book does not contain text,use menu instead if any.
            if(dict.getMenu( head )) {
              auto candidateItems = dict.candidate( head.page, head.offset );
              for ( Epwing::Book::EpwingHeadword word: candidateItems ) {
                addWordToChunks( word, chunks, indexedWords, wordCount, articleCount );
              }
            }
            else {
              throw exEbLibrary( dict.errorString().toUtf8().data() );
            }
          }

          dict.clearBuffers();

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

          idxHeader.wordCount    = wordCount;
          idxHeader.articleCount = articleCount;

          idx.rewind();

          idx.write( &idxHeader, sizeof( idxHeader ) );


        } // If need to rebuild

        dictionaries.push_back( std::make_shared< EpwingDictionary >( dictId, indexFile, dictFiles, sb ) );
      }
      catch ( std::exception & e ) {
        gdWarning( "Epwing dictionary initializing failed: %s, error: %s\n", dir.toUtf8().data(), e.what() );
        continue;
      }
    }
  }
  return dictionaries;
}

} // namespace Epwing

#endif
