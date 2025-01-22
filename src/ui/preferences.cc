#include "keyboardstate.hh"
#include "language.hh"
#include "preferences.hh"
#include "help.hh"

#include <QDebug>
#include <QDir>
#include <QFontDatabase>
#include <QMessageBox>
#include <QThread>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QStyleFactory>

Preferences::Preferences( QWidget * parent, Config::Class & cfg_ ):
  QDialog( parent ),
  cfg( cfg_ ),
  helpAction( this )
{
  Config::Preferences const & p = cfg_.preferences;
  ui.setupUi( this );

  connect( ui.enableScanPopupModifiers,
           &QAbstractButton::toggled,
           this,
           &Preferences::enableScanPopupModifiersToggled );

  connect( ui.showScanFlag, &QAbstractButton::toggled, this, &Preferences::showScanFlagToggled );

  helpAction.setShortcut( QKeySequence( "F1" ) );
  helpAction.setShortcutContext( Qt::WidgetWithChildrenShortcut );

  connect( &helpAction, &QAction::triggered, [ this ]() {
    const auto * currentTab = ui.tabWidget->currentWidget();
    if ( ui.tab_popup == currentTab ) {
      Help::openHelpWebpage( Help::section::ui_popup );
    }
    else if ( ui.tab_FTS == currentTab ) {
      Help::openHelpWebpage( Help::section::ui_fulltextserch );
    }
    else {
      Help::openHelpWebpage();
    }
  } );
  connect( ui.buttonBox, &QDialogButtonBox::helpRequested, &helpAction, &QAction::trigger );

  addAction( &helpAction );

  // Load values into form

  ui.interfaceLanguage->addItem( tr( "System default" ), QString() );
  // See which other translations do we have

  QStringList availLocs = QDir( Config::getLocDir() ).entryList( QStringList( "*.qm" ), QDir::Files );

  // We need to sort by language name -- otherwise list looks really weird
  QMultiMap< QString, QString > sortedLocs;
  sortedLocs.insert( Language::languageForLocale( "en_US" ), "en_US" );
  for ( const auto & availLoc : availLocs ) {
    // Here we assume the xx_YY naming, where xx is language and YY is region.
    //remove .qm suffix.
    QString locale = availLoc.left( availLoc.size() - 3 );

    if ( locale == "qt" ) {
      continue; // We skip qt's own localizations
    }

    auto language = Language::languageForLocale( locale );
    if ( language.isEmpty() ) {
      qWarning() << "can not found the corresponding language from locale:" << locale;
    }
    else {
      sortedLocs.insert( language, locale );
    }
  }

  for ( auto i = sortedLocs.begin(); i != sortedLocs.end(); ++i ) {
    ui.interfaceLanguage->addItem( i.key(), i.value() );
  }

  for ( int x = 0; x < ui.interfaceLanguage->count(); ++x ) {
    if ( ui.interfaceLanguage->itemData( x ).toString() == p.interfaceLanguage ) {
      ui.interfaceLanguage->setCurrentIndex( x );
      prevInterfaceLanguage = x;
      break;
    }
  }

  //System Font
  if ( !p.interfaceFont.isEmpty() ) {
    ui.systemFont->setCurrentText( p.interfaceFont );
  }


  prevWebFontFamily = p.customFonts;
  prevSysFont       = p.interfaceFont;

  if ( !p.customFonts.standard.isEmpty() ) {
    ui.font_standard->setCurrentText( p.customFonts.standard );
  }
  else {
    ui.font_standard->setCurrentFont(
      QWebEngineProfile::defaultProfile()->settings()->fontFamily( QWebEngineSettings::StandardFont ) );
  }

  if ( !p.customFonts.serif.isEmpty() ) {
    ui.font_serif->setCurrentText( p.customFonts.serif );
  }
  else {
    ui.font_serif->setCurrentFont(
      QWebEngineProfile::defaultProfile()->settings()->fontFamily( QWebEngineSettings::SerifFont ) );
  }

  if ( !p.customFonts.sansSerif.isEmpty() ) {
    ui.font_sans->setCurrentText( p.customFonts.sansSerif );
  }
  else {
    ui.font_sans->setCurrentFont(
      QWebEngineProfile::defaultProfile()->settings()->fontFamily( QWebEngineSettings::SansSerifFont ) );
  }

  if ( !p.customFonts.monospace.isEmpty() ) {
    ui.font_monospace->setCurrentText( p.customFonts.monospace );
  }
  else {
    ui.font_monospace->setCurrentFont(
      QWebEngineProfile::defaultProfile()->settings()->fontFamily( QWebEngineSettings::FixedFont ) );
  }

  ui.displayStyle->addItem( QIcon( ":/icons/programicon_old.png" ), tr( "Default" ), QString() );
  ui.displayStyle->addItem( QIcon( ":/icons/programicon.png" ), tr( "Classic" ), QString( "classic" ) );
  ui.displayStyle->addItem( QIcon( ":/icons/programicon.png" ), tr( "Modern" ), QString( "modern" ) );
  ui.displayStyle->addItem( QIcon( ":/icons/icon32_dsl.png" ), tr( "Lingvo" ), QString( "lingvo" ) );
  ui.displayStyle->addItem( QIcon( ":/icons/icon32_bgl.png" ), tr( "Babylon" ), QString( "babylon" ) );
  ui.displayStyle->addItem( QIcon( ":/icons/icon32_lingoes.png" ), tr( "Lingoes" ), QString( "lingoes" ) );
  ui.displayStyle->addItem( QIcon( ":/icons/icon32_lingoes.png" ), tr( "Lingoes-Blue" ), QString( "lingoes-blue" ) );

  for ( int x = 0; x < ui.displayStyle->count(); ++x ) {
    if ( ui.displayStyle->itemData( x ).toString() == p.displayStyle ) {
      ui.displayStyle->setCurrentIndex( x );
      break;
    }
  }

#if !defined( Q_OS_WIN )
  ui.InterfaceStyle->addItem( "Default", "Default" );

  for ( const auto & style : QStyleFactory::keys() ) {
    ui.InterfaceStyle->addItem( style, style );
  }

  for ( int i = 0; i < ui.InterfaceStyle->count(); ++i ) {
    if ( ui.InterfaceStyle->itemData( i ).toString() == p.interfaceStyle ) {
      ui.InterfaceStyle->setCurrentIndex( i );
      prevInterfaceStyle = i;
      break;
    }
  }
#endif

#if defined( Q_OS_WIN )
  ui.interfaceStyleLabel->hide();
  ui.InterfaceStyle->hide();
#endif

#ifdef Q_OS_WIN32
  // 1 MB stands for 2^20 bytes on Windows. "MiB" is never used by this OS.
  ui.maxNetworkCacheSize->setSuffix( tr( " MB" ) );
#endif
  ui.maxNetworkCacheSize->setToolTip( ui.maxNetworkCacheSize->toolTip().arg( Config::getCacheDir() ) );

  ui.newTabsOpenAfterCurrentOne->setChecked( p.newTabsOpenAfterCurrentOne );
  ui.newTabsOpenInBackground->setChecked( p.newTabsOpenInBackground );
  ui.hideSingleTab->setChecked( p.hideSingleTab );
  ui.mruTabOrder->setChecked( p.mruTabOrder );
  ui.enableTrayIcon->setChecked( p.enableTrayIcon );

#ifdef Q_OS_MACOS // macOS uses the dock menu instead of the tray icon
  ui.enableTrayIcon->hide();
#endif

  ui.startToTray->setChecked( p.startToTray );
  ui.closeToTray->setChecked( p.closeToTray );
  ui.cbAutostart->setChecked( p.autoStart );
  ui.doubleClickTranslates->setChecked( p.doubleClickTranslates );
  ui.selectBySingleClick->setChecked( p.selectWordBySingleClick );
  ui.autoScrollToTargetArticle->setChecked( p.autoScrollToTargetArticle );
  ui.escKeyHidesMainWindow->setChecked( p.escKeyHidesMainWindow );

  ui.darkMode->addItem( tr( "Enable" ), QVariant::fromValue( Config::Dark::On ) );
  ui.darkMode->addItem( tr( "Disable" ), QVariant::fromValue( Config::Dark::Off ) );

  if ( auto i = ui.darkMode->findData( QVariant::fromValue( p.darkMode ) ); i != -1 ) {
    ui.darkMode->setCurrentIndex( i );
  }

  ui.darkReaderMode->addItem( tr( "Automatic" ), QVariant::fromValue( Config::Dark::Auto ) );
  ui.darkReaderMode->setItemData( 0, tr( "Auto does nothing on some systems." ), Qt::ToolTipRole );
  ui.darkReaderMode->addItem( tr( "Enable" ), QVariant::fromValue( Config::Dark::On ) );
  ui.darkReaderMode->addItem( tr( "Disable" ), QVariant::fromValue( Config::Dark::Off ) );

  if ( auto i = ui.darkReaderMode->findData( QVariant::fromValue( p.darkReaderMode ) ); i != -1 ) {
    ui.darkReaderMode->setCurrentIndex( i );
  }


#ifndef Q_OS_WIN32
  // TODO: make this availiable on other platforms
  ui.darkModeLabel->hide();
  ui.darkMode->hide();
#endif

  /// Hotkey Tab
  ui.enableMainWindowHotkey->setChecked( p.enableMainWindowHotkey );
  ui.mainWindowHotkey->setKeySequence( p.mainWindowHotkey );
  ui.enableClipboardHotkey->setChecked( p.enableClipboardHotkey );
  ui.clipboardHotkey->setKeySequence( p.clipboardHotkey );

#if QT_VERSION >= QT_VERSION_CHECK( 6, 5, 0 )
  // Bound by current global hotkey implementations
  ui.mainWindowHotkey->setMaximumSequenceLength( 2 );
  ui.clipboardHotkey->setMaximumSequenceLength( 2 );
#endif

  ui.startWithScanPopupOn->setChecked( p.startWithScanPopupOn );
  ui.enableScanPopupModifiers->setChecked( p.enableScanPopupModifiers );

  ui.altKey->setChecked( p.scanPopupModifiers & KeyboardState::Alt );
  ui.ctrlKey->setChecked( p.scanPopupModifiers & KeyboardState::Ctrl );
  ui.shiftKey->setChecked( p.scanPopupModifiers & KeyboardState::Shift );
  ui.winKey->setChecked( p.scanPopupModifiers & KeyboardState::Win );

  ui.ignoreOwnClipboardChanges->setChecked( p.ignoreOwnClipboardChanges );
  ui.scanToMainWindow->setChecked( p.scanToMainWindow );

  ui.storeHistory->setChecked( p.storeHistory );
  ui.historyMaxSizeField->setValue( p.maxStringsInHistory );
  ui.historySaveIntervalField->setValue( p.historyStoreInterval );
  ui.alwaysExpandOptionalParts->setChecked( p.alwaysExpandOptionalParts );

  ui.favoritesSaveIntervalField->setValue( p.favoritesStoreInterval );
  ui.confirmFavoritesDeletion->setChecked( p.confirmFavoritesDeletion );

  ui.collapseBigArticles->setChecked( p.collapseBigArticles );
  on_collapseBigArticles_toggled( ui.collapseBigArticles->isChecked() );
  ui.articleSizeLimit->setValue( p.articleSizeLimit );

  ui.limitInputPhraseLength->setChecked( p.limitInputPhraseLength );
  on_limitInputPhraseLength_toggled( ui.limitInputPhraseLength->isChecked() );
  ui.inputPhraseLengthLimit->setValue( p.inputPhraseLengthLimit );

  ui.ignoreDiacritics->setChecked( p.ignoreDiacritics );

  ui.ignorePunctuation->setChecked( p.ignorePunctuation );
  ui.sessionCollapse->setChecked( p.sessionCollapse );

  ui.synonymSearchEnabled->setChecked( p.synonymSearchEnabled );

  ui.stripClipboard->setChecked( p.stripClipboard );

  ui.raiseWindowOnSearch->setChecked( p.raiseWindowOnSearch );

  ui.maxDictsInContextMenu->setValue( p.maxDictionaryRefsInContextMenu );

  // Different platforms have different keys available

#ifdef Q_OS_WIN32
  ui.winKey->hide();
#else
  #ifdef Q_OS_MAC
  ui.altKey->setText( "Opt" );
  ui.winKey->setText( "Ctrl" );
  ui.ctrlKey->setText( "Cmd" );
  #endif
#endif

  //Platform-specific options

#ifdef HAVE_X11
  ui.enableX11SelectionTrack->setChecked( p.trackSelectionScan );
  ui.enableClipboardTrack->setChecked( p.trackClipboardScan );
  ui.showScanFlag->setChecked( p.showScanFlag );
  ui.delayTimer->setValue( p.selectionChangeDelayTimer );
#else
  ui.enableX11SelectionTrack->hide();
  ui.enableClipboardTrack->hide();
  ui.showScanFlag->hide();
  ui.ignoreOwnClipboardChanges->hide();
  ui.delayTimer->hide();
#endif

  // Sound

  ui.pronounceOnLoadMain->setChecked( p.pronounceOnLoadMain );
  ui.pronounceOnLoadPopup->setChecked( p.pronounceOnLoadPopup );

  ui.internalPlayerBackend->addItems( InternalPlayerBackend::availableBackends() );

  // Make sure that exactly one radio button in the group is checked and that
  // on_useExternalPlayer_toggled() is called.
  ui.useExternalPlayer->setChecked( true );

  if ( ui.internalPlayerBackend->count() > 0 ) {
    // Checking ui.useInternalPlayer automatically unchecks ui.useExternalPlayer.
    ui.useInternalPlayer->setChecked( p.useInternalPlayer );

    int index = ui.internalPlayerBackend->findText( p.internalPlayerBackend.getName() );
    if ( index >= 0 ) {
      ui.internalPlayerBackend->setCurrentIndex( index );
    }
    else {
      // Find no backend, just do nothing and let just let Qt select the first one.
    }
  }
  else {
    ui.useInternalPlayer->hide();
    ui.internalPlayerBackend->hide();
  }

  ui.audioPlaybackProgram->setText( p.audioPlaybackProgram );

  // Proxy server

  ui.useProxyServer->setChecked( p.proxyServer.enabled );

  ui.proxyType->addItem( "SOCKS5" );
  ui.proxyType->addItem( "HTTP Transp." );
  ui.proxyType->addItem( "HTTP Caching" );

  ui.proxyType->setCurrentIndex( p.proxyServer.type );

  ui.proxyHost->setText( p.proxyServer.host );
  ui.proxyPort->setValue( p.proxyServer.port );

  ui.proxyUser->setText( p.proxyServer.user );
  ui.proxyPassword->setText( p.proxyServer.password );

  if ( p.proxyServer.useSystemProxy ) {
    ui.systemProxy->setChecked( true );
    ui.customSettingsGroup->setEnabled( false );
  }
  else {
    ui.customProxy->setChecked( true );
    ui.customSettingsGroup->setEnabled( p.proxyServer.enabled );
  }

  //anki connect
  ui.useAnkiConnect->setChecked( p.ankiConnectServer.enabled );
  ui.ankiHost->setText( p.ankiConnectServer.host );
  ui.ankiPort->setValue( p.ankiConnectServer.port );
  ui.ankiModel->setText( p.ankiConnectServer.model );
  ui.ankiDeck->setText( p.ankiConnectServer.deck );
  //anki connect fields
  ui.ankiText->setText( p.ankiConnectServer.text );
  ui.ankiWord->setText( p.ankiConnectServer.word );
  ui.ankiSentence->setText( p.ankiConnectServer.sentence );

  connect( ui.customProxy, &QAbstractButton::toggled, this, &Preferences::customProxyToggled );

  connect( ui.useProxyServer, &QGroupBox::toggled, this, &Preferences::customProxyToggled );

  ui.checkForNewReleases->setChecked( p.checkForNewReleases );
  ui.disallowContentFromOtherSites->setChecked( p.disallowContentFromOtherSites );
  ui.hideGoldenDictHeader->setChecked( p.hideGoldenDictHeader );
  ui.maxNetworkCacheSize->setValue( p.maxNetworkCacheSize );
  ui.clearNetworkCacheOnExit->setChecked( p.clearNetworkCacheOnExit );

  //Misc
  ui.removeInvalidIndexOnExit->setChecked( p.removeInvalidIndexOnExit );
  ui.enableApplicationLog->setChecked( p.enableApplicationLog );

  // Add-on styles
  ui.addonStylesLabel->setVisible( ui.addonStyles->count() > 1 );
  ui.addonStyles->setCurrentStyle( p.addonStyle );

  // Full-text search parameters
  ui.ftsGroupBox->setChecked( p.fts.enabled );

  ui.allowAard->setChecked( !p.fts.disabledTypes.contains( "AARD", Qt::CaseInsensitive ) );
  ui.allowBGL->setChecked( !p.fts.disabledTypes.contains( "BGL", Qt::CaseInsensitive ) );
  ui.allowDictD->setChecked( !p.fts.disabledTypes.contains( "DICTD", Qt::CaseInsensitive ) );
  ui.allowDSL->setChecked( !p.fts.disabledTypes.contains( "DSL", Qt::CaseInsensitive ) );
  ui.allowMDict->setChecked( !p.fts.disabledTypes.contains( "MDICT", Qt::CaseInsensitive ) );
  ui.allowSDict->setChecked( !p.fts.disabledTypes.contains( "SDICT", Qt::CaseInsensitive ) );
  ui.allowSlob->setChecked( !p.fts.disabledTypes.contains( "SLOB", Qt::CaseInsensitive ) );
  ui.allowStardict->setChecked( !p.fts.disabledTypes.contains( "STARDICT", Qt::CaseInsensitive ) );
  ui.allowXDXF->setChecked( !p.fts.disabledTypes.contains( "XDXF", Qt::CaseInsensitive ) );
  ui.allowZim->setChecked( !p.fts.disabledTypes.contains( "ZIM", Qt::CaseInsensitive ) );
  ui.allowEpwing->setChecked( !p.fts.disabledTypes.contains( "EPWING", Qt::CaseInsensitive ) );
  ui.allowGls->setChecked( !p.fts.disabledTypes.contains( "GLS", Qt::CaseInsensitive ) );

#ifndef MAKE_ZIM_SUPPORT
  ui.allowZim->hide();
#endif
#ifndef EPWING_SUPPORT
  ui.allowEpwing->hide();
#endif
  ui.maxDictionarySize->setValue( p.fts.maxDictionarySize );

  ui.parallelThreads->setMaximum( QThread::idealThreadCount() );
  ui.parallelThreads->setValue( p.fts.parallelThreads );
}

