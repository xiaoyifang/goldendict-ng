/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "loaddictionaries.hh"
#include "initializing.hh"
#include "dict/bgl.hh"
#include "dict/stardict.hh"
#include "dict/lsa.hh"
#include "dict/dsl.hh"
#include "dict/mediawiki.hh"
#include "dict/sounddir.hh"
#include "dict/hunspell.hh"
#include "dictdfiles.hh"
#include "dict/romaji.hh"
#include "dict/customtransliteration.hh"
#include "dict/russiantranslit.hh"
#include "dict/german.hh"
#include "dict/greektranslit.hh"
#include "dict/belarusiantranslit.hh"
#include "dict/website.hh"
#include "dict/forvo.hh"
#include "dict/programs.hh"
#include "dict/voiceengines.hh"
#include "gddebug.hh"
#include "dict/xdxf.hh"
#include "dict/sdict.hh"
#include "dict/aard.hh"
#include "dict/zipsounds.hh"
#include "dict/mdx.hh"
#include "dict/zim.hh"
#include "dictserver.hh"
#include "dict/slob.hh"
#include "dict/gls.hh"
#include "dict/lingualibre.hh"
#include "metadata.hh"

#ifndef NO_EPWING_SUPPORT
  #include "dict/epwing.hh"
#endif

#ifdef MAKE_CHINESE_CONVERSION_SUPPORT
  #include "dict/chinese.hh"
#endif

#include <QMessageBox>
#include <QDir>

#include <set>

using std::set;

using std::string;
using std::vector;

LoadDictionaries::LoadDictionaries( Config::Class const & cfg ):
  paths( cfg.paths ),
  soundDirs( cfg.soundDirs ),
  hunspell( cfg.hunspell ),
  transliteration( cfg.transliteration ),
  exceptionText( "Load did not finish" ), // Will be cleared upon success
  maxHeadwordSize( cfg.maxHeadwordSize ),
  maxHeadwordToExpand( cfg.maxHeadwordsToExpand )
{
  // Populate name filters

  nameFilters << "*.bgl"
              << "*.ifo"
              << "*.lsa"
              << "*.dat"
              << "*.dsl"
              << "*.dsl.dz"
              << "*.index"
              << "*.xdxf"
              << "*.xdxf.dz"
              << "*.dct"
              << "*.aar"
              << "*.zips"
              << "*.mdx"
              << "*.gls"
              << "*.gls.dz"
              << "*.slob"
#ifdef MAKE_ZIM_SUPPORT
              << "*.zim"
              << "*.zimaa"
#endif
#ifndef NO_EPWING_SUPPORT
              << "*catalogs"
#endif
    ;
}

void LoadDictionaries::run()
{
  try {
    for ( const auto & path : paths ) {
      qDebug() << "handle path:" << path.path;
      handlePath( path );
    }

    // Make soundDirs
    {
      vector< sptr< Dictionary::Class > > soundDirDictionaries =
        SoundDir::makeDictionaries( soundDirs, Config::getIndexDir().toStdString(), *this );

      dictionaries.insert( dictionaries.end(), soundDirDictionaries.begin(), soundDirDictionaries.end() );
    }

    // Make hunspells
    {
      vector< sptr< Dictionary::Class > > hunspellDictionaries = HunspellMorpho::makeDictionaries( hunspell );

      dictionaries.insert( dictionaries.end(), hunspellDictionaries.begin(), hunspellDictionaries.end() );
    }

    //handle the custom dictionary name&fts option
    for ( const auto & dict : dictionaries ) {
      auto baseDir = dict->getContainingFolder();
      if ( baseDir.isEmpty() )
        continue;

      auto filePath = Utils::Path::combine( baseDir, "metadata.toml" );

      auto dictMetaData = Metadata::load( filePath.toStdString() );
      if ( dictMetaData && dictMetaData->name ) {
        dict->setName( dictMetaData->name.value() );
      }
      if ( dictMetaData && dictMetaData->fullindex ) {
        dict->setFtsEnable( dictMetaData->fullindex.value() );
      }
    }

    exceptionText.clear();
  }
  catch ( std::exception & e ) {
    exceptionText = e.what();
  }
}

