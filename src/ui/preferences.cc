#include "keyboardstate.hh"
#include "language.hh"
#include "preferences.hh"
#include "help.hh"

#include <QDebug>
#include <QDir>
#include <QFontDatabase>
#include <QMessageBox>
#include <QWebEngineProfile>
#include <QWebEngineSettings>

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

  connect( ui.altKey, &QAbstractButton::clicked, this, &Preferences::wholeAltClicked );
  connect( ui.ctrlKey, &QAbstractButton::clicked, this, &Preferences::wholeCtrlClicked );
  connect( ui.shiftKey, &QAbstractButton::clicked, this, &Preferences::wholeShiftClicked );

  connect( ui.leftAlt, &QAbstractButton::clicked, this, &Preferences::sideAltClicked );
  connect( ui.rightAlt, &QAbstractButton::clicked, this, &Preferences::sideAltClicked );
  connect( ui.leftCtrl, &QAbstractButton::clicked, this, &Preferences::sideCtrlClicked );
  connect( ui.rightCtrl, &QAbstractButton::clicked, this, &Preferences::sideCtrlClicked );
  connect( ui.leftShift, &QAbstractButton::clicked, this, &Preferences::sideShiftClicked );
  connect( ui.rightShift, &QAbstractButton::clicked, this, &Preferences::sideShiftClicked );


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
  //  ui.fontFamilies->insertItem(0, tr( "System default" ), QString() );

  // See which other translations do we have

  QStringList availLocs = QDir( Config::getLocDir() ).entryList( QStringList( "*.qm" ),
                                                                 QDir::Files );

  // We need to sort by language name -- otherwise list looks really weird
  QMultiMap< QString, QString > sortedLocs;
  sortedLocs.insert( Language::languageForLocale( "en_US" ), "en_US");
  for ( const auto & availLoc : availLocs ) {
    // Here we assume the xx_YY naming, where xx is language and YY is region.
    //remove .qm suffix.
    QString locale = availLoc.left( availLoc.size() - 3 );

    if ( locale == "qt" )
      continue; // We skip qt's own localizations

    auto language = Language::languageForLocale( locale );
    if ( language.isEmpty() ) {
      qWarning() << "can not found the corresponding language from locale:" << locale;
    }
    else {
      sortedLocs.insert( language, locale );
    }
  }

  for ( auto i = sortedLocs.begin(); i != sortedLocs.end(); ++i ) {
    ui.interfaceLanguage->addItem(  i.key(), i.value() );
  }

  for( int x = 0; x < ui.interfaceLanguage->count(); ++x ) {
    if ( ui.interfaceLanguage->itemData( x ).toString() == p.interfaceLanguage )
    {
      ui.interfaceLanguage->setCurrentIndex( x );
      prevInterfaceLanguage = x;
      break;
    }
  }

  prevWebFontFamily = p.webFontFamily;

  if(!p.webFontFamily.isEmpty())
    ui.fontFamilies->setCurrentText( p.webFontFamily );
  else {
    ui.fontFamilies->setCurrentFont( QApplication::font() );
  }

  ui.displayStyle->addItem( QIcon( ":/icons/programicon_old.png" ), tr( "Default" ), QString() );
  ui.displayStyle->addItem( QIcon( ":/icons/programicon.png" ), tr( "Classic" ), QString( "classic" ) );
  ui.displayStyle->addItem( QIcon( ":/icons/programicon.png" ), tr( "Modern" ), QString( "modern" ) );
  ui.displayStyle->addItem( QIcon( ":/icons/icon32_dsl.png" ), tr( "Lingvo" ), QString( "lingvo" ) );
  ui.displayStyle->addItem( QIcon( ":/icons/icon32_bgl.png" ), tr( "Babylon" ), QString( "babylon" ) );
  ui.displayStyle->addItem( QIcon( ":/icons/icon32_lingoes.png" ), tr( "Lingoes" ), QString( "lingoes" ) );
  ui.displayStyle->addItem( QIcon( ":/icons/icon32_lingoes.png" ), tr( "Lingoes-Blue" ), QString( "lingoes-blue" ) );

  for( int x = 0; x < ui.displayStyle->count(); ++x )
    if ( ui.displayStyle->itemData( x ).toString() == p.displayStyle )
    {
      ui.displayStyle->setCurrentIndex( x );
      break;
    }

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
  ui.startToTray->setChecked( p.startToTray );
  ui.closeToTray->setChecked( p.closeToTray );
  ui.cbAutostart->setChecked( p.autoStart );
  ui.doubleClickTranslates->setChecked( p.doubleClickTranslates );
  ui.selectBySingleClick->setChecked( p.selectWordBySingleClick);
  ui.autoScrollToTargetArticle->setChecked( p.autoScrollToTargetArticle );
  ui.escKeyHidesMainWindow->setChecked( p.escKeyHidesMainWindow );
  ui.darkMode->setChecked(p.darkMode);
  ui.darkReaderMode -> setChecked(p.darkReaderMode);
