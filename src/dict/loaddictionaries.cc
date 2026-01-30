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
#include "dict/website.hh"
#include "dict/forvo.hh"
#include "dict/programs.hh"
#include "dict/voiceengines.hh"
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
#include "common/globalbroadcaster.hh"

#include "dict/transliteration/belarusian.hh"
#include "dict/transliteration/custom.hh"
#include "dict/transliteration/german.hh"
#include "dict/transliteration/greek.hh"
#include "dict/transliteration/romaji.hh"
#include "dict/transliteration/russian.hh"

#ifdef EPWING_SUPPORT
  #include "dict/epwing.hh"
#endif

#ifdef MAKE_CHINESE_CONVERSION_SUPPORT
  #include "dict/transliteration/chinese.hh"
#endif

#include <QThreadPool>
#include <QtConcurrent>
#include <functional>
#include <QMessageBox>
#include <QDir>
#include <QString>

#include <set>

using std::set;

using std::string;
using std::vector;

LoadDictionaries::LoadDictionaries( const Config::Class & cfg ):
  paths( cfg.paths ),
  soundDirs( cfg.soundDirs ),
  hunspell( cfg.hunspell ),
  transliteration( cfg.transliteration ),
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
#ifdef EPWING_SUPPORT
              << "*catalogs"
#endif
    ;
}

void LoadDictionaries::load()
{
  try {
    QtConcurrent::blockingMap( paths, [ this ]( const Config::Path & path ) {
      qDebug() << "collect files from path:" << path.path;
      try {
        collectFiles( path );
      }
      catch ( const std::exception & e ) {
        qWarning() << "Error collecting files from path:" << path.path << "-" << e.what();
        auto exception = "[" + path.path.toStdString() + "]:" + e.what();
        QMutexLocker _( &dictionariesMutex );
        exceptionTexts << QString::fromUtf8( exception );
      }
    } );

    // Helper to parallelize by file for formats that support it
    auto parallelFileLoader = [ this ]( const char * extension, auto makeFunc, auto... args ) {
      std::vector< std::string > triggerFiles;
      for ( const auto & f : allCollectedFiles ) {
        if ( Utils::endsWithIgnoreCase( f, extension ) ) {
          triggerFiles.push_back( f );
        }
      }

      if ( !triggerFiles.empty() ) {
        QtConcurrent::blockingMap( triggerFiles, [ this, makeFunc, args... ]( const std::string & f ) {
          addDicts( makeFunc( std::vector< std::string >{ f }, Config::getIndexDir().toStdString(), *this, args... ) );
        } );
      }
    };

    // Make dictionaries from all collected files in parallel
    std::vector< std::function< void() > > formatLoaders = {
      [ & ]() { parallelFileLoader( ".mdx", Mdx::makeDictionaries ); },
      [ & ]() { parallelFileLoader( ".ifo", Stardict::makeDictionaries, maxHeadwordToExpand ); },
      [ & ]() { parallelFileLoader( ".dsl", Dsl::makeDictionaries, maxHeadwordSize ); },
      [ & ]() { parallelFileLoader( ".dsl.dz", Dsl::makeDictionaries, maxHeadwordSize ); },
      [ & ]() { parallelFileLoader( ".bgl", Bgl::makeDictionaries ); },
      [ & ]() { parallelFileLoader( ".lsa", Lsa::makeDictionaries ); },
      [ & ]() { parallelFileLoader( ".index", DictdFiles::makeDictionaries ); },
      [ & ]() { parallelFileLoader( ".xdxf", Xdxf::makeDictionaries ); },
      [ & ]() { parallelFileLoader( ".xdxf.dz", Xdxf::makeDictionaries ); },
      [ & ]() { parallelFileLoader( ".dct", Sdict::makeDictionaries ); },
      [ & ]() { parallelFileLoader( ".aar", Aard::makeDictionaries, maxHeadwordToExpand ); },
      [ & ]() { parallelFileLoader( ".zips", ZipSounds::makeDictionaries ); },
      [ & ]() { parallelFileLoader( ".gls", Gls::makeDictionaries ); },
      [ & ]() { parallelFileLoader( ".gls.dz", Gls::makeDictionaries ); },
      [ & ]() { parallelFileLoader( ".slob", Slob::makeDictionaries, maxHeadwordToExpand ); },
#ifdef MAKE_ZIM_SUPPORT
      [ & ]() { parallelFileLoader( ".zim", Zim::makeDictionaries, maxHeadwordToExpand ); },
      [ & ]() { parallelFileLoader( ".zimaa", Zim::makeDictionaries, maxHeadwordToExpand ); },
#endif
#ifdef EPWING_SUPPORT
      [ this ]() { addDicts( Epwing::makeDictionaries( allCollectedFiles, Config::getIndexDir().toStdString(), *this ) ); }
#endif
    };

    QtConcurrent::blockingMap( formatLoaders, []( const std::function< void() > & loader ) {
      loader();
    } );

    // Make soundDirs
    {
      vector< sptr< Dictionary::Class > > soundDirDictionaries =
        SoundDir::makeDictionaries( soundDirs, Config::getIndexDir().toStdString(), *this );

      addDicts( soundDirDictionaries );
    }

    // Make hunspells
    {
      vector< sptr< Dictionary::Class > > hunspellDictionaries = HunspellMorpho::makeDictionaries( hunspell );

      addDicts( hunspellDictionaries );
    }

    //handle the custom dictionary name&fts option
    QtConcurrent::blockingMap( dictionaries, [ this ]( const sptr< Dictionary::Class > & dict ) {
      auto baseDir = dict->getContainingFolder();
      if ( baseDir.isEmpty() ) {
        return;
      }

      auto filePath = Utils::Path::combine( baseDir, "metadata.toml" );

      auto dictMetaData = Metadata::load( filePath.toStdString() );
      if ( dictMetaData ) {
        if ( dictMetaData->name ) {
          dict->setName( dictMetaData->name.value() );
        }
        if ( dictMetaData->fullindex ) {
          dict->setFtsEnable( dictMetaData->fullindex.value() );
        }
      }
    } );

    // Save configuration to ensure custom dictionary names and FTS options are preserved
    Config::Class * cfg = GlobalBroadcaster::instance()->getConfig();

    if ( cfg && cfg->dirty ) {
      Config::save( *cfg );
    }
  }
  catch ( std::exception & e ) {
    exceptionTexts << QString::fromUtf8( e.what() );
  }

  emit finished();
}

