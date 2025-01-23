/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "audio/internalplayerbackend.hh"
#include "ex.hh"
#include <QDateTime>
#include <QDomDocument>
#include <QKeySequence>
#include <QList>
#include <QLocale>
#include <QMetaType>
#include <QObject>
#include <QSet>
#include <QSize>
#include <QString>
#include <QThread>
#include <optional>

/// Special group IDs
enum GroupId : unsigned {
  AllGroupId  = UINT_MAX - 1, /// The 'All' group
  HelpGroupId = UINT_MAX,     /// The fictitious 'Help' group
  NoGroupId   = 0,            /// Invalid value, used to specify that no group id is specified at all.
};

/// GoldenDict's configuration
namespace Config {

// Tri states enum for Dark and Dark reader mode
enum class Dark : std::uint8_t {
  Off = 0,
  On  = 1,
  Auto = 2,
};

/// Dictionaries which are temporarily disabled via the dictionary bar.
using MutedDictionaries = QSet< QString >;

/// A path where to search for the dictionaries
struct Path
{
  QString path;
  bool recursive;

  Path():
    recursive( false )
  {
  }
  Path( QString const & path_, bool recursive_ ):
    path( path_ ),
    recursive( recursive_ )
  {
  }

  bool operator==( Path const & other ) const
  {
    return path == other.path && recursive == other.recursive;
  }
};

/// A list of paths where to search for the dictionaries
using Paths = QList< Path >;

/// A directory holding bunches of audiofiles, which is indexed into a separate
/// dictionary.
struct SoundDir
{
  QString path, name;
  QString iconFilename;

  SoundDir() {}

  SoundDir( QString const & path_, QString const & name_, QString iconFilename_ = "" ):
    path( path_ ),
    name( name_ ),
    iconFilename( iconFilename_ )
  {
  }

  bool operator==( SoundDir const & other ) const
  {
    return path == other.path && name == other.name && iconFilename == other.iconFilename;
  }
};

/// A list of SoundDirs
using SoundDirs = QList< SoundDir >;

struct DictionaryRef
{
  QString id;   // Dictionrary id, which is usually an md5 hash
  QString name; // Dictionary name, used to recover when its id changes

  DictionaryRef() {}

  DictionaryRef( QString const & id_, QString const & name_ ):
    id( id_ ),
    name( name_ )
  {
  }

  bool operator==( DictionaryRef const & other ) const
  {
    return id == other.id && name == other.name;
  }
};

/// A dictionary group
struct Group
{
  unsigned id;
  QString name, icon;
  QByteArray iconData;
  QKeySequence shortcut;
  QString favoritesFolder;
  QList< DictionaryRef > dictionaries;
  Config::MutedDictionaries mutedDictionaries;      // Disabled via dictionary bar
  Config::MutedDictionaries popupMutedDictionaries; // Disabled via dictionary bar in popup

  Group():
    id( 0 )
  {
  }

  bool operator==( Group const & other ) const
  {
    return id == other.id && name == other.name && icon == other.icon && favoritesFolder == other.favoritesFolder
      && dictionaries == other.dictionaries && shortcut == other.shortcut
      && mutedDictionaries == other.mutedDictionaries && popupMutedDictionaries == other.popupMutedDictionaries
      && iconData == other.iconData;
  }

  bool operator!=( Group const & other ) const
  {
    return !operator==( other );
  }
};

/// All the groups
struct Groups: public QList< Group >
{
  unsigned nextId; // Id to use to create the group next time

  Groups():
    nextId( 1 )
  {
  }
};

/// Proxy server configuration
struct ProxyServer
{
  bool enabled;
  bool useSystemProxy;

  enum Type {
    Socks5 = 0,
    HttpConnect,
    HttpGet
  } type;

  QString host;
  unsigned port;
  QString user, password;

  ProxyServer();
};

struct AnkiConnectServer
{
  bool enabled;

  QString host;
  int port; // Port will be passed to QUrl::setPort() which expects an int.

  QString deck;
  QString model;

  QString word;
  QString text;
  QString sentence;

  AnkiConnectServer();
};

// A hotkey -- currently qt modifiers plus one or two keys
struct HotKey
{
  Qt::KeyboardModifiers modifiers;
  int key1, key2;