void Preferences::buildDisabledTypes( QString & disabledTypes, bool is_checked, QString name )
{
  if ( !is_checked ) {
    if ( !disabledTypes.isEmpty() ) {
      disabledTypes += ',';
    }
    disabledTypes += name;
  }
}

Config::Preferences Preferences::getPreferences()
{
  Config::Preferences p;

  p.interfaceLanguage = ui.interfaceLanguage->itemData( ui.interfaceLanguage->currentIndex() ).toString();

  p.interfaceFont = ui.systemFont->currentText();

  Config::CustomFonts c;
  c.standard    = ui.font_standard->currentText();
  c.serif       = ui.font_serif->currentText();
  c.sansSerif   = ui.font_sans->currentText();
  c.monospace   = ui.font_monospace->currentText();
  p.customFonts = c;


  p.displayStyle = ui.displayStyle->itemData( ui.displayStyle->currentIndex() ).toString();
#if !defined( Q_OS_WIN )
  p.interfaceStyle = ui.InterfaceStyle->itemData( ui.InterfaceStyle->currentIndex() ).toString();
#endif

  p.newTabsOpenAfterCurrentOne = ui.newTabsOpenAfterCurrentOne->isChecked();
  p.newTabsOpenInBackground    = ui.newTabsOpenInBackground->isChecked();
  p.hideSingleTab              = ui.hideSingleTab->isChecked();
  p.mruTabOrder                = ui.mruTabOrder->isChecked();
  p.enableTrayIcon             = ui.enableTrayIcon->isChecked();
  p.startToTray                = ui.startToTray->isChecked();
  p.closeToTray                = ui.closeToTray->isChecked();
  p.autoStart                  = ui.cbAutostart->isChecked();
  p.doubleClickTranslates      = ui.doubleClickTranslates->isChecked();
  p.selectWordBySingleClick    = ui.selectBySingleClick->isChecked();
  p.autoScrollToTargetArticle  = ui.autoScrollToTargetArticle->isChecked();
  p.escKeyHidesMainWindow      = ui.escKeyHidesMainWindow->isChecked();

  p.darkMode       = ui.darkMode->currentData().value< Config::Dark >();
  p.darkReaderMode = ui.darkReaderMode->currentData().value< Config::Dark >();

  p.enableMainWindowHotkey = ui.enableMainWindowHotkey->isChecked();
  p.mainWindowHotkey       = ui.mainWindowHotkey->keySequence();
  p.enableClipboardHotkey  = ui.enableClipboardHotkey->isChecked();
  p.clipboardHotkey        = ui.clipboardHotkey->keySequence();

  p.startWithScanPopupOn     = ui.startWithScanPopupOn->isChecked();
  p.enableScanPopupModifiers = ui.enableScanPopupModifiers->isChecked();

  p.scanPopupModifiers += ui.altKey->isChecked() ? KeyboardState::Alt : 0;
  p.scanPopupModifiers += ui.ctrlKey->isChecked() ? KeyboardState::Ctrl : 0;
  p.scanPopupModifiers += ui.shiftKey->isChecked() ? KeyboardState::Shift : 0;
  p.scanPopupModifiers += ui.winKey->isChecked() ? KeyboardState::Win : 0;

  p.ignoreOwnClipboardChanges = ui.ignoreOwnClipboardChanges->isChecked();
  p.scanToMainWindow          = ui.scanToMainWindow->isChecked();
#ifdef HAVE_X11
  p.trackSelectionScan        = ui.enableX11SelectionTrack->isChecked();
  p.trackClipboardScan        = ui.enableClipboardTrack->isChecked();
  p.showScanFlag              = ui.showScanFlag->isChecked();
  p.selectionChangeDelayTimer = ui.delayTimer->value();
#endif

  p.storeHistory              = ui.storeHistory->isChecked();
  p.maxStringsInHistory       = ui.historyMaxSizeField->text().toUInt();
  p.historyStoreInterval      = ui.historySaveIntervalField->text().toUInt();
  p.alwaysExpandOptionalParts = ui.alwaysExpandOptionalParts->isChecked();

  p.favoritesStoreInterval   = ui.favoritesSaveIntervalField->text().toUInt();
  p.confirmFavoritesDeletion = ui.confirmFavoritesDeletion->isChecked();

  p.collapseBigArticles    = ui.collapseBigArticles->isChecked();
  p.articleSizeLimit       = ui.articleSizeLimit->value();
  p.limitInputPhraseLength = ui.limitInputPhraseLength->isChecked();
  p.inputPhraseLengthLimit = ui.inputPhraseLengthLimit->value();
  p.ignoreDiacritics       = ui.ignoreDiacritics->isChecked();
  p.ignorePunctuation      = ui.ignorePunctuation->isChecked();
  p.sessionCollapse        = ui.sessionCollapse->isChecked();
  p.stripClipboard         = ui.stripClipboard->isChecked();
  p.raiseWindowOnSearch    = ui.raiseWindowOnSearch->isChecked();

  p.synonymSearchEnabled = ui.synonymSearchEnabled->isChecked();

  p.maxDictionaryRefsInContextMenu = ui.maxDictsInContextMenu->text().toInt();

  p.pronounceOnLoadMain  = ui.pronounceOnLoadMain->isChecked();
  p.pronounceOnLoadPopup = ui.pronounceOnLoadPopup->isChecked();
  p.useInternalPlayer    = ui.useInternalPlayer->isChecked();
  p.internalPlayerBackend.setName( ui.internalPlayerBackend->currentText() );
  p.audioPlaybackProgram = ui.audioPlaybackProgram->text();

  p.proxyServer.enabled        = ui.useProxyServer->isChecked();
  p.proxyServer.useSystemProxy = ui.systemProxy->isChecked();

  p.proxyServer.type = (Config::ProxyServer::Type)ui.proxyType->currentIndex();

  p.proxyServer.host = ui.proxyHost->text();
  p.proxyServer.port = (unsigned)ui.proxyPort->value();

  p.proxyServer.user     = ui.proxyUser->text();
  p.proxyServer.password = ui.proxyPassword->text();

  //anki connect
  p.ankiConnectServer.enabled = ui.useAnkiConnect->isChecked();
  p.ankiConnectServer.host    = ui.ankiHost->text();
  p.ankiConnectServer.port    = (unsigned)ui.ankiPort->value();
  p.ankiConnectServer.deck    = ui.ankiDeck->text();
  p.ankiConnectServer.model   = ui.ankiModel->text();
  //anki connect fields
  p.ankiConnectServer.text     = ui.ankiText->text();
  p.ankiConnectServer.word     = ui.ankiWord->text();
  p.ankiConnectServer.sentence = ui.ankiSentence->text();

  p.checkForNewReleases           = ui.checkForNewReleases->isChecked();
  p.disallowContentFromOtherSites = ui.disallowContentFromOtherSites->isChecked();
  p.hideGoldenDictHeader          = ui.hideGoldenDictHeader->isChecked();
  p.maxNetworkCacheSize           = ui.maxNetworkCacheSize->value();
  p.clearNetworkCacheOnExit       = ui.clearNetworkCacheOnExit->isChecked();

  p.removeInvalidIndexOnExit = ui.removeInvalidIndexOnExit->isChecked();
  p.enableApplicationLog     = ui.enableApplicationLog->isChecked();

  p.addonStyle = ui.addonStyles->getCurrentStyle();

  p.fts.enabled           = ui.ftsGroupBox->isChecked();
  p.fts.maxDictionarySize = ui.maxDictionarySize->value();
  p.fts.parallelThreads   = ui.parallelThreads->value();

  buildDisabledTypes( p.fts.disabledTypes, ui.allowAard->isChecked(), "AARD" );
  buildDisabledTypes( p.fts.disabledTypes, ui.allowBGL->isChecked(), "BGL" );
  buildDisabledTypes( p.fts.disabledTypes, ui.allowDictD->isChecked(), "DICTD" );
  buildDisabledTypes( p.fts.disabledTypes, ui.allowDSL->isChecked(), "DSL" );
  buildDisabledTypes( p.fts.disabledTypes, ui.allowMDict->isChecked(), "MDICT" );
  buildDisabledTypes( p.fts.disabledTypes, ui.allowSDict->isChecked(), "SDICT" );
  buildDisabledTypes( p.fts.disabledTypes, ui.allowSlob->isChecked(), "SLOB" );
  buildDisabledTypes( p.fts.disabledTypes, ui.allowStardict->isChecked(), "STARDICT" );
  buildDisabledTypes( p.fts.disabledTypes, ui.allowXDXF->isChecked(), "XDXF" );
  buildDisabledTypes( p.fts.disabledTypes, ui.allowZim->isChecked(), "ZIM" );
  buildDisabledTypes( p.fts.disabledTypes, ui.allowEpwing->isChecked(), "EPWING" );
  buildDisabledTypes( p.fts.disabledTypes, ui.allowGls->isChecked(), "GLS" );

  return p;
}

