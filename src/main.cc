﻿/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "config.hh"
#include "logger.hh"
#include "mainwindow.hh"
#include "termination.hh"
#include "version.hh"
#include <QByteArray>
#include <QCommandLineParser>
#include <QFile>
#include <QIcon>
#include <QMessageBox>
#include <QMutex>
#include <QSessionManager>
#include <QString>
#include <QtWebEngineCore/QWebEngineUrlScheme>
#include <stdio.h>
#include <QStyleFactory>
#if defined( Q_OS_UNIX )
  #include <clocale>
  #include "unix/ksignalhandler.hh"
#endif

#ifdef Q_OS_WIN32
  #include <windows.h>
#endif

#if defined( USE_BREAKPAD )
  #if defined( Q_OS_MAC )
    #include "client/mac/handler/exception_handler.h"
  #elif defined( Q_OS_LINUX )
    #include "client/linux/handler/exception_handler.h"
  #elif defined( Q_OS_WIN32 )
    #include "client/windows/handler/exception_handler.h"
  #endif
#endif

#if defined( USE_BREAKPAD )
  #ifdef Q_OS_WIN32
bool callback( const wchar_t * dump_path,
               const wchar_t * id,
               void * context,
               EXCEPTION_POINTERS * exinfo,
               MDRawAssertionInfo * assertion,
               bool succeeded )
{
  if ( succeeded ) {
    qDebug() << "Create dump file success";
  }
  else {
    qDebug() << "Create dump file failed";
  }
  return succeeded;
}
  #endif
  #ifdef Q_OS_LINUX
bool callback( const google_breakpad::MinidumpDescriptor & descriptor, void * context, bool succeeded )
{
  if ( succeeded ) {
    qDebug() << "Create dump file success";
  }
  else {
    qDebug() << "Create dump file failed";
  }
  return succeeded;
}
  #endif
  #ifdef Q_OS_MAC
bool callback( const char * dump_dir, const char * minidump_id, void * context, bool succeeded )
{
  if ( succeeded ) {
    qDebug() << "Create dump file success";
  }
  else {
    qDebug() << "Create dump file failed";
  }
  return succeeded;
}
  #endif
#endif


struct GDOptions
{
  bool logFile     = false;
  bool togglePopup = false;
  QString word, groupName, popupGroupName;
  QString window;

  inline bool needSetGroup() const
  {
    return !groupName.isEmpty();
  }

  inline QString getGroupName() const
  {
    return groupName;
  }

  inline bool needSetPopupGroup() const
  {
    return !popupGroupName.isEmpty();
  }

  inline QString getPopupGroupName() const
  {
    return popupGroupName;
  }

  inline bool needTranslateWord() const
  {
    return !word.isEmpty();
  }

  inline QString wordToTranslate() const
  {
    return word;
  }

  inline bool needTogglePopup() const
  {
    return togglePopup;
  }
  bool notts;
  bool resetState = false;
};