void LoadDictionaries::addDicts( const std::vector< sptr< Dictionary::Class > > & dicts )
{
  std::move( dicts.begin(), dicts.end(), std::back_inserter( dictionaries ) );
}

void LoadDictionaries::handlePath( Config::Path const & path )
{
  vector< string > allFiles;

  QDir dir( path.path );

  QFileInfoList entries = dir.entryInfoList( nameFilters, QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot );

  for ( QFileInfoList::const_iterator i = entries.constBegin(); i != entries.constEnd(); ++i ) {
    QString fullName = i->absoluteFilePath();

    if ( path.recursive && i->isDir() ) {
      // Make sure the path doesn't look like with dsl resources
      if ( !fullName.endsWith( ".dsl.files", Qt::CaseInsensitive )
           && !fullName.endsWith( ".dsl.dz.files", Qt::CaseInsensitive ) )
        handlePath( Config::Path( fullName, true ) );
    }

    if ( !i->isDir() )
      allFiles.push_back( QDir::toNativeSeparators( fullName ).toStdString() );
  }

  addDicts( Bgl::makeDictionaries( allFiles, Config::getIndexDir().toStdString(), *this ) );
  addDicts( Stardict::makeDictionaries( allFiles, Config::getIndexDir().toStdString(), *this, maxHeadwordToExpand ) );
  addDicts( Lsa::makeDictionaries( allFiles, Config::getIndexDir().toStdString(), *this ) );
  addDicts( Dsl::makeDictionaries( allFiles, Config::getIndexDir().toStdString(), *this, maxHeadwordSize ) );
  addDicts( DictdFiles::makeDictionaries( allFiles, Config::getIndexDir().toStdString(), *this ) );
  addDicts( Xdxf::makeDictionaries( allFiles, Config::getIndexDir().toStdString(), *this ) );
  addDicts( Sdict::makeDictionaries( allFiles, Config::getIndexDir().toStdString(), *this ) );
  addDicts( Aard::makeDictionaries( allFiles, Config::getIndexDir().toStdString(), *this, maxHeadwordToExpand ) );
  addDicts( ZipSounds::makeDictionaries( allFiles, Config::getIndexDir().toStdString(), *this ) );
  addDicts( Mdx::makeDictionaries( allFiles, Config::getIndexDir().toStdString(), *this ) );
  addDicts( Gls::makeDictionaries( allFiles, Config::getIndexDir().toStdString(), *this ) );
  addDicts( Slob::makeDictionaries( allFiles, Config::getIndexDir().toStdString(), *this, maxHeadwordToExpand ) );
#ifdef MAKE_ZIM_SUPPORT
  addDicts( Zim::makeDictionaries( allFiles, Config::getIndexDir().toStdString(), *this, maxHeadwordToExpand ) );
#endif
#ifndef NO_EPWING_SUPPORT
  addDicts( Epwing::makeDictionaries( allFiles, Config::getIndexDir().toStdString(), *this ) );
#endif
}

void LoadDictionaries::indexingDictionary( string const & dictionaryName ) noexcept
{
  emit indexingDictionarySignal( QString::fromUtf8( dictionaryName.c_str() ) );
}

void LoadDictionaries::loadingDictionary( string const & dictionaryName ) noexcept
{
  emit loadingDictionarySignal( QString::fromUtf8( dictionaryName.c_str() ) );
}