  /// Hotkey's constructor, take a QKeySequence's first two keys
  /// 1st key's modifier will be the `modifiers` above
  /// 1st key without modifier will becomes `key1`
  /// 2nd key without modifier will becomes `key2`
  /// The relation between the int and qt's KeyCode should consult qt's doc
  HotKey( QKeySequence const & );

  QKeySequence toKeySequence() const;
};

struct FullTextSearch
{
  int searchMode;
  bool enabled;

  quint32 maxDictionarySize;
  quint32 parallelThreads = QThread::idealThreadCount() / 3 + 1;
  QByteArray dialogGeometry;
  QString disabledTypes;

  FullTextSearch():
    searchMode( 0 ),
    enabled( true ),
    maxDictionarySize( 0 )
  {
  }
};

struct CustomFonts
{
  QString standard;
  QString serif;
  QString sansSerif;
  QString monospace;

  bool operator==( CustomFonts const & other ) const
  {
    return standard == other.standard && serif == other.serif && sansSerif == other.sansSerif
      && monospace == other.monospace;
  }

  bool operator!=( CustomFonts const & other ) const
  {
    return !operator==( other );
  }

  QDomElement toElement( QDomDocument dd ) const
  {
    QDomElement proxy = dd.createElement( "customFonts" );

    QDomAttr standard_attr = dd.createAttribute( "standard" );
    standard_attr.setValue( this->standard );
    proxy.setAttributeNode( standard_attr );

    QDomAttr serif_attr = dd.createAttribute( "serif" );
    serif_attr.setValue( this->serif );
    proxy.setAttributeNode( serif_attr );

    QDomAttr sans_attr = dd.createAttribute( "sans-serif" );
    sans_attr.setValue( this->sansSerif );
    proxy.setAttributeNode( sans_attr );
    QDomAttr mono_attr = dd.createAttribute( "monospace" );
    mono_attr.setValue( this->monospace );
    proxy.setAttributeNode( mono_attr );
    return proxy;
  }

  static CustomFonts fromElement( QDomElement proxy )
  {
    CustomFonts c;
    c.standard  = proxy.attribute( "standard" );
    c.serif     = proxy.attribute( "serif" );
    c.sansSerif = proxy.attribute( "sans-serif" );
    c.monospace = proxy.attribute( "monospace" );
    return c;
  }
};

/// Various user preferences
struct Preferences
{
  QString interfaceLanguage; // Empty value corresponds to system default
  QString interfaceFont;     //Empty as default value.

  CustomFonts customFonts;
  bool newTabsOpenAfterCurrentOne;
  bool newTabsOpenInBackground;
  bool hideSingleTab;
  bool mruTabOrder;
  bool hideMenubar;

#ifdef Q_OS_MACOS // macOS uses the dock menu instead of the tray icon
  bool closeToTray    = false;
  bool enableTrayIcon = false;
  bool startToTray    = false;
#else
  bool enableTrayIcon = true;
  bool closeToTray    = true;
  bool startToTray    = false;
#endif

  bool autoStart;
  bool doubleClickTranslates;
  bool selectWordBySingleClick;
  bool autoScrollToTargetArticle;
  bool escKeyHidesMainWindow;
  bool alwaysOnTop;

  /// An old UI mode when tranlateLine and wordList
  /// are in the dockable side panel, not on the toolbar.
  bool searchInDock;

  bool enableMainWindowHotkey;
  bool enableClipboardHotkey;

  QKeySequence mainWindowHotkey;
  QKeySequence clipboardHotkey;

  bool startWithScanPopupOn;
  bool enableScanPopupModifiers;
  unsigned long scanPopupModifiers; // Combination of KeyboardState::Modifier
  bool ignoreOwnClipboardChanges;

  bool scanToMainWindow;
  bool ignoreDiacritics;
  bool ignorePunctuation;
  bool sessionCollapse = false;
#ifdef HAVE_X11
  bool trackClipboardScan;
  bool trackSelectionScan;
  bool showScanFlag;
  int selectionChangeDelayTimer;
#endif

  // Whether the word should be pronounced on page load, in main window/popup
  bool pronounceOnLoadMain, pronounceOnLoadPopup;
  bool useInternalPlayer;
  InternalPlayerBackend internalPlayerBackend{};
  QString audioPlaybackProgram;