void processCommandLine( QCoreApplication * app, GDOptions * result )
{
  QCommandLineParser qcmd;

  qcmd.setApplicationDescription( QObject::tr( "A dictionary lookup program." ) );
  qcmd.addHelpOption(); // -h --help

  qcmd.addPositionalArgument( "word", QObject::tr( "Word or sentence to query." ), "[word]" );

  QCommandLineOption logFileOption( QStringList() << "l"
                                                  << "log-to-file",
                                    QObject::tr( "Save debug messages to gd_log.txt in the config folder" ) + '.' );

  QCommandLineOption resetState( QStringList() << "r"
                                               << "reset-window-state",
                                 QObject::tr( "Reset window state." ) );

  QCommandLineOption notts( "no-tts", QObject::tr( "Disable tts." ) );

  QCommandLineOption groupNameOption( QStringList() << "g"
                                                    << "group-name",
                                      QObject::tr( "Change the group of main window." ),
                                      "groupName" );
  QCommandLineOption popupGroupNameOption( QStringList() << "p"
                                                         << "popup-group-name",
                                           QObject::tr( "Change the group of popup." ),
                                           "popupGroupName" );

  QCommandLineOption window_popupOption( QStringList() << "s"
                                                       << "scanpopup",
                                         QObject::tr( "Force the word to be translated in scanpopup" ) );

  QCommandLineOption window_mainWindowOption( QStringList() << "m"
                                                            << "main-window",
                                              QObject::tr( "Force the word to be translated in the mainwindow" ) );

  QCommandLineOption togglePopupOption( QStringList() << "t"
                                                      << "toggle-popup",
                                        QObject::tr( "Toggle popup." ) );

  QCommandLineOption printVersion( QStringList() << "v"
                                                 << "version",
                                   QObject::tr( "Print version and diagnosis info." ) );

  qcmd.addOption( logFileOption );
  qcmd.addOption( groupNameOption );
  qcmd.addOption( popupGroupNameOption );
  qcmd.addOption( window_popupOption );
  qcmd.addOption( window_mainWindowOption );
  qcmd.addOption( togglePopupOption );
  qcmd.addOption( notts );
  qcmd.addOption( resetState );
  qcmd.addOption( printVersion );

  QCommandLineOption doNothingOption( "disable-web-security" ); // ignore the --disable-web-security
  doNothingOption.setFlags( QCommandLineOption::HiddenFromHelp );
  qcmd.addOption( doNothingOption );

  qcmd.process( *app );

  if ( qcmd.isSet( logFileOption ) ) {
    result->logFile = true;
  }

  if ( qcmd.isSet( groupNameOption ) ) {
    result->groupName = qcmd.value( groupNameOption );
  }

  if ( qcmd.isSet( popupGroupNameOption ) ) {
    result->popupGroupName = qcmd.value( popupGroupNameOption );
  }
  if ( qcmd.isSet( window_popupOption ) ) {
    result->window = "popup";
  }
  if ( qcmd.isSet( window_mainWindowOption ) ) {
    result->window = "main";
  }
  if ( qcmd.isSet( togglePopupOption ) ) {
    result->togglePopup = true;
  }

  if ( qcmd.isSet( notts ) ) {
    result->notts = true;
  }

  if ( qcmd.isSet( resetState ) ) {
    result->resetState = true;
  }

  if ( qcmd.isSet( printVersion ) ) {
    qInfo() << qPrintable( Version::everything() );
    std::exit( 0 );
  }

  const QStringList posArgs = qcmd.positionalArguments();
  if ( !posArgs.empty() ) {
    result->word = posArgs.at( 0 );

#if defined( Q_OS_LINUX ) || defined( Q_OS_WIN )
    // handle url scheme like "goldendict://" or "dict://" on windows/linux
    auto schemePos = result->word.indexOf( "://" );
    if ( schemePos != -1 ) {
      result->word.remove( 0, schemePos + 3 );
      // In microsoft Words, the / will be automatically appended
      if ( result->word.endsWith( "/" ) ) {
        result->word.chop( 1 );
      }
    }

    // Handle cases where we get encoded URL
    if ( result->word.startsWith( QStringLiteral( "xn--" ) ) ) {
      // For `kde-open` or `gio` or others, URL are encoded into ACE or Punycode
      result->word = QUrl::fromAce( result->word.toLatin1(), QUrl::IgnoreIDNWhitelist );
    }
    else if ( result->word.startsWith( QStringLiteral( "%" ) ) ) {
      // For Firefox or other browsers where URL are percent encoded
      result->word = QUrl::fromPercentEncoding( result->word.toLatin1() );
    }
#endif
  }
}