void LoadDictionaries::addDicts( const std::vector< sptr< Dictionary::Class > > & dicts )
{
  QMutexLocker _( &dictionariesMutex );
  std::move( dicts.begin(), dicts.end(), std::back_inserter( dictionaries ) );
}

void LoadDictionaries::collectFiles( const Config::Path & path )
{
  QDir dir( path.path );

  QFileInfoList entries = dir.entryInfoList( nameFilters, QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot );

  std::vector< std::string > localFiles;

  for ( QFileInfoList::const_iterator i = entries.constBegin(); i != entries.constEnd(); ++i ) {
    QString fullName = i->absoluteFilePath();

    if ( path.recursive && i->isDir() ) {
      // Make sure the path doesn't look like with dsl resources
      if ( !fullName.endsWith( ".dsl.files", Qt::CaseInsensitive )
           && !fullName.endsWith( ".dsl.dz.files", Qt::CaseInsensitive ) ) {
        collectFiles( Config::Path( fullName, true ) );
      }
    }

    if ( !i->isDir() ) {
      localFiles.push_back( QDir::toNativeSeparators( fullName ).toStdString() );
    }
  }

  if ( !localFiles.empty() ) {
    QMutexLocker _( &filesMutex );
    std::move( localFiles.begin(), localFiles.end(), std::back_inserter( allCollectedFiles ) );
  }
}

void LoadDictionaries::indexingDictionary( const string & dictionaryName ) noexcept
{
  emit indexingDictionarySignal( QString::fromUtf8( dictionaryName.c_str() ) );
}

void LoadDictionaries::loadingDictionary( const string & dictionaryName ) noexcept
{
  emit loadingDictionarySignal( QString::fromUtf8( dictionaryName.c_str() ) );
}


