/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "config.hh"
#include "folding.hh"
#include <QSaveFile>
#include <QFile>
#include <QtXml>
#include <QApplication>
#include <QStyle>

#ifdef Q_OS_WIN32
  //this is a windows header file.
  #include <Shlobj.h>
#endif

#include <stdint.h>

#include <QStandardPaths>

#if defined( HAVE_X11 )
  // Whether XDG Base Directory specification might be followed.
  // Only Qt5 builds are supported, as Qt4 doesn't provide all functions needed
  // to get XDG Base Directory compliant locations.
  #define XDG_BASE_DIRECTORY_COMPLIANCE
#endif

namespace Config {

namespace {
#ifdef XDG_BASE_DIRECTORY_COMPLIANCE
const char xdgSubdirName[] = "goldendict";

QDir getDataDir()
{
  QDir dir = QStandardPaths::writableLocation( QStandardPaths::GenericDataLocation );
  dir.mkpath( xdgSubdirName );
  if ( !dir.cd( xdgSubdirName ) )
    throw exCantUseDataDir();

  return dir;
}
#endif

QString portableHomeDirPath()
{
  return QCoreApplication::applicationDirPath() + "/portable";
}

QDir getHomeDir()
{
  if ( isPortableVersion() ) {
    return QDir( portableHomeDirPath() );
  }

  QDir result;

  result = QDir::home();
#ifdef Q_OS_WIN32
  if ( result.cd( "Application Data/GoldenDict" ) )
    return result;
  char const * pathInHome = "GoldenDict";
  result                  = QDir::fromNativeSeparators( QString::fromWCharArray( _wgetenv( L"APPDATA" ) ) );
#else
  char const * pathInHome = ".goldendict";
  #ifdef XDG_BASE_DIRECTORY_COMPLIANCE
  // check if an old config dir is present, otherwise use standards-compliant location
  if ( !result.exists( pathInHome ) ) {
    result.setPath( QStandardPaths::writableLocation( QStandardPaths::ConfigLocation ) );
    pathInHome = xdgSubdirName;
  }
  #endif
#endif

  result.mkpath( pathInHome );

  if ( !result.cd( pathInHome ) ) {
    throw exCantUseHomeDir();
  }

  return result;
}

} // namespace

ProxyServer::ProxyServer():
  enabled( false ),
  useSystemProxy( false ),
  type( Socks5 ),
  port( 3128 )
{
}

AnkiConnectServer::AnkiConnectServer():
  enabled( false ),
  host( "127.0.0.1" ),
  port( 8765 ),
  word( "word" ),
  text( "selected_text" ),
  sentence( "marked_sentence" )
{
}

HotKey::HotKey( QKeySequence const & seq ):
  modifiers( seq[ 0 ].keyboardModifiers() ),
  key1( seq[ 0 ].key() ),
  key2( seq[ 1 ].key() )
{
}

QKeySequence HotKey::toKeySequence() const
{
  if ( key2 != 0 && key2 != Qt::Key::Key_unknown ) {
    return { QKeyCombination( modifiers, static_cast< Qt::Key >( key1 ) ),
             QKeyCombination( modifiers, static_cast< Qt::Key >( key2 ) ) };
  }
  return { QKeyCombination( modifiers, static_cast< Qt::Key >( key1 ) ) };
  ;
}

QString Preferences::sanitizeInputPhrase( QString const & inputWord ) const
{
  QString result = inputWord;
  if ( stripClipboard ) {
    auto parts = inputWord.split( QChar::LineFeed, Qt::SkipEmptyParts );
    if ( !parts.empty() ) {
      result = parts[ 0 ];
    }
  }

  if ( limitInputPhraseLength && result.size() > inputPhraseLengthLimit ) {
    qDebug( "Ignoring an input phrase %lld symbols long. The configured maximum input phrase length is %d symbols.",
            result.size(),
            inputPhraseLengthLimit );
    return {};
  }

  // Simplify whitespaces and remove soft hyphens (0xAD);
  return Folding::trimWhitespace( result.simplified() );
}

Preferences::Preferences():
  newTabsOpenAfterCurrentOne( false ),
  newTabsOpenInBackground( true ),
  hideSingleTab( false ),
  mruTabOrder( false ),
  hideMenubar( false ),
  autoStart( false ),
  doubleClickTranslates( true ),
  selectWordBySingleClick( false ),
  autoScrollToTargetArticle( true ),
  escKeyHidesMainWindow( false ),
  alwaysOnTop( false ),
  searchInDock( false ),
// on macOS, register hotkeys will override system shortcuts, disabled for now to avoid troubles.
#ifdef Q_OS_MACOS
  enableMainWindowHotkey( false ),
  enableClipboardHotkey( false ),
#else
  enableMainWindowHotkey( true ),
  enableClipboardHotkey( true ),
#endif
  mainWindowHotkey( QKeySequence( "Ctrl+F11, Ctrl+F11" ) ),
  clipboardHotkey( QKeySequence( "Ctrl+C, Ctrl+C" ) ),
  startWithScanPopupOn( false ),
  enableScanPopupModifiers( false ),
  scanPopupModifiers( 0 ),
  ignoreOwnClipboardChanges( false ),
  scanToMainWindow( false ),
  ignoreDiacritics( false ),
  ignorePunctuation( true ),
#ifdef HAVE_X11
  // Enable both Clipboard and Selection by default so that X users can enjoy full
  // power and disable optionally.
  trackClipboardScan( true ),
  trackSelectionScan( true ),
  showScanFlag( false ),
  selectionChangeDelayTimer( 500 ),
#endif
  pronounceOnLoadMain( false ),
  pronounceOnLoadPopup( false ),
  useInternalPlayer( InternalPlayerBackend::anyAvailable() ),
  checkForNewReleases( true ),
  disallowContentFromOtherSites( false ),
  hideGoldenDictHeader( false ),
  maxNetworkCacheSize( 50 ),
  clearNetworkCacheOnExit( true ),
  zoomFactor( 1 ),
  helpZoomFactor( 1 ),
  wordsZoomLevel( 0 ),
  maxStringsInHistory( 500 ),
  storeHistory( 1 ),
  alwaysExpandOptionalParts( true ),
  confirmFavoritesDeletion( true ),
  collapseBigArticles( false ),
  articleSizeLimit( 2000 ),
  limitInputPhraseLength( false ),
  inputPhraseLengthLimit( 1000 ),
  maxDictionaryRefsInContextMenu( 20 ),
  synonymSearchEnabled( true ),
  stripClipboard( false ),
#if !defined( Q_OS_WIN )
  interfaceStyle( "Default" ),
#endif
  raiseWindowOnSearch( true )
{
}

Chinese::Chinese():
  enable( false ),
  enableSCToTWConversion( true ),
  enableSCToHKConversion( true ),
  enableTCToSCConversion( true )
{
}

Romaji::Romaji():
  enable( false ),
  enableHepburn( true ),
  enableHiragana( true ),
  enableKatakana( true )
{
}

Group * Class::getGroup( unsigned id )
{
  for ( auto & group : groups ) {
    if ( group.id == id ) {
      return &group;
    }
  }
  return 0;
}

Group const * Class::getGroup( unsigned id ) const
{
  for ( const auto & group : groups ) {
    if ( group.id == id ) {
      return &group;
    }
  }
  return 0;
}

void Events::signalMutedDictionariesChanged()
{
  emit mutedDictionariesChanged();
}

namespace {

MediaWikis makeDefaultMediaWikis( bool enable )
{
  MediaWikis mw;

  mw.push_back(
    MediaWiki( "ae6f89aac7151829681b85f035d54e48", "English Wikipedia", "https://en.wikipedia.org/w", enable, "" ) );
  mw.push_back(
    MediaWiki( "affcf9678e7bfe701c9b071f97eccba3", "English Wiktionary", "https://en.wiktionary.org/w", enable, "" ) );
  mw.push_back(
    MediaWiki( "8e0c1c2b6821dab8bdba8eb869ca7176", "Russian Wikipedia", "https://ru.wikipedia.org/w", false, "" ) );
  mw.push_back(
    MediaWiki( "b09947600ae3902654f8ad4567ae8567", "Russian Wiktionary", "https://ru.wiktionary.org/w", false, "" ) );
  mw.push_back(
    MediaWiki( "a8a66331a1242ca2aeb0b4aed361c41d", "German Wikipedia", "https://de.wikipedia.org/w", false, "" ) );
  mw.push_back(
    MediaWiki( "21c64bca5ec10ba17ff19f3066bc962a", "German Wiktionary", "https://de.wiktionary.org/w", false, "" ) );
  mw.push_back(
    MediaWiki( "96957cb2ad73a20c7a1d561fc83c253a", "Portuguese Wikipedia", "https://pt.wikipedia.org/w", false, "" ) );
  mw.push_back( MediaWiki( "ed4c3929196afdd93cc08b9a903aad6a",
                           "Portuguese Wiktionary",
                           "https://pt.wiktionary.org/w",
                           false,
                           "" ) );
  mw.push_back(
    MediaWiki( "f3b4ec8531e52ddf5b10d21e4577a7a2", "Greek Wikipedia", "https://el.wikipedia.org/w", false, "" ) );
  mw.push_back(
    MediaWiki( "5d45232075d06e002dea72fe3e137da1", "Greek Wiktionary", "https://el.wiktionary.org/w", false, "" ) );
  mw.push_back( MediaWiki( "c015d60c4949ad75b5b7069c2ff6dc2c",
                           "Traditional Chinese Wikipedia",
                           "https://zh.wikipedia.org/w",
                           false,
                           "",
                           "zh-hant" ) );
  mw.push_back( MediaWiki( "d50828ad6e115bc9d3421b6821543108",
                           "Traditional Chinese Wiktionary",
                           "https://zh.wiktionary.org/w",
                           false,
                           "",
                           "zh-hant" ) );
  mw.push_back( MediaWiki( "438b17b48cbda1a22d317fea37ec3110",
                           "Simplified Chinese Wikipedia",
                           "https://zh.wikipedia.org/w",
                           false,
                           "",
                           "zh-hans" ) );
  mw.push_back( MediaWiki( "b68b9fb71b5a8c766cc7a5ea8237fc6b",
                           "Simplified Chinese Wiktionary",
                           "https://zh.wiktionary.org/w",
                           false,
                           "",
                           "zh-hans" ) );

  return mw;
}

WebSites makeDefaultWebSites()
{
  WebSites ws;

  ws.push_back( WebSite( "b88cb2898e634c6638df618528284c2d",
                         "Google En-En (Oxford)",
                         "https://www.google.com/search?q=define:%GDWORD%&hl=en",
                         false,
                         "",
                         true ) );
  ws.push_back( WebSite( "f376365a0de651fd7505e7e5e683aa45",
                         "Urban Dictionary",
                         "https://www.urbandictionary.com/define.php?term=%GDWORD%",
                         false,
                         "",
                         true ) );
  ws.push_back( WebSite( "324ca0306187df7511b26d3847f4b07c",
                         "Multitran (En-Ru)",
                         "https://www.multitran.com/m.exe?s=%GDWORD%&l1=1&l2=2",
                         false,
                         "",
                         true ) );
  ws.push_back( WebSite( "379a0ce02a34747d642cb0d7de1b2882",
                         "Merriam-Webster (En)",
                         "https://www.merriam-webster.com/dictionary/%GDWORD%",
                         false,
                         "",
                         true ) );

  return ws;
}

DictServers makeDefaultDictServers()
{
  DictServers ds;
  ds.push_back( DictServer( "6eb6f7a1df225e1cfdcd4cf8e9e4771b", "dict.org", "dict://dict.org", true, "wn", "", "" ) );
  return ds;
}

Programs makeDefaultPrograms()
{
  Programs programs;

  // The following list doesn't make a lot of sense under Windows
#ifndef Q_OS_WIN
  programs.push_back(
    Program( false, Program::Audio, "428b4c2b905ef568a43d9a16f59559b0", "Festival", "festival --tts", "" ) );
  programs.push_back(
    Program( false, Program::Audio, "2cf8b3a60f27e1ac812de0b57c148340", "Espeak", "espeak %GDWORD%", "" ) );
  programs.push_back( Program( false,
                               Program::Html,
                               "4f898f7582596cea518c6b0bfdceb8b3",
                               "Manpages",
                               "man -a --html=/bin/cat %GDWORD%",
                               "" ) );
#endif

  return programs;
}

/// Sets option to true of false if node is "1" or "0" respectively, or leaves
/// it intact if it's neither "1" nor "0".
void applyBoolOption( bool & option, QDomNode const & node )
{
  QString value = node.toElement().text();

  if ( value == "1" ) {
    option = true;
  }
  else if ( value == "0" ) {
    option = false;
  }
}

Group loadGroup( QDomElement grp, unsigned * nextId = 0 )
{
  Group g;

  if ( grp.hasAttribute( "id" ) ) {
    g.id = grp.attribute( "id" ).toUInt();
  }
  else {
    g.id = nextId ? ( *nextId )++ : 0;
  }

  g.name            = grp.attribute( "name" );
  g.icon            = grp.attribute( "icon" );
  g.favoritesFolder = grp.attribute( "favoritesFolder" );

  if ( !grp.attribute( "iconData" ).isEmpty() ) {
    g.iconData = QByteArray::fromBase64( grp.attribute( "iconData" ).toLatin1() );
  }

  if ( !grp.attribute( "shortcut" ).isEmpty() ) {
    g.shortcut = QKeySequence::fromString( grp.attribute( "shortcut" ) );
  }

  QDomNodeList dicts = grp.elementsByTagName( "dictionary" );

  for ( int y = 0; y < dicts.length(); ++y ) {
    g.dictionaries.push_back(
      DictionaryRef( dicts.item( y ).toElement().text(), dicts.item( y ).toElement().attribute( "name" ) ) );
  }

  QDomNode muted = grp.namedItem( "mutedDictionaries" );
  dicts          = muted.toElement().elementsByTagName( "mutedDictionary" );
  for ( int x = 0; x < dicts.length(); ++x ) {
    g.mutedDictionaries.insert( dicts.item( x ).toElement().text() );
  }

  dicts = muted.toElement().elementsByTagName( "popupMutedDictionary" );
  for ( int x = 0; x < dicts.length(); ++x ) {
    g.popupMutedDictionaries.insert( dicts.item( x ).toElement().text() );
  }

  return g;
}

MutedDictionaries loadMutedDictionaries( const QDomNode & mutedDictionaries )
{
  MutedDictionaries result;

  if ( !mutedDictionaries.isNull() ) {
    QDomNodeList nl = mutedDictionaries.toElement().elementsByTagName( "mutedDictionary" );

    for ( int x = 0; x < nl.length(); ++x ) {
      result.insert( nl.item( x ).toElement().text() );
    }
  }

  return result;
}

void saveMutedDictionaries( QDomDocument & dd, QDomElement & muted, MutedDictionaries const & mutedDictionaries )
{
  for ( const auto & mutedDictionarie : mutedDictionaries ) {
    QDomElement dict = dd.createElement( "mutedDictionary" );
    muted.appendChild( dict );

    QDomText value = dd.createTextNode( mutedDictionarie );
    dict.appendChild( value );
  }
}

} // namespace

bool fromConfig2Preference( const QDomNode & node, const QString & expectedValue, bool defaultValue = false )
{
  if ( !node.isNull() ) {
    return ( node.toElement().text() == expectedValue );
  }
  return defaultValue;
}

Class load()
{
  QString configName = getConfigFileName();

  bool loadFromTemplate = false;

  if ( !QFile::exists( configName ) ) {
    // Make the default config, save it and return it
    Class c;

#ifdef Q_OS_LINUX
    if ( QDir( "/usr/share/stardict/dic" ).exists() )
      c.paths.push_back( Path( "/usr/share/stardict/dic", true ) );

    if ( QDir( "/usr/share/dictd" ).exists() )
      c.paths.push_back( Path( "/usr/share/dictd", true ) );

    if ( QDir( "/usr/share/opendict/dictionaries" ).exists() )
      c.paths.push_back( Path( "/usr/share/opendict/dictionaries", true ) );

    if ( QDir( "/usr/share/goldendict-wordnet" ).exists() )
      c.paths.push_back( Path( "/usr/share/goldendict-wordnet", true ) );

    if ( QDir( "/usr/share/WyabdcRealPeopleTTS" ).exists() )
      c.soundDirs.push_back( SoundDir( "/usr/share/WyabdcRealPeopleTTS", "WyabdcRealPeopleTTS" ) );

    if ( QDir( "/usr/share/myspell/dicts" ).exists() )
      c.hunspell.dictionariesPath = "/usr/share/myspell/dicts";
#endif

    // Put portable hard-code directory the the config for the first time.
    if ( isPortableVersion() ) {
      // For portable version, hardcode some settings
      c.paths.push_back( Path( getPortableVersionDictionaryDir(), true ) );
    }

    QString possibleMorphologyPath = getProgramDataDir() + "/content/morphology";

    if ( QDir( possibleMorphologyPath ).exists() ) {
      c.hunspell.dictionariesPath = possibleMorphologyPath;
    }

    c.mediawikis  = makeDefaultMediaWikis( true );
    c.webSites    = makeDefaultWebSites();
    c.dictServers = makeDefaultDictServers();
    c.programs    = makeDefaultPrograms();

    // Check if we have a template config file. If we do, load it instead

    configName       = getProgramDataDir() + "/content/defconfig";
    loadFromTemplate = QFile( configName ).exists();

    if ( !loadFromTemplate ) {
      save( c );

      return c;
    }
  }

  getStylesDir();

  QFile configFile( configName );

  if ( !configFile.open( QFile::ReadOnly ) ) {
    throw exCantReadConfigFile();
  }

  QDomDocument dd;

  QString errorStr;
  int errorLine, errorColumn;

  if ( !loadFromTemplate ) {
    // Load the config as usual
    if ( !dd.setContent( &configFile, false, &errorStr, &errorLine, &errorColumn ) ) {
      qDebug( "Error: %s at %d,%d", errorStr.toLocal8Bit().constData(), errorLine, errorColumn );
      throw exMalformedConfigFile();
    }
  }
  else {
    // We need to replace all %PROGRAMDIR% with the program data dir
    QByteArray data = configFile.readAll();

    data.replace( "%PROGRAMDIR%", getProgramDataDir().toUtf8() );

    QBuffer bufferedData( &data );

    if ( !dd.setContent( &bufferedData, false, &errorStr, &errorLine, &errorColumn ) ) {
      qDebug( "Error: %s at %d,%d", errorStr.toLocal8Bit().constData(), errorLine, errorColumn );
      throw exMalformedConfigFile();
    }
  }

  configFile.close();

  QDomNode root = dd.namedItem( "config" );

  Class c;

  // Put the hard-code portable directory to the first.
  // To allow additional directories, this path should not be saved.
  if ( isPortableVersion() ) {
    // For portable version, hardcode some settings
    c.paths.push_back( Path( getPortableVersionDictionaryDir(), true ) );
  }

  QDomNode paths = root.namedItem( "paths" );

  if ( !paths.isNull() ) {
    QDomNodeList nl = paths.toElement().elementsByTagName( "path" );

    for ( int x = 0; x < nl.length(); ++x ) {
      c.paths.push_back(
        Path( nl.item( x ).toElement().text(), nl.item( x ).toElement().attribute( "recursive" ) == "1" ) );
    }
  }

  QDomNode soundDirs = root.namedItem( "sounddirs" );

  if ( !soundDirs.isNull() ) {
    QDomNodeList nl = soundDirs.toElement().elementsByTagName( "sounddir" );

    for ( int x = 0; x < nl.length(); ++x ) {
      c.soundDirs.push_back( SoundDir( nl.item( x ).toElement().text(),
                                       nl.item( x ).toElement().attribute( "name" ),
                                       nl.item( x ).toElement().attribute( "icon" ) ) );
    }
  }

  QDomNode dictionaryOrder = root.namedItem( "dictionaryOrder" );

  if ( !dictionaryOrder.isNull() ) {
    c.dictionaryOrder = loadGroup( dictionaryOrder.toElement() );
  }

  QDomNode inactiveDictionaries = root.namedItem( "inactiveDictionaries" );

  if ( !inactiveDictionaries.isNull() ) {
    c.inactiveDictionaries = loadGroup( inactiveDictionaries.toElement() );
  }

  QDomNode groups = root.namedItem( "groups" );

  if ( !groups.isNull() ) {
    c.groups.nextId = groups.toElement().attribute( "nextId", "1" ).toUInt();

    QDomNodeList nl = groups.toElement().elementsByTagName( "group" );

    for ( int x = 0; x < nl.length(); ++x ) {
      QDomElement grp = nl.item( x ).toElement();

      c.groups.push_back( loadGroup( grp, &c.groups.nextId ) );
    }
  }

  QDomNode hunspell = root.namedItem( "hunspell" );

  if ( !hunspell.isNull() ) {
    c.hunspell.dictionariesPath = hunspell.toElement().attribute( "dictionariesPath" );

    QDomNodeList nl = hunspell.toElement().elementsByTagName( "enabled" );

    for ( int x = 0; x < nl.length(); ++x ) {
      c.hunspell.enabledDictionaries.push_back( nl.item( x ).toElement().text() );
    }
  }

  QDomNode transliteration = root.namedItem( "transliteration" );

  if ( !transliteration.isNull() ) {
    applyBoolOption( c.transliteration.enableRussianTransliteration,
                     transliteration.namedItem( "enableRussianTransliteration" ) );

    applyBoolOption( c.transliteration.enableGermanTransliteration,
                     transliteration.namedItem( "enableGermanTransliteration" ) );

    applyBoolOption( c.transliteration.enableGreekTransliteration,
                     transliteration.namedItem( "enableGreekTransliteration" ) );

    applyBoolOption( c.transliteration.enableBelarusianTransliteration,
                     transliteration.namedItem( "enableBelarusianTransliteration" ) );

#ifdef MAKE_CHINESE_CONVERSION_SUPPORT
    QDomNode chinese = transliteration.namedItem( "chinese" );

    if ( !chinese.isNull() ) {
      applyBoolOption( c.transliteration.chinese.enable, chinese.namedItem( "enable" ) );
      applyBoolOption( c.transliteration.chinese.enableSCToTWConversion,
                       chinese.namedItem( "enableSCToTWConversion" ) );
      applyBoolOption( c.transliteration.chinese.enableSCToHKConversion,
                       chinese.namedItem( "enableSCToHKConversion" ) );
      applyBoolOption( c.transliteration.chinese.enableTCToSCConversion,
                       chinese.namedItem( "enableTCToSCConversion" ) );
    }
#endif

    QDomNode romaji = transliteration.namedItem( "romaji" );

    if ( !romaji.isNull() ) {
      applyBoolOption( c.transliteration.romaji.enable, romaji.namedItem( "enable" ) );
      applyBoolOption( c.transliteration.romaji.enableHepburn, romaji.namedItem( "enableHepburn" ) );
      applyBoolOption( c.transliteration.romaji.enableHiragana, romaji.namedItem( "enableHiragana" ) );
      applyBoolOption( c.transliteration.romaji.enableKatakana, romaji.namedItem( "enableKatakana" ) );
    }

    QDomNode customtrans = transliteration.namedItem( "customtrans" );

    if ( !customtrans.isNull() ) {
      applyBoolOption( c.transliteration.customTrans.enable, customtrans.namedItem( "enable" ) );
      c.transliteration.customTrans.context = customtrans.namedItem( "context" ).toElement().text();
    }
  }

  QDomNode lingua = root.namedItem( "lingua" );

  if ( !lingua.isNull() ) {
    applyBoolOption( c.lingua.enable, lingua.namedItem( "enable" ) );
    c.lingua.languageCodes = lingua.namedItem( "languageCodes" ).toElement().text();
  }


  QDomNode forvo = root.namedItem( "forvo" );

  if ( !forvo.isNull() ) {
    applyBoolOption( c.forvo.enable, forvo.namedItem( "enable" ) );

    c.forvo.apiKey        = forvo.namedItem( "apiKey" ).toElement().text();
    c.forvo.languageCodes = forvo.namedItem( "languageCodes" ).toElement().text();
  }
  else {
    c.forvo.languageCodes = "en, ru"; // Default demo values
  }

  QDomNode programs = root.namedItem( "programs" );

  if ( !programs.isNull() ) {
    QDomNodeList nl = programs.toElement().elementsByTagName( "program" );

    for ( int x = 0; x < nl.length(); ++x ) {
      QDomElement pr = nl.item( x ).toElement();

      Program p;

      p.id           = pr.attribute( "id" );
      p.name         = pr.attribute( "name" );
      p.commandLine  = pr.attribute( "commandLine" );
      p.enabled      = ( pr.attribute( "enabled" ) == "1" );
      p.type         = ( Program::Type )( pr.attribute( "type" ).toInt() );
      p.iconFilename = pr.attribute( "icon" );

      c.programs.push_back( p );
    }
  }
  else {
    c.programs = makeDefaultPrograms();
  }

  QDomNode mws = root.namedItem( "mediawikis" );

  if ( !mws.isNull() ) {
    QDomNodeList nl = mws.toElement().elementsByTagName( "mediawiki" );

    for ( int x = 0; x < nl.length(); ++x ) {
      QDomElement mw = nl.item( x ).toElement();

      MediaWiki w;

      w.id      = mw.attribute( "id" );
      w.name    = mw.attribute( "name" );
      w.url     = mw.attribute( "url" );
      w.enabled = ( mw.attribute( "enabled" ) == "1" );
      w.icon    = mw.attribute( "icon" );
      w.lang    = mw.attribute( "lang" );

      c.mediawikis.push_back( w );
    }
  }
  else {
    // When upgrading, populate the list with some choices, but don't enable
    // anything.
    c.mediawikis = makeDefaultMediaWikis( false );
  }

  QDomNode wss = root.namedItem( "websites" );

  if ( !wss.isNull() ) {
    QDomNodeList nl = wss.toElement().elementsByTagName( "website" );

    for ( int x = 0; x < nl.length(); ++x ) {
      QDomElement ws = nl.item( x ).toElement();

      WebSite w;

      w.id            = ws.attribute( "id" );
      w.name          = ws.attribute( "name" );
      w.url           = ws.attribute( "url" );
      w.enabled       = ( ws.attribute( "enabled" ) == "1" );
      w.iconFilename  = ws.attribute( "icon" );
      w.inside_iframe = ( ws.attribute( "inside_iframe", "1" ) == "1" );

      c.webSites.push_back( w );
    }
  }
  else {
    // Upgrading
    c.webSites = makeDefaultWebSites();
  }

  QDomNode dss = root.namedItem( "dictservers" );

  if ( !dss.isNull() ) {
    QDomNodeList nl = dss.toElement().elementsByTagName( "server" );

    for ( int x = 0; x < nl.length(); ++x ) {
      QDomElement ds = nl.item( x ).toElement();

      DictServer d;

      d.id           = ds.attribute( "id" );
      d.name         = ds.attribute( "name" );
      d.url          = ds.attribute( "url" );
      d.enabled      = ( ds.attribute( "enabled" ) == "1" );
      d.databases    = ds.attribute( "databases" );
      d.strategies   = ds.attribute( "strategies" );
      d.iconFilename = ds.attribute( "icon" );

      c.dictServers.push_back( d );
    }
  }
  else {
    // Upgrading
    c.dictServers = makeDefaultDictServers();
  }
#ifdef TTS_SUPPORT
  QDomNode ves = root.namedItem( "voiceEngines" );

  if ( !ves.isNull() ) {
    QDomNodeList nl = ves.toElement().elementsByTagName( "voiceEngine" );

    for ( int x = 0; x < nl.length(); ++x ) {
      QDomElement ve = nl.item( x ).toElement();
      VoiceEngine v;

      v.enabled      = ve.attribute( "enabled" ) == "1";
      v.engine_name  = ve.attribute( "engine_name" );
      v.name         = ve.attribute( "name" );
      v.voice_name   = ve.attribute( "voice_name" );
      v.locale       = QLocale( ve.attribute( "locale" ) );
      v.iconFilename = ve.attribute( "icon" );
      v.volume       = ve.attribute( "volume", "50" ).toInt();
      if ( ( v.volume < 0 ) || ( v.volume > 100 ) ) {
        v.volume = 50;
      }
      v.rate = ve.attribute( "rate", "0" ).toInt();
      if ( ( v.rate < -10 ) || ( v.rate > 10 ) ) {
        v.rate = 0;
      }
      c.voiceEngines.push_back( v );
    }
  }
#endif

  c.mutedDictionaries      = loadMutedDictionaries( root.namedItem( "mutedDictionaries" ) );
  c.popupMutedDictionaries = loadMutedDictionaries( root.namedItem( "popupMutedDictionaries" ) );

  QDomNode preferences = root.namedItem( "preferences" );

  if ( !preferences.isNull() ) {
    c.preferences.interfaceLanguage = preferences.namedItem( "interfaceLanguage" ).toElement().text();
    c.preferences.displayStyle      = preferences.namedItem( "displayStyle" ).toElement().text();
    c.preferences.interfaceFont     = preferences.namedItem( "interfaceFont" ).toElement().text();
#if !defined( Q_OS_WIN )
    c.preferences.interfaceStyle = preferences.namedItem( "interfaceStyle" ).toElement().text();
#endif
    c.preferences.newTabsOpenAfterCurrentOne =
      ( preferences.namedItem( "newTabsOpenAfterCurrentOne" ).toElement().text() == "1" );
    c.preferences.newTabsOpenInBackground =
      ( preferences.namedItem( "newTabsOpenInBackground" ).toElement().text() == "1" );
    c.preferences.hideSingleTab = ( preferences.namedItem( "hideSingleTab" ).toElement().text() == "1" );
    c.preferences.mruTabOrder   = ( preferences.namedItem( "mruTabOrder" ).toElement().text() == "1" );
    c.preferences.hideMenubar   = ( preferences.namedItem( "hideMenubar" ).toElement().text() == "1" );
#ifndef Q_OS_MACOS // // macOS uses the dock menu instead of the tray icon
    c.preferences.enableTrayIcon = ( preferences.namedItem( "enableTrayIcon" ).toElement().text() == "1" );
    c.preferences.startToTray    = ( preferences.namedItem( "startToTray" ).toElement().text() == "1" );
    c.preferences.closeToTray    = ( preferences.namedItem( "closeToTray" ).toElement().text() == "1" );
#endif
    c.preferences.autoStart      = ( preferences.namedItem( "autoStart" ).toElement().text() == "1" );
    c.preferences.alwaysOnTop    = ( preferences.namedItem( "alwaysOnTop" ).toElement().text() == "1" );
    c.preferences.searchInDock   = ( preferences.namedItem( "searchInDock" ).toElement().text() == "1" );

    if ( !preferences.namedItem( "customFonts" ).isNull() ) {
      CustomFonts fonts         = CustomFonts::fromElement( preferences.namedItem( "customFonts" ).toElement() );
      c.preferences.customFonts = fonts;
    }

    if ( !preferences.namedItem( "doubleClickTranslates" ).isNull() ) {
      c.preferences.doubleClickTranslates =
        ( preferences.namedItem( "doubleClickTranslates" ).toElement().text() == "1" );
    }

    if ( !preferences.namedItem( "selectWordBySingleClick" ).isNull() ) {
      c.preferences.selectWordBySingleClick =
        ( preferences.namedItem( "selectWordBySingleClick" ).toElement().text() == "1" );
    }

    if ( !preferences.namedItem( "autoScrollToTargetArticle" ).isNull() ) {
      c.preferences.autoScrollToTargetArticle =
        ( preferences.namedItem( "autoScrollToTargetArticle" ).toElement().text() == "1" );
    }

    if ( !preferences.namedItem( "escKeyHidesMainWindow" ).isNull() ) {
      c.preferences.escKeyHidesMainWindow =
        ( preferences.namedItem( "escKeyHidesMainWindow" ).toElement().text() == "1" );
    }

    if ( !preferences.namedItem( "darkMode" ).isNull() ) {
      c.preferences.darkMode = static_cast< Dark >( preferences.namedItem( "darkMode" ).toElement().text().toInt() );
    }

    if ( !preferences.namedItem( "darkReaderMode" ).isNull() ) {
      c.preferences.darkReaderMode =
        static_cast< Dark >( preferences.namedItem( "darkReaderMode" ).toElement().text().toInt() );
    }

    if ( !preferences.namedItem( "zoomFactor" ).isNull() ) {
      c.preferences.zoomFactor = preferences.namedItem( "zoomFactor" ).toElement().text().toDouble();
    }

    if ( !preferences.namedItem( "helpZoomFactor" ).isNull() ) {
      c.preferences.helpZoomFactor = preferences.namedItem( "helpZoomFactor" ).toElement().text().toDouble();
    }

    if ( !preferences.namedItem( "wordsZoomLevel" ).isNull() ) {
      c.preferences.wordsZoomLevel = preferences.namedItem( "wordsZoomLevel" ).toElement().text().toInt();
    }

    applyBoolOption( c.preferences.enableMainWindowHotkey, preferences.namedItem( "enableMainWindowHotkey" ) );
    if ( !preferences.namedItem( "mainWindowHotkey" ).isNull() ) {
      c.preferences.mainWindowHotkey =
        QKeySequence::fromString( preferences.namedItem( "mainWindowHotkey" ).toElement().text() );
    }
    applyBoolOption( c.preferences.enableClipboardHotkey, preferences.namedItem( "enableClipboardHotkey" ) );
    if ( !preferences.namedItem( "clipboardHotkey" ).isNull() ) {
      c.preferences.clipboardHotkey =
        QKeySequence::fromString( preferences.namedItem( "clipboardHotkey" ).toElement().text() );
    }

    c.preferences.startWithScanPopupOn = ( preferences.namedItem( "startWithScanPopupOn" ).toElement().text() == "1" );
    c.preferences.enableScanPopupModifiers =
      ( preferences.namedItem( "enableScanPopupModifiers" ).toElement().text() == "1" );
    c.preferences.scanPopupModifiers = ( preferences.namedItem( "scanPopupModifiers" ).toElement().text().toULong() );
    c.preferences.ignoreOwnClipboardChanges =
      ( preferences.namedItem( "ignoreOwnClipboardChanges" ).toElement().text() == "1" );
    c.preferences.scanToMainWindow = ( preferences.namedItem( "scanToMainWindow" ).toElement().text() == "1" );
    c.preferences.ignoreDiacritics = ( preferences.namedItem( "ignoreDiacritics" ).toElement().text() == "1" );
    if ( !preferences.namedItem( "ignorePunctuation" ).isNull() ) {
      c.preferences.ignorePunctuation = ( preferences.namedItem( "ignorePunctuation" ).toElement().text() == "1" );
    }

    if ( !preferences.namedItem( "sessionCollapse" ).isNull() ) {
      c.preferences.sessionCollapse = ( preferences.namedItem( "sessionCollapse" ).toElement().text() == "1" );
    }

#ifdef HAVE_X11
    c.preferences.trackClipboardScan = ( preferences.namedItem( "trackClipboardScan" ).toElement().text() == "1" );
    c.preferences.trackSelectionScan = ( preferences.namedItem( "trackSelectionScan" ).toElement().text() == "1" );
    c.preferences.showScanFlag       = ( preferences.namedItem( "showScanFlag" ).toElement().text() == "1" );
    c.preferences.selectionChangeDelayTimer =
      preferences.namedItem( "selectionChangeDelayTimer" ).toElement().text().toInt();
#endif

    c.preferences.pronounceOnLoadMain  = ( preferences.namedItem( "pronounceOnLoadMain" ).toElement().text() == "1" );
    c.preferences.pronounceOnLoadPopup = ( preferences.namedItem( "pronounceOnLoadPopup" ).toElement().text() == "1" );

    if ( InternalPlayerBackend::anyAvailable() ) {
      if ( !preferences.namedItem( "useInternalPlayer" ).isNull() ) {
        c.preferences.useInternalPlayer = ( preferences.namedItem( "useInternalPlayer" ).toElement().text() == "1" );
      }
    }
    else {
      c.preferences.useInternalPlayer = false;
    }

    if ( !preferences.namedItem( "internalPlayerBackend" ).isNull() ) {
      c.preferences.internalPlayerBackend.setName(
        preferences.namedItem( "internalPlayerBackend" ).toElement().text() );
    }

    if ( !preferences.namedItem( "audioPlaybackProgram" ).isNull() ) {
      c.preferences.audioPlaybackProgram = preferences.namedItem( "audioPlaybackProgram" ).toElement().text();
    }
    else {
      c.preferences.audioPlaybackProgram = "vlc --intf dummy --play-and-exit";
    }

    QDomNode proxy = preferences.namedItem( "proxyserver" );

    if ( !proxy.isNull() ) {
      c.preferences.proxyServer.enabled        = ( proxy.toElement().attribute( "enabled" ) == "1" );
      c.preferences.proxyServer.useSystemProxy = ( proxy.toElement().attribute( "useSystemProxy" ) == "1" );
      c.preferences.proxyServer.type     = (ProxyServer::Type)proxy.namedItem( "type" ).toElement().text().toULong();
      c.preferences.proxyServer.host     = proxy.namedItem( "host" ).toElement().text();
      c.preferences.proxyServer.port     = proxy.namedItem( "port" ).toElement().text().toULong();
      c.preferences.proxyServer.user     = proxy.namedItem( "user" ).toElement().text();
      c.preferences.proxyServer.password       = proxy.namedItem( "password" ).toElement().text();
    }

    QDomNode ankiConnectServer = preferences.namedItem( "ankiConnectServer" );

    if ( !ankiConnectServer.isNull() ) {
      c.preferences.ankiConnectServer.enabled = ( ankiConnectServer.toElement().attribute( "enabled" ) == "1" );
      c.preferences.ankiConnectServer.host    = ankiConnectServer.namedItem( "host" ).toElement().text();
      c.preferences.ankiConnectServer.port    = ankiConnectServer.namedItem( "port" ).toElement().text().toInt();
      c.preferences.ankiConnectServer.deck    = ankiConnectServer.namedItem( "deck" ).toElement().text();
      c.preferences.ankiConnectServer.model   = ankiConnectServer.namedItem( "model" ).toElement().text();

      c.preferences.ankiConnectServer.word     = ankiConnectServer.namedItem( "word" ).toElement().text();
      c.preferences.ankiConnectServer.text     = ankiConnectServer.namedItem( "text" ).toElement().text();
      c.preferences.ankiConnectServer.sentence = ankiConnectServer.namedItem( "sentence" ).toElement().text();
    }

    if ( !preferences.namedItem( "checkForNewReleases" ).isNull() ) {
      c.preferences.checkForNewReleases = ( preferences.namedItem( "checkForNewReleases" ).toElement().text() == "1" );
    }

    if ( !preferences.namedItem( "disallowContentFromOtherSites" ).isNull() ) {
      c.preferences.disallowContentFromOtherSites =
        ( preferences.namedItem( "disallowContentFromOtherSites" ).toElement().text() == "1" );
    }

    if ( !preferences.namedItem( "hideGoldenDictHeader" ).isNull() ) {
      c.preferences.hideGoldenDictHeader =
        ( preferences.namedItem( "hideGoldenDictHeader" ).toElement().text() == "1" );
    }

    if ( !preferences.namedItem( "maxNetworkCacheSize" ).isNull() ) {
      c.preferences.maxNetworkCacheSize = preferences.namedItem( "maxNetworkCacheSize" ).toElement().text().toInt();
    }

    if ( !preferences.namedItem( "clearNetworkCacheOnExit" ).isNull() ) {
      c.preferences.clearNetworkCacheOnExit =
        ( preferences.namedItem( "clearNetworkCacheOnExit" ).toElement().text() == "1" );
    }


    if ( !preferences.namedItem( "removeInvalidIndexOnExit" ).isNull() ) {
      c.preferences.removeInvalidIndexOnExit =
        ( preferences.namedItem( "removeInvalidIndexOnExit" ).toElement().text() == "1" );
    }

    if ( !preferences.namedItem( "enableApplicationLog" ).isNull() ) {
      c.preferences.enableApplicationLog =
        ( preferences.namedItem( "enableApplicationLog" ).toElement().text() == "1" );
    }

    if ( !preferences.namedItem( "maxStringsInHistory" ).isNull() ) {
      c.preferences.maxStringsInHistory = preferences.namedItem( "maxStringsInHistory" ).toElement().text().toUInt();
    }

    if ( !preferences.namedItem( "storeHistory" ).isNull() ) {
      c.preferences.storeHistory = preferences.namedItem( "storeHistory" ).toElement().text().toUInt();
    }

    if ( !preferences.namedItem( "alwaysExpandOptionalParts" ).isNull() ) {
      c.preferences.alwaysExpandOptionalParts =
        preferences.namedItem( "alwaysExpandOptionalParts" ).toElement().text().toUInt();
    }

    if ( !preferences.namedItem( "addonStyle" ).isNull() ) {
      c.preferences.addonStyle = preferences.namedItem( "addonStyle" ).toElement().text();
    }

    if ( !preferences.namedItem( "historyStoreInterval" ).isNull() ) {
      c.preferences.historyStoreInterval = preferences.namedItem( "historyStoreInterval" ).toElement().text().toUInt();
    }

    if ( !preferences.namedItem( "favoritesStoreInterval" ).isNull() ) {
      c.preferences.favoritesStoreInterval =
        preferences.namedItem( "favoritesStoreInterval" ).toElement().text().toUInt();
    }

    if ( !preferences.namedItem( "confirmFavoritesDeletion" ).isNull() ) {
      c.preferences.confirmFavoritesDeletion =
        ( preferences.namedItem( "confirmFavoritesDeletion" ).toElement().text() == "1" );
    }

    if ( !preferences.namedItem( "collapseBigArticles" ).isNull() ) {
      c.preferences.collapseBigArticles = ( preferences.namedItem( "collapseBigArticles" ).toElement().text() == "1" );
    }

    if ( !preferences.namedItem( "articleSizeLimit" ).isNull() ) {
      c.preferences.articleSizeLimit = preferences.namedItem( "articleSizeLimit" ).toElement().text().toInt();
    }

    if ( !preferences.namedItem( "limitInputPhraseLength" ).isNull() ) {
      c.preferences.limitInputPhraseLength =
        ( preferences.namedItem( "limitInputPhraseLength" ).toElement().text() == "1" );
    }

    if ( !preferences.namedItem( "inputPhraseLengthLimit" ).isNull() ) {
      c.preferences.inputPhraseLengthLimit =
        preferences.namedItem( "inputPhraseLengthLimit" ).toElement().text().toInt();
    }

    if ( !preferences.namedItem( "maxDictionaryRefsInContextMenu" ).isNull() ) {
      c.preferences.maxDictionaryRefsInContextMenu =
        preferences.namedItem( "maxDictionaryRefsInContextMenu" ).toElement().text().toUShort();
    }

    if ( !preferences.namedItem( "synonymSearchEnabled" ).isNull() ) {
      c.preferences.synonymSearchEnabled =
        ( preferences.namedItem( "synonymSearchEnabled" ).toElement().text() == "1" );
    }

    if ( !preferences.namedItem( "stripClipboard" ).isNull() ) {
      c.preferences.stripClipboard = ( preferences.namedItem( "stripClipboard" ).toElement().text() == "1" );
    }

    if ( !preferences.namedItem( "raiseWindowOnSearch" ).isNull() ) {
      c.preferences.raiseWindowOnSearch = ( preferences.namedItem( "raiseWindowOnSearch" ).toElement().text() == "1" );
    }

    QDomNode fts = preferences.namedItem( "fullTextSearch" );

    if ( !fts.isNull() ) {
      if ( !fts.namedItem( "searchMode" ).isNull() ) {
        c.preferences.fts.searchMode = fts.namedItem( "searchMode" ).toElement().text().toInt();
      }

      if ( !fts.namedItem( "dialogGeometry" ).isNull() ) {
        c.preferences.fts.dialogGeometry =
          QByteArray::fromBase64( fts.namedItem( "dialogGeometry" ).toElement().text().toLatin1() );
      }

      if ( !fts.namedItem( "disabledTypes" ).isNull() ) {
        c.preferences.fts.disabledTypes = fts.namedItem( "disabledTypes" ).toElement().text();
      }

      if ( !fts.namedItem( "enabled" ).isNull() ) {
        c.preferences.fts.enabled = ( fts.namedItem( "enabled" ).toElement().text() == "1" );
      }

      if ( !fts.namedItem( "maxDictionarySize" ).isNull() ) {
        c.preferences.fts.maxDictionarySize = fts.namedItem( "maxDictionarySize" ).toElement().text().toUInt();
      }

      if ( !fts.namedItem( "parallelThreads" ).isNull() ) {
        c.preferences.fts.parallelThreads = fts.namedItem( "parallelThreads" ).toElement().text().toUInt();
      }
    }
  }

  c.lastMainGroupId  = root.namedItem( "lastMainGroupId" ).toElement().text().toUInt();
  c.lastPopupGroupId = root.namedItem( "lastPopupGroupId" ).toElement().text().toUInt();

  QDomNode popupWindowState = root.namedItem( "popupWindowState" );

  if ( !popupWindowState.isNull() ) {
    c.popupWindowState = QByteArray::fromBase64( popupWindowState.toElement().text().toLatin1() );
  }

  QDomNode popupWindowGeometry = root.namedItem( "popupWindowGeometry" );

  if ( !popupWindowGeometry.isNull() ) {
    c.popupWindowGeometry = QByteArray::fromBase64( popupWindowGeometry.toElement().text().toLatin1() );
  }

  c.pinPopupWindow = ( root.namedItem( "pinPopupWindow" ).toElement().text() == "1" );

  c.popupWindowAlwaysOnTop = ( root.namedItem( "popupWindowAlwaysOnTop" ).toElement().text() == "1" );

  QDomNode mainWindowState = root.namedItem( "mainWindowState" );

  if ( !mainWindowState.isNull() ) {
    c.mainWindowState = QByteArray::fromBase64( mainWindowState.toElement().text().toLatin1() );
  }

  QDomNode mainWindowGeometry = root.namedItem( "mainWindowGeometry" );

  if ( !mainWindowGeometry.isNull() ) {
    c.mainWindowGeometry = QByteArray::fromBase64( mainWindowGeometry.toElement().text().toLatin1() );
  }

  QDomNode dictInfoGeometry = root.namedItem( "dictInfoGeometry" );

  if ( !dictInfoGeometry.isNull() ) {
    c.dictInfoGeometry = QByteArray::fromBase64( dictInfoGeometry.toElement().text().toLatin1() );
  }

  QDomNode inspectorGeometry = root.namedItem( "inspectorGeometry" );

  if ( !inspectorGeometry.isNull() ) {
    c.inspectorGeometry = QByteArray::fromBase64( inspectorGeometry.toElement().text().toLatin1() );
  }

  QDomNode dictionariesDialogGeometry = root.namedItem( "dictionariesDialogGeometry" );

  if ( !dictionariesDialogGeometry.isNull() ) {
    c.dictionariesDialogGeometry = QByteArray::fromBase64( dictionariesDialogGeometry.toElement().text().toLatin1() );
  }

  QDomNode timeForNewReleaseCheck = root.namedItem( "timeForNewReleaseCheck" );

  if ( !timeForNewReleaseCheck.isNull() ) {
    c.timeForNewReleaseCheck = QDateTime::fromString( timeForNewReleaseCheck.toElement().text(), Qt::ISODate );
  }

  c.skippedRelease = root.namedItem( "skippedRelease" ).toElement().text();

  c.showingDictBarNames = ( root.namedItem( "showingDictBarNames" ).toElement().text() == "1" );

  QDomNode usingToolbarsIconSize = root.namedItem( "usingToolbarsIconSize" );
  if ( !usingToolbarsIconSize.isNull() ) {
    c.usingToolbarsIconSize = static_cast< ToolbarsIconSize >( usingToolbarsIconSize.toElement().text().toInt() );
  }

  if ( !root.namedItem( "historyExportPath" ).isNull() ) {
    c.historyExportPath = root.namedItem( "historyExportPath" ).toElement().text();
  }

  if ( !root.namedItem( "resourceSavePath" ).isNull() ) {
    c.resourceSavePath = root.namedItem( "resourceSavePath" ).toElement().text();
  }

  if ( !root.namedItem( "articleSavePath" ).isNull() ) {
    c.articleSavePath = root.namedItem( "articleSavePath" ).toElement().text();
  }

  if ( !root.namedItem( "maxHeadwordSize" ).isNull() ) {
    unsigned int value = root.namedItem( "maxHeadwordSize" ).toElement().text().toUInt();
    if ( value != 0 ) // 0 is invalid value for our purposes
    {
      c.maxHeadwordSize = value;
    }
  }

  if ( !root.namedItem( "maxHeadwordsToExpand" ).isNull() ) {
    c.maxHeadwordsToExpand = root.namedItem( "maxHeadwordsToExpand" ).toElement().text().toUInt();
  }

  QDomNode headwordsDialog = root.namedItem( "headwordsDialog" );

  if ( !headwordsDialog.isNull() ) {
    if ( !headwordsDialog.namedItem( "searchMode" ).isNull() ) {
      c.headwordsDialog.searchMode = headwordsDialog.namedItem( "searchMode" ).toElement().text().toInt();
    }

    if ( !headwordsDialog.namedItem( "matchCase" ).isNull() ) {
      c.headwordsDialog.matchCase = ( headwordsDialog.namedItem( "matchCase" ).toElement().text() == "1" );
    }

    if ( !headwordsDialog.namedItem( "autoApply" ).isNull() ) {
      c.headwordsDialog.autoApply = ( headwordsDialog.namedItem( "autoApply" ).toElement().text() == "1" );
    }

    if ( !headwordsDialog.namedItem( "headwordsExportPath" ).isNull() ) {
      c.headwordsDialog.headwordsExportPath = headwordsDialog.namedItem( "headwordsExportPath" ).toElement().text();
    }

    if ( !headwordsDialog.namedItem( "headwordsDialogGeometry" ).isNull() ) {
      c.headwordsDialog.headwordsDialogGeometry =
        QByteArray::fromBase64( headwordsDialog.namedItem( "headwordsDialogGeometry" ).toElement().text().toLatin1() );
    }
  }

  return c;
}

namespace {
void saveGroup( Group const & data, QDomElement & group )
{
  QDomDocument dd = group.ownerDocument();

  QDomAttr id = dd.createAttribute( "id" );

  id.setValue( QString::number( data.id ) );

  group.setAttributeNode( id );

  QDomAttr name = dd.createAttribute( "name" );

  name.setValue( data.name );

  group.setAttributeNode( name );

  if ( data.favoritesFolder.size() ) {
    QDomAttr folder = dd.createAttribute( "favoritesFolder" );
    folder.setValue( data.favoritesFolder );
    group.setAttributeNode( folder );
  }

  if ( data.icon.size() ) {
    QDomAttr icon = dd.createAttribute( "icon" );

    icon.setValue( data.icon );

    group.setAttributeNode( icon );
  }

  if ( data.iconData.size() ) {
    QDomAttr iconData = dd.createAttribute( "iconData" );

    iconData.setValue( QString::fromLatin1( data.iconData.toBase64() ) );

    group.setAttributeNode( iconData );
  }

  if ( !data.shortcut.isEmpty() ) {
    QDomAttr shortcut = dd.createAttribute( "shortcut" );

    shortcut.setValue( data.shortcut.toString() );

    group.setAttributeNode( shortcut );
  }

  for ( const auto & dictionarie : data.dictionaries ) {
    QDomElement dictionary = dd.createElement( "dictionary" );

    group.appendChild( dictionary );

    QDomText value = dd.createTextNode( dictionarie.id );

    dictionary.appendChild( value );

    QDomAttr name = dd.createAttribute( "name" );

    name.setValue( dictionarie.name );

    dictionary.setAttributeNode( name );
  }

  QDomElement muted = dd.createElement( "mutedDictionaries" );
  group.appendChild( muted );

  for ( const auto & mutedDictionarie : data.mutedDictionaries ) {
    QDomElement dict = dd.createElement( "mutedDictionary" );
    muted.appendChild( dict );

    QDomText value = dd.createTextNode( mutedDictionarie );
    dict.appendChild( value );
  }

  for ( const auto & popupMutedDictionarie : data.popupMutedDictionaries ) {
    QDomElement dict = dd.createElement( "popupMutedDictionary" );
    muted.appendChild( dict );

    QDomText value = dd.createTextNode( popupMutedDictionarie );
    dict.appendChild( value );
  }
}

} // namespace

void save( Class const & c )
{
  QSaveFile configFile( getConfigFileName() );

  if ( !configFile.open( QFile::WriteOnly ) ) {
    throw exCantWriteConfigFile();
  }

  QDomDocument dd;

  QDomElement root = dd.createElement( "config" );
  dd.appendChild( root );

  {
    QDomElement paths = dd.createElement( "paths" );
    root.appendChild( paths );

    // Save all paths except the hard-code portable path,
    // which is stored in the first element of list.
    qsizetype pos = Config::isPortableVersion();

    for ( const auto & i : c.paths.mid( pos ) ) {
      QDomElement path = dd.createElement( "path" );
      paths.appendChild( path );

      QDomAttr recursive = dd.createAttribute( "recursive" );
      recursive.setValue( i.recursive ? "1" : "0" );
      path.setAttributeNode( recursive );

      QDomText value = dd.createTextNode( i.path );

      path.appendChild( value );
    }
  }

  {
    QDomElement soundDirs = dd.createElement( "sounddirs" );
    root.appendChild( soundDirs );

    for ( const auto & i : c.soundDirs ) {
      QDomElement soundDir = dd.createElement( "sounddir" );
      soundDirs.appendChild( soundDir );

      QDomAttr name = dd.createAttribute( "name" );
      name.setValue( i.name );
      soundDir.setAttributeNode( name );

      QDomAttr icon = dd.createAttribute( "icon" );
      icon.setValue( i.iconFilename );
      soundDir.setAttributeNode( icon );

      QDomText value = dd.createTextNode( i.path );

      soundDir.appendChild( value );
    }
  }

  {
    QDomElement dictionaryOrder = dd.createElement( "dictionaryOrder" );
    root.appendChild( dictionaryOrder );
    saveGroup( c.dictionaryOrder, dictionaryOrder );
  }

  {
    QDomElement inactiveDictionaries = dd.createElement( "inactiveDictionaries" );
    root.appendChild( inactiveDictionaries );
    saveGroup( c.inactiveDictionaries, inactiveDictionaries );
  }

  {
    QDomElement groups = dd.createElement( "groups" );
    root.appendChild( groups );

    QDomAttr nextId = dd.createAttribute( "nextId" );
    nextId.setValue( QString::number( c.groups.nextId ) );
    groups.setAttributeNode( nextId );

    for ( const auto & i : c.groups ) {
      QDomElement group = dd.createElement( "group" );
      groups.appendChild( group );

      saveGroup( i, group );
    }
  }

  {
    QDomElement hunspell = dd.createElement( "hunspell" );
    QDomAttr path        = dd.createAttribute( "dictionariesPath" );
    path.setValue( c.hunspell.dictionariesPath );
    hunspell.setAttributeNode( path );
    root.appendChild( hunspell );

    for ( const auto & enabledDictionarie : c.hunspell.enabledDictionaries ) {
      QDomElement en = dd.createElement( "enabled" );
      QDomText value = dd.createTextNode( enabledDictionarie );

      en.appendChild( value );
      hunspell.appendChild( en );
    }
  }

  {
    // Russian translit

    QDomElement transliteration = dd.createElement( "transliteration" );
    root.appendChild( transliteration );

    QDomElement opt = dd.createElement( "enableRussianTransliteration" );
    opt.appendChild( dd.createTextNode( c.transliteration.enableRussianTransliteration ? "1" : "0" ) );
    transliteration.appendChild( opt );

    // German translit

    opt = dd.createElement( "enableGermanTransliteration" );
    opt.appendChild( dd.createTextNode( c.transliteration.enableGermanTransliteration ? "1" : "0" ) );
    transliteration.appendChild( opt );

    // Greek translit

    opt = dd.createElement( "enableGreekTransliteration" );
    opt.appendChild( dd.createTextNode( c.transliteration.enableGreekTransliteration ? "1" : "0" ) );
    transliteration.appendChild( opt );

    // Belarusian translit

    opt = dd.createElement( "enableBelarusianTransliteration" );
    opt.appendChild( dd.createTextNode( c.transliteration.enableBelarusianTransliteration ? "1" : "0" ) );
    transliteration.appendChild( opt );

#ifdef MAKE_CHINESE_CONVERSION_SUPPORT
    // Chinese

    QDomElement chinese = dd.createElement( "chinese" );
    transliteration.appendChild( chinese );

    opt = dd.createElement( "enable" );
    opt.appendChild( dd.createTextNode( c.transliteration.chinese.enable ? "1" : "0" ) );
    chinese.appendChild( opt );

    opt = dd.createElement( "enableSCToTWConversion" );
    opt.appendChild( dd.createTextNode( c.transliteration.chinese.enableSCToTWConversion ? "1" : "0" ) );
    chinese.appendChild( opt );

    opt = dd.createElement( "enableSCToHKConversion" );
    opt.appendChild( dd.createTextNode( c.transliteration.chinese.enableSCToHKConversion ? "1" : "0" ) );
    chinese.appendChild( opt );

    opt = dd.createElement( "enableTCToSCConversion" );
    opt.appendChild( dd.createTextNode( c.transliteration.chinese.enableTCToSCConversion ? "1" : "0" ) );
    chinese.appendChild( opt );
#endif

    // Romaji

    QDomElement romaji = dd.createElement( "romaji" );
    transliteration.appendChild( romaji );

    opt = dd.createElement( "enable" );
    opt.appendChild( dd.createTextNode( c.transliteration.romaji.enable ? "1" : "0" ) );
    romaji.appendChild( opt );

    opt = dd.createElement( "enableHepburn" );
    opt.appendChild( dd.createTextNode( c.transliteration.romaji.enableHepburn ? "1" : "0" ) );
    romaji.appendChild( opt );

    opt = dd.createElement( "enableHiragana" );
    opt.appendChild( dd.createTextNode( c.transliteration.romaji.enableHiragana ? "1" : "0" ) );
    romaji.appendChild( opt );

    opt = dd.createElement( "enableKatakana" );
    opt.appendChild( dd.createTextNode( c.transliteration.romaji.enableKatakana ? "1" : "0" ) );
    romaji.appendChild( opt );

    //custom transliteration
    QDomElement customtrans = dd.createElement( "customtrans" );
    transliteration.appendChild( customtrans );

    opt = dd.createElement( "enable" );
    opt.appendChild( dd.createTextNode( c.transliteration.customTrans.enable ? "1" : "0" ) );
    customtrans.appendChild( opt );

    opt = dd.createElement( "context" );
    opt.appendChild( dd.createTextNode( c.transliteration.customTrans.context ) );
    customtrans.appendChild( opt );
  }

  {
    // Lingua

    QDomElement lingua = dd.createElement( "lingua" );
    root.appendChild( lingua );

    QDomElement opt = dd.createElement( "enable" );
    opt.appendChild( dd.createTextNode( c.lingua.enable ? "1" : "0" ) );
    lingua.appendChild( opt );

    opt = dd.createElement( "languageCodes" );
    opt.appendChild( dd.createTextNode( c.lingua.languageCodes ) );
    lingua.appendChild( opt );
  }

  {
    // Forvo

    QDomElement forvo = dd.createElement( "forvo" );
    root.appendChild( forvo );

    QDomElement opt = dd.createElement( "enable" );
    opt.appendChild( dd.createTextNode( c.forvo.enable ? "1" : "0" ) );
    forvo.appendChild( opt );

    opt = dd.createElement( "apiKey" );
    opt.appendChild( dd.createTextNode( c.forvo.apiKey ) );
    forvo.appendChild( opt );

    opt = dd.createElement( "languageCodes" );
    opt.appendChild( dd.createTextNode( c.forvo.languageCodes ) );
    forvo.appendChild( opt );
  }

  {
    QDomElement mws = dd.createElement( "mediawikis" );
    root.appendChild( mws );

    for ( const auto & mediawiki : c.mediawikis ) {
      QDomElement mw = dd.createElement( "mediawiki" );
      mws.appendChild( mw );

      QDomAttr id = dd.createAttribute( "id" );
      id.setValue( mediawiki.id );
      mw.setAttributeNode( id );

      QDomAttr name = dd.createAttribute( "name" );
      name.setValue( mediawiki.name );
      mw.setAttributeNode( name );

      QDomAttr url = dd.createAttribute( "url" );
      url.setValue( mediawiki.url );
      mw.setAttributeNode( url );

      QDomAttr enabled = dd.createAttribute( "enabled" );
      enabled.setValue( mediawiki.enabled ? "1" : "0" );
      mw.setAttributeNode( enabled );

      QDomAttr icon = dd.createAttribute( "icon" );
      icon.setValue( mediawiki.icon );
      mw.setAttributeNode( icon );

      QDomAttr lang = dd.createAttribute( "lang" );
      lang.setValue( mediawiki.lang );
      mw.setAttributeNode( lang );
    }
  }

  {
    QDomElement wss = dd.createElement( "websites" );
    root.appendChild( wss );

    for ( const auto & webSite : c.webSites ) {
      QDomElement ws = dd.createElement( "website" );
      wss.appendChild( ws );

      QDomAttr id = dd.createAttribute( "id" );
      id.setValue( webSite.id );
      ws.setAttributeNode( id );

      QDomAttr name = dd.createAttribute( "name" );
      name.setValue( webSite.name );
      ws.setAttributeNode( name );

      QDomAttr url = dd.createAttribute( "url" );
      url.setValue( webSite.url );
      ws.setAttributeNode( url );

      QDomAttr enabled = dd.createAttribute( "enabled" );
      enabled.setValue( webSite.enabled ? "1" : "0" );
      ws.setAttributeNode( enabled );

      QDomAttr icon = dd.createAttribute( "icon" );
      icon.setValue( webSite.iconFilename );
      ws.setAttributeNode( icon );

      QDomAttr inside_iframe = dd.createAttribute( "inside_iframe" );
      inside_iframe.setValue( webSite.inside_iframe ? "1" : "0" );
      ws.setAttributeNode( inside_iframe );
    }
  }

  {
    QDomElement dss = dd.createElement( "dictservers" );
    root.appendChild( dss );

    for ( const auto & dictServer : c.dictServers ) {
      QDomElement ds = dd.createElement( "server" );
      dss.appendChild( ds );

      QDomAttr id = dd.createAttribute( "id" );
      id.setValue( dictServer.id );
      ds.setAttributeNode( id );

      QDomAttr name = dd.createAttribute( "name" );
      name.setValue( dictServer.name );
      ds.setAttributeNode( name );

      QDomAttr url = dd.createAttribute( "url" );
      url.setValue( dictServer.url );
      ds.setAttributeNode( url );

      QDomAttr enabled = dd.createAttribute( "enabled" );
      enabled.setValue( dictServer.enabled ? "1" : "0" );
      ds.setAttributeNode( enabled );

      QDomAttr databases = dd.createAttribute( "databases" );
      databases.setValue( dictServer.databases );
      ds.setAttributeNode( databases );

      QDomAttr strategies = dd.createAttribute( "strategies" );
      strategies.setValue( dictServer.strategies );
      ds.setAttributeNode( strategies );

      QDomAttr icon = dd.createAttribute( "icon" );
      icon.setValue( dictServer.iconFilename );
      ds.setAttributeNode( icon );
    }
  }

  {
    QDomElement programs = dd.createElement( "programs" );
    root.appendChild( programs );

    for ( const auto & program : c.programs ) {
      QDomElement p = dd.createElement( "program" );
      programs.appendChild( p );

      QDomAttr id = dd.createAttribute( "id" );
      id.setValue( program.id );
      p.setAttributeNode( id );

      QDomAttr name = dd.createAttribute( "name" );
      name.setValue( program.name );
      p.setAttributeNode( name );

      QDomAttr commandLine = dd.createAttribute( "commandLine" );
      commandLine.setValue( program.commandLine );
      p.setAttributeNode( commandLine );

      QDomAttr enabled = dd.createAttribute( "enabled" );
      enabled.setValue( program.enabled ? "1" : "0" );
      p.setAttributeNode( enabled );

      QDomAttr type = dd.createAttribute( "type" );
      type.setValue( QString::number( program.type ) );
      p.setAttributeNode( type );

      QDomAttr icon = dd.createAttribute( "icon" );
      icon.setValue( program.iconFilename );
      p.setAttributeNode( icon );
    }
  }
#ifdef TTS_SUPPORT
  {
    QDomNode ves = dd.createElement( "voiceEngines" );
    root.appendChild( ves );

    for ( const auto & voiceEngine : c.voiceEngines ) {
      QDomElement v = dd.createElement( "voiceEngine" );
      ves.appendChild( v );

      QDomAttr id = dd.createAttribute( "engine_name" );
      id.setValue( voiceEngine.engine_name );
      v.setAttributeNode( id );

      QDomAttr locale = dd.createAttribute( "locale" );
      locale.setValue( voiceEngine.locale.name() );
      v.setAttributeNode( locale );

      QDomAttr name = dd.createAttribute( "name" );
      name.setValue( voiceEngine.name );
      v.setAttributeNode( name );

      QDomAttr voice_name = dd.createAttribute( "voice_name" );
      voice_name.setValue( voiceEngine.voice_name );
      v.setAttributeNode( voice_name );

      QDomAttr enabled = dd.createAttribute( "enabled" );
      enabled.setValue( voiceEngine.enabled ? "1" : "0" );
      v.setAttributeNode( enabled );

      QDomAttr icon = dd.createAttribute( "icon" );
      icon.setValue( voiceEngine.iconFilename );
      v.setAttributeNode( icon );

      QDomAttr volume = dd.createAttribute( "volume" );
      volume.setValue( QString::number( voiceEngine.volume ) );
      v.setAttributeNode( volume );

      QDomAttr rate = dd.createAttribute( "rate" );
      rate.setValue( QString::number( voiceEngine.rate ) );
      v.setAttributeNode( rate );
    }
  }
#endif

  {
    QDomElement muted = dd.createElement( "mutedDictionaries" );
    root.appendChild( muted );
    saveMutedDictionaries( dd, muted, c.mutedDictionaries );
  }

  {
    QDomElement muted = dd.createElement( "popupMutedDictionaries" );
    root.appendChild( muted );
    saveMutedDictionaries( dd, muted, c.popupMutedDictionaries );
  }

  {
    QDomElement preferences = dd.createElement( "preferences" );
    root.appendChild( preferences );

    QDomElement opt = dd.createElement( "interfaceLanguage" );
    opt.appendChild( dd.createTextNode( c.preferences.interfaceLanguage ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "interfaceFont" );
    opt.appendChild( dd.createTextNode( c.preferences.interfaceFont ) );
    preferences.appendChild( opt );

    opt             = dd.createElement( "customFonts" );
    auto customFont = c.preferences.customFonts.toElement( dd );
    preferences.appendChild( customFont );

    opt = dd.createElement( "displayStyle" );
    opt.appendChild( dd.createTextNode( c.preferences.displayStyle ) );
    preferences.appendChild( opt );

#if !defined( Q_OS_WIN )
    opt = dd.createElement( "interfaceStyle" );
    opt.appendChild( dd.createTextNode( c.preferences.interfaceStyle ) );
    preferences.appendChild( opt );
#endif

    opt = dd.createElement( "newTabsOpenAfterCurrentOne" );
    opt.appendChild( dd.createTextNode( c.preferences.newTabsOpenAfterCurrentOne ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "newTabsOpenInBackground" );
    opt.appendChild( dd.createTextNode( c.preferences.newTabsOpenInBackground ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "hideSingleTab" );
    opt.appendChild( dd.createTextNode( c.preferences.hideSingleTab ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "mruTabOrder" );
    opt.appendChild( dd.createTextNode( c.preferences.mruTabOrder ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "hideMenubar" );
    opt.appendChild( dd.createTextNode( c.preferences.hideMenubar ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "enableTrayIcon" );
    opt.appendChild( dd.createTextNode( c.preferences.enableTrayIcon ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "startToTray" );
    opt.appendChild( dd.createTextNode( c.preferences.startToTray ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "closeToTray" );
    opt.appendChild( dd.createTextNode( c.preferences.closeToTray ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "autoStart" );
    opt.appendChild( dd.createTextNode( c.preferences.autoStart ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "doubleClickTranslates" );
    opt.appendChild( dd.createTextNode( c.preferences.doubleClickTranslates ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "selectWordBySingleClick" );
    opt.appendChild( dd.createTextNode( c.preferences.selectWordBySingleClick ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "autoScrollToTargetArticle" );
    opt.appendChild( dd.createTextNode( c.preferences.autoScrollToTargetArticle ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "escKeyHidesMainWindow" );
    opt.appendChild( dd.createTextNode( c.preferences.escKeyHidesMainWindow ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "darkMode" );
    opt.appendChild( dd.createTextNode( QString::number( static_cast< int >( c.preferences.darkMode ) ) ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "darkReaderMode" );
    opt.appendChild( dd.createTextNode( QString::number( static_cast< int >( c.preferences.darkReaderMode ) ) ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "zoomFactor" );
    opt.appendChild( dd.createTextNode( QString::number( c.preferences.zoomFactor ) ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "helpZoomFactor" );
    opt.appendChild( dd.createTextNode( QString::number( c.preferences.helpZoomFactor ) ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "wordsZoomLevel" );
    opt.appendChild( dd.createTextNode( QString::number( c.preferences.wordsZoomLevel ) ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "enableMainWindowHotkey" );
    opt.appendChild( dd.createTextNode( c.preferences.enableMainWindowHotkey ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "mainWindowHotkey" );
    opt.appendChild( dd.createTextNode( c.preferences.mainWindowHotkey.toString() ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "enableClipboardHotkey" );
    opt.appendChild( dd.createTextNode( c.preferences.enableClipboardHotkey ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "clipboardHotkey" );
    opt.appendChild( dd.createTextNode( c.preferences.clipboardHotkey.toString() ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "startWithScanPopupOn" );
    opt.appendChild( dd.createTextNode( c.preferences.startWithScanPopupOn ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "enableScanPopupModifiers" );
    opt.appendChild( dd.createTextNode( c.preferences.enableScanPopupModifiers ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "scanPopupModifiers" );
    opt.appendChild( dd.createTextNode( QString::number( c.preferences.scanPopupModifiers ) ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "ignoreOwnClipboardChanges" );
    opt.appendChild( dd.createTextNode( c.preferences.ignoreOwnClipboardChanges ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "scanToMainWindow" );
    opt.appendChild( dd.createTextNode( c.preferences.scanToMainWindow ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "ignoreDiacritics" );
    opt.appendChild( dd.createTextNode( c.preferences.ignoreDiacritics ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "ignorePunctuation" );
    opt.appendChild( dd.createTextNode( c.preferences.ignorePunctuation ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "sessionCollapse" );
    opt.appendChild( dd.createTextNode( c.preferences.sessionCollapse ? "1" : "0" ) );
    preferences.appendChild( opt );

#ifdef HAVE_X11
    opt = dd.createElement( "trackClipboardScan" );
    opt.appendChild( dd.createTextNode( c.preferences.trackClipboardScan ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "trackSelectionScan" );
    opt.appendChild( dd.createTextNode( c.preferences.trackSelectionScan ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "showScanFlag" );
    opt.appendChild( dd.createTextNode( c.preferences.showScanFlag ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "selectionChangeDelayTimer" );
    opt.appendChild( dd.createTextNode( QString::number( c.preferences.selectionChangeDelayTimer ) ) );
    preferences.appendChild( opt );

#endif

    opt = dd.createElement( "pronounceOnLoadMain" );
    opt.appendChild( dd.createTextNode( c.preferences.pronounceOnLoadMain ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "pronounceOnLoadPopup" );
    opt.appendChild( dd.createTextNode( c.preferences.pronounceOnLoadPopup ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "useInternalPlayer" );
    opt.appendChild( dd.createTextNode( c.preferences.useInternalPlayer ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "internalPlayerBackend" );
    opt.appendChild( dd.createTextNode( c.preferences.internalPlayerBackend.getName() ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "audioPlaybackProgram" );
    opt.appendChild( dd.createTextNode( c.preferences.audioPlaybackProgram ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "alwaysOnTop" );
    opt.appendChild( dd.createTextNode( c.preferences.alwaysOnTop ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "searchInDock" );
    opt.appendChild( dd.createTextNode( c.preferences.searchInDock ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "historyStoreInterval" );
    opt.appendChild( dd.createTextNode( QString::number( c.preferences.historyStoreInterval ) ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "favoritesStoreInterval" );
    opt.appendChild( dd.createTextNode( QString::number( c.preferences.favoritesStoreInterval ) ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "confirmFavoritesDeletion" );
    opt.appendChild( dd.createTextNode( c.preferences.confirmFavoritesDeletion ? "1" : "0" ) );
    preferences.appendChild( opt );

    {
      QDomElement proxy = dd.createElement( "proxyserver" );
      preferences.appendChild( proxy );

      QDomAttr enabled = dd.createAttribute( "enabled" );
      enabled.setValue( c.preferences.proxyServer.enabled ? "1" : "0" );
      proxy.setAttributeNode( enabled );

      QDomAttr useSystemProxy = dd.createAttribute( "useSystemProxy" );
      useSystemProxy.setValue( c.preferences.proxyServer.useSystemProxy ? "1" : "0" );
      proxy.setAttributeNode( useSystemProxy );

      opt = dd.createElement( "type" );
      opt.appendChild( dd.createTextNode( QString::number( c.preferences.proxyServer.type ) ) );
      proxy.appendChild( opt );

      opt = dd.createElement( "host" );
      opt.appendChild( dd.createTextNode( c.preferences.proxyServer.host ) );
      proxy.appendChild( opt );

      opt = dd.createElement( "port" );
      opt.appendChild( dd.createTextNode( QString::number( c.preferences.proxyServer.port ) ) );
      proxy.appendChild( opt );

      opt = dd.createElement( "user" );
      opt.appendChild( dd.createTextNode( c.preferences.proxyServer.user ) );
      proxy.appendChild( opt );

      opt = dd.createElement( "password" );
      opt.appendChild( dd.createTextNode( c.preferences.proxyServer.password ) );
      proxy.appendChild( opt );
    }

    //anki connect
    {
      QDomElement proxy = dd.createElement( "ankiConnectServer" );
      preferences.appendChild( proxy );

      QDomAttr enabled = dd.createAttribute( "enabled" );
      enabled.setValue( c.preferences.ankiConnectServer.enabled ? "1" : "0" );
      proxy.setAttributeNode( enabled );

      opt = dd.createElement( "host" );
      opt.appendChild( dd.createTextNode( c.preferences.ankiConnectServer.host ) );
      proxy.appendChild( opt );

      opt = dd.createElement( "port" );
      opt.appendChild( dd.createTextNode( QString::number( c.preferences.ankiConnectServer.port ) ) );
      proxy.appendChild( opt );

      opt = dd.createElement( "deck" );
      opt.appendChild( dd.createTextNode( c.preferences.ankiConnectServer.deck ) );
      proxy.appendChild( opt );

      opt = dd.createElement( "model" );
      opt.appendChild( dd.createTextNode( c.preferences.ankiConnectServer.model ) );
      proxy.appendChild( opt );

      opt = dd.createElement( "text" );
      opt.appendChild( dd.createTextNode( c.preferences.ankiConnectServer.text ) );
      proxy.appendChild( opt );

      opt = dd.createElement( "word" );
      opt.appendChild( dd.createTextNode( c.preferences.ankiConnectServer.word ) );
      proxy.appendChild( opt );

      opt = dd.createElement( "sentence" );
      opt.appendChild( dd.createTextNode( c.preferences.ankiConnectServer.sentence ) );
      proxy.appendChild( opt );
    }

    opt = dd.createElement( "checkForNewReleases" );
    opt.appendChild( dd.createTextNode( c.preferences.checkForNewReleases ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "disallowContentFromOtherSites" );
    opt.appendChild( dd.createTextNode( c.preferences.disallowContentFromOtherSites ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "hideGoldenDictHeader" );
    opt.appendChild( dd.createTextNode( c.preferences.hideGoldenDictHeader ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "maxNetworkCacheSize" );
    opt.appendChild( dd.createTextNode( QString::number( c.preferences.maxNetworkCacheSize ) ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "clearNetworkCacheOnExit" );
    opt.appendChild( dd.createTextNode( c.preferences.clearNetworkCacheOnExit ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "removeInvalidIndexOnExit" );
    opt.appendChild( dd.createTextNode( c.preferences.removeInvalidIndexOnExit ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "enableApplicationLog" );
    opt.appendChild( dd.createTextNode( c.preferences.enableApplicationLog ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "maxStringsInHistory" );
    opt.appendChild( dd.createTextNode( QString::number( c.preferences.maxStringsInHistory ) ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "storeHistory" );
    opt.appendChild( dd.createTextNode( QString::number( c.preferences.storeHistory ) ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "alwaysExpandOptionalParts" );
    opt.appendChild( dd.createTextNode( QString::number( c.preferences.alwaysExpandOptionalParts ) ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "addonStyle" );
    opt.appendChild( dd.createTextNode( c.preferences.addonStyle ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "collapseBigArticles" );
    opt.appendChild( dd.createTextNode( c.preferences.collapseBigArticles ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "articleSizeLimit" );
    opt.appendChild( dd.createTextNode( QString::number( c.preferences.articleSizeLimit ) ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "limitInputPhraseLength" );
    opt.appendChild( dd.createTextNode( c.preferences.limitInputPhraseLength ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "inputPhraseLengthLimit" );
    opt.appendChild( dd.createTextNode( QString::number( c.preferences.inputPhraseLengthLimit ) ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "maxDictionaryRefsInContextMenu" );
    opt.appendChild( dd.createTextNode( QString::number( c.preferences.maxDictionaryRefsInContextMenu ) ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "synonymSearchEnabled" );
    opt.appendChild( dd.createTextNode( c.preferences.synonymSearchEnabled ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "stripClipboard" );
    opt.appendChild( dd.createTextNode( c.preferences.stripClipboard ? "1" : "0" ) );
    preferences.appendChild( opt );

    opt = dd.createElement( "raiseWindowOnSearch" );
    opt.appendChild( dd.createTextNode( c.preferences.raiseWindowOnSearch ? "1" : "0" ) );
    preferences.appendChild( opt );

    {
      QDomNode hd = dd.createElement( "fullTextSearch" );
      preferences.appendChild( hd );

      QDomElement opt = dd.createElement( "searchMode" );
      opt.appendChild( dd.createTextNode( QString::number( c.preferences.fts.searchMode ) ) );
      hd.appendChild( opt );

      opt = dd.createElement( "dialogGeometry" );
      opt.appendChild( dd.createTextNode( QString::fromLatin1( c.preferences.fts.dialogGeometry.toBase64() ) ) );
      hd.appendChild( opt );

      opt = dd.createElement( "disabledTypes" );
      opt.appendChild( dd.createTextNode( c.preferences.fts.disabledTypes ) );
      hd.appendChild( opt );

      opt = dd.createElement( "enabled" );
      opt.appendChild( dd.createTextNode( c.preferences.fts.enabled ? "1" : "0" ) );
      hd.appendChild( opt );

      opt = dd.createElement( "maxDictionarySize" );
      opt.appendChild( dd.createTextNode( QString::number( c.preferences.fts.maxDictionarySize ) ) );
      hd.appendChild( opt );

      opt = dd.createElement( "parallelThreads" );
      opt.appendChild( dd.createTextNode( QString::number( c.preferences.fts.parallelThreads ) ) );
      hd.appendChild( opt );
    }
  }

  {
    QDomElement opt = dd.createElement( "lastMainGroupId" );
    opt.appendChild( dd.createTextNode( QString::number( c.lastMainGroupId ) ) );
    root.appendChild( opt );

    opt = dd.createElement( "lastPopupGroupId" );
    opt.appendChild( dd.createTextNode( QString::number( c.lastPopupGroupId ) ) );
    root.appendChild( opt );

    opt = dd.createElement( "popupWindowState" );
    opt.appendChild( dd.createTextNode( QString::fromLatin1( c.popupWindowState.toBase64() ) ) );
    root.appendChild( opt );

    opt = dd.createElement( "popupWindowGeometry" );
    opt.appendChild( dd.createTextNode( QString::fromLatin1( c.popupWindowGeometry.toBase64() ) ) );
    root.appendChild( opt );

    opt = dd.createElement( "pinPopupWindow" );
    opt.appendChild( dd.createTextNode( c.pinPopupWindow ? "1" : "0" ) );
    root.appendChild( opt );

    opt = dd.createElement( "popupWindowAlwaysOnTop" );
    opt.appendChild( dd.createTextNode( c.popupWindowAlwaysOnTop ? "1" : "0" ) );
    root.appendChild( opt );

    opt = dd.createElement( "mainWindowState" );
    opt.appendChild( dd.createTextNode( QString::fromLatin1( c.mainWindowState.toBase64() ) ) );
    root.appendChild( opt );

    opt = dd.createElement( "mainWindowGeometry" );
    opt.appendChild( dd.createTextNode( QString::fromLatin1( c.mainWindowGeometry.toBase64() ) ) );
    root.appendChild( opt );

    opt = dd.createElement( "dictInfoGeometry" );
    opt.appendChild( dd.createTextNode( QString::fromLatin1( c.dictInfoGeometry.toBase64() ) ) );
    root.appendChild( opt );

    opt = dd.createElement( "inspectorGeometry" );
    opt.appendChild( dd.createTextNode( QString::fromLatin1( c.inspectorGeometry.toBase64() ) ) );
    root.appendChild( opt );

    opt = dd.createElement( "dictionariesDialogGeometry" );
    opt.appendChild( dd.createTextNode( QString::fromLatin1( c.dictionariesDialogGeometry.toBase64() ) ) );
    root.appendChild( opt );

    opt = dd.createElement( "timeForNewReleaseCheck" );
    opt.appendChild( dd.createTextNode( c.timeForNewReleaseCheck.toString( Qt::ISODate ) ) );
    root.appendChild( opt );

    opt = dd.createElement( "skippedRelease" );
    opt.appendChild( dd.createTextNode( c.skippedRelease ) );
    root.appendChild( opt );

    opt = dd.createElement( "showingDictBarNames" );
    opt.appendChild( dd.createTextNode( c.showingDictBarNames ? "1" : "0" ) );
    root.appendChild( opt );

    opt = dd.createElement( "usingToolbarsIconSize" );
    opt.appendChild( dd.createTextNode( QString::number( static_cast< int >( c.usingToolbarsIconSize ) ) ) );
    root.appendChild( opt );

    if ( !c.historyExportPath.isEmpty() ) {
      opt = dd.createElement( "historyExportPath" );
      opt.appendChild( dd.createTextNode( c.historyExportPath ) );
      root.appendChild( opt );
    }

    if ( !c.resourceSavePath.isEmpty() ) {
      opt = dd.createElement( "resourceSavePath" );
      opt.appendChild( dd.createTextNode( c.resourceSavePath ) );
      root.appendChild( opt );
    }

    if ( !c.articleSavePath.isEmpty() ) {
      opt = dd.createElement( "articleSavePath" );
      opt.appendChild( dd.createTextNode( c.articleSavePath ) );
      root.appendChild( opt );
    }

    opt = dd.createElement( "maxHeadwordSize" );
    opt.appendChild( dd.createTextNode( QString::number( c.maxHeadwordSize ) ) );
    root.appendChild( opt );

    opt = dd.createElement( "maxHeadwordsToExpand" );
    opt.appendChild( dd.createTextNode( QString::number( c.maxHeadwordsToExpand ) ) );
    root.appendChild( opt );
  }

  {
    QDomNode hd = dd.createElement( "headwordsDialog" );
    root.appendChild( hd );

    QDomElement opt = dd.createElement( "searchMode" );
    opt.appendChild( dd.createTextNode( QString::number( c.headwordsDialog.searchMode ) ) );
    hd.appendChild( opt );

    opt = dd.createElement( "matchCase" );
    opt.appendChild( dd.createTextNode( c.headwordsDialog.matchCase ? "1" : "0" ) );
    hd.appendChild( opt );

    opt = dd.createElement( "autoApply" );
    opt.appendChild( dd.createTextNode( c.headwordsDialog.autoApply ? "1" : "0" ) );
    hd.appendChild( opt );

    opt = dd.createElement( "headwordsExportPath" );
    opt.appendChild( dd.createTextNode( c.headwordsDialog.headwordsExportPath ) );
    hd.appendChild( opt );

    opt = dd.createElement( "headwordsDialogGeometry" );
    opt.appendChild( dd.createTextNode( QString::fromLatin1( c.headwordsDialog.headwordsDialogGeometry.toBase64() ) ) );
    hd.appendChild( opt );
  }

  configFile.write( dd.toByteArray() );
  if ( !configFile.commit() ) {
    throw exCantWriteConfigFile();
  }
}

QString getConfigFileName()
{
  return getHomeDir().absoluteFilePath( "config" );
}

QString getConfigDir()
{
  return getHomeDir().path() + QDir::separator();
}

QString getIndexDir()
{
  QDir result = getHomeDir();

#ifdef XDG_BASE_DIRECTORY_COMPLIANCE
  // store index in XDG_CACHE_HOME in non-portable version
  // *and* when an old index directory in GoldenDict home doesn't exist
  if ( !isPortableVersion() && !result.exists( "index" ) ) {
    result.setPath( getCacheDir() );
  }
#endif

  result.mkpath( "index" );

  if ( !result.cd( "index" ) ) {
    throw exCantUseIndexDir();
  }

  return result.path() + QDir::separator();
}

QString getPidFileName()
{
  return getHomeDir().filePath( "pid" );
}

QString getHistoryFileName()
{
  QString homeHistoryPath = getHomeDir().filePath( "history" );

#ifdef XDG_BASE_DIRECTORY_COMPLIANCE
  // use separate data dir for history, if it is not already stored alongside
  // configuration in non-portable mode
  if ( !isPortableVersion() && !QFile::exists( homeHistoryPath ) ) {
    return getDataDir().filePath( "history" );
  }
#endif

  return homeHistoryPath;
}

QString getFavoritiesFileName()
{
  return getHomeDir().filePath( "favorites" );
}

QString getUserCssFileName()
{
  return getHomeDir().filePath( "article-style.css" );
}

std::optional< std::string > getUserJsFileName()
{
  QString userJsPath = getHomeDir().filePath( "article-script.js" );
  if ( QFileInfo::exists( userJsPath ) ) {
    return userJsPath.toStdString();
  }
  else {
    return std::nullopt;
  }
}

QString getUserCssPrintFileName()
{
  return getHomeDir().filePath( "article-style-print.css" );
}

QString getUserQtCssFileName()
{
  return getHomeDir().filePath( "qt-style.css" );
}

QString getProgramDataDir() noexcept
{
  if ( isPortableVersion() ) {
    return QCoreApplication::applicationDirPath();
  }
  // TODO: rewrite this in QStandardPaths::AppDataLocation
#ifdef PROGRAM_DATA_DIR
  return PROGRAM_DATA_DIR;
#else
  return QCoreApplication::applicationDirPath();
#endif
}

QString getLocDir() noexcept
{
  if ( QDir( getProgramDataDir() ).cd( "locale" ) ) {
    return getProgramDataDir() + "/locale";
  }
  else {
    return QCoreApplication::applicationDirPath() + "/locale";
  }
}

QString getHelpDir() noexcept
{
  if ( QDir( getProgramDataDir() ).cd( "help" ) ) {
    return getProgramDataDir() + "/help";
  }
  else {
    return QCoreApplication::applicationDirPath() + "/help";
  }
}

#ifdef MAKE_CHINESE_CONVERSION_SUPPORT
QString getOpenCCDir() noexcept
{
  #if defined( Q_OS_WIN )
  if ( QDir( "opencc" ).exists() )
    return "opencc";
  else
    return QCoreApplication::applicationDirPath() + "/opencc";
  #elif defined( Q_OS_MAC )
  QString path = QCoreApplication::applicationDirPath() + "/opencc";
  if ( QDir( path ).exists() ) {
    return path;
  }

  return QString();
  #else
  return QString();
  #endif
}
#endif

bool isPortableVersion() noexcept
{
  struct IsPortable
  {
    bool isPortable;

    IsPortable():
      isPortable( QFileInfo( portableHomeDirPath() ).isDir() )
    {
    }
  };

  static IsPortable p;

  return p.isPortable;
}

QString getPortableVersionDictionaryDir() noexcept
{
  if ( isPortableVersion() ) {
    return getProgramDataDir() + "/content";
  }
  else {
    return QString();
  }
}

QString getPortableVersionMorphoDir() noexcept
{
  if ( isPortableVersion() ) {
    return getPortableVersionDictionaryDir() + "/morphology";
  }
  else {
    return QString();
  }
}

QString getStylesDir()
{
  QDir result = getHomeDir();

  result.mkpath( "styles" );

  if ( !result.cd( "styles" ) ) {
    return QString();
  }

  return result.path() + QDir::separator();
}

QString getCacheDir() noexcept
{
  return isPortableVersion() ? portableHomeDirPath() + "/cache"
#ifdef HAVE_X11
                               :
                               QStandardPaths::writableLocation( QStandardPaths::GenericCacheLocation ) + "/goldendict";
#else
                               :
                               QStandardPaths::writableLocation( QStandardPaths::CacheLocation );
#endif
}
} // namespace Config