int main( int argc, char ** argv )
{
#if defined( Q_OS_UNIX ) && !defined( Q_OS_MACOS )
  // GoldenDict use lots of X11 functions and it currently cannot work
  // natively on Wayland. This workaround will force GoldenDict to use
  // XWayland.

  if ( qEnvironmentVariableIsEmpty( "GOLDENDICT_FORCE_WAYLAND" ) ) {
    char * xdg_envc     = getenv( "XDG_SESSION_TYPE" );
    QString xdg_session = xdg_envc ? QString::fromLatin1( xdg_envc ) : QString();
    if ( !QString::compare( xdg_session, QString( "wayland" ), Qt::CaseInsensitive ) ) {
      setenv( "QT_QPA_PLATFORM", "xcb", 1 );
    }
  }
#endif

#ifdef Q_OS_MAC
  setenv( "LANG", "en_US.UTF-8", 1 );
#endif

#ifdef Q_OS_WIN32
  // attach the new console to this application's process
  if ( AttachConsole( ATTACH_PARENT_PROCESS ) ) {
    // reopen the std I/O streams to redirect I/O to the new console
    auto ret1 = freopen( "CON", "w", stdout );
    auto ret2 = freopen( "CON", "w", stderr );
    if ( ret1 == nullptr || ret2 == nullptr ) {
      qDebug() << "Attaching console stdout or stderr failed";
    }
  }

  qputenv( "QT_QPA_PLATFORM", "windows:darkmode=1" );

#endif


  //high dpi screen support
  if ( !qEnvironmentVariableIsSet( "QT_ENABLE_HIGHDPI_SCALING" )
       || qEnvironmentVariableIsEmpty( "QT_ENABLE_HIGHDPI_SCALING" ) ) {
    qputenv( "QT_ENABLE_HIGHDPI_SCALING", "1" );
  }
  QApplication::setHighDpiScaleFactorRoundingPolicy( Qt::HighDpiScaleFactorRoundingPolicy::PassThrough );

  QHotkeyApplication app( "GoldenDict-ng", argc, argv );

  QHotkeyApplication::setApplicationName( "GoldenDict-ng" );
  QHotkeyApplication::setOrganizationDomain( "https://github.com/xiaoyifang/goldendict-ng" );
#ifndef Q_OS_MACOS
  // macOS icon is defined in Info.plist
  QHotkeyApplication::setWindowIcon( QIcon( ":/icons/programicon.png" ) );
#endif

#ifdef Q_OS_WIN
  // TODO: Force fusion because Qt6.7's "ModernStyle"'s dark theme have problems, need to test / reconsider in future
  QHotkeyApplication::setStyle( QStyleFactory::create( "WindowsVista" ) );
#endif


#if defined( USE_BREAKPAD )
  QString appDirPath = Config::getConfigDir() + "crash";

  QDir dir;
  if ( !dir.exists( appDirPath ) ) {
    dir.mkpath( appDirPath );
  }
  #ifdef Q_OS_WIN32

  google_breakpad::ExceptionHandler eh( appDirPath.toStdWString(),
                                        NULL,
                                        callback,
                                        NULL,
                                        google_breakpad::ExceptionHandler::HANDLER_ALL );
  #elif defined( Q_OS_MAC )


  google_breakpad::ExceptionHandler eh( appDirPath.toStdString(), 0, callback, 0, true, NULL );

  #else

  google_breakpad::ExceptionHandler eh( google_breakpad::MinidumpDescriptor( appDirPath.toStdString() ),
                                        /*FilterCallback*/ 0,
                                        callback,
                                        /*context*/ 0,
                                        true,
                                        -1 );
  #endif

#endif

  GDOptions gdcl{};

  if ( argc > 1 ) {
    processCommandLine( &app, &gdcl );
  }

  installTerminationHandler();

#ifdef __WIN32

  // Under Windows, increase the amount of fopen()-able file descriptors from
  // the default 512 up to 2048.
  _setmaxstdio( 2048 );

#endif

  const QStringList localSchemes =
    { "gdlookup", "gdau", "gico", "qrcx", "bres", "bword", "gdprg", "gdvideo", "gdtts", "ifr", "entry" };

  for ( const auto & localScheme : localSchemes ) {
    QWebEngineUrlScheme webUiScheme( localScheme.toLatin1() );
    webUiScheme.setSyntax( QWebEngineUrlScheme::Syntax::Host );
    webUiScheme.setFlags( QWebEngineUrlScheme::LocalScheme | QWebEngineUrlScheme::LocalAccessAllowed
                          | QWebEngineUrlScheme::CorsEnabled );
    QWebEngineUrlScheme::registerScheme( webUiScheme );
  }

  QFont f = QApplication::font();
  f.setStyleStrategy( QFont::PreferAntialias );
  QApplication::setFont( f );

  if ( app.isRunning() ) {
    bool wasMessage = false;

    //TODO .all the following messages can be combined into one.
    if ( gdcl.needSetGroup() ) {
      app.sendMessage( QString( "setGroup: " ) + gdcl.getGroupName() );
      wasMessage = true;
    }

    if ( gdcl.needSetPopupGroup() ) {
      app.sendMessage( QString( "setPopupGroup: " ) + gdcl.getPopupGroupName() );
      wasMessage = true;
    }

    if ( !gdcl.window.isEmpty() ) {
      app.sendMessage( QString( "window:" ) + gdcl.window );
      wasMessage = true;
    }

    if ( gdcl.needTranslateWord() ) {
      app.sendMessage( QString( "translateWord: " ) + gdcl.wordToTranslate() );
      wasMessage = true;
    }

    if ( gdcl.needTogglePopup() ) {
      app.sendMessage( QStringLiteral( "toggleScanPopup" ) );
      wasMessage = true;
    }

    if ( !wasMessage ) {
      app.sendMessage( "bringToFront" );
    }

    return 0; // Another instance is running
  }

#ifdef MAKE_CHINESE_CONVERSION_SUPPORT
  // OpenCC needs to load it's data files by relative path on Windows and OS X
  QDir::setCurrent( Config::getProgramDataDir() );
#endif

  // Load translations for system locale
  QString localeName = QLocale::system().name();

  Config::Class cfg;
  for ( ;; ) {
    try {
      cfg = Config::load();
    }
    catch ( Config::exError & ) {
      QMessageBox mb(
        QMessageBox::Warning,
        QHotkeyApplication::applicationName(),
        QHotkeyApplication::translate( "Main", "Error in configuration file. Continue with default settings?" ),
        QMessageBox::Yes | QMessageBox::No );
      mb.exec();
      if ( mb.result() != QMessageBox::Yes ) {
        return -1;
      }

      QString configFile = Config::getConfigFileName();
      QFile::rename( configFile,
                     configFile % QStringLiteral( "." )
                       % QDateTime::currentDateTime().toString( QStringLiteral( "yyyyMMdd_HHmmss" ) )
                       % QStringLiteral( ".bad" ) );
      continue;
    }
    break;
  }

  if ( gdcl.notts ) {
    cfg.notts = true;
#ifdef TTS_SUPPORT
    cfg.voiceEngines.clear();
#endif
  }

  cfg.resetState = gdcl.resetState;

  // Log to file enabled through command line or preference
  Logger::switchLoggingMethod( gdcl.logFile || cfg.preferences.enableApplicationLog );

  //System Font
  auto font = QApplication::font();
  if ( !cfg.preferences.interfaceFont.isEmpty() && font.family() != cfg.preferences.interfaceFont ) {
    font.setFamily( cfg.preferences.interfaceFont );
    QApplication::setFont( font );
  }

  // Update locale if the user's choice disagrees with the system
  QLocale locale = QLocale::system();
  if ( !cfg.preferences.interfaceLanguage.isEmpty() && locale.name() != cfg.preferences.interfaceLanguage ) {
    locale = QLocale( cfg.preferences.interfaceLanguage );
  }

  QLocale::setDefault( locale );
  QApplication::setLayoutDirection( locale.textDirection() );

  // Load translations, note some quirks:
  // * For Windows, windeployqt will combine multiple qt modules translations into `qt_*` thus no `qtwebengine_*` exists
  // * Only try loading qt & webengine translator GD's translations success to avoid inconsistency
  // * Use the QLocale based loading QTranslator::load

  QTranslator gd_ts;

  QTranslator qt_ts;
  QTranslator webengine_ts;

  auto loadTranslation = [ &locale ]( QTranslator & qtranslator,
                                      const QString & filename,
                                      const QString & prefix,
                                      const QString & directory ) -> bool {
    if ( qtranslator.load( locale, filename, prefix, directory ) ) {
      qDebug() << "Loaded translator: " << qtranslator.filePath();
      return true;
    }
    else {
      qDebug() << "Failed to load: " << filename << prefix << " from " << directory;
      return false;
    }
  };

  if ( loadTranslation( gd_ts, QString(), QString(), Config::getLocDir() ) ) {
    QCoreApplication::installTranslator( &gd_ts );

    if ( loadTranslation( qt_ts, "qt", "_", QLibraryInfo::path( QLibraryInfo::TranslationsPath ) ) ) {
      QCoreApplication::installTranslator( &qt_ts );
    }
    if ( loadTranslation( webengine_ts, "qtwebengine", "_", QLibraryInfo::path( QLibraryInfo::TranslationsPath ) ) ) {
      QCoreApplication::installTranslator( &webengine_ts );
    }
  }

  // Prevent app from quitting spontaneously when it works with popup
  // and with the main window closed.
  app.setQuitOnLastWindowClosed( false );

  MainWindow m( cfg );

  /// Session manager things.
  // Redirect commit data request to Mainwindow's handler.
  QObject::connect(
    &app,
    &QGuiApplication::commitDataRequest,
    &m,
    [ &m ]( QSessionManager & ) {
      m.commitData();
    },
    Qt::DirectConnection );

  // Just don't restart. This probably isn't really needed.
  QObject::connect(
    &app,
    &QGuiApplication::saveStateRequest,
    &app,
    []( QSessionManager & mgr ) {
      mgr.setRestartHint( QSessionManager::RestartNever );
    },
    Qt::DirectConnection );

  QObject::connect( &app, &QtSingleApplication::messageReceived, &m, &MainWindow::messageFromAnotherInstanceReceived );

  if ( gdcl.needSetGroup() ) {
    m.setGroupByName( gdcl.getGroupName(), true );
  }

  if ( gdcl.needSetPopupGroup() ) {
    m.setGroupByName( gdcl.getPopupGroupName(), false );
  }

  if ( gdcl.needTranslateWord() ) {
    m.wordReceived( gdcl.wordToTranslate() );
  }

#ifdef Q_OS_UNIX
  // handle Unix's shutdown signals for graceful exit
  KSignalHandler::self()->watchSignal( SIGINT );
  KSignalHandler::self()->watchSignal( SIGTERM );
  QObject::connect( KSignalHandler::self(), &KSignalHandler::signalReceived, &m, &MainWindow::quitApp );
#endif
  int r = app.exec();
  Logger::closeLogFile();

  return r;
}