void Preferences::enableScanPopupModifiersToggled( bool b )
{
  ui.scanPopupModifiers->setEnabled( b );
  if ( b ) {
    ui.showScanFlag->setChecked( false );
  }
}

void Preferences::showScanFlagToggled( bool b )
{
  if ( b ) {
    ui.enableScanPopupModifiers->setChecked( false );
  }
}

void Preferences::on_enableMainWindowHotkey_toggled( bool checked )
{
  ui.mainWindowHotkey->setEnabled( checked );
}

void Preferences::on_enableClipboardHotkey_toggled( bool checked )
{
  ui.clipboardHotkey->setEnabled( checked );
}

void Preferences::on_buttonBox_accepted()
{
  QString promptText;

  if ( prevInterfaceLanguage != ui.interfaceLanguage->currentIndex() ) {
    promptText = tr( "Restart the program to apply the language change." );
    promptText += "\n";
  }

#if !defined( Q_OS_WIN )
  if ( prevInterfaceStyle != ui.InterfaceStyle->currentIndex() ) {
    promptText += tr( "Restart to apply the interface style change." );
    promptText += "\n";
  }
#endif

  if ( ui.systemFont->currentText() != prevSysFont ) {
    promptText += tr( "Restart to apply the interface font change." );
  }

  if ( !promptText.isEmpty() ) {
    QMessageBox::information( this, tr( "Restart needed" ), promptText );
  }

  auto c = getPreferences();
  if ( c.customFonts != prevWebFontFamily ) {
    QWebEngineProfile::defaultProfile()->settings()->setFontFamily( QWebEngineSettings::StandardFont,
                                                                    c.customFonts.standard );
    QWebEngineProfile::defaultProfile()->settings()->setFontFamily( QWebEngineSettings::SerifFont,
                                                                    c.customFonts.serif );
    QWebEngineProfile::defaultProfile()->settings()->setFontFamily( QWebEngineSettings::SansSerifFont,
                                                                    c.customFonts.sansSerif );
    QWebEngineProfile::defaultProfile()->settings()->setFontFamily( QWebEngineSettings::FixedFont,
                                                                    c.customFonts.monospace );
  }

  //change interface font.
  if ( ui.systemFont->currentText() != prevSysFont ) {
    auto font = QApplication::font();
    font.setFamily( ui.systemFont->currentText() );
    QApplication::setFont( font );
  }
}

void Preferences::on_useExternalPlayer_toggled( bool enabled )
{
  ui.internalPlayerBackend->setEnabled( !enabled );
  ui.audioPlaybackProgram->setEnabled( enabled );
}

void Preferences::customProxyToggled( bool )
{
  ui.customSettingsGroup->setEnabled( ui.customProxy->isChecked() && ui.useProxyServer->isChecked() );
}

void Preferences::on_maxNetworkCacheSize_valueChanged( int value )
{
  ui.clearNetworkCacheOnExit->setEnabled( value != 0 );
}

void Preferences::on_collapseBigArticles_toggled( bool checked )
{
  ui.articleSizeLimit->setEnabled( checked );
}

void Preferences::on_limitInputPhraseLength_toggled( bool checked )
{
  ui.inputPhraseLengthLimit->setEnabled( checked );
}