#ifndef Q_OS_WIN32
  ui.darkMode->hide();
#endif

  ui.enableMainWindowHotkey->setChecked( p.enableMainWindowHotkey );
  ui.mainWindowHotkey->setKeySequence( p.mainWindowHotkey );
  ui.enableClipboardHotkey->setChecked( p.enableClipboardHotkey );
  ui.clipboardHotkey->setKeySequence( p.clipboardHotkey );

  ui.startWithScanPopupOn->setChecked( p.startWithScanPopupOn );
  ui.enableScanPopupModifiers->setChecked( p.enableScanPopupModifiers );

  ui.altKey->setChecked( p.scanPopupModifiers & KeyboardState::Alt );
  ui.ctrlKey->setChecked( p.scanPopupModifiers & KeyboardState::Ctrl );
  ui.shiftKey->setChecked( p.scanPopupModifiers & KeyboardState::Shift );
  ui.winKey->setChecked( p.scanPopupModifiers & KeyboardState::Win );
  ui.leftAlt->setChecked( p.scanPopupModifiers & KeyboardState::LeftAlt );
  ui.rightAlt->setChecked( p.scanPopupModifiers & KeyboardState::RightAlt );
  ui.leftCtrl->setChecked( p.scanPopupModifiers & KeyboardState::LeftCtrl );
  ui.rightCtrl->setChecked( p.scanPopupModifiers & KeyboardState::RightCtrl );
  ui.leftShift->setChecked( p.scanPopupModifiers & KeyboardState::LeftShift );
  ui.rightShift->setChecked( p.scanPopupModifiers & KeyboardState::RightShift );

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
  ui.leftAlt->hide();
  ui.rightAlt->hide();
  ui.leftCtrl->hide();
  ui.rightCtrl->hide();
  ui.leftShift->hide();
  ui.rightShift->hide();
#ifdef Q_OS_MAC
  ui.altKey->setText( "Opt" );
  ui.winKey->setText( "Ctrl" );
  ui.ctrlKey->setText( "Cmd" );
#endif
#endif

//Platform-specific options

