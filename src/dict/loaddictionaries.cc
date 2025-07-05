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

void LoadDictionaries::run()
{
  try {
    for ( const auto & path : paths ) {
      qDebug() << "handle path:" << path.path;
      try {
        handlePath( path );
      }
      catch ( const std::exception & e ) {
        qWarning() << "Error handling path:" << path.path << "-" << e.what();
        //hold last exception message.
        auto exception = "[" + path.path.toStdString() + "]:" + e.what();
        exceptionTexts << QString::fromUtf8( exception );
      }
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
      if ( baseDir.isEmpty() ) {
        continue;
      }

      auto filePath = Utils::Path::combine( baseDir, "metadata.toml" );

      auto dictMetaData = Metadata::load( filePath.toStdString() );
      if ( dictMetaData && dictMetaData->name ) {
        dict->setName( dictMetaData->name.value() );
      }
      if ( dictMetaData && dictMetaData->fullindex ) {
        dict->setFtsEnable( dictMetaData->fullindex.value() );
      }
    }
  }
  catch ( std::exception & e ) {
    exceptionTexts << QString::fromUtf8( e.what() );
  }
}

void LoadDictionaries::addDicts( const std::vector< sptr< Dictionary::Class > > & dicts )
{
  std::move( dicts.begin(), dicts.end(), std::back_inserter( dictionaries ) );
}

void LoadDictionaries::handlePath( const Config::Path & path )
{
  vector< string > allFiles;

  QDir dir( path.path );

  QFileInfoList entries = dir.entryInfoList( nameFilters, QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot );

  for ( QFileInfoList::const_iterator i = entries.constBegin(); i != entries.constEnd(); ++i ) {
    QString fullName = i->absoluteFilePath();

    if ( path.recursive && i->isDir() ) {
      // Make sure the path doesn't look like with dsl resources
      if ( !fullName.endsWith( ".dsl.files", Qt::CaseInsensitive )
           && !fullName.endsWith( ".dsl.dz.files", Qt::CaseInsensitive ) ) {
        handlePath( Config::Path( fullName, true ) );
      }
    }

    if ( !i->isDir() ) {
      allFiles.push_back( QDir::toNativeSeparators( fullName ).toStdString() );
    }
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
#ifdef EPWING_SUPPORT
  addDicts( Epwing::makeDictionaries( allFiles, Config::getIndexDir().toStdString(), *this ) );
#endif
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

  QObject::connect( &loadDicts, &QThread::finished, &localLoop, &QEventLoop::quit );

  loadDicts.start();

  localLoop.exec();

  loadDicts.wait();

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
  for ( const auto & dictionarie : dictionaries ) {
    dictionarie->deferredInit();
  }
}