  ProxyServer proxyServer;
  AnkiConnectServer ankiConnectServer;

  bool checkForNewReleases;
  bool disallowContentFromOtherSites;
  bool hideGoldenDictHeader;
  int maxNetworkCacheSize;
  bool clearNetworkCacheOnExit;
  bool removeInvalidIndexOnExit = false;
  bool enableApplicationLog     = false;

  qreal zoomFactor;
  qreal helpZoomFactor;
  int wordsZoomLevel;

  unsigned maxStringsInHistory;
  unsigned storeHistory;
  bool alwaysExpandOptionalParts;

  unsigned historyStoreInterval   = 15; // unit is minutes
  unsigned favoritesStoreInterval = 15;

  bool confirmFavoritesDeletion;

  bool collapseBigArticles;
  int articleSizeLimit;

  bool limitInputPhraseLength;
  int inputPhraseLengthLimit;
  QString sanitizeInputPhrase( QString const & inputWord ) const;

  unsigned short maxDictionaryRefsInContextMenu;

  bool synonymSearchEnabled;
  bool stripClipboard;
  bool raiseWindowOnSearch;

  FullTextSearch fts;

  // Appearances

  Dark darkMode       = Dark::Off;
  Dark darkReaderMode =
#if defined( Q_OS_MACOS )
    Dark::Auto;
#else
    Dark::Off;
#endif

  QString addonStyle;
  QString displayStyle; // Article Display style (Which also affect interface style on windows)

#if !defined( Q_OS_WIN )
  // QApplication style https://doc.qt.io/qt-6/qapplication.html#setStyle
  // In addition to Qt's styles, "Default" is added as default.
  QString interfaceStyle;
#endif

  Preferences();
};

/// A MediaWiki network dictionary definition
struct MediaWiki
{
  QString id, name, url;
  bool enabled;
  QString icon;
  QString lang;

  MediaWiki():
    enabled( false )
  {
  }

  MediaWiki( QString const & id_,
             QString const & name_,
             QString const & url_,
             bool enabled_,
             QString const & icon_,
             QString const & lang_ = "" ):
    id( id_ ),
    name( name_ ),
    url( url_ ),
    enabled( enabled_ ),
    icon( icon_ ),
    lang( lang_ )
  {
  }

  bool operator==( MediaWiki const & other ) const
  {
    return id == other.id && name == other.name && url == other.url && enabled == other.enabled && icon == other.icon
      && lang == other.lang;
  }
};

/// Any website which can be queried though a simple template substitution
struct WebSite
{
  QString id, name, url;
  bool enabled;
  QString iconFilename;
  bool inside_iframe = false;

  WebSite():
    enabled( false )
  {
  }

  WebSite( QString const & id_,
           QString const & name_,
           QString const & url_,
           bool enabled_,
           QString const & iconFilename_,
           bool inside_iframe_ ):
    id( id_ ),
    name( name_ ),
    url( url_ ),
    enabled( enabled_ ),
    iconFilename( iconFilename_ ),
    inside_iframe( inside_iframe_ )
  {
  }

  bool operator==( WebSite const & other ) const
  {
    return id == other.id && name == other.name && url == other.url && enabled == other.enabled
      && iconFilename == other.iconFilename && inside_iframe == other.inside_iframe;
  }
};

/// All the WebSites
using WebSites = QList< WebSite >;

/// Any DICT server
struct DictServer
{
  QString id, name, url;
  bool enabled;
  QString databases;
  QString strategies;
  QString iconFilename;

  DictServer():
    enabled( false )
  {
  }

  DictServer( QString const & id_,
              QString const & name_,
              QString const & url_,
              bool enabled_,
              QString const & databases_,
              QString const & strategies_,
              QString const & iconFilename_ ):
    id( id_ ),
    name( name_ ),
    url( url_ ),
    enabled( enabled_ ),
    databases( databases_ ),
    strategies( strategies_ ),
    iconFilename( iconFilename_ )
  {
  }

  bool operator==( DictServer const & other ) const
  {
    return id == other.id && name == other.name && url == other.url && enabled == other.enabled
      && databases == other.databases && strategies == other.strategies && iconFilename == other.iconFilename;
  }
};

/// All the DictServers
using DictServers = QList< DictServer >;

/// Hunspell configuration
struct Hunspell
{
  QString dictionariesPath;

