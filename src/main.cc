/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include <stdio.h>
#include <QIcon>
#include "gdappstyle.hh"
#include "mainwindow.hh"
#include "config.hh"
#include <QWebEngineProfile>
#include "hotkeywrapper.hh"
#ifdef HAVE_X11
#include <fixx11h.h>
#endif

#if defined( Q_OS_UNIX )
#include <clocale>
#endif

#ifdef Q_OS_WIN32
#include <QtCore/qt_windows.h>
#endif

#include "termination.hh"
#include "atomic_rename.hh"
#include <QByteArray>
#include <QCommandLineParser>
#include <QFile>
#include <QMessageBox>
#include <QString>
#include <QtWebEngineCore/QWebEngineUrlScheme>

#include "gddebug.hh"

void gdMessageHandler( QtMsgType type, const QMessageLogContext &context, const QString &mess )
{
  Q_UNUSED( context );
  QString message( mess );
  QByteArray msg = message.toUtf8().constData();

  switch (type) {

    case QtDebugMsg:
      if( logFilePtr && logFilePtr->isOpen() )
        message.insert( 0, "Debug: " );
      else
        fprintf(stderr, "Debug: %s\n", msg.constData());
      break;

    case QtWarningMsg:
      if( logFilePtr && logFilePtr->isOpen() )
        message.insert( 0, "Warning: " );
      else
        fprintf(stderr, "Warning: %s\n", msg.constData());
      break;

    case QtCriticalMsg:
      if( logFilePtr && logFilePtr->isOpen() )
        message.insert( 0, "Critical: " );
      else
        fprintf(stderr, "Critical: %s\n", msg.constData());
      break;

    case QtFatalMsg:
      if( logFilePtr && logFilePtr->isOpen() )
      {
        logFilePtr->write( "Fatal: " );
        logFilePtr->write( msg );
        logFilePtr->flush();
      }
      else
        fprintf(stderr, "Fatal: %s\n", msg.constData());
      abort();
    case QtInfoMsg:
      if( logFilePtr && logFilePtr->isOpen() )
        message.insert( 0, "Info: " );
      else
        fprintf(stderr, "Info: %s\n", msg.constData());
      break;
  }

  if( logFilePtr && logFilePtr->isOpen() )
  {
    message.insert( 0, QDateTime::currentDateTime().toString( "yyyy-MM-dd HH:mm:ss.zzz " ) );
    message.append( "\n" );
    logFilePtr->write( message.toUtf8() );
    logFilePtr->flush();
  }
}

struct GDOptions
{
  bool logFile     = false;
  bool togglePopup = false;
  QString word, groupName, popupGroupName;

  inline bool needSetGroup() const
  { return !groupName.isEmpty(); }

  inline QString getGroupName() const
  { return groupName; }

  inline bool needSetPopupGroup() const
  { return !popupGroupName.isEmpty(); }

  inline QString getPopupGroupName() const
  { return popupGroupName; }

  inline bool needLogFile() const
  { return logFile; }

  inline bool needTranslateWord() const
  { return !word.isEmpty(); }

  inline QString wordToTranslate() const
  { return word; }

  inline bool needTogglePopup() const
  { return togglePopup; }
};

void processCommandLine( QCoreApplication * app, GDOptions * result)
{
  QCommandLineParser qcmd;

  qcmd.setApplicationDescription( QObject::tr( "A dictionary lookup program." ) );
  qcmd.addHelpOption(); // -h --help

  qcmd.addPositionalArgument( "word", QObject::tr( "Word or sentence to query." ), "[word]" );

  QCommandLineOption logFileOption( QStringList() << "l"
                                                  << "log-to-file",
                                    QObject::tr( "Save debug messages to gd_log.txt in the config folder." ) );

  QCommandLineOption groupNameOption( QStringList() << "g"
                                                    << "group-name",
                                      QObject::tr( "Change the group of main window." ),
                                      "groupName" );
  QCommandLineOption popupGroupNameOption( QStringList() << "p"
                                                         << "popup-group-name",
                                           QObject::tr( "Change the group of popup." ),
                                           "popupGroupName" );

  QCommandLineOption togglePopupOption( QStringList() << "t"
                                                      << "toggle-scan-popup",
                                        QObject::tr( "Toggle scan popup." ) );

  qcmd.addOption( logFileOption );
  qcmd.addOption( groupNameOption );
  qcmd.addOption( popupGroupNameOption );
  qcmd.addOption( togglePopupOption );

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

  if ( qcmd.isSet( togglePopupOption ) ) {
    result->togglePopup = true;
  }

  const QStringList posArgs = qcmd.positionalArguments();
  if ( !posArgs.empty() ) {
    result->word = posArgs.at( 0 );

#if defined( Q_OS_LINUX ) || defined( Q_OS_WIN )
    // handle url scheme like "goldendict://" on windows
    result->word.remove( "goldendict://" );
    // In microsoft Words, the / will be automatically appended
    if ( result->word.endsWith( "/" ) ) {
      result->word.chop( 1 );
    }
#endif
  }
}