#ifdef HAVE_X11
  ui.enableX11SelectionTrack->setChecked(p.trackSelectionScan);
  ui.enableClipboardTrack ->setChecked(p.trackClipboardScan);
  ui.showScanFlag->setChecked(p.showScanFlag);
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

  ui.internalPlayerBackend->addItems( Config::InternalPlayerBackend::nameList() );

  // Make sure that exactly one radio button in the group is checked and that
  // on_useExternalPlayer_toggled() is called.
  ui.useExternalPlayer->setChecked( true );

  if( ui.internalPlayerBackend->count() > 0 )
  {
    // Checking ui.useInternalPlayer automatically unchecks ui.useExternalPlayer.
    ui.useInternalPlayer->setChecked( p.useInternalPlayer );

    int index = ui.internalPlayerBackend->findText( p.internalPlayerBackend.uiName() );
    if( index < 0 ) // The specified backend is unavailable.
      index = ui.internalPlayerBackend->findText( Config::InternalPlayerBackend::defaultBackend().uiName() );
    Q_ASSERT( index >= 0 && "Logic error: the default backend must be present in the backend name list." );
    ui.internalPlayerBackend->setCurrentIndex( index );
  }
  else
  {
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

  if( p.proxyServer.useSystemProxy )
  {
    ui.systemProxy->setChecked( true );
    ui.customSettingsGroup->setEnabled( false );
  }
  else
  {
    ui.customProxy->setChecked( true );
    ui.customSettingsGroup->setEnabled( p.proxyServer.enabled );
  }

  //anki connect
  ui.useAnkiConnect->setChecked( p.ankiConnectServer.enabled );
  ui.ankiHost->setText( p.ankiConnectServer.host );
  ui.ankiPort->setValue( p.ankiConnectServer.port );
  ui.ankiModel->setText( p.ankiConnectServer.model );
  ui.ankiDeck->setText(p.ankiConnectServer.deck);
  //anki connect fields
  ui.ankiText->setText(p.ankiConnectServer.text);
  ui.ankiWord->setText(p.ankiConnectServer.word);
  ui.ankiSentence->setText(p.ankiConnectServer.sentence);

  connect( ui.customProxy, &QAbstractButton::toggled, this, &Preferences::customProxyToggled );

  connect( ui.useProxyServer, &QGroupBox::toggled, this, &Preferences::customProxyToggled );

  ui.checkForNewReleases->setChecked( p.checkForNewReleases );
  ui.disallowContentFromOtherSites->setChecked( p.disallowContentFromOtherSites );
  ui.hideGoldenDictHeader->setChecked( p.hideGoldenDictHeader );
  ui.maxNetworkCacheSize->setValue( p.maxNetworkCacheSize );
  ui.clearNetworkCacheOnExit->setChecked( p.clearNetworkCacheOnExit );

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
  ui.allowSlob->hide();
#endif
#ifdef NO_EPWING_SUPPORT
  ui.allowEpwing->hide();
#endif
  ui.maxDictionarySize->setValue( p.fts.maxDictionarySize );
}

void Preferences::buildDisabledTypes( QString & disabledTypes, bool is_checked, QString name )
{
  if ( !is_checked ) {
    if ( !disabledTypes.isEmpty() )
      disabledTypes += ',';
    disabledTypes += name;
  }
}