  using Dictionaries = QList< QString >;

  Dictionaries enabledDictionaries;

  bool operator==( Hunspell const & other ) const
  {
    return dictionariesPath == other.dictionariesPath && enabledDictionaries == other.enabledDictionaries;
  }

  bool operator!=( Hunspell const & other ) const
  {
    return !operator==( other );
  }
};

/// All the MediaWikis
using MediaWikis = QList< MediaWiki >;


/// Chinese transliteration configuration
struct Chinese
{
  bool enable;

  bool enableSCToTWConversion;
  bool enableSCToHKConversion;
  bool enableTCToSCConversion;

  Chinese();

  bool operator==( Chinese const & other ) const
  {
    return enable == other.enable && enableSCToTWConversion == other.enableSCToTWConversion
      && enableSCToHKConversion == other.enableSCToHKConversion
      && enableTCToSCConversion == other.enableTCToSCConversion;
  }

  bool operator!=( Chinese const & other ) const
  {
    return !operator==( other );
  }
};


struct CustomTrans
{
  bool enable = false;

  QString context;

  bool operator==( CustomTrans const & other ) const
  {
    return enable == other.enable && context == other.context;
  }

  bool operator!=( CustomTrans const & other ) const
  {
    return !operator==( other );
  }
};

/// Romaji transliteration configuration
struct Romaji
{
  bool enable;

  bool enableHepburn;
  bool enableHiragana;
  bool enableKatakana;

  Romaji();

  bool operator==( Romaji const & other ) const
  {
    return enable == other.enable && enableHepburn == other.enableHepburn && enableHiragana == other.enableHiragana
      && enableKatakana == other.enableKatakana;
  }

  bool operator!=( Romaji const & other ) const
  {
    return !operator==( other );
  }
};

struct Transliteration
{
  bool enableRussianTransliteration;
  bool enableGermanTransliteration;
  bool enableGreekTransliteration;
  bool enableBelarusianTransliteration;

  CustomTrans customTrans;
#ifdef MAKE_CHINESE_CONVERSION_SUPPORT
  Chinese chinese;
#endif
  Romaji romaji;

  bool operator==( Transliteration const & other ) const
  {
    return enableRussianTransliteration == other.enableRussianTransliteration
      && enableGermanTransliteration == other.enableGermanTransliteration
      && enableGreekTransliteration == other.enableGreekTransliteration
      && enableBelarusianTransliteration == other.enableBelarusianTransliteration && customTrans == other.customTrans &&
#ifdef MAKE_CHINESE_CONVERSION_SUPPORT
      chinese == other.chinese &&
#endif
      romaji == other.romaji;
  }

  bool operator!=( Transliteration const & other ) const
  {
    return !operator==( other );
  }

  Transliteration():
    enableRussianTransliteration( false ),
    enableGermanTransliteration( false ),
    enableGreekTransliteration( false ),
    enableBelarusianTransliteration( false )
  {
  }
};

struct Lingua
{
  bool enable = false;
  QString languageCodes;

  bool operator==( Lingua const & other ) const
  {
    return enable == other.enable && languageCodes == other.languageCodes;
  }

  bool operator!=( Lingua const & other ) const
  {
    return !operator==( other );
  }
};

struct Forvo
{
  bool enable;
  QString apiKey;
  QString languageCodes;

  Forvo():
    enable( false )
  {
  }

  bool operator==( Forvo const & other ) const
  {
    return enable == other.enable && apiKey == other.apiKey && languageCodes == other.languageCodes;
  }

  bool operator!=( Forvo const & other ) const
  {
    return !operator==( other );
  }
};

struct Program
{
  bool enabled;
  // NOTE: the value of this enum is used for config
  enum Type {
    Invalid      = -1, // Init value
    Audio        = 0,
    PlainText    = 1,
    Html         = 2,
    PrefixMatch  = 3,
    MaxTypeValue = 4
  };
  Type type = Invalid;
  QString id, name, commandLine;
  QString iconFilename;

  Program():
    enabled( false )
  {
  }