class LogFilePtrGuard
{
  QFile logFile;
  Q_DISABLE_COPY( LogFilePtrGuard ) 
public:
  LogFilePtrGuard() { logFilePtr = &logFile; }
  ~LogFilePtrGuard() { logFilePtr = 0; }
};

int main( int argc, char ** argv )
{
#ifdef Q_OS_UNIX
    // GoldenDict use lots of X11 functions and it currently cannot work
    // natively on Wayland. This workaround will force GoldenDict to use
    // XWayland.
    char * xdg_envc = getenv("XDG_SESSION_TYPE");
    QString xdg_session = xdg_envc ? QString::fromLatin1(xdg_envc) : QString();
    if (!QString::compare(xdg_session, QString("wayland"), Qt::CaseInsensitive))
    {
        setenv("QT_QPA_PLATFORM", "xcb", 1);
    }
#endif
  #ifdef Q_OS_MAC
    setenv("LANG", "en_US.UTF-8", 1);

  #endif

#if defined( Q_OS_UNIX )
  setlocale( LC_ALL, "" ); // use correct char set mapping
#endif

  //high dpi screen support
#if ( QT_VERSION < QT_VERSION_CHECK( 6, 0, 0 ) )
  QApplication::setAttribute( Qt::AA_EnableHighDpiScaling );
  QApplication::setAttribute( Qt::AA_UseHighDpiPixmaps );
#endif
  qputenv( "QT_AUTO_SCREEN_SCALE_FACTOR", "1" );
  QApplication::setHighDpiScaleFactorRoundingPolicy( Qt::HighDpiScaleFactorRoundingPolicy::PassThrough );


  char ARG_DISABLE_WEB_SECURITY[] = "--disable-web-security";
  int newArgc                     = argc + 1 + 1;
  char ** newArgv                 = new char *[ newArgc ];
  for( int i = 0; i < argc; i++ )
  {
        newArgv[ i ] = argv[ i ];
  }
  newArgv[ argc ]     = ARG_DISABLE_WEB_SECURITY;
  newArgv[ argc + 1 ] = nullptr;

  QHotkeyApplication app( "GoldenDict", newArgc, newArgv );

  app.setApplicationName( "GoldenDict" );
  app.setOrganizationDomain( "https://github.com/xiaoyifang/goldendict" );
#ifndef Q_OS_MAC
  app.setWindowIcon( QIcon( ":/icons/programicon.png" ) );
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

  QStringList localSchemes={"gdlookup","gdau","gico","qrcx","bres","bword","gdprg","gdvideo","gdpicture","gdtts","ifr", "entry"};

  for (int i = 0; i < localSchemes.size(); ++i)
  {
      QString localScheme=localSchemes.at(i);
      QWebEngineUrlScheme webUiScheme(localScheme.toLatin1());
      webUiScheme.setFlags(QWebEngineUrlScheme::SecureScheme |
                           QWebEngineUrlScheme::LocalScheme |
                           QWebEngineUrlScheme::LocalAccessAllowed|
                           QWebEngineUrlScheme::CorsEnabled);
      QWebEngineUrlScheme::registerScheme(webUiScheme);
  }

  LogFilePtrGuard logFilePtrGuard;

  if ( app.isRunning() )
  {
    bool wasMessage = false;

    if( gdcl.needSetGroup() )
    {
      app.sendMessage( QString( "setGroup: " ) + gdcl.getGroupName() );
      wasMessage = true;
    }

    if( gdcl.needSetPopupGroup() )
    {
      app.sendMessage( QString( "setPopupGroup: " ) + gdcl.getPopupGroupName() );
      wasMessage = true;
    }

    if( gdcl.needTranslateWord() )
    {
      app.sendMessage( QString( "translateWord: " ) + gdcl.wordToTranslate() );
      wasMessage = true;
    }

    if ( gdcl.needTogglePopup() ) {
      app.sendMessage( QStringLiteral( "toggleScanPopup" ) );
      wasMessage = true;
    }

    if( !wasMessage )
      app.sendMessage("bringToFront");

    return 0; // Another instance is running
  }

#ifdef MAKE_CHINESE_CONVERSION_SUPPORT
  // OpenCC needs to load it's data files by relative path on Windows and OS X
  QDir::setCurrent( Config::getProgramDataDir() );
#endif

  // Load translations for system locale
  QString localeName = QLocale::system().name();

  Config::Class cfg;
  for( ; ; )
  {
    try
    {
      cfg = Config::load();
    }
    catch( Config::exError & )
    {
      QMessageBox mb( QMessageBox::Warning, app.applicationName(),
                      app.translate( "Main", "Error in configuration file. Continue with default settings?" ),
                      QMessageBox::Yes | QMessageBox::No );
      mb.exec();
      if( mb.result() != QMessageBox::Yes )
        return -1;

      QString configFile = Config::getConfigFileName();
      renameAtomically( configFile, configFile + ".bad" );
      continue;
    }
    break;
  }

  if( gdcl.needLogFile() )
  {
    // Open log file
    logFilePtr->setFileName( Config::getConfigDir() + "gd_log.txt" );
    logFilePtr->remove();
    logFilePtr->open( QFile::ReadWrite );

    // Write UTF-8 BOM
    QByteArray line;
    line.append( 0xEF ).append( 0xBB ).append( 0xBF );
    logFilePtr->write( line );

    // Install message handler
    qInstallMessageHandler( gdMessageHandler );
  }

  if ( Config::isPortableVersion() )
  {
    // For portable version, hardcode some settings

    cfg.paths.clear();
    cfg.paths.push_back( Config::Path( Config::getPortableVersionDictionaryDir(), true ) );
    cfg.soundDirs.clear();
    cfg.hunspell.dictionariesPath = Config::getPortableVersionMorphoDir();
  }

  // Reload translations for user selected locale is nesessary
  QTranslator qtTranslator;
  QTranslator translator;
  if( !cfg.preferences.interfaceLanguage.isEmpty() && localeName != cfg.preferences.interfaceLanguage )
  {
    localeName = cfg.preferences.interfaceLanguage;
  }

  QLocale locale( localeName );
  QLocale::setDefault( locale );
  if( !qtTranslator.load( "qt_" + localeName, Config::getLocDir() ) )
  {
    qtTranslator.load( "qt_" + localeName, QLibraryInfo::location( QLibraryInfo::TranslationsPath ) );
    app.installTranslator( &qtTranslator );
  }

  translator.load( Config::getLocDir() + "/" + localeName );
  app.installTranslator( &translator );

  QTranslator webengineTs;
  if( webengineTs.load( "qtwebengine_" + localeName, Config::getLocDir() ) )
  {
    app.installTranslator( &webengineTs );
  }

  // Prevent app from quitting spontaneously when it works with scan popup
  // and with the main window closed.
  app.setQuitOnLastWindowClosed( false );

  MainWindow m( cfg );

  app.addDataCommiter( m );

  QObject::connect( &app, &QtSingleApplication::messageReceived, &m, &MainWindow::messageFromAnotherInstanceReceived );

  if( gdcl.needSetGroup() )
    m.setGroupByName( gdcl.getGroupName(), true );

  if( gdcl.needSetPopupGroup() )
    m.setGroupByName( gdcl.getPopupGroupName(), false );

  if( gdcl.needTranslateWord() )
    m.wordReceived( gdcl.wordToTranslate() );

  int r = app.exec();

  app.removeDataCommiter( m );

  if( logFilePtr->isOpen() )
    logFilePtr->close();

  return r;
}