Config::Preferences Preferences::getPreferences()
{
  Config::Preferences p;

  p.interfaceLanguage =
    ui.interfaceLanguage->itemData(
      ui.interfaceLanguage->currentIndex() ).toString();

  //bypass the first default
  if(ui.fontFamilies->currentIndex()>0)
    p.webFontFamily = ui.fontFamilies->currentText();
  else
    p.webFontFamily = "";

  p.displayStyle =
    ui.displayStyle->itemData(
      ui.displayStyle->currentIndex() ).toString();

  p.newTabsOpenAfterCurrentOne = ui.newTabsOpenAfterCurrentOne->isChecked();
  p.newTabsOpenInBackground = ui.newTabsOpenInBackground->isChecked();
  p.hideSingleTab = ui.hideSingleTab->isChecked();
  p.mruTabOrder = ui.mruTabOrder->isChecked();
  p.enableTrayIcon = ui.enableTrayIcon->isChecked();
  p.startToTray = ui.startToTray->isChecked();
  p.closeToTray = ui.closeToTray->isChecked();
  p.autoStart = ui.cbAutostart->isChecked();
  p.doubleClickTranslates = ui.doubleClickTranslates->isChecked();
  p.selectWordBySingleClick = ui.selectBySingleClick->isChecked();
  p.autoScrollToTargetArticle = ui.autoScrollToTargetArticle->isChecked();
  p.escKeyHidesMainWindow = ui.escKeyHidesMainWindow->isChecked();

  p.darkMode = ui.darkMode->isChecked();
  p.darkReaderMode = ui.darkReaderMode->isChecked();
  p.enableMainWindowHotkey = ui.enableMainWindowHotkey->isChecked();
  p.mainWindowHotkey = ui.mainWindowHotkey->keySequence();
  p.enableClipboardHotkey = ui.enableClipboardHotkey->isChecked();
  p.clipboardHotkey = ui.clipboardHotkey->keySequence();

  p.startWithScanPopupOn = ui.startWithScanPopupOn->isChecked();
  p.enableScanPopupModifiers = ui.enableScanPopupModifiers->isChecked();

  p.scanPopupModifiers += ui.altKey->isChecked() ? KeyboardState::Alt : 0;
  p.scanPopupModifiers += ui.ctrlKey->isChecked() ? KeyboardState::Ctrl: 0;
  p.scanPopupModifiers += ui.shiftKey->isChecked() ? KeyboardState::Shift: 0;
  p.scanPopupModifiers += ui.winKey->isChecked() ? KeyboardState::Win: 0;
  p.scanPopupModifiers += ui.leftAlt->isChecked() ? KeyboardState::LeftAlt: 0;
  p.scanPopupModifiers += ui.rightAlt->isChecked() ? KeyboardState::RightAlt: 0;
  p.scanPopupModifiers += ui.leftCtrl->isChecked() ? KeyboardState::LeftCtrl: 0;
  p.scanPopupModifiers += ui.rightCtrl->isChecked() ? KeyboardState::RightCtrl: 0;
  p.scanPopupModifiers += ui.leftShift->isChecked() ? KeyboardState::LeftShift: 0;
  p.scanPopupModifiers += ui.rightShift->isChecked() ? KeyboardState::RightShift: 0;

  p.ignoreOwnClipboardChanges = ui.ignoreOwnClipboardChanges->isChecked();
  p.scanToMainWindow = ui.scanToMainWindow->isChecked();
#ifdef HAVE_X11
  p.trackSelectionScan = ui.enableX11SelectionTrack ->isChecked();
  p.trackClipboardScan = ui.enableClipboardTrack ->isChecked();
  p.showScanFlag= ui.showScanFlag->isChecked();
  p.selectionChangeDelayTimer = ui.delayTimer->value();
#endif

  p.storeHistory = ui.storeHistory->isChecked();
  p.maxStringsInHistory = ui.historyMaxSizeField->text().toUInt();
  p.historyStoreInterval = ui.historySaveIntervalField->text().toUInt();
  p.alwaysExpandOptionalParts = ui.alwaysExpandOptionalParts->isChecked();

  p.favoritesStoreInterval = ui.favoritesSaveIntervalField->text().toUInt();
  p.confirmFavoritesDeletion = ui.confirmFavoritesDeletion->isChecked();

  p.collapseBigArticles = ui.collapseBigArticles->isChecked();
  p.articleSizeLimit = ui.articleSizeLimit->value();
  p.limitInputPhraseLength = ui.limitInputPhraseLength->isChecked();
  p.inputPhraseLengthLimit = ui.inputPhraseLengthLimit->value();
  p.ignoreDiacritics = ui.ignoreDiacritics->isChecked();
  p.ignorePunctuation = ui.ignorePunctuation->isChecked();
  p.sessionCollapse        = ui.sessionCollapse->isChecked();
  p.stripClipboard = ui.stripClipboard->isChecked();
  p.raiseWindowOnSearch = ui.raiseWindowOnSearch->isChecked();

  p.synonymSearchEnabled = ui.synonymSearchEnabled->isChecked();

  p.maxDictionaryRefsInContextMenu = ui.maxDictsInContextMenu->text().toInt();

  p.pronounceOnLoadMain = ui.pronounceOnLoadMain->isChecked();
  p.pronounceOnLoadPopup = ui.pronounceOnLoadPopup->isChecked();
  p.useInternalPlayer = ui.useInternalPlayer->isChecked();
  p.internalPlayerBackend.setUiName( ui.internalPlayerBackend->currentText() );
  p.audioPlaybackProgram = ui.audioPlaybackProgram->text();

  p.proxyServer.enabled = ui.useProxyServer->isChecked();
  p.proxyServer.useSystemProxy = ui.systemProxy->isChecked();

  p.proxyServer.type = ( Config::ProxyServer::Type ) ui.proxyType->currentIndex();

  p.proxyServer.host = ui.proxyHost->text();
  p.proxyServer.port = ( unsigned ) ui.proxyPort->value();

  p.proxyServer.user = ui.proxyUser->text();
  p.proxyServer.password = ui.proxyPassword->text();

  //anki connect
  p.ankiConnectServer.enabled = ui.useAnkiConnect->isChecked();
  p.ankiConnectServer.host    = ui.ankiHost->text();
  p.ankiConnectServer.port    = (unsigned)ui.ankiPort->value();
  p.ankiConnectServer.deck = ui.ankiDeck->text();
  p.ankiConnectServer.model = ui.ankiModel->text();
  //anki connect fields
  p.ankiConnectServer.text = ui.ankiText->text();
  p.ankiConnectServer.word = ui.ankiWord->text();
  p.ankiConnectServer.sentence = ui.ankiSentence->text();

  p.checkForNewReleases = ui.checkForNewReleases->isChecked();
  p.disallowContentFromOtherSites = ui.disallowContentFromOtherSites->isChecked();
  p.hideGoldenDictHeader = ui.hideGoldenDictHeader->isChecked();
  p.maxNetworkCacheSize = ui.maxNetworkCacheSize->value();
  p.clearNetworkCacheOnExit = ui.clearNetworkCacheOnExit->isChecked();

  p.addonStyle = ui.addonStyles->getCurrentStyle();

  p.fts.enabled = ui.ftsGroupBox->isChecked();
  p.fts.maxDictionarySize = ui.maxDictionarySize->value();

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
  if( b )
    ui.showScanFlag->setChecked( false );
}