  Program( bool enabled_,
           Type type_,
           QString const & id_,
           QString const & name_,
           QString const & commandLine_,
           QString const & iconFilename_ ):
    enabled( enabled_ ),
    type( type_ ),
    id( id_ ),
    name( name_ ),
    commandLine( commandLine_ ),
    iconFilename( iconFilename_ )
  {
  }

  bool operator==( Program const & other ) const
  {
    return enabled == other.enabled && type == other.type && name == other.name && commandLine == other.commandLine
      && iconFilename == other.iconFilename;
  }

  bool operator!=( Program const & other ) const
  {
    return !operator==( other );
  }
};

using Programs = QList< Program >;

#ifdef TTS_SUPPORT
struct VoiceEngine
{
  bool enabled;
  //engine name.
  QString engine_name;
  QString name;
  //voice name.
  QString voice_name;
  QString iconFilename;
  QLocale locale;
  int volume; // 0~1 allowed
  int rate;   // -1 ~ 1 allowed

  VoiceEngine():
    enabled( false ),
    volume( 50 ),
    rate( 0 )
  {
  }
  VoiceEngine( QString engine_nane_, QString name_, QString voice_name_, QLocale locale_, int volume_, int rate_ ):
    enabled( false ),
    engine_name( engine_nane_ ),
    name( name_ ),
    voice_name( voice_name_ ),
    locale( locale_ ),
    volume( volume_ ),
    rate( rate_ )
  {
  }

  bool operator==( VoiceEngine const & other ) const
  {
    return enabled == other.enabled && engine_name == other.engine_name && name == other.name
      && voice_name == other.voice_name && locale == other.locale && iconFilename == other.iconFilename
      && volume == other.volume && rate == other.rate;
  }

  bool operator!=( VoiceEngine const & other ) const
  {
    return !operator==( other );
  }
};

using VoiceEngines = QList< VoiceEngine >;
#endif

struct HeadwordsDialog
{
  int searchMode;
  bool matchCase;
  bool autoApply;
  QString headwordsExportPath;
  QByteArray headwordsDialogGeometry;

  HeadwordsDialog():
    searchMode( 0 ),
    matchCase( false ),
    autoApply( false )
  {
  }
};

// TODO: arbitrary sizing
enum class ToolbarsIconSize : std::uint8_t {
  Small  = 0,
  Normal = 1,
  Large  = 2,
};

struct GroupBackup
{
  Group dictionaryOrder;
  Group inactiveDictionaries;
  Groups groups;
};

struct Class
{
  Paths paths;
  SoundDirs soundDirs;
  Group dictionaryOrder;
  Group inactiveDictionaries;
  Groups groups;
  Preferences preferences;
  MediaWikis mediawikis;
  WebSites webSites;
  DictServers dictServers;
  Hunspell hunspell;
  Transliteration transliteration;
  Lingua lingua;
  Forvo forvo;
  Programs programs;
#ifdef TTS_SUPPORT
  VoiceEngines voiceEngines;
#endif

  unsigned lastMainGroupId;  // Last used group in main window
  unsigned lastPopupGroupId; // Last used group in popup window

  QByteArray popupWindowState;           // Binary state saved by QMainWindow
  QByteArray popupWindowGeometry;        // Geometry saved by QMainWindow
  QByteArray dictInfoGeometry;           // Geometry of "Dictionary info" window
  QByteArray inspectorGeometry;          // Geometry of Web Engine inspector window
  QByteArray dictionariesDialogGeometry; // Geometry of Dictionaries dialog

  QString historyExportPath; // Path for export/import history
  QString resourceSavePath;  // Path to save images/audio
  QString articleSavePath;   // Path to save articles

  bool pinPopupWindow;         // Last pin status
  bool popupWindowAlwaysOnTop = false; // Last status of pinned popup window

  QByteArray mainWindowState;    // Binary state saved by QMainWindow
  QByteArray mainWindowGeometry; // Geometry saved by QMainWindow

  MutedDictionaries mutedDictionaries;      // Disabled via dictionary bar
  MutedDictionaries popupMutedDictionaries; // Disabled via dictionary bar in popup

  QDateTime timeForNewReleaseCheck; // Last time when the release was checked.

  QString skippedRelease; // Empty by default