void loadDictionaries( QWidget * parent,
                       bool showInitially,
                       Config::Class const & cfg,
                       std::vector< sptr< Dictionary::Class > > & dictionaries,
                       QNetworkAccessManager & dictNetMgr,
                       bool doDeferredInit_ )
{
  dictionaries.clear();

  ::Initializing init( parent, showInitially );

  // Start a thread to load all the dictionaries

  LoadDictionaries loadDicts( cfg );

  QObject::connect( &loadDicts, &LoadDictionaries::indexingDictionarySignal, &init, &Initializing::indexing );
  QObject::connect( &loadDicts, &LoadDictionaries::loadingDictionarySignal, &init, &Initializing::loading );

  QEventLoop localLoop;

  QObject::connect( &loadDicts, &QThread::finished, &localLoop, &QEventLoop::quit );

  loadDicts.start();

  localLoop.exec();

  loadDicts.wait();

  if ( loadDicts.getExceptionText().size() ) {
    QMessageBox::critical( parent,
                           QCoreApplication::translate( "LoadDictionaries", "Error loading dictionaries" ),
                           QString::fromUtf8( loadDicts.getExceptionText().c_str() ) );

    return;
  }

  dictionaries = loadDicts.getDictionaries();

  // Helper function that will add a vector of dictionary::Class to the dictionary list
  // Implemented as lambda to access method's `dictionaries` variable
  auto static addDicts = [ &dictionaries ]( const vector< sptr< Dictionary::Class > > & dicts ) {
    std::move( dicts.begin(), dicts.end(), std::back_inserter( dictionaries ) );
  };

  ///// We create transliterations synchronously since they are very simple

#ifdef MAKE_CHINESE_CONVERSION_SUPPORT
  addDicts( Chinese::makeDictionaries( cfg.transliteration.chinese ) );
#endif

  addDicts( Romaji::makeDictionaries( cfg.transliteration.romaji ) );
  addDicts( CustomTranslit::makeDictionaries( cfg.transliteration.customTrans ) );

  // Make Russian transliteration
  if ( cfg.transliteration.enableRussianTransliteration )
    dictionaries.push_back( RussianTranslit::makeDictionary() );

  // Make German transliteration
  if ( cfg.transliteration.enableGermanTransliteration )
    dictionaries.push_back( GermanTranslit::makeDictionary() );

  // Make Greek transliteration
  if ( cfg.transliteration.enableGreekTransliteration )
    dictionaries.push_back( GreekTranslit::makeDictionary() );

  // Make Belarusian transliteration
  if ( cfg.transliteration.enableBelarusianTransliteration ) {
    addDicts( BelarusianTranslit::makeDictionaries() );
  }

  addDicts( MediaWiki::makeDictionaries( loadDicts, cfg.mediawikis, dictNetMgr ) );
  addDicts( WebSite::makeDictionaries( cfg.webSites, dictNetMgr ) );
  addDicts( Forvo::makeDictionaries( loadDicts, cfg.forvo, dictNetMgr ) );
  addDicts( Lingua::makeDictionaries( loadDicts, cfg.lingua, dictNetMgr ) );
  addDicts( Programs::makeDictionaries( cfg.programs ) );
  addDicts( VoiceEngines::makeDictionaries( cfg.voiceEngines ) );
  addDicts( DictServer::makeDictionaries( cfg.dictServers ) );


  GD_DPRINTF( "Load done\n" );

  // Remove any stale index files

  set< string > ids;
  std::pair< std::set< string >::iterator, bool > ret;

  for ( unsigned x = dictionaries.size(); x--; ) {
    ret = ids.insert( dictionaries[ x ]->getId() );
    if ( !ret.second ) {
      gdWarning( R"(Duplicate dictionary ID found: ID=%s, name="%s", path="%s")",
                 dictionaries[ x ]->getId().c_str(),
                 dictionaries[ x ]->getName().c_str(),
                 dictionaries[ x ]->getDictionaryFilenames().empty() ?
                   "" :
                   dictionaries[ x ]->getDictionaryFilenames()[ 0 ].c_str() );
    }
  }

  // Run deferred inits

  if ( doDeferredInit_ )
    doDeferredInit( dictionaries );
}

void doDeferredInit( std::vector< sptr< Dictionary::Class > > & dictionaries )
{
  for ( const auto & dictionarie : dictionaries )
    dictionarie->deferredInit();
}