void Preferences::showScanFlagToggled( bool b )
{
  if( b )
    ui.enableScanPopupModifiers->setChecked( false );
}



void Preferences::wholeAltClicked( bool b )
{
  if ( b )
  {
    ui.leftAlt->setChecked( false );
    ui.rightAlt->setChecked( false );
  }
}

void Preferences::wholeCtrlClicked( bool b )
{
  if ( b )
  {
    ui.leftCtrl->setChecked( false );
    ui.rightCtrl->setChecked( false );
  }
}

void Preferences::wholeShiftClicked( bool b )
{
  if ( b )
  {
    ui.leftShift->setChecked( false );
    ui.rightShift->setChecked( false );
  }
}

void Preferences::sideAltClicked( bool )
{
  if ( ui.leftAlt->isChecked() || ui.rightAlt->isChecked() )
    ui.altKey->setChecked( false );
}

void Preferences::sideCtrlClicked( bool )
{
  if ( ui.leftCtrl->isChecked() || ui.rightCtrl->isChecked() )
    ui.ctrlKey->setChecked( false );
}

void Preferences::sideShiftClicked( bool )
{
  if ( ui.leftShift->isChecked() || ui.rightShift->isChecked() )
    ui.shiftKey->setChecked( false );
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
  if ( prevInterfaceLanguage != ui.interfaceLanguage->currentIndex() )
    QMessageBox::information( this, tr( "Changing Language" ),
                              tr( "Restart the program to apply the language change." ) );

  auto currentFontFamily = ui.fontFamilies->currentFont().family();
  if( prevWebFontFamily != currentFontFamily )
  {
    //reset to default font .
    if( currentFontFamily.isEmpty() )
    {
      QWebEngineProfile::defaultProfile()->settings()->resetFontFamily( QWebEngineSettings::StandardFont );
    }
    else
    {
      QWebEngineProfile::defaultProfile()->settings()->setFontFamily( QWebEngineSettings::StandardFont,
                                                                      currentFontFamily );
    }
  }
}

void Preferences::on_useExternalPlayer_toggled( bool enabled )
{
  ui.internalPlayerBackend->setEnabled( !enabled );
  ui.audioPlaybackProgram->setEnabled( enabled );
}

void Preferences::customProxyToggled( bool )
{
  ui.customSettingsGroup->setEnabled( ui.customProxy->isChecked()
                                      && ui.useProxyServer->isChecked() );
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