  bool showingDictBarNames;

  ToolbarsIconSize usingToolbarsIconSize = ToolbarsIconSize::Normal;

  /// Maximum size for the headwords.
  /// Bigger headwords won't be indexed. For now, only in DSL.
  unsigned int maxHeadwordSize;

  unsigned int maxHeadwordsToExpand;

  HeadwordsDialog headwordsDialog;

  Class():
    lastMainGroupId( 0 ),
    lastPopupGroupId( 0 ),
    pinPopupWindow( false ),
    showingDictBarNames( false ),
    maxHeadwordSize( 256U ),
    maxHeadwordsToExpand( 0 )
  {
  }
  Group * getGroup( unsigned id );
  Group const * getGroup( unsigned id ) const;
  //disable tts dictionary. does not need to save to persistent file
  bool notts = false;
  bool resetState = false;
};

/// Configuration-specific events. Some parts of the program need to react
/// to specific changes in configuration. The object of this class is used
/// to emit signals when such events happen -- and the listeners connect to
/// them to be notified of them.
/// This class is separate from the main Class since QObjects can't be copied.
class Events: public QObject
{
  Q_OBJECT

public:

  /// Signals that the value of the mutedDictionaries has changed.
  /// This emits mutedDictionariesChanged() signal, so the subscribers will
  /// be notified.
  void signalMutedDictionariesChanged();

signals:

  /// THe value of the mutedDictionaries has changed.
  void mutedDictionariesChanged();

private:
};

DEF_EX( exError, "Error with the program's configuration", std::exception )
DEF_EX( exCantUseDataDir, "Can't use XDG_DATA_HOME directory to store GoldenDict data", exError )
DEF_EX( exCantUseHomeDir, "Can't use home directory to store GoldenDict preferences", exError )
DEF_EX( exCantUseIndexDir, "Can't use index directory to store GoldenDict index files", exError )
DEF_EX( exCantReadConfigFile, "Can't read the configuration file", exError )
DEF_EX( exCantWriteConfigFile, "Can't write the configuration file", exError )
DEF_EX( exMalformedConfigFile, "The configuration file is malformed", exError )

/// Loads the configuration, or creates the default one if none is present
Class load();

/// Saves the configuration
void save( Class const & );

/// Returns the configuration file name.
QString getConfigFileName();

/// Returns the main configuration directory.
QString getConfigDir();

/// Returns the index directory, where the indices are to be stored.
QString getIndexDir();

/// Returns the filename of a .pid file which should store current pid of
/// the process.
QString getPidFileName();

/// Returns the filename of a history file which stores search history.
QString getHistoryFileName();

/// Returns the filename of a favorities file.
QString getFavoritiesFileName();

/// Returns the user .css file name (article-style.css).
QString getUserCssFileName();

/// Returns the user .js file name (article-script.js).
std::optional< std::string > getUserJsFileName();

/// Returns the user .css file name used for printing only.
QString getUserCssPrintFileName();

/// Returns the user .css file name for the Qt interface customization.
QString getUserQtCssFileName();

/// Returns the program's data dir. Under Linux that would be something like
/// /usr/share/apps/goldendict, under Windows C:/Program Files/GoldenDict.
QString getProgramDataDir() noexcept;

/// Returns the directory storing program localizized files (.qm).
QString getLocDir() noexcept;

/// Returns the directory storing program help files (.qch).
QString getHelpDir() noexcept;

#ifdef MAKE_CHINESE_CONVERSION_SUPPORT
/// Returns the directory storing OpenCC configuration and dictionary files (.json and .ocd).
QString getOpenCCDir() noexcept;
#endif

/// Returns true if the program is configured as a portable version. In that
/// mode, all the settings and indices are kept in the program's directory.
bool isPortableVersion() noexcept;

/// Returns directory with dictionaries for portable version. It is content/
/// in the application's directory.
QString getPortableVersionDictionaryDir() noexcept;

/// Returns directory with morpgologies for portable version. It is
/// content/morphology in the application's directory.
QString getPortableVersionMorphoDir() noexcept;

/// Returns the add-on styles directory.
QString getStylesDir();

/// Returns the directory where user-specific non-essential (cached) data should be written.
QString getCacheDir() noexcept;

} // namespace Config