void loadDictionaries( QWidget * parent,
                       const Config::Class & cfg,
                       std::vector< sptr< Dictionary::Class > > & dictionaries,
                       QNetworkAccessManager & dictNetMgr,
                       bool doDeferredInit_ )
{
  dictionaries.clear();

  bool showSplashWindow = !cfg.preferences.enableTrayIcon || !cfg.preferences.startToTray;
  // Start a thread to load all the dictionaries
  ::Initializing init( parent, showSplashWindow );

  LoadDictionaries loadDicts( cfg );

  QObject::connect( &loadDicts, &LoadDictionaries::indexingDictionarySignal, &init, &Initializing::indexing );
  QObject::connect( &loadDicts, &LoadDictionaries::loadingDictionarySignal, &init, &Initializing::loading );
  QEventLoop localLoop;

  QObject::connect( &loadDicts, &LoadDictionaries::finished, &localLoop, &QEventLoop::quit );

  QThreadPool::globalInstance()->start( [ &loadDicts ]() {
    loadDicts.load();
  } );

  localLoop.exec();

  if ( loadDicts.getExceptionText().size() ) {
    QMessageBox::critical( parent,
                           QCoreApplication::translate( "LoadDictionaries", "Error loading dictionaries" ),
                           QString::fromUtf8( loadDicts.getExceptionText().c_str() ) );
  }

  dictionaries = loadDicts.getDictionaries();

  // Helper function that will add a vector of dictionary::Class to the dictionary list
  // Implemented as lambda to access method's `dictionaries` variable
  auto static addDicts = [ &dictionaries ]( const vector< sptr< Dictionary::Class > > & dicts ) {
    std::move( dicts.begin(), dicts.end(), std::back_inserter( dictionaries ) );
  };

  ///// We create transliterations synchronously since they are very simple

#ifdef MAKE_CHINESE_CONVERSION_SUPPORT
  addDicts( ChineseTranslit::makeDictionaries( cfg.transliteration.chinese ) );
#endif

  addDicts( RomajiTranslit::makeDictionaries( cfg.transliteration.romaji ) );
  addDicts( CustomTranslit::makeDictionaries( cfg.transliteration.customTrans ) );

  // Make Russian transliteration
  if ( cfg.transliteration.enableRussianTransliteration ) {
    dictionaries.push_back( RussianTranslit::makeDictionary() );
  }

  // Make German transliteration
  if ( cfg.transliteration.enableGermanTransliteration ) {
    dictionaries.push_back( GermanTranslit::makeDictionary() );
  }

  // Make Greek transliteration
  if ( cfg.transliteration.enableGreekTransliteration ) {
    dictionaries.push_back( GreekTranslit::makeDictionary() );
  }

  // Make Belarusian transliteration
  if ( cfg.transliteration.enableBelarusianTransliteration ) {
    addDicts( BelarusianTranslit::makeDictionaries() );
  }

  addDicts( MediaWiki::makeDictionaries( loadDicts, cfg.mediawikis, dictNetMgr ) );
  addDicts( WebSite::makeDictionaries( cfg.webSites, dictNetMgr ) );
  addDicts( Forvo::makeDictionaries( loadDicts, cfg.forvo, dictNetMgr ) );
  addDicts( Lingua::makeDictionaries( loadDicts, cfg.lingua, dictNetMgr ) );
  addDicts( Programs::makeDictionaries( cfg.programs ) );
#ifdef TTS_SUPPORT
  addDicts( VoiceEngines::makeDictionaries( cfg.voiceEngines ) );
#endif
  addDicts( DictServer::makeDictionaries( cfg.dictServers ) );


  qDebug( "Load done" );

  // Remove any stale index files

  set< string > ids;
  std::pair< std::set< string >::iterator, bool > ret;

  for ( unsigned x = dictionaries.size(); x--; ) {
    ret = ids.insert( dictionaries[ x ]->getId() );
    if ( !ret.second ) {
      qWarning( R"(Duplicate dictionary ID found: ID=%s, name="%s", path="%s")",
                dictionaries[ x ]->getId().c_str(),
                dictionaries[ x ]->getName().c_str(),
                dictionaries[ x ]->getDictionaryFilenames().empty() ?
                  "" :
                  dictionaries[ x ]->getDictionaryFilenames()[ 0 ].c_str() );
    }
  }

  // Run deferred inits

  if ( doDeferredInit_ ) {
    doDeferredInit( dictionaries );
  }
}

void doDeferredInit( std::vector< sptr< Dictionary::Class > > & dictionaries )
{
  QtConcurrent::blockingMap( dictionaries, []( const sptr< Dictionary::Class > & dict ) {
    dict->deferredInit();
  } );
}
