/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include <Qt>
#include <QScopeGuard>
#ifdef EPWING_SUPPORT
  #include "dict/epwing_book.hh"
#endif

#include "mainwindow.hh"
#include <QWebEngineProfile>
#include "edit_dictionaries.hh"
#include "dict/loaddictionaries.hh"
#include "preferences.hh"
#include "about.hh"
#include "mruqmenu.hh"
#include "gestures.hh"
#include "dictheadwords.hh"
#include <QTextStream>
#include <QDir>
#include <QUrl>
#include <QMessageBox>
#include <QIcon>
#include <QList>
#include <QToolBar>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QProcess>
#include <QCryptographicHash>
#include <QFileDialog>
#include <QPrinter>
#include <QPageSetupDialog>
#include <QPrintPreviewDialog>
#include <QPrintDialog>
#include <QRunnable>
#include <QThreadPool>
#include <QSslConfiguration>
#include <QStyleFactory>
#include <QStyleHints>

#include "weburlrequestinterceptor.hh"
#include "folding.hh"

#include <set>
#include <map>

#include "dictinfo.hh"
#include "historypanewidget.hh"
#include "utils.hh"
#include "help.hh"
#include "ui_authentication.h"
#include "resourceschemehandler.hh"
#include <QListWidgetItem>

#include "globalregex.hh"

#ifdef Q_OS_MAC
  #include "macos/macmouseover.hh"
#endif

#ifdef Q_OS_WIN32
  #include <windows.h>
#endif

#include <QGuiApplication>
#include <QWebEngineSettings>
#include <QProxyStyle>
#include <QShortcut>

#ifdef HAVE_X11
  #if ( QT_VERSION >= QT_VERSION_CHECK( 6, 0, 0 ) )
    #include <QGuiApplication>
  #else
    #include <QX11Info>
  #endif
  #include <X11/Xlib.h>
  #include <fixx11h.h>
#endif

#define MIN_THREAD_COUNT 4

using std::set;
using std::map;
using std::pair;

#ifndef QT_NO_SSL

class InitSSLRunnable: public QRunnable
{
  void run() override
  {
    /// This action force SSL library initialisation which may continue a few seconds
    QSslConfiguration::setDefaultConfiguration( QSslConfiguration::defaultConfiguration() );
  }
};

#endif

namespace {
QString ApplicationSettingName = "GoldenDict";
}

void MainWindow::changeWebEngineViewFont() const
{
  if ( cfg.preferences.customFonts.standard.isEmpty() ) {
    QWebEngineProfile::defaultProfile()->settings()->resetFontFamily( QWebEngineSettings::StandardFont );
  }
  else {
    QWebEngineProfile::defaultProfile()->settings()->setFontFamily( QWebEngineSettings::StandardFont,
                                                                    cfg.preferences.customFonts.standard );
  }

  if ( cfg.preferences.customFonts.serif.isEmpty() ) {
    QWebEngineProfile::defaultProfile()->settings()->resetFontFamily( QWebEngineSettings::SerifFont );
  }
  else {
    QWebEngineProfile::defaultProfile()->settings()->setFontFamily( QWebEngineSettings::SerifFont,
                                                                    cfg.preferences.customFonts.serif );
  }

  if ( cfg.preferences.customFonts.sansSerif.isEmpty() ) {
    QWebEngineProfile::defaultProfile()->settings()->resetFontFamily( QWebEngineSettings::SansSerifFont );
  }
  else {
    QWebEngineProfile::defaultProfile()->settings()->setFontFamily( QWebEngineSettings::SansSerifFont,
                                                                    cfg.preferences.customFonts.sansSerif );
  }

  if ( cfg.preferences.customFonts.monospace.isEmpty() ) {
    QWebEngineProfile::defaultProfile()->settings()->resetFontFamily( QWebEngineSettings::FixedFont );
  }
  else {
    QWebEngineProfile::defaultProfile()->settings()->setFontFamily( QWebEngineSettings::FixedFont,
                                                                    cfg.preferences.customFonts.monospace );
  }
}

MainWindow::MainWindow( Config::Class & cfg_ ):
  trayIcon( nullptr ),
  foundInDictsLabel( &dictsPaneTitleBar ),
  escAction( this ),
  focusTranslateLineAction( this ),
  addTabAction( this ),
  closeCurrentTabAction( this ),
  closeAllTabAction( this ),
  closeRestTabAction( this ),
  switchToNextTabAction( this ),
  switchToPrevTabAction( this ),
  showDictBarNamesAction( tr( "Show Names in Dictionary &Bar" ), this ),
  useSmallIconsInToolbarsAction( tr( "Show &Small Icons in Toolbars" ), this ),
  useLargeIconsInToolbarsAction( tr( "Show &Large Icons in Toolbars" ), this ),
  useNormalIconsInToolbarsAction( tr( "Show &Normal Icons in Toolbars" ), this ),
  toggleMenuBarAction( tr( "&Menubar" ), this ),
  focusHeadwordsDlgAction( this ),
  focusArticleViewAction( this ),
  addAllTabToFavoritesAction( this ),
  stopAudioAction( this ),
  trayIconMenu( this ),
  addTab( this ),
  cfg( cfg_ ),
  history( cfg_.preferences.maxStringsInHistory, cfg_.maxHeadwordSize ),
  dictionaryBar( this, configEvents, cfg.preferences.maxDictionaryRefsInContextMenu ),
  articleMaker( dictionaries, groupInstances, cfg.preferences ),
  articleNetMgr( this,
                 dictionaries,
                 articleMaker,
                 cfg.preferences.disallowContentFromOtherSites,
                 cfg.preferences.hideGoldenDictHeader ),
  dictNetMgr( this ),
  audioPlayerFactory(
    cfg.preferences.useInternalPlayer, cfg.preferences.internalPlayerBackend, cfg.preferences.audioPlaybackProgram ),
  wordFinder( this ),
  wordListSelChanged( false ),
  wasMaximized( false ),
  headwordsDlg( nullptr ),
  ftsIndexing( dictionaries ),
  ftsDlg( nullptr ),
  starIcon( ":/icons/star.svg" ),
  blueStarIcon( ":/icons/star_blue.svg" )
{
  if ( QThreadPool::globalInstance()->maxThreadCount() < MIN_THREAD_COUNT ) {
    QThreadPool::globalInstance()->setMaxThreadCount( MIN_THREAD_COUNT );
  }

#ifndef QT_NO_SSL
  QThreadPool::globalInstance()->start( new InitSSLRunnable );
#endif

  GlobalBroadcaster::instance()->setPreference( &cfg.preferences );

  localSchemeHandler     = new LocalSchemeHandler( articleNetMgr, this );
  QStringList htmlScheme = { "gdlookup", "bword", "entry" };
  for ( const auto & localScheme : htmlScheme ) {
    QWebEngineProfile::defaultProfile()->installUrlSchemeHandler( localScheme.toLatin1(), localSchemeHandler );
  }

  iframeSchemeHandler = new IframeSchemeHandler( this );
  QWebEngineProfile::defaultProfile()->installUrlSchemeHandler( "ifr", iframeSchemeHandler );

  QStringList localSchemes = { "gdau", "gico", "qrcx", "bres", "gdprg", "gdvideo", "gdtts" };
  resourceSchemeHandler    = new ResourceSchemeHandler( articleNetMgr, this );
  for ( const auto & localScheme : localSchemes ) {
    QWebEngineProfile::defaultProfile()->installUrlSchemeHandler( localScheme.toLatin1(), resourceSchemeHandler );
  }

  QWebEngineProfile::defaultProfile()->setUrlRequestInterceptor( new WebUrlRequestInterceptor( this ) );

  if ( !cfg.preferences.hideGoldenDictHeader ) {
    QWebEngineProfile::defaultProfile()->setHttpUserAgent( QWebEngineProfile::defaultProfile()->httpUserAgent()
                                                           + " GoldenDict/WebEngine" );
  }

#ifdef EPWING_SUPPORT
  Epwing::initialize();
#endif

  ui.setupUi( this );

  // Set own gesture recognizers
#ifndef Q_OS_MAC
  Gestures::registerRecognizers();
#endif
  // use our own, custom statusbar
  setStatusBar( nullptr );
  mainStatusBar = new MainStatusBar( this );

  // Make the toolbar
  navToolbar = addToolBar( tr( "&Navigation" ) );
  navToolbar->setObjectName( "navToolbar" );

  navBack = navToolbar->addAction( QIcon( ":/icons/previous.svg" ), tr( "Back" ) );
  navToolbar->widgetForAction( navBack )->setObjectName( "backButton" );
  navForward = navToolbar->addAction( QIcon( ":/icons/next.svg" ), tr( "Forward" ) );
  navToolbar->widgetForAction( navForward )->setObjectName( "forwardButton" );

  QWidget * translateBoxWidget     = new QWidget( this );
  QHBoxLayout * translateBoxLayout = new QHBoxLayout( translateBoxWidget );
  translateBoxWidget->setLayout( translateBoxLayout );
  translateBoxLayout->setContentsMargins( 0, 0, 0, 0 );
  translateBoxLayout->setSpacing( 0 );

  // translate box
  groupListInToolbar = new GroupComboBox( navToolbar );
  groupListInToolbar->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::MinimumExpanding );
  groupListInToolbar->setSizeAdjustPolicy( QComboBox::AdjustToContents );
  translateBoxLayout->addWidget( groupListInToolbar );

  translateBox = new TranslateBox( navToolbar );
  translateBox->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::MinimumExpanding );
  translateBoxLayout->addWidget( translateBox );
  translateBoxToolBarAction = navToolbar->addWidget( translateBoxWidget );

  // popup
  navToolbar->addSeparator();

  enableScanningAction = navToolbar->addAction( QIcon( ":/icons/wizard.svg" ), tr( "Toggle clipboard monitoring" ) );
  enableScanningAction->setCheckable( true );

  navToolbar->widgetForAction( enableScanningAction )->setObjectName( "scanPopupButton" );

  navToolbar->addSeparator();

  // sound
  navPronounce = navToolbar->addAction( QIcon( ":/icons/playsound_full.png" ), tr( "Pronounce Word (Alt+S)" ) );
  navPronounce->setShortcut( QKeySequence( "Alt+S" ) );
  navPronounce->setEnabled( false );
  navToolbar->widgetForAction( navPronounce )->setObjectName( "soundButton" );

  connect( navPronounce, &QAction::triggered, [ this ]() {
    getCurrentArticleView()->playSound();
  } );

  // zooming
  // named separator (to be able to hide it via CSS)
  navToolbar->widgetForAction( navToolbar->addSeparator() )->setObjectName( "separatorBeforeZoom" );

  zoomIn = navToolbar->addAction( QIcon( ":/icons/icon32_zoomin.png" ), tr( "Zoom In" ) );
  zoomIn->setShortcuts( QList< QKeySequence >() << QKeySequence::ZoomIn << QKeySequence( "Ctrl+=" ) );
  navToolbar->widgetForAction( zoomIn )->setObjectName( "zoomInButton" );

  zoomOut = navToolbar->addAction( QIcon( ":/icons/icon32_zoomout.png" ), tr( "Zoom Out" ) );
  zoomOut->setShortcut( QKeySequence::ZoomOut );
  navToolbar->widgetForAction( zoomOut )->setObjectName( "zoomOutButton" );

  zoomBase = navToolbar->addAction( QIcon( ":/icons/icon32_zoombase.png" ), tr( "Normal Size" ) );
  zoomBase->setShortcut( QKeySequence( "Ctrl+0" ) );
  navToolbar->widgetForAction( zoomBase )->setObjectName( "zoomBaseButton" );

  // named separator (to be able to hide it via CSS)
  navToolbar->widgetForAction( navToolbar->addSeparator() )->setObjectName( "separatorBeforeSave" );

  navToolbar->addAction( ui.saveArticle );
  navToolbar->widgetForAction( ui.saveArticle )->setObjectName( "saveArticleButton" );

  navToolbar->addAction( ui.print );
  navToolbar->widgetForAction( ui.print )->setObjectName( "printButton" );

  navToolbar->widgetForAction( navToolbar->addSeparator() )->setObjectName( "separatorBeforeAddToFavorites" );

  addToFavorites = navToolbar->addAction( starIcon, tr( "Add current tab to Favorites" ) );
  navToolbar->widgetForAction( addToFavorites )->setObjectName( "addToFavoritesButton" );

  connect( addToFavorites, &QAction::triggered, this, &MainWindow::handleAddToFavoritesButton );
  connect( ui.actionAddToFavorites, &QAction::triggered, this, &MainWindow::addCurrentTabToFavorites );

  beforeOptionsSeparator = navToolbar->addSeparator();
  navToolbar->widgetForAction( beforeOptionsSeparator )->setObjectName( "beforeOptionsSeparator" );
  beforeOptionsSeparator->setVisible( cfg.preferences.hideMenubar );

  QMenu * buttonMenu = new QMenu( this );
  buttonMenu->addAction( ui.dictionaries );
  buttonMenu->addAction( ui.preferences );
  buttonMenu->addSeparator();
  buttonMenu->addMenu( ui.menuFavorites );
  buttonMenu->addMenu( ui.menuHistory );
  buttonMenu->addSeparator();
  buttonMenu->addMenu( ui.menuFile );
  buttonMenu->addMenu( ui.menuView );
  buttonMenu->addMenu( ui.menuSearch );
  buttonMenu->addMenu( ui.menu_Help );

  ui.fullTextSearchAction->setEnabled( cfg.preferences.fts.enabled );

  menuButton = new QToolButton( navToolbar );
  menuButton->setPopupMode( QToolButton::InstantPopup );
  menuButton->setMenu( buttonMenu );
  menuButton->setIcon( QIcon( ":/icons/menu_button.svg" ) );
  menuButton->addAction( ui.menuOptions );
  menuButton->setToolTip( tr( "Menu Button" ) );
  menuButton->setObjectName( "menuButton" );
  menuButton->setFocusPolicy( Qt::NoFocus );

  menuButtonAction = navToolbar->addWidget( menuButton );
  menuButtonAction->setVisible( cfg.preferences.hideMenubar );

  // Make the search pane's titlebar
  //  groupLabel.setText( tr( "Look up in:" ) );
  groupListInDock = new GroupComboBox( &searchPaneTitleBar );

  searchPaneTitleBarLayout.setContentsMargins( 3, 5, 3, 5 );
  //  searchPaneTitleBarLayout.addWidget( &groupLabel );
  searchPaneTitleBarLayout.addWidget( groupListInDock );
  searchPaneTitleBarLayout.addStretch();

  searchPaneTitleBar.setLayout( &searchPaneTitleBarLayout );

  ui.searchPane->setTitleBarWidget( &searchPaneTitleBar );
  connect( ui.searchPane->toggleViewAction(), &QAction::triggered, this, &MainWindow::updateSearchPaneAndBar );

  if ( cfg.preferences.searchInDock ) {
    groupList     = groupListInDock;
    translateLine = ui.translateLine;
  }
  else {
    groupList     = groupListInToolbar;
    translateLine = translateBox->translateLine();
  }
  connect( &wordFinder, &WordFinder::updated, this, &MainWindow::prefixMatchUpdated );
  connect( &wordFinder, &WordFinder::finished, this, &MainWindow::prefixMatchFinished );


  groupList->setFocusPolicy( Qt::ClickFocus );
  ui.wordList->setFocusPolicy( Qt::ClickFocus );

  wordListDefaultFont      = ui.wordList->font();
  translateLineDefaultFont = translateLine->font();
  groupListDefaultFont     = groupList->font();

  // Make the dictionaries pane's titlebar
  foundInDictsLabel.setText( tr( "Found in Dictionaries:" ) );
  dictsPaneTitleBarLayout.addWidget( &foundInDictsLabel );
  dictsPaneTitleBarLayout.setContentsMargins( 5, 5, 5, 5 );
  dictsPaneTitleBar.setLayout( &dictsPaneTitleBarLayout );
  dictsPaneTitleBar.setObjectName( "dictsPaneTitleBar" );
  ui.dictsPane->setTitleBarWidget( &dictsPaneTitleBar );
  ui.dictsList->setContextMenuPolicy( Qt::CustomContextMenu );

  connect( ui.dictsPane, &QDockWidget::visibilityChanged, this, &MainWindow::dictsPaneVisibilityChanged );

  connect( ui.dictsList, &QListWidget::itemClicked, this, &MainWindow::foundDictsPaneClicked );

  connect( ui.dictsList, &QWidget::customContextMenuRequested, this, &MainWindow::foundDictsContextMenuRequested );

  connect( zoomIn, &QAction::triggered, this, &MainWindow::zoomin );
  connect( zoomOut, &QAction::triggered, this, &MainWindow::zoomout );
  connect( zoomBase, &QAction::triggered, this, &MainWindow::unzoom );

  ui.menuZoom->addAction( zoomIn );
  ui.menuZoom->addAction( zoomOut );
  ui.menuZoom->addAction( zoomBase );

  ui.menuZoom->addSeparator();

  wordsZoomIn = new QShortcut( this );
  wordsZoomIn->setKey( QKeySequence( "Alt+=" ) );
  wordsZoomOut = new QShortcut( this );
  wordsZoomOut->setKey( QKeySequence( "Alt+-" ) );
  wordsZoomBase = new QShortcut( this );
  wordsZoomBase->setKey( QKeySequence( "Alt+0" ) );

  connect( wordsZoomIn, &QShortcut::activated, this, &MainWindow::doWordsZoomIn );
  connect( wordsZoomOut, &QShortcut::activated, this, &MainWindow::doWordsZoomOut );
  connect( wordsZoomBase, &QShortcut::activated, this, &MainWindow::doWordsZoomBase );

// tray icon
#ifndef Q_OS_MACOS // macOS uses the dock menu instead of the tray icon
  connect( trayIconMenu.addAction( tr( "Show &Main Window" ) ), &QAction::triggered, this, [ this ] {
    this->toggleMainWindow( true );
  } );
#endif
  trayIconMenu.addAction( enableScanningAction );

#ifndef Q_OS_MACOS // macOS uses the dock menu instead of the tray icon
  trayIconMenu.addSeparator();
  connect( trayIconMenu.addAction( tr( "&Quit" ) ), &QAction::triggered, this, &MainWindow::quitApp );
#endif

  addGlobalAction( &escAction, [ this ]() {
    handleEsc();
  } );
  escAction.setShortcut( QKeySequence( "Esc" ) );

  addGlobalAction( &focusTranslateLineAction, [ this ]() {
    focusTranslateLine();
  } );
  focusTranslateLineAction.setShortcuts( QList< QKeySequence >()
                                         << QKeySequence( "Alt+D" ) << QKeySequence( "Ctrl+L" ) );

  addGlobalAction( &focusHeadwordsDlgAction, [ this ]() {
    focusHeadwordsDialog();
  } );
  focusHeadwordsDlgAction.setShortcut( QKeySequence( "Ctrl+D" ) );

  addGlobalAction( &focusArticleViewAction, [ this ]() {
    focusArticleView();
  } );
  focusArticleViewAction.setShortcut( QKeySequence( "Ctrl+N" ) );

  addGlobalAction( ui.fullTextSearchAction, [ this ]() {
    showFullTextSearchDialog();
  } );

  addGlobalAction( &stopAudioAction, [ this ]() {
    stopAudio();
  } );
  stopAudioAction.setShortcut( QKeySequence( "Ctrl+Shift+S" ) );

  addTabAction.setShortcutContext( Qt::WidgetWithChildrenShortcut );
  addTabAction.setShortcut( QKeySequence( "Ctrl+T" ) );

  // Tab management
  tabListMenu = new MRUQMenu( tr( "Opened tabs" ), ui.tabWidget );

  connect( tabListMenu, &MRUQMenu::requestTabChange, ui.tabWidget, &MainTabWidget::setCurrentIndex );

  connect( &addTabAction, &QAction::triggered, this, &MainWindow::addNewTab );

  addAction( &addTabAction );

  closeCurrentTabAction.setShortcutContext( Qt::WidgetWithChildrenShortcut );
  closeCurrentTabAction.setShortcut( QKeySequence( "Ctrl+W" ) );
  closeCurrentTabAction.setText( tr( "Close current tab" ) );

  connect( &closeCurrentTabAction, &QAction::triggered, this, &MainWindow::closeCurrentTab );

  addAction( &closeCurrentTabAction );

  closeAllTabAction.setShortcutContext( Qt::WidgetWithChildrenShortcut );
  closeAllTabAction.setShortcut( QKeySequence( "Ctrl+Shift+W" ) );
  closeAllTabAction.setText( tr( "Close all tabs" ) );

  connect( &closeAllTabAction, &QAction::triggered, this, &MainWindow::closeAllTabs );

  addAction( &closeAllTabAction );

  closeRestTabAction.setShortcutContext( Qt::WidgetWithChildrenShortcut );
  closeRestTabAction.setText( tr( "Close all tabs except current" ) );

  connect( &closeRestTabAction, &QAction::triggered, this, &MainWindow::closeRestTabs );

  addAction( &closeRestTabAction );

  switchToNextTabAction.setShortcutContext( Qt::WidgetWithChildrenShortcut );
  switchToNextTabAction.setShortcut( QKeySequence( "Ctrl+PgDown" ) );

  connect( &switchToNextTabAction, &QAction::triggered, this, &MainWindow::switchToNextTab );

  addAction( &switchToNextTabAction );

  switchToPrevTabAction.setShortcutContext( Qt::WidgetWithChildrenShortcut );
  switchToPrevTabAction.setShortcut( QKeySequence( "Ctrl+PgUp" ) );

  connect( &switchToPrevTabAction, &QAction::triggered, this, &MainWindow::switchToPrevTab );

  addAction( &switchToPrevTabAction );

  addAllTabToFavoritesAction.setText( tr( "Add all tabs to Favorites" ) );

  connect( &addAllTabToFavoritesAction, &QAction::triggered, this, &MainWindow::addAllTabsToFavorites );

  tabMenu = new QMenu( this );
  tabMenu->addAction( &closeCurrentTabAction );
  tabMenu->addAction( &closeRestTabAction );
  tabMenu->addSeparator();
  tabMenu->addAction( &closeAllTabAction );
  tabMenu->addSeparator();
  tabMenu->addAction( addToFavorites );
  tabMenu->addAction( &addAllTabToFavoritesAction );

  // Dictionary bar names
  showDictBarNamesAction.setCheckable( true );
  showDictBarNamesAction.setChecked( cfg.showingDictBarNames );

  connect( &showDictBarNamesAction, &QAction::triggered, this, &MainWindow::showDictBarNamesTriggered );

  useSmallIconsInToolbarsAction.setCheckable( true );
  useSmallIconsInToolbarsAction.setChecked( cfg.usingToolbarsIconSize == Config::ToolbarsIconSize::Small );
  useLargeIconsInToolbarsAction.setCheckable( true );
  useLargeIconsInToolbarsAction.setChecked( cfg.usingToolbarsIconSize == Config::ToolbarsIconSize::Large );
  useNormalIconsInToolbarsAction.setCheckable( true );
  useNormalIconsInToolbarsAction.setChecked( cfg.usingToolbarsIconSize == Config::ToolbarsIconSize::Normal );

  // icon action group,default exclusive option.
  smallLargeIconGroup->addAction( &useLargeIconsInToolbarsAction );
  smallLargeIconGroup->addAction( &useSmallIconsInToolbarsAction );
  smallLargeIconGroup->addAction( &useNormalIconsInToolbarsAction );

  connect( smallLargeIconGroup, &QActionGroup::triggered, this, &MainWindow::iconSizeActionTriggered );

  // Toggle Menubar
  toggleMenuBarAction.setCheckable( true );
  toggleMenuBarAction.setChecked( !cfg.preferences.hideMenubar );
  toggleMenuBarAction.setShortcut( QKeySequence( "Ctrl+M" ) );

  connect( &toggleMenuBarAction, &QAction::triggered, this, &MainWindow::toggleMenuBarTriggered );

  // Populate 'View' menu
  ui.menuView->addAction( &toggleMenuBarAction );
  ui.menuView->addSeparator();
  ui.menuView->addAction( ui.searchPane->toggleViewAction() );
  ui.searchPane->toggleViewAction()->setShortcut( QKeySequence( "Ctrl+S" ) );
  ui.menuView->addAction( ui.dictsPane->toggleViewAction() );
  ui.dictsPane->toggleViewAction()->setShortcut( QKeySequence( "Ctrl+R" ) );
  ui.menuView->addAction( ui.favoritesPane->toggleViewAction() );
  ui.menuView->addAction( ui.historyPane->toggleViewAction() );

  ui.menuView->addSeparator();
  ui.menuView->addAction( dictionaryBar.toggleViewAction() );
  ui.menuView->addAction( navToolbar->toggleViewAction() );
  ui.menuView->addSeparator();
  ui.menuView->addAction( &showDictBarNamesAction );
  ui.menuView->addSeparator();
  ui.menuView->addAction( &useSmallIconsInToolbarsAction );
  ui.menuView->addAction( &useNormalIconsInToolbarsAction );
  ui.menuView->addAction( &useLargeIconsInToolbarsAction );
  ui.menuView->addSeparator();
  ui.alwaysOnTop->setChecked( cfg.preferences.alwaysOnTop );
  ui.menuView->addAction( ui.alwaysOnTop );

  // Dictionary bar

  Instances::Group const * igrp = groupInstances.findGroup( cfg.lastMainGroupId );
  if ( cfg.lastMainGroupId == GroupId::AllGroupId ) {
    if ( igrp ) {
      igrp->checkMutedDictionaries( &cfg.mutedDictionaries );
    }
    dictionaryBar.setMutedDictionaries( &cfg.mutedDictionaries );
  }
  else {
    Config::Group * grp = cfg.getGroup( cfg.lastMainGroupId );
    if ( igrp && grp ) {
      igrp->checkMutedDictionaries( &grp->mutedDictionaries );
    }
    dictionaryBar.setMutedDictionaries( grp ? &grp->mutedDictionaries : nullptr );
  }
  GlobalBroadcaster::instance()->currentGroupId = cfg.lastMainGroupId;

  showDictBarNamesTriggered(); // Make update its state according to initial
                               // setting

  connect( this, &MainWindow::clickOnDictPane, &dictionaryBar, &DictionaryBar::dictsPaneClicked );

  addToolBar( &dictionaryBar );

  connect( dictionaryBar.toggleViewAction(), &QAction::triggered, this, &MainWindow::dictionaryBarToggled );
  // This one will be disconnected once the slot is activated. It exists
  // only to handle the initial appearance of the dictionary bar.
  connect( dictionaryBar.toggleViewAction(), &QAction::toggled, this, &MainWindow::dictionaryBarToggled );

  connect( &dictionaryBar, &DictionaryBar::editGroupRequested, this, &MainWindow::editCurrentGroup );

  connect( &dictionaryBar, &DictionaryBar::showDictionaryInfo, this, &MainWindow::showDictionaryInfo );

  connect( &dictionaryBar, &DictionaryBar::showDictionaryHeadwords, this, &MainWindow::showDictionaryHeadwords );

  connect( &dictionaryBar, &DictionaryBar::openDictionaryFolder, this, &MainWindow::openDictionaryFolder );

  // Favorites

  ui.favoritesPaneWidget->setUp( &cfg, ui.menuFavorites );
  ui.favoritesPaneWidget->setSaveInterval( cfg.preferences.favoritesStoreInterval );

  connect( ui.favoritesPane, &QDockWidget::visibilityChanged, this, &MainWindow::updateFavoritesMenu );
  connect( ui.showHideFavorites, &QAction::triggered, this, &MainWindow::toggle_favoritesPane );

  connect( ui.favoritesPaneWidget,
           &FavoritesPaneWidget::favoritesItemRequested,
           this,
           &MainWindow::headwordFromFavorites );

  // History
  ui.historyPaneWidget->setUp( &cfg, &history, ui.menuHistory );
  history.enableAdd( cfg.preferences.storeHistory );

  connect( ui.historyPaneWidget, &HistoryPaneWidget::historyItemRequested, this, &MainWindow::showHistoryItem );

  connect( ui.historyPane, &QDockWidget::visibilityChanged, this, &MainWindow::updateHistoryMenu );
  connect( ui.showHideHistory, &QAction::triggered, this, &MainWindow::toggle_historyPane );

  connect( navBack, &QAction::triggered, this, &MainWindow::backClicked );
  connect( navForward, &QAction::triggered, this, &MainWindow::forwardClicked );

  addTab.setAutoRaise( true );
  addTab.setToolTip( tr( "New Tab" ) );
  addTab.setFocusPolicy( Qt::NoFocus );
  addTab.setIcon( QIcon( ":/icons/addtab.svg" ) );

  ui.tabWidget->setHideSingleTab( cfg.preferences.hideSingleTab );
  ui.tabWidget->clear();

  ui.tabWidget->setCornerWidget( &addTab, Qt::TopLeftCorner );
  //ui.tabWidget->setCornerWidget( &closeTab, Qt::TopRightCorner );

  ui.tabWidget->setMovable( true );

#ifndef Q_OS_WIN32
  ui.tabWidget->setDocumentMode( true );
#endif

  ui.tabWidget->setContextMenuPolicy( Qt::CustomContextMenu );

  connect( &addTab, &QAbstractButton::clicked, this, &MainWindow::addNewTab );

  connect( ui.tabWidget, &MainTabWidget::tabBarDoubleClicked, [ this ]( const int index ) {
    if ( -1 == index ) { // empty space at tabbar clicked.
      this->addNewTab();
    }
  } );

  connect( ui.tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::tabCloseRequested );

  connect( ui.tabWidget, &QTabWidget::currentChanged, this, &MainWindow::tabSwitched );

  connect( ui.tabWidget, &QWidget::customContextMenuRequested, this, &MainWindow::tabMenuRequested );

  ui.tabWidget->setTabsClosable( true );

  connect( ui.quit, &QAction::triggered, this, &MainWindow::quitApp );

  connect( ui.dictionaries, &QAction::triggered, this, &MainWindow::editDictionaries );

  connect( ui.preferences, &QAction::triggered, this, &MainWindow::editPreferences );

  connect( ui.visitHomepage, &QAction::triggered, this, &MainWindow::visitHomepage );
  connect( ui.visitForum, &QAction::triggered, this, &MainWindow::visitForum );
  connect( ui.openConfigFolder, &QAction::triggered, this, &MainWindow::openConfigFolder );
  connect( ui.about, &QAction::triggered, this, &MainWindow::showAbout );
  connect( ui.showReference, &QAction::triggered, []() {
    Help::openHelpWebpage();
  } );

  connect( groupListInDock, &GroupComboBox::currentIndexChanged, this, &MainWindow::currentGroupChanged );

  connect( groupListInToolbar, &GroupComboBox::currentIndexChanged, this, &MainWindow::currentGroupChanged );

  connect( ui.translateLine, &QLineEdit::textChanged, this, &MainWindow::translateInputChanged );

  connect( translateBox->translateLine(), &QLineEdit::textEdited, this, &MainWindow::translateInputChanged );

  connect( ui.translateLine, &QLineEdit::returnPressed, [ this ]() {
    translateInputFinished( true );
  } );
  connect( translateBox, &TranslateBox::returnPressed, [ this ]() {
    translateInputFinished( true );
  } );

  connect( ui.wordList, &QListWidget::itemSelectionChanged, this, &MainWindow::wordListSelectionChanged );

  connect( ui.wordList, &QListWidget::itemClicked, this, &MainWindow::wordListItemActivated );

  connect( ui.dictsList, &QListWidget::itemSelectionChanged, this, &MainWindow::dictsListSelectionChanged );

  connect( ui.dictsList, &QListWidget::itemDoubleClicked, this, &MainWindow::dictsListItemActivated );

  connect( &configEvents, &Config::Events::mutedDictionariesChanged, this, &MainWindow::mutedDictionariesChanged );

  this->installEventFilter( this );

  ui.translateLine->installEventFilter( this );
  translateBox->translateLine()->installEventFilter( this );

  ui.wordList->installEventFilter( this );

  ui.wordList->viewport()->installEventFilter( this );

  ui.dictsList->installEventFilter( this );
  ui.dictsList->viewport()->installEventFilter( this );
  //tabWidget doesn't propagate Ctrl+Tab to the parent widget unless event filter is installed
  ui.tabWidget->installEventFilter( this );

  ui.historyList->installEventFilter( this );

  ui.favoritesTree->installEventFilter( this );

  groupListInDock->installEventFilter( this );
  groupListInToolbar->installEventFilter( this );

  connect( &ftsIndexing, &FTS::FtsIndexing::newIndexingName, this, &MainWindow::showFTSIndexingName );
  connect( GlobalBroadcaster::instance(),
           &GlobalBroadcaster::indexingDictionary,
           this,
           &MainWindow::showFTSIndexingName );

  connect( &GlobalBroadcaster::instance()->pronounce_engine,
           &PronounceEngine::emitAudio,
           this,
           [ this ]( auto audioUrl ) {
             auto view = getCurrentArticleView();
             view->setAudioLink( audioUrl );
             if ( !isActiveWindow() ) {
               return;
             }
             if ( ( cfg.preferences.pronounceOnLoadMain ) && view != nullptr ) {

               view->playAudio( QUrl::fromEncoded( audioUrl.toUtf8() ) );
             }
           } );
  applyProxySettings();

  //set  webengineview font
  changeWebEngineViewFont();

  connect( &dictNetMgr, &QNetworkAccessManager::proxyAuthenticationRequired, this, &MainWindow::proxyAuthentication );

  connect( &articleNetMgr,
           &QNetworkAccessManager::proxyAuthenticationRequired,
           this,
           &MainWindow::proxyAuthentication );

  setupNetworkCache( cfg.preferences.maxNetworkCacheSize );

  makeDictionaries();

  // After we have dictionaries and groups, we can populate history
  //  historyChanged();

  setWindowTitle( "GoldenDict-ng" );

  // Create tab list menu
  createTabList();


#if defined( Q_OS_LINUX )
  defaultInterfaceStyle = QApplication::style()->name();
#elif defined( Q_OS_MAC )
  defaultInterfaceStyle = "Fusion";
#endif

  updateAppearances( cfg.preferences.addonStyle,
                     cfg.preferences.displayStyle,
                     cfg.preferences.darkMode
#if !defined( Q_OS_WIN )
                     ,
                     cfg.preferences.interfaceStyle
#endif
  );

  // Show the initial welcome text
  addNewTab();
  ArticleView * view = getCurrentArticleView();
  history.enableAdd( false );
  view->showDefinition( tr( "Welcome!" ), GroupId::HelpGroupId );
  history.enableAdd( cfg.preferences.storeHistory );

  // restore should be called after all UI initialized but not necessarily after show()
  // This must be called before show() as of Qt6.5 on Windows, not sure if it is a bug
  // Due to a bug of WebEngine, this also must be called after WebEngine has a view loaded https://bugreports.qt.io/browse/QTBUG-115074
  if ( cfg.mainWindowState.size() && !cfg.resetState ) {
    restoreState( cfg.mainWindowState );
  }
  if ( cfg.mainWindowGeometry.size() ) {
    restoreGeometry( cfg.mainWindowGeometry );
  }

  // Show window unless it is configured not to
  if ( !cfg.preferences.enableTrayIcon || !cfg.preferences.startToTray ) {
    show();
    focusTranslateLine();
  }

  // Scanpopup related
  scanPopup =
    new ScanPopup( nullptr, cfg, articleNetMgr, audioPlayerFactory.player(), dictionaries, groupInstances, history );

  scanPopup->setStyleSheet( styleSheet() );

  connect( scanPopup, &ScanPopup::editGroupRequest, this, &MainWindow::editDictionaries, Qt::QueuedConnection );

  connect( scanPopup, &ScanPopup::sendPhraseToMainWindow, this, [ this ]( QString const & word ) {
    wordReceived( word );
  } );

  connect( scanPopup, &ScanPopup::inspectSignal, this, &MainWindow::inspectElement );
  connect( scanPopup, &ScanPopup::forceAddWordToHistory, this, &MainWindow::forceAddWordToHistory );
  connect( scanPopup, &ScanPopup::showDictionaryInfo, this, &MainWindow::showDictionaryInfo );
  connect( scanPopup, &ScanPopup::openDictionaryFolder, this, &MainWindow::openDictionaryFolder );
  connect( scanPopup, &ScanPopup::sendWordToHistory, this, &MainWindow::addWordToHistory );
  connect( this, &MainWindow::setPopupGroupByName, scanPopup, &ScanPopup::setGroupByName );
  connect( scanPopup, &ScanPopup::sendWordToFavorites, this, &MainWindow::addWordToFavorites );

#ifdef Q_OS_MAC
  macClipboard = new gd_clipboard( this );
  connect( macClipboard, &gd_clipboard::changed, this, &MainWindow::clipboardChange );
#endif

  connect( enableScanningAction, &QAction::toggled, this, [ = ]( bool on ) {
    if ( on ) {
      enableScanningAction->setIcon( QIcon( ":/icons/wizard-selected.svg" ) );
    }
    else {
      enableScanningAction->setIcon( QIcon( ":/icons/wizard.svg" ) );
    }

#ifdef Q_OS_MAC
    if ( !MacMouseOver::isAXAPIEnabled() ) {
      mainStatusBar->showMessage( tr( "Accessibility API is not enabled" ), 10000, QPixmap( ":/icons/error.svg" ) );
    }

    if ( on ) {
      macClipboard->start();
    }
    else {
      macClipboard->stop();
    }
#else
    if ( on ) {
      connect( QApplication::clipboard(), &QClipboard::changed, this, &MainWindow::clipboardChange );
    }
    else {
      disconnect( QApplication::clipboard(), &QClipboard::changed, this, &MainWindow::clipboardChange );
    }
#endif

    installHotKeys();
    trayIconUpdateOrInit();
  } );

  if ( cfg.preferences.startWithScanPopupOn ) {
    enableScanningAction->trigger();
  }

  updateSearchPaneAndBar( cfg.preferences.searchInDock );
  ui.searchPane->setVisible( cfg.preferences.searchInDock );

  trayIconUpdateOrInit();

  // Update zoomers
  adjustCurrentZoomFactor();
  scaleArticlesByCurrentZoomFactor();
  applyWordsZoomLevel();

  // Update autostart info
  setAutostart( cfg.preferences.autoStart );

  // Initialize global hotkeys
  installHotKeys();

  if ( cfg.preferences.alwaysOnTop ) {
    on_alwaysOnTop_triggered( true );
  }

  if ( cfg.preferences.hideMenubar ) {
    toggleMenuBarTriggered( false );
  }

  // makeDictionaries() didn't do deferred init - we do it here, at the end.
  doDeferredInit( dictionaries );

  updateStatusLine();

#ifdef Q_OS_MAC
  if ( cfg.preferences.startWithScanPopupOn && !MacMouseOver::isAXAPIEnabled() ) {
    mainStatusBar->showMessage( tr( "Accessibility API is not enabled" ), 10000, QPixmap( ":/icons/error.svg" ) );
  }
#endif

  wasMaximized = isMaximized();

  history.setSaveInterval( cfg.preferences.historyStoreInterval );
#ifndef Q_OS_MACOS
  ui.centralWidget->grabGesture( Gestures::GDPinchGestureType );
  ui.centralWidget->grabGesture( Gestures::GDSwipeGestureType );
#endif

  if ( layoutDirection() == Qt::RightToLeft ) {
    // Adjust button icons for Right-To-Left layout
    navBack->setIcon( QIcon( ":/icons/next.svg" ) );
    navForward->setIcon( QIcon( ":/icons/previous.svg" ) );
  }

  inspector.reset( new ArticleInspector( this ) );

#ifdef Q_OS_WIN
  // Regiser and update URL Scheme for windows
  // https://learn.microsoft.com/en-us/previous-versions/windows/internet-explorer/ie-developer/platform-apis/aa767914(v=vs.85)

  // Windows will automatically map registry key to Computer\HKEY_CLASSES_ROOT\ */
  QSettings urlRegistry( R"(HKEY_CURRENT_USER\Software\Classes)", QSettings::NativeFormat );

  urlRegistry.beginGroup( "goldendict" );
  urlRegistry.setValue( "Default", "URL: goldendict Protocol" );
  urlRegistry.setValue( "URL Protocol", "" );
  urlRegistry.setValue( "shell/open/command/Default",
                        QString( "\"%1\"" ).arg( QDir::toNativeSeparators( QApplication::applicationFilePath() ) )
                          + " \"%1\"" );
  urlRegistry.endGroup();
#endif

  iconSizeActionTriggered( nullptr );

  if ( cfg.preferences.checkForNewReleases ) {
    QTimer::singleShot( 0, this, &MainWindow::checkNewRelease );
  }
}

void MainWindow::prefixMatchUpdated()
{
  updateMatchResults( false );
}

void MainWindow::prefixMatchFinished()
{
  updateMatchResults( true );
}

void MainWindow::updateMatchResults( bool finished )
{
  WordFinder::SearchResults const & results = wordFinder.getResults();

  if ( cfg.preferences.searchInDock ) {
    ui.wordList->setUpdatesEnabled( false );
    //clear all existed items
    ui.wordList->clear();

    for ( const auto & result : results ) {
      auto i = new QListWidgetItem( result.first, ui.wordList );
      i->setToolTip( result.first );

      if ( result.second ) {
        QFont f = i->font();
        f.setItalic( true );
        i->setFont( f );
      }

      i->setTextAlignment( Qt::AlignLeft );
    }

    if ( ui.wordList->count() ) {
      ui.wordList->scrollToItem( ui.wordList->item( 0 ), QAbstractItemView::PositionAtTop );
      ui.wordList->setCurrentItem( nullptr, QItemSelectionModel::Clear );
    }

    ui.wordList->setUpdatesEnabled( true );
    ui.wordList->unsetCursor();
  }
  else {

    QStringList _results;
    _results.clear();

    for ( const auto & result : results ) {
      _results << result.first;
    }
    translateBox->setModel( _results );
  }

  if ( finished ) {

    refreshTranslateLine();

    if ( !wordFinder.getErrorString().isEmpty() ) {
      emit showStatusBarMessage( tr( "WARNING: %1" ).arg( wordFinder.getErrorString() ),
                                 20000,
                                 QPixmap( ":/icons/error.svg" ) );
    }
  }
}

void MainWindow::refreshTranslateLine()
{
  if ( !translateLine ) {
    return;
  }

  // Visually mark the input line to mark if there's no results
  bool setMark = wordFinder.getResults().empty() && !wordFinder.wasSearchUncertain();
  Utils::Widget::setNoResultColor( translateLine, setMark );
}

void MainWindow::clipboardChange( QClipboard::Mode m )
{
  if ( !scanPopup ) {
    return;
  }

#if defined( HAVE_X11 )
  if ( m == QClipboard::Clipboard ) {
    if ( !cfg.preferences.trackClipboardScan )
      return;
    scanPopup->translateWordFromClipboard();
    return;
  }

  if ( m == QClipboard::Selection ) {

    // Multiple ways to stopping a word from showing up when selecting

    // Explicitly disabled on preferences
    if ( !cfg.preferences.trackSelectionScan )
      return;

    // Explicitly disabled on preferences to ignore gd's own selection

    if ( cfg.preferences.ignoreOwnClipboardChanges && QApplication::clipboard()->ownsSelection() ) {
      return;
    }

    // Keyboard Modifier
    if ( cfg.preferences.enableScanPopupModifiers
         && !KeyboardState::checkModifiersPressed( cfg.preferences.scanPopupModifiers ) ) {
      return;
    }

    // Show a Flag instead of translate directly.
    // And hand over the control of showing the popup to scanFlag
    if ( cfg.preferences.showScanFlag ) {
      scanPopup->showScanFlag();
      return;
    }

    // Use delay show to prevent multiple popups while selection in progress
    scanPopup->selectionDelayTimer.start();
  }
#elif defined( Q_OS_MAC )
  scanPopup->translateWord( macClipboard->text() );
#else
  scanPopup->translateWordFromClipboard();
#endif
}

void MainWindow::ctrlTabPressed()
{
  emit fillWindowsMenu();
  tabListButton->click();
}

void MainWindow::updateSearchPaneAndBar( bool searchInDock )
{
  QString text = translateLine->text();

  if ( ftsDlg ) {
    removeGroupComboBoxActionsFromDialog( ftsDlg, groupList );
  }
  if ( headwordsDlg ) {
    removeGroupComboBoxActionsFromDialog( headwordsDlg, groupList );
  }

  if ( searchInDock ) {
    cfg.preferences.searchInDock = true;

    navToolbar->setAllowedAreas( Qt::AllToolBarAreas );

    groupList     = groupListInDock;
    translateLine = ui.translateLine;

    translateBoxToolBarAction->setVisible( false );

    translateBox->setPopupEnabled( false );
  }
  else {
    cfg.preferences.searchInDock = false;

    // handle the main toolbar, it must not be on the side, since it should
    // contain the group widget and the translate line. Valid locations: Top and Bottom.
    navToolbar->setAllowedAreas( Qt::BottomToolBarArea | Qt::TopToolBarArea );
    if ( toolBarArea( navToolbar ) & ( Qt::LeftToolBarArea | Qt::RightToolBarArea ) ) {
      if ( toolBarArea( &dictionaryBar ) == Qt::TopToolBarArea ) {
        insertToolBar( &dictionaryBar, navToolbar );
      }
      else {
        addToolBar( Qt::TopToolBarArea, navToolbar );
      }
    }

    groupList     = groupListInToolbar;
    translateLine = translateBox->translateLine();

    translateBoxToolBarAction->setVisible( true );
  }

  if ( ftsDlg ) {
    addGroupComboBoxActionsToDialog( ftsDlg, groupList );
  }
  if ( headwordsDlg ) {
    addGroupComboBoxActionsToDialog( headwordsDlg, groupList );
  }

  translateLine->setToolTip( tr(
    "String to search in dictionaries. The wildcards '*', '?' and sets of symbols '[...]' are allowed.\nTo find '*', '?', '[', ']' symbols use '\\*', '\\?', '\\[', '\\]' respectively" ) );

  // reset the flag when switching UI modes
  wordListSelChanged = false;

  updateGroupList( false );
  applyWordsZoomLevel();

  setInputLineText( text, WildcardPolicy::WildcardsAreAlreadyEscaped, DisablePopup );
  focusTranslateLine();
}

void MainWindow::mousePressEvent( QMouseEvent * event )
{
  if ( handleBackForwardMouseButtons( event ) ) {
    return;
  }

  if ( event->button() != Qt::MiddleButton ) {
    return QMainWindow::mousePressEvent( event );
  }

  // middle clicked
  QString subtype = "plain";

  QString str = QApplication::clipboard()->text( subtype, QClipboard::Selection );
  setInputLineText( str, WildcardPolicy::EscapeWildcards, NoPopupChange );

  QKeyEvent ev( QEvent::KeyPress, Qt::Key_Enter, Qt::NoModifier );
  QApplication::sendEvent( translateLine, &ev );
}

MainWindow::~MainWindow()
{
  closeHeadwordsDialog();

  ftsIndexing.stopIndexing();
#ifndef Q_OS_MACOS
  ui.centralWidget->ungrabGesture( Gestures::GDPinchGestureType );
  ui.centralWidget->ungrabGesture( Gestures::GDSwipeGestureType );
#endif
  //  Gestures::unregisterRecognizers();

  // Close all tabs -- they should be destroyed before network managers
  // do.
  while ( ui.tabWidget->count() ) {
    QWidget * w = ui.tabWidget->widget( 0 );

    ui.tabWidget->removeTab( 0 );

    delete w;
  }

  if ( scanPopup ) {
    delete scanPopup;
    scanPopup = nullptr;
  }

#ifdef EPWING_SUPPORT
  Epwing::finalize();
#endif
}

void MainWindow::addGlobalAction( QAction * action, const std::function< void() > & slotFunc )
{
  action->setShortcutContext( Qt::WidgetWithChildrenShortcut );
  connect( action, &QAction::triggered, this, slotFunc );

  ui.centralWidget->addAction( action );
  ui.dictsPane->addAction( action );
  ui.searchPaneWidget->addAction( action );
  ui.favoritesPane->addAction( action );
  ui.historyPane->addAction( action );
  groupList->addAction( action );
  translateBox->addAction( action );
}

void MainWindow::addGlobalActionsToDialog( QDialog * dialog )
{
  dialog->addAction( &focusTranslateLineAction );
  dialog->addAction( &focusHeadwordsDlgAction );
  dialog->addAction( &focusArticleViewAction );
  dialog->addAction( ui.fullTextSearchAction );
}

void MainWindow::addGroupComboBoxActionsToDialog( QDialog * dialog, GroupComboBox * pGroupComboBox )
{
  dialog->addActions( pGroupComboBox->getExternActions() );
}

void MainWindow::removeGroupComboBoxActionsFromDialog( QDialog * dialog, GroupComboBox * pGroupComboBox )
{
  QList< QAction * > actions = pGroupComboBox->getExternActions();
  for ( QList< QAction * >::iterator it = actions.begin(); it != actions.end(); ++it ) {
    dialog->removeAction( *it );
  }
}

void MainWindow::commitData()
{
  if ( cfg.preferences.clearNetworkCacheOnExit ) {
    if ( QAbstractNetworkCache * cache = articleNetMgr.cache() ) {
      cache->clear();
    }
  }

  //if the dictionaries is empty ,large chance that the config has corrupt.
  if ( cfg.preferences.removeInvalidIndexOnExit && !dictMap.isEmpty() ) {
    QDir const dir( Config::getIndexDir() );

    QFileInfoList const entries = dir.entryInfoList( QDir::Files | QDir::NoDotAndDotDot );

    for ( auto & file : entries ) {
      QString const fileName = file.fileName();

      if ( dictMap.contains( fileName.toStdString() ) ) {
        continue;
      }
      //remove both normal index and fts index.
      auto filePath = file.absoluteFilePath();
      qDebug() << "remove invalid index files & fts dirs";

      QFile::remove( filePath );
      QDir d( filePath + "_FTS_x" );
      if ( d.exists() ) {
        d.removeRecursively();
      }
      //temp dir
      QDir dtemp( filePath + "_FTS_x_temp" );
      if ( dtemp.exists() ) {
        dtemp.removeRecursively();
      }
    }


    //remove temp directories.
    QFileInfoList const dirs = dir.entryInfoList( QDir::Dirs | QDir::NoDotAndDotDot );

    for ( auto & file : dirs ) {
      QString const fileName = file.fileName();

      if ( !fileName.endsWith( "_temp" ) ) {
        continue;
      }

      const QFileInfo info( fileName );
      const QDateTime lastModified = info.lastModified();

      //if the temp directory has not been modified within 7 days,remove the temp directory.
      if ( lastModified.addDays( 7 ) > QDateTime::currentDateTime() ) {
        continue;
      }
      QDir d( fileName );
      d.removeRecursively();
    }
  }


  try {
    // Save MainWindow state and geometry
    cfg.mainWindowState    = saveState();
    cfg.mainWindowGeometry = saveGeometry();

    // Save popup window state and geometry
    if ( scanPopup ) {
      scanPopup->saveConfigData();
    }

    // Save any changes in last chosen groups etc
    try {
      Config::save( cfg );
    }
    catch ( std::exception & e ) {
      qWarning( "Configuration saving failed, error: %s", e.what() );
    }

    // Save history
    history.save();

    // Save favorites
    ui.favoritesPaneWidget->saveData();
  }
  catch ( std::exception & e ) {
    qWarning( "Commit data failed, error: %s", e.what() );
  }
}

QPrinter & MainWindow::getPrinter()
{
  if ( printer.get() ) {
    return *printer;
  }

  printer = std::make_shared< QPrinter >( QPrinter::HighResolution );

  return *printer;
}

void MainWindow::updateAppearances( QString const & addonStyle,
                                    QString const & displayStyle,
                                    Config::Dark darkMode
#if !defined( Q_OS_WIN )
                                    ,
                                    const QString & interfaceStyle
#endif
)
{
#ifdef Q_OS_WIN32
  if ( darkMode == Config::Dark::On ) {
    //https://forum.qt.io/topic/101391/windows-10-dark-theme

    QPalette darkPalette;
    QColor darkColor     = QColor( 45, 45, 45 );
    QColor disabledColor = QColor( 127, 127, 127 );
    darkPalette.setColor( QPalette::Window, darkColor );
    darkPalette.setColor( QPalette::WindowText, Qt::white );
    darkPalette.setColor( QPalette::Base, QColor( 18, 18, 18 ) );
    darkPalette.setColor( QPalette::AlternateBase, darkColor );
    darkPalette.setColor( QPalette::ToolTipBase, Qt::white );
    darkPalette.setColor( QPalette::ToolTipText, Qt::white );
    darkPalette.setColor( QPalette::Text, Qt::white );
    darkPalette.setColor( QPalette::Disabled, QPalette::Text, disabledColor );
    darkPalette.setColor( QPalette::Button, darkColor );
    darkPalette.setColor( QPalette::ButtonText, Qt::white );
    darkPalette.setColor( QPalette::Dark, QColor( 35, 35, 35 ) );
    darkPalette.setColor( QPalette::Shadow, QColor( 20, 20, 20 ) );
    darkPalette.setColor( QPalette::Disabled, QPalette::ButtonText, disabledColor );
    darkPalette.setColor( QPalette::BrightText, Qt::red );
    darkPalette.setColor( QPalette::Link, QColor( 42, 130, 218 ) );

    darkPalette.setColor( QPalette::Highlight, QColor( 42, 130, 218 ) );
    darkPalette.setColor( QPalette::HighlightedText, Qt::black );
    darkPalette.setColor( QPalette::Disabled, QPalette::HighlightedText, disabledColor );

    qApp->setPalette( darkPalette );
    qApp->setStyle( "Fusion" );
  }
  else {
    qApp->setStyle( "WindowsVista" );
    qApp->setPalette( QPalette() );
  }
#endif

#if !defined( Q_OS_WIN )
  if ( interfaceStyle == "Default" ) {
    QApplication::setStyle( QStyleFactory::create( defaultInterfaceStyle ) );
  }
  else {
    if ( QStyleFactory::keys().contains( interfaceStyle ) ) {
      QApplication::setStyle( QStyleFactory::create( interfaceStyle ) );
    }
  }
#endif


  QByteArray css{};
#if defined( Q_OS_WIN )
  QFile winCssFile( ":qt-style-win.css" );
  winCssFile.open( QFile::ReadOnly );
  css += winCssFile.readAll();

  // Load an additional stylesheet
  // Dark Mode doesn't work nice with custom qt style sheets,
  if ( darkMode == Config::Dark::Off ) {
    QFile additionalStyle( QString( ":qt-%1.css" ).arg( displayStyle ) );
    if ( additionalStyle.open( QFile::ReadOnly ) ) {
      css += additionalStyle.readAll();
    }
  }

#endif

  // Try loading a style sheet if there's one
  QFile cssFile( Config::getUserQtCssFileName() );

  if ( cssFile.open( QFile::ReadOnly ) ) {
    css += cssFile.readAll();
  }

  if ( !addonStyle.isEmpty() ) {
    QString name = Config::getStylesDir() + addonStyle + QDir::separator() + "qt-style.css";
    QFile addonCss( name );
    if ( addonCss.open( QFile::ReadOnly ) ) {
      css += addonCss.readAll();
    }
  }

#ifdef Q_OS_WIN32
  if ( darkMode == Config::Dark::On ) {
    css += "QToolTip { color: #ffffff; background-color: #2a82da; border: 1px solid white; }";
  }
#endif

  if ( !css.isEmpty() ) {
    setStyleSheet( css );
  }
}

void MainWindow::trayIconUpdateOrInit()
{
#ifdef Q_OS_MACOS
  trayIconMenu.setAsDockMenu();
  ui.actionCloseToTray->setVisible( false );
#else

  if ( !cfg.preferences.enableTrayIcon ) {
    if ( trayIcon ) {
      delete trayIcon;
      trayIcon = nullptr;
    }
  }
  else {
    if ( !trayIcon ) {
      trayIcon = new QSystemTrayIcon( this );
      trayIcon->setContextMenu( &trayIconMenu );
      trayIcon->setToolTip( QApplication::applicationName() );
      connect( trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::trayIconActivated );
      trayIcon->show();
    }
    // Update the icon to reflect the scanning mode
    trayIcon->setIcon( enableScanningAction->isChecked() ?
                         QIcon::fromTheme( "goldendict-scan-tray", QIcon( ":/icons/programicon_scan.png" ) ) :
                         QIcon::fromTheme( "goldendict-tray", QIcon( ":/icons/programicon_old.png" ) ) );
  }

  // The 'Close to tray' action is associated with the tray icon, so we hide
  // or show it here.
  ui.actionCloseToTray->setVisible( cfg.preferences.enableTrayIcon );
#endif
}

void MainWindow::wheelEvent( QWheelEvent * ev )
{
  if ( ev->modifiers().testFlag( Qt::ControlModifier ) ) {
    if ( ev->angleDelta().y() > 0 ) {
      zoomin();
    }
    else if ( ev->angleDelta().y() < 0 ) {
      zoomout();
    }
    ev->accept();
  }
  else {
    ev->ignore();
  }
}

void MainWindow::closeEvent( QCloseEvent * ev )
{
  if ( cfg.preferences.enableTrayIcon && cfg.preferences.closeToTray ) {
    if ( !cfg.preferences.searchInDock ) {
      translateBox->setPopupEnabled( false );
    }

#ifdef Q_OS_MACOS
    if ( !ev->spontaneous() || !isVisible() ) {
      return;
    }
#endif
#ifdef HAVE_X11
    // Don't ignore the close event, because doing so cancels session logout if
    // the main window is visible when the user attempts to log out.
    // The main window will be only hidden, because QApplication::quitOnLastWindowClosed
    // property is false and Qt::WA_DeleteOnClose widget  is not set.
    Q_ASSERT( !QApplication::quitOnLastWindowClosed() );
    Q_ASSERT( !testAttribute( Qt::WA_DeleteOnClose ) );
#else
    // Ignore the close event because closing the main window breaks global hotkeys on Windows.
    ev->ignore();
    hide();
#endif
  }
  else {
    ev->accept();
    quitApp();
  }
}

void MainWindow::quitApp()
{
  if ( inspector && inspector->isVisible() ) {
    inspector->close();
  }
  commitData();
  qApp->quit();
}

void MainWindow::applyProxySettings()
{
  if ( cfg.preferences.proxyServer.enabled && cfg.preferences.proxyServer.useSystemProxy ) {
    QList< QNetworkProxy > proxies = QNetworkProxyFactory::systemProxyForQuery();
    if ( !cfg.preferences.proxyServer.systemProxyUser.isEmpty() ) {
      proxies.first().setUser( cfg.preferences.proxyServer.systemProxyUser );
      proxies.first().setPassword( cfg.preferences.proxyServer.systemProxyPassword );
    }
    QNetworkProxy::setApplicationProxy( proxies.first() );
    return;
  }

  QNetworkProxy::ProxyType type = QNetworkProxy::NoProxy;

  if ( cfg.preferences.proxyServer.enabled ) {
    switch ( cfg.preferences.proxyServer.type ) {
      case Config::ProxyServer::Socks5:
        type = QNetworkProxy::Socks5Proxy;
        break;
      case Config::ProxyServer::HttpConnect:
        type = QNetworkProxy::HttpProxy;
        break;
      case Config::ProxyServer::HttpGet:
        type = QNetworkProxy::HttpCachingProxy;
        break;

      default:
        break;
    }
  }

  QNetworkProxy proxy( type );

  if ( cfg.preferences.proxyServer.enabled ) {
    proxy.setHostName( cfg.preferences.proxyServer.host );
    proxy.setPort( cfg.preferences.proxyServer.port );

    if ( cfg.preferences.proxyServer.user.size() ) {
      proxy.setUser( cfg.preferences.proxyServer.user );
    }

    if ( cfg.preferences.proxyServer.password.size() ) {
      proxy.setPassword( cfg.preferences.proxyServer.password );
    }
  }

  QNetworkProxy::setApplicationProxy( proxy );
}

void MainWindow::setupNetworkCache( int maxSize )
{
  // x << 20 == x * 2^20 converts mebibytes to bytes.
  qint64 const maxCacheSizeInBytes = maxSize <= 0 ? qint64( 0 ) : static_cast< qint64 >( maxSize ) << 20;

  if ( QAbstractNetworkCache * abstractCache = articleNetMgr.cache() ) {
    QNetworkDiskCache * const diskCache = qobject_cast< QNetworkDiskCache * >( abstractCache );
    Q_ASSERT_X( diskCache, Q_FUNC_INFO, "Unexpected network cache type." );
    diskCache->setMaximumCacheSize( maxCacheSizeInBytes );
    return;
  }
  if ( maxCacheSizeInBytes == 0 ) {
    return; // There is currently no cache and it is not needed.
  }

  QString cacheDirectory = Config::getCacheDir();
  if ( !QDir().mkpath( cacheDirectory ) ) {
    cacheDirectory = QStandardPaths::writableLocation( QStandardPaths::CacheLocation );
    qWarning( "Cannot create a cache directory %s. use default cache path.", cacheDirectory.toUtf8().constData() );
  }

  QNetworkDiskCache * const diskCache = new QNetworkDiskCache( this );
  diskCache->setMaximumCacheSize( maxCacheSizeInBytes );
  diskCache->setCacheDirectory( cacheDirectory );
  articleNetMgr.setCache( diskCache );
}

void MainWindow::makeDictionaries()
{

  wordFinder.clear();

  dictionariesUnmuted.clear();

  ftsIndexing.stopIndexing();
  ftsIndexing.clearDictionaries();

  loadDictionaries( this, cfg, dictionaries, dictNetMgr, false );

  //create map
  dictMap = Dictionary::dictToMap( dictionaries );

  for ( unsigned x = 0; x < dictionaries.size(); x++ ) {
    dictionaries[ x ]->setFTSParameters( cfg.preferences.fts );
    dictionaries[ x ]->setSynonymSearchEnabled( cfg.preferences.synonymSearchEnabled );
  }

  ftsIndexing.setDictionaries( dictionaries );
  ftsIndexing.doIndexing();

  updateStatusLine();
  updateGroupList( false );
}

void MainWindow::updateStatusLine()
{
  unsigned articleCount = 0, wordCount = 0;

  for ( unsigned x = dictionaries.size(); x--; ) {
    articleCount += dictionaries[ x ]->getArticleCount();
    wordCount += dictionaries[ x ]->getWordCount();
  }

  mainStatusBar->showMessage(
    tr( "%1 dictionaries, %2 articles, %3 words" ).arg( dictionaries.size() ).arg( articleCount ).arg( wordCount ),
    10000 );
}

void MainWindow::updateGroupList( bool reload )
{
  bool haveGroups = !cfg.groups.empty();

  groupList->setVisible( haveGroups );

  //  groupLabel.setText( haveGroups ? tr( "Look up in:" ) : tr( "Look up:" ) );

  // currentIndexChanged() signal is very trigger-happy. To avoid triggering
  // it, we disconnect it while we're clearing and filling back groups.
  disconnect( groupList, &GroupComboBox::currentIndexChanged, this, &MainWindow::currentGroupChanged );

  groupInstances.clear();

  // Add dictionaryOrder first, as the 'All' group.
  {
    Instances::Group g( cfg.dictionaryOrder, dictionaries, Config::Group() );

    // Add any missing entries to dictionary order
    Instances::complementDictionaryOrder( g,
                                          Instances::Group( cfg.inactiveDictionaries, dictionaries, Config::Group() ),
                                          dictionaries );

    g.name = tr( "All" );
    g.id   = GroupId::AllGroupId;
    g.icon = "folder.png";

    groupInstances.push_back( g );
  }

  GlobalBroadcaster::instance()->groupFolderMap.clear();
  for ( auto & group : cfg.groups ) {
    groupInstances.push_back( Instances::Group( group, dictionaries, cfg.inactiveDictionaries ) );
    GlobalBroadcaster::instance()->groupFolderMap.insert( group.id, group.favoritesFolder );
  }

  // Update names for dictionaries that are present, so that they could be
  // found in case they got moved.
  Instances::updateNames( cfg, dictionaries );

  groupList->fill( groupInstances );
  groupList->setCurrentGroup( cfg.lastMainGroupId );

  updateDictionaryBar();

  if ( reload ) {
    qDebug() << "Reloading all the tabs...";

    for ( int i = 0; i < ui.tabWidget->count(); ++i ) {
      auto & view = dynamic_cast< ArticleView & >( *( ui.tabWidget->widget( i ) ) );

      view.reload();
    }
  }

  connect( groupList, &GroupComboBox::currentIndexChanged, this, &MainWindow::currentGroupChanged );
}

void MainWindow::updateDictionaryBar()
{
  if ( !dictionaryBar.toggleViewAction()->isChecked() ) {
    return; // It's not enabled, therefore hidden -- don't waste time
  }

  unsigned currentId     = groupList->getCurrentGroup();
  Instances::Group * grp = groupInstances.findGroup( currentId );

  dictionaryBar.setMutedDictionaries( nullptr );
  if ( grp ) { // Should always be !0, but check as a safeguard
    if ( currentId == GroupId::AllGroupId ) {
      dictionaryBar.setMutedDictionaries( &cfg.mutedDictionaries );
    }
    else {
      Config::Group * _grp = cfg.getGroup( currentId );
      dictionaryBar.setMutedDictionaries( _grp ? &_grp->mutedDictionaries : nullptr );
    }

    dictionaryBar.setDictionaries( grp->dictionaries );

    if ( useSmallIconsInToolbarsAction.isChecked() ) {
      dictionaryBar.setDictionaryIconSize( DictionaryBar::IconSize::Small );
    }
    else if ( useLargeIconsInToolbarsAction.isChecked() ) {
      dictionaryBar.setDictionaryIconSize( DictionaryBar::IconSize::Large );
    }
    else {
      dictionaryBar.setDictionaryIconSize( DictionaryBar::IconSize::Normal );
    }
  }
}

vector< sptr< Dictionary::Class > > const & MainWindow::getActiveDicts()
{
  if ( groupInstances.empty() ) {
    return dictionaries;
  }

  int current = groupList->currentIndex();

  if ( current < 0 || current >= (int)groupInstances.size() ) {
    // This shouldn't ever happen
    return dictionaries;
  }

  Config::MutedDictionaries const * mutedDictionaries = dictionaryBar.getMutedDictionaries();
  if ( !dictionaryBar.toggleViewAction()->isChecked() || mutedDictionaries == nullptr ) {
    return groupInstances[ current ].dictionaries;
  }
  else {
    vector< sptr< Dictionary::Class > > const & activeDicts = groupInstances[ current ].dictionaries;

    // Populate the special dictionariesUnmuted array with only unmuted
    // dictionaries

    dictionariesUnmuted.clear();
    dictionariesUnmuted.reserve( activeDicts.size() );

    for ( unsigned x = 0; x < activeDicts.size(); ++x ) {
      if ( !mutedDictionaries->contains( QString::fromStdString( activeDicts[ x ]->getId() ) ) ) {
        dictionariesUnmuted.push_back( activeDicts[ x ] );
      }
    }

    return dictionariesUnmuted;
  }
}

void MainWindow::createTabList()
{
  tabListMenu->setIcon( QIcon( ":/icons/windows-list.svg" ) );
  connect( tabListMenu, &QMenu::aboutToShow, this, &MainWindow::fillWindowsMenu );
  connect( tabListMenu, &QMenu::triggered, this, &MainWindow::switchToWindow );

  tabListButton = new QToolButton( ui.tabWidget );
  tabListButton->setAutoRaise( true );
  tabListButton->setIcon( tabListMenu->icon() );
  tabListButton->setMenu( tabListMenu );
  tabListButton->setToolTip( tr( "Open Tabs List" ) );
  tabListButton->setPopupMode( QToolButton::InstantPopup );
  ui.tabWidget->setCornerWidget( tabListButton );
  tabListButton->setFocusPolicy( Qt::NoFocus );
}

void MainWindow::fillWindowsMenu()
{
  tabListMenu->clear();

  if ( cfg.preferences.mruTabOrder ) {
    for ( int i = 0; i < mruList.count(); i++ ) {
      QAction * act = tabListMenu->addAction( ui.tabWidget->tabIcon( ui.tabWidget->indexOf( mruList.at( i ) ) ),
                                              ui.tabWidget->tabText( ui.tabWidget->indexOf( mruList.at( i ) ) ) );

      //remember the index of the Tab to be later used in ctrlReleased()
      act->setData( ui.tabWidget->indexOf( mruList.at( i ) ) );

      if ( ui.tabWidget->currentIndex() == ui.tabWidget->indexOf( mruList.at( i ) ) ) {
        QFont f( act->font() );
        f.setBold( true );
        act->setFont( f );
      }
    }
    if ( tabListMenu->actions().size() > 1 ) {
      tabListMenu->setActiveAction( tabListMenu->actions().at( 1 ) );
    }
  }
  else {
    for ( int i = 0; i < ui.tabWidget->count(); i++ ) {
      QAction * act = tabListMenu->addAction( ui.tabWidget->tabIcon( i ), ui.tabWidget->tabText( i ) );
      act->setData( i );
      if ( ui.tabWidget->currentIndex() == i ) {
        QFont f( act->font() );
        f.setBold( true );
        act->setFont( f );
      }
    }
  }
}

void MainWindow::switchToWindow( QAction * act )
{
  int idx = act->data().toInt();
  ui.tabWidget->setCurrentIndex( idx );
}

void MainWindow::addNewTab()
{
  createNewTab( true, tr( "(untitled)" ) );
}

ArticleView * MainWindow::createNewTab( bool switchToIt, QString const & name )
{
  ArticleView * view = new ArticleView( this,
                                        articleNetMgr,
                                        audioPlayerFactory.player(),
                                        dictionaries,
                                        groupInstances,
                                        false,
                                        cfg,
                                        translateLine,
                                        dictionaryBar.toggleViewAction(),
                                        groupList->getCurrentGroup() );

  connect( view, &ArticleView::inspectSignal, this, &MainWindow::inspectElement );

  connect( view, &ArticleView::titleChanged, this, &MainWindow::titleChanged );

  connect( view, &ArticleView::pageLoaded, this, &MainWindow::pageLoaded );

  connect( view, &ArticleView::updateFoundInDictsList, this, &MainWindow::updateFoundInDictsList );

  connect( view, &ArticleView::openLinkInNewTab, this, &MainWindow::openLinkInNewTab );

  connect( view, &ArticleView::showDefinitionInNewTab, this, &MainWindow::showDefinitionInNewTab );

  connect( view, &ArticleView::typingEvent, this, &MainWindow::typingEvent );

  connect( view, &ArticleView::activeArticleChanged, this, &MainWindow::activeArticleChanged );

  connect( view, &ArticleView::statusBarMessage, this, &MainWindow::showStatusBarMessage );

  connect( view, &ArticleView::showDictsPane, this, &MainWindow::showDictsPane );

  connect( view, &ArticleView::forceAddWordToHistory, this, &MainWindow::forceAddWordToHistory );

  connect( view, &ArticleView::sendWordToHistory, this, &MainWindow::addWordToHistory );

  connect( view, &ArticleView::sendWordToInputLine, this, &MainWindow::sendWordToInputLine );

  connect( view, &ArticleView::storeResourceSavePath, this, &MainWindow::storeResourceSavePath );

  connect( view, &ArticleView::zoomIn, this, &MainWindow::zoomin );

  connect( view, &ArticleView::zoomOut, this, &MainWindow::zoomout );
  connect( view, &ArticleView::saveBookmarkSignal, this, &MainWindow::addBookmarkToFavorite );

  connect( ui.searchInPageAction, &QAction::triggered, view, [ this, view ]() {
#ifdef Q_OS_MACOS
    //workaround to fix macos popup page search Ctrl + F
    if ( scanPopup && scanPopup->isActiveWindow() ) {
      scanPopup->openSearch();
      return;
    }
#endif
    view->openSearch();
  } );

  view->setSelectionBySingleClick( cfg.preferences.selectWordBySingleClick );

  int index = cfg.preferences.newTabsOpenAfterCurrentOne ? ui.tabWidget->currentIndex() + 1 : ui.tabWidget->count();

  QString escaped = Utils::escapeAmps( name );

  ui.tabWidget->insertTab( index, view, escaped );
  mruList.append( dynamic_cast< QWidget * >( view ) );

  if ( switchToIt ) {
    ui.tabWidget->setCurrentIndex( index );
  }

  view->setZoomFactor( cfg.preferences.zoomFactor );

#ifdef Q_OS_WIN32
  view->installEventFilter( this );
#endif
  return view;
}

void MainWindow::inspectElement( QWebEnginePage * page )
{
  inspector->triggerAction( page );
}

void MainWindow::tabCloseRequested( int x )
{
  QWidget * w = ui.tabWidget->widget( x );

  mruList.removeOne( w );

  // In MRU case: First, we switch to the appropriate tab
  // and only then remove the old one.

  //activate a tab in accordance with MRU
  if ( cfg.preferences.mruTabOrder && !mruList.empty() ) {
    ui.tabWidget->setCurrentWidget( mruList.at( 0 ) );
  }
  else if ( ui.tabWidget->count() > 1 ) {
    //activate neighboring tab
    int n = x >= ui.tabWidget->count() - 1 ? x - 1 : x + 1;
    if ( n >= 0 ) {
      ui.tabWidget->setCurrentIndex( n );
    }
  }

  ui.tabWidget->removeTab( x );
  delete w;

  // if everything is closed, add a new tab
  if ( ui.tabWidget->count() == 0 ) {
    addNewTab();
  }
}

void MainWindow::closeCurrentTab()
{
  tabCloseRequested( ui.tabWidget->currentIndex() );
}

void MainWindow::closeAllTabs()
{
  while ( ui.tabWidget->count() > 1 ) {
    closeCurrentTab();
  }

  // close last tab
  closeCurrentTab();
}

void MainWindow::closeRestTabs()
{
  if ( ui.tabWidget->count() < 2 ) {
    return;
  }

  int idx = ui.tabWidget->currentIndex();

  for ( int i = 0; i < idx; i++ ) {
    tabCloseRequested( 0 );
  }

  ui.tabWidget->setCurrentIndex( 0 );

  while ( ui.tabWidget->count() > 1 ) {
    tabCloseRequested( 1 );
  }
}

void MainWindow::switchToNextTab()
{
  if ( ui.tabWidget->count() < 2 ) {
    return;
  }

  ui.tabWidget->setCurrentIndex( ( ui.tabWidget->currentIndex() + 1 ) % ui.tabWidget->count() );
}

void MainWindow::switchToPrevTab()
{
  if ( ui.tabWidget->count() < 2 ) {
    return;
  }

  if ( !ui.tabWidget->currentIndex() ) {
    ui.tabWidget->setCurrentIndex( ui.tabWidget->count() - 1 );
  }
  else {
    ui.tabWidget->setCurrentIndex( ui.tabWidget->currentIndex() - 1 );
  }
}

void MainWindow::backClicked()
{
  ArticleView * view = getCurrentArticleView();

  view->back();
}

void MainWindow::forwardClicked()
{
  ArticleView * view = getCurrentArticleView();

  view->forward();
}

void MainWindow::titleChanged( ArticleView * view, QString const & title )
{
  //the title can be url if html title is empty.according to qwebenginepage title() document.
  QString escaped;
  if ( title != nullptr && title.contains( ":" ) ) {
    //check if the title is url.
    QUrl url( title );
    escaped = Utils::Url::queryItemValue( url, "word" );
  }
  else {
    escaped = title;
  }
  escaped = Utils::escapeAmps( escaped );

  int index = ui.tabWidget->indexOf( view );
  if ( !escaped.isEmpty() ) {
    ui.tabWidget->setTabText( index, escaped );
  }

  if ( index == ui.tabWidget->currentIndex() ) {
    // Set icon for "Add to Favorites" action
    if ( isWordPresentedInFavorites( title, cfg.lastMainGroupId ) ) {
      addToFavorites->setIcon( blueStarIcon );
      addToFavorites->setToolTip( tr( "Remove current tab from Favorites" ) );
    }
    else {
      addToFavorites->setIcon( starIcon );
      addToFavorites->setToolTip( tr( "Add current tab to Favorites" ) );
    }

    updateWindowTitle();
  }
}

void MainWindow::iconChanged( ArticleView * view, QIcon const & icon )
{
  ui.tabWidget->setTabIcon( ui.tabWidget->indexOf( view ), groupInstances.size() > 1 ? icon : QIcon() );
}

void MainWindow::updateWindowTitle()
{
  ArticleView * view = getCurrentArticleView();
  if ( view ) {
    QString str = view->getTitle();
    if ( !str.isEmpty() ) {
      setWindowTitle( QString( "%1 - %2" ).arg( str, "GoldenDict-ng" ) );
    }
  }
}

void MainWindow::pageLoaded( ArticleView * view )
{
  if ( view != getCurrentArticleView() ) {
    return; // It was background action
  }

  updateBackForwardButtons();
  updatePronounceAvailability();
}

void MainWindow::showStatusBarMessage( QString const & message, int timeout, QPixmap const & icon )
{
  if ( message.isEmpty() ) {
    mainStatusBar->clearMessage();
  }
  else {
    mainStatusBar->showMessage( message, timeout, icon );
  }
}

void MainWindow::tabSwitched( int )
{
  translateBox->setPopupEnabled( false );
  updateBackForwardButtons();
  updatePronounceAvailability();
  updateFoundInDictsList();
  updateWindowTitle();
  if ( mruList.size() > 1 ) {
    int from = mruList.indexOf( ui.tabWidget->widget( ui.tabWidget->currentIndex() ) );
    if ( from > 0 ) {
      mruList.move( from, 0 );
    }
  }

  // Set icon for "Add to Favorites" action
  QString headword = ui.tabWidget->tabText( ui.tabWidget->currentIndex() );
  if ( isWordPresentedInFavorites( unescapeTabHeader( headword ), cfg.lastMainGroupId ) ) {
    addToFavorites->setIcon( blueStarIcon );
    addToFavorites->setToolTip( tr( "Remove current tab from Favorites" ) );
  }
  else {
    addToFavorites->setIcon( starIcon );
    addToFavorites->setToolTip( tr( "Add current tab to Favorites" ) );
  }

  auto view = getCurrentArticleView();
  if ( view ) {
    groupList->setCurrentGroup( view->getCurrentGroupId() );
  }
}

void MainWindow::tabMenuRequested( QPoint pos )
{
  //  // do not show this menu for single tab
  //  if ( ui.tabWidget->count() < 2 )
  //    return;

  tabMenu->popup( ui.tabWidget->mapToGlobal( pos ) );
}

void MainWindow::dictionaryBarToggled( bool )
{
  // From now on, only the triggered() signal is interesting to us
  disconnect( dictionaryBar.toggleViewAction(), &QAction::toggled, this, &MainWindow::dictionaryBarToggled );

  updateDictionaryBar();         // Updates dictionary bar contents if it's shown
  applyMutedDictionariesState(); // Visibility change affects searches and results
}

void MainWindow::showDictsPane()
{
  if ( !ui.dictsPane->isVisible() ) {
    ui.dictsPane->show();
  }
}

void MainWindow::dictsPaneVisibilityChanged( bool visible )
{
  if ( visible ) {
    updateFoundInDictsList();
  }
}

void MainWindow::updateFoundInDictsList()
{
  if ( !ui.dictsList->isVisible() ) {
    // nothing to do, the list is not visible
    return;
  }

  ui.dictsList->clear();

  ArticleView * view = getCurrentArticleView();

  if ( view ) {
    QStringList ids  = view->getArticlesList();
    QString activeId = view->getActiveArticleId();

    for ( QStringList::const_iterator i = ids.constBegin(); i != ids.constEnd(); ++i ) {
      // Find this dictionary

      for ( unsigned x = dictionaries.size(); x--; ) {
        if ( dictionaries[ x ]->getId() == i->toUtf8().data() ) {
          QString dictName = QString::fromUtf8( dictionaries[ x ]->getName().c_str() );
          QString dictId   = QString::fromUtf8( dictionaries[ x ]->getId().c_str() );
          QListWidgetItem * item =
            new QListWidgetItem( dictionaries[ x ]->getIcon(), dictName, ui.dictsList, QListWidgetItem::Type );
          item->setData( Qt::UserRole, QVariant( dictId ) );
          item->setToolTip( dictName );

          ui.dictsList->addItem( item );
          if ( dictId == activeId ) {
            ui.dictsList->setCurrentItem( item );
          }
          break;
        }
      }
    }

    //if no item in dict List panel has been choose ,select first one.
    if ( ui.dictsList->count() > 0 && ui.dictsList->selectedItems().empty() ) {
      ui.dictsList->setCurrentRow( 0 );
    }
  }
}

void MainWindow::updateBackForwardButtons()
{
  ArticleView * view = getCurrentArticleView();

  if ( view ) {
    navBack->setEnabled( view->canGoBack() );
    navForward->setEnabled( view->canGoForward() );
  }
}

void MainWindow::updatePronounceAvailability()
{
  if ( ui.tabWidget->count() > 0 ) {
    getCurrentArticleView()->hasSound( [ this ]( bool has ) {
      navPronounce->setEnabled( has );
    } );
  }
  else {
    navPronounce->setEnabled( false );
  }
}

void MainWindow::editDictionaries( unsigned editDictionaryGroup )
{
  hotkeyWrapper.reset(); // No hotkeys while we're editing dictionaries
  closeHeadwordsDialog();
  closeFullTextSearchDialog();

  wordFinder.clear();
  dictionariesUnmuted.clear();

  { // Limit existence of newCfg

    Config::Class newCfg = cfg;
    EditDictionaries dicts( this, newCfg, dictionaries, groupInstances, dictNetMgr );

    connect( &dicts, &EditDictionaries::showDictionaryInfo, this, &MainWindow::showDictionaryInfo );

    connect( &dicts, &EditDictionaries::showDictionaryHeadwords, this, &MainWindow::showDictionaryHeadwords );

    if ( editDictionaryGroup != GroupId::NoGroupId ) {
      dicts.editGroup( editDictionaryGroup );
    }

    dicts.restoreGeometry( cfg.dictionariesDialogGeometry );
    dicts.exec();
    cfg.dictionariesDialogGeometry = newCfg.dictionariesDialogGeometry = dicts.saveGeometry();

    if ( dicts.areDictionariesChanged() || dicts.areGroupsChanged() ) {
      ftsIndexing.stopIndexing();
      ftsIndexing.clearDictionaries();
      // Set muted dictionaries from old groups
      for ( auto & group : newCfg.groups ) {
        unsigned id = group.id;
        if ( id != GroupId::NoGroupId ) {
          Config::Group const * grp = cfg.getGroup( id );
          if ( grp ) {
            group.mutedDictionaries      = grp->mutedDictionaries;
            group.popupMutedDictionaries = grp->popupMutedDictionaries;
          }
        }
      }

      cfg = newCfg;

      updateGroupList();

      Config::save( cfg );

      updateSuggestionList();

      for ( auto & dictionary : dictionaries ) {
        dictionary->setFTSParameters( cfg.preferences.fts );
        dictionary->setSynonymSearchEnabled( cfg.preferences.synonymSearchEnabled );
      }

      ftsIndexing.setDictionaries( dictionaries );
      ftsIndexing.doIndexing();
    }
  }

  scanPopup->refresh();
  installHotKeys();
}

void MainWindow::editCurrentGroup()
{
  editDictionaries( groupList->getCurrentGroup() );
}

void MainWindow::editPreferences()
{
  hotkeyWrapper.reset(); // So we could use the keys it hooks
  closeHeadwordsDialog();
  closeFullTextSearchDialog();

  ftsIndexing.stopIndexing();
  ftsIndexing.clearDictionaries();

  Preferences preferences( this, cfg );

  preferences.show();

  if ( preferences.exec() == QDialog::Accepted ) {
    Config::Preferences p = preferences.getPreferences();

    // These parameters are not set in dialog
    p.zoomFactor     = cfg.preferences.zoomFactor;
    p.helpZoomFactor = cfg.preferences.helpZoomFactor;
    p.wordsZoomLevel = cfg.preferences.wordsZoomLevel;
    p.hideMenubar    = cfg.preferences.hideMenubar;
    p.searchInDock   = cfg.preferences.searchInDock;
    p.alwaysOnTop    = cfg.preferences.alwaysOnTop;

    p.proxyServer.systemProxyUser     = cfg.preferences.proxyServer.systemProxyUser;
    p.proxyServer.systemProxyPassword = cfg.preferences.proxyServer.systemProxyPassword;

    p.fts.dialogGeometry = cfg.preferences.fts.dialogGeometry;

    p.fts.searchMode = cfg.preferences.fts.searchMode;

    // See if we need to update Appearances
    if ( cfg.preferences.displayStyle != p.displayStyle || cfg.preferences.darkMode != p.darkMode
#if !defined( Q_OS_WIN )
         || cfg.preferences.interfaceStyle != p.interfaceStyle
#endif
    ) {
      updateAppearances( p.addonStyle,
                         p.displayStyle,
                         p.darkMode
#if !defined( Q_OS_WIN )
                         ,
                         p.interfaceStyle
#endif
      );
    }


    if ( cfg.preferences.historyStoreInterval != p.historyStoreInterval ) {
      history.setSaveInterval( p.historyStoreInterval );
    }

    if ( cfg.preferences.favoritesStoreInterval != p.favoritesStoreInterval ) {
      ui.favoritesPaneWidget->setSaveInterval( p.favoritesStoreInterval );
    }

    if ( cfg.preferences.maxNetworkCacheSize != p.maxNetworkCacheSize ) {
      setupNetworkCache( p.maxNetworkCacheSize );
    }

    bool needReload =
      ( cfg.preferences.displayStyle != p.displayStyle || cfg.preferences.addonStyle != p.addonStyle
        || cfg.preferences.darkReaderMode != p.darkReaderMode
        || cfg.preferences.collapseBigArticles != p.collapseBigArticles
        || cfg.preferences.articleSizeLimit != p.articleSizeLimit
        || cfg.preferences.alwaysExpandOptionalParts != p.alwaysExpandOptionalParts // DSL format's special feature
        || p.darkReaderMode == Config::Dark::Auto // We cannot know if a reload is needed, just do it regardless.
      );

    // This line must be here because the components below require cfg's value to reconfigure
    // After this point, p must not be accessed.
    cfg.preferences = p;

    // Loop through all tabs and reload pages due to ArticleMaker's change.
    for ( int x = 0; x < ui.tabWidget->count(); ++x ) {
      auto & view = dynamic_cast< ArticleView & >( *( ui.tabWidget->widget( x ) ) );

      view.setSelectionBySingleClick( p.selectWordBySingleClick );
      view.syncBackgroundColorWithCfgDarkReader();
      if ( needReload ) {
        view.reload();
      }

#if QT_VERSION >= QT_VERSION_CHECK( 6, 5, 0 )
      if ( cfg.preferences.darkReaderMode == Config::Dark::Auto ) {
        connect( QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, &view, &ArticleView::reload );
      }
      else {
        disconnect( QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, &view, &ArticleView::reload );
      }
#endif
    }

    audioPlayerFactory.setPreferences( cfg.preferences.useInternalPlayer,
                                       cfg.preferences.internalPlayerBackend,
                                       cfg.preferences.audioPlaybackProgram );

    trayIconUpdateOrInit();
    applyProxySettings();

    ui.tabWidget->setHideSingleTab( cfg.preferences.hideSingleTab );

    setAutostart( cfg.preferences.autoStart );

    history.enableAdd( cfg.preferences.storeHistory );
    history.setMaxSize( cfg.preferences.maxStringsInHistory );
    ui.historyPaneWidget->updateHistoryCounts();

    for ( const auto & dictionarie : dictionaries ) {
      dictionarie->setFTSParameters( cfg.preferences.fts );
      dictionarie->setSynonymSearchEnabled( cfg.preferences.synonymSearchEnabled );
    }

    ui.fullTextSearchAction->setEnabled( cfg.preferences.fts.enabled );

    Config::save( cfg );
  }

  scanPopup->refresh();
  installHotKeys();

  ftsIndexing.setDictionaries( dictionaries );
  ftsIndexing.doIndexing();
}

void MainWindow::currentGroupChanged( int )
{
  unsigned grg_id               = groupList->getCurrentGroup();
  cfg.lastMainGroupId           = grg_id;
  Instances::Group const * igrp = groupInstances.findGroup( grg_id );
  if ( grg_id == GroupId::AllGroupId ) {
    if ( igrp ) {
      igrp->checkMutedDictionaries( &cfg.mutedDictionaries );
    }
    dictionaryBar.setMutedDictionaries( &cfg.mutedDictionaries );
  }
  else {
    Config::Group * grp = cfg.getGroup( grg_id );
    if ( grp ) {
      if ( igrp ) {
        igrp->checkMutedDictionaries( &grp->mutedDictionaries );
      }
      dictionaryBar.setMutedDictionaries( &grp->mutedDictionaries );
    }
    else {
      dictionaryBar.setMutedDictionaries( nullptr );
    }
  }

  if ( igrp ) {
    GlobalBroadcaster::instance()->currentGroupId = grg_id;
    ui.tabWidget->setTabIcon( ui.tabWidget->currentIndex(), igrp->makeIcon() );
  }
  else {
    ui.tabWidget->setTabIcon( ui.tabWidget->currentIndex(), QIcon() );
  }

  updateDictionaryBar();

  // Update word search results
  translateBox->setPopupEnabled( false );
  updateSuggestionList();

  if ( auto view = getCurrentArticleView() ) {
    if ( view->getCurrentGroupId() != grg_id ) {
      view->setCurrentGroupId( grg_id );
      QString word = Folding::unescapeWildcardSymbols( view->getWord() );
      respondToTranslationRequest( word, false );
    }
  }

  if ( ftsDlg ) {
    ftsDlg->setCurrentGroup( grg_id );
  }
}

void MainWindow::translateInputChanged( QString const & newValue )
{
  updateSuggestionList( newValue );
  // Save translate line text. Later it can be passed to external applications.
  GlobalBroadcaster::instance()->translateLineText = newValue;
}

void MainWindow::updateSuggestionList()
{
  updateSuggestionList( translateLine->text() );
}

void MainWindow::updateSuggestionList( QString const & newValue )
{
  // If there's some status bar message present, clear it since it may be
  // about the previous search that has failed.
  if ( !mainStatusBar->currentMessage().isEmpty() ) {
    mainStatusBar->clearMessage();
  }

  // If some word is selected in the word list, unselect it. This prevents
  // triggering a set of spurious activation signals when the list changes.

  if ( ui.wordList->selectionModel()->hasSelection() ) {
    ui.wordList->setCurrentItem( nullptr, QItemSelectionModel::Clear );
  }

  QString req = newValue.trimmed();

  if ( !req.size() ) {
    // An empty request always results in an empty result
    wordFinder.cancel();
    ui.wordList->clear();
    ui.wordList->unsetCursor();

    // Reset the noResults mark if it's on right now
    Utils::Widget::setNoResultColor( translateLine, false );
    return;
  }

  ui.wordList->setCursor( Qt::WaitCursor );
  wordFinder.prefixMatch( req, getActiveDicts() );
}

void MainWindow::translateInputFinished( bool checkModifiers )
{
  QString word = Folding::unescapeWildcardSymbols( translateLine->text() );
  respondToTranslationRequest( word, checkModifiers );
}

void MainWindow::respondToTranslationRequest( QString const & word,
                                              bool checkModifiers,
                                              QString const & scrollTo,
                                              bool focus )
{
  if ( !word.isEmpty() ) {
    Qt::KeyboardModifiers mods = QApplication::keyboardModifiers();
    if ( checkModifiers && ( mods & ( Qt::ControlModifier | Qt::ShiftModifier ) ) ) {
      addNewTab();
    }

    showTranslationFor( word, 0, scrollTo );

    if ( cfg.preferences.searchInDock ) {
      if ( ui.searchPane->isFloating() ) {
        activateWindow();
      }
    }

    if ( focus ) {
      focusArticleView();
    }
  }
}

void MainWindow::setInputLineText( QString text, WildcardPolicy wildcardPolicy, TranslateBoxPopup popupAction )
{
  if ( wildcardPolicy == WildcardPolicy::EscapeWildcards ) {
    text = Folding::escapeWildcardSymbols( text );
  }

  if ( popupAction == NoPopupChange || cfg.preferences.searchInDock ) {
    translateLine->setText( text );
  }
  else {
    translateBox->setText( text, popupAction == EnablePopup );
  }

  updateSuggestionList();
  GlobalBroadcaster::instance()->translateLineText = text;
}

void MainWindow::handleEsc()
{
  ArticleView * view = getCurrentArticleView();
  if ( view && view->closeSearch() ) {
    return;
  }

  if ( cfg.preferences.escKeyHidesMainWindow ) {
    toggleMainWindow( false );
  }
  else {
    focusTranslateLine();
  }
}

void MainWindow::focusTranslateLine()
{
  if ( cfg.preferences.searchInDock ) {
    if ( ui.searchPane->isFloating() ) {
      ui.searchPane->activateWindow();
    }
  }
  else {
    if ( !isActiveWindow() ) {
      activateWindow();
    }
  }

  translateLine->clearFocus();
  translateLine->setFocus();
  translateLine->selectAll();
}

void MainWindow::applyMutedDictionariesState()
{
  translateBox->setPopupEnabled( false );

  updateSuggestionList();

  ArticleView * view = getCurrentArticleView();

  if ( view ) {
    // Update active article view
    view->updateMutedContents();
  }
}

bool MainWindow::handleBackForwardMouseButtons( QMouseEvent * event )
{
  if ( event->button() == Qt::XButton1 ) {
    backClicked();
    return true;
  }
  else if ( event->button() == Qt::XButton2 ) {
    forwardClicked();
    return true;
  }
  else {
    return false;
  }
}

bool MainWindow::eventFilter( QObject * obj, QEvent * ev )
{
  if ( ev->type() == QEvent::ShortcutOverride || ev->type() == QEvent::KeyPress ) {
    auto * ke = dynamic_cast< QKeyEvent * >( ev );
    // Handle F3/Shift+F3 shortcuts
    int const key = ke->key();
    if ( key == Qt::Key_F3 ) {
      ArticleView * view = getCurrentArticleView();
      if ( view && view->handleF3( obj, ev ) ) {
        return true;
      }
    }

    //workaround to fix #660
    if ( obj == this && ev->type() == QEvent::KeyPress
         && ( key == Qt::Key_Up || key == Qt::Key_Down || key == Qt::Key_Space || key == Qt::Key_PageUp
              || key == Qt::Key_PageDown ) ) {
      ArticleView * view = getCurrentArticleView();
      if ( view ) {
        view->focus();
        return true;
      }
    }
  }

  // when the main window is moved or resized, hide the word list suggestions
  if ( obj == this && ( ev->type() == QEvent::Move || ev->type() == QEvent::Resize ) ) {
    if ( !cfg.preferences.searchInDock ) {
      translateBox->setPopupEnabled( false );
      return false;
    }
  }

  if ( obj == this && ev->type() == QEvent::WindowStateChange ) {
    auto stev    = dynamic_cast< QWindowStateChangeEvent * >( ev );
    wasMaximized = ( stev->oldState() == Qt::WindowMaximized && isMinimized() );
  }

  if ( ev->type() == QEvent::MouseButtonPress ) {
    auto event = dynamic_cast< QMouseEvent * >( ev );

    return handleBackForwardMouseButtons( event );
  }

  if ( ev->type() == QEvent::KeyPress ) {
    auto keyevent = dynamic_cast< QKeyEvent * >( ev );

    bool const handleCtrlTab = ( obj == translateLine || obj == ui.wordList || obj == ui.historyList
                                 || obj == ui.favoritesTree || obj == ui.dictsList || obj == groupList );

    if ( keyevent->modifiers() == Qt::ControlModifier && keyevent->key() == Qt::Key_Tab ) {
      if ( cfg.preferences.mruTabOrder ) {
        ctrlTabPressed();
        return true;
      }
      else if ( handleCtrlTab ) {
        QApplication::sendEvent( ui.tabWidget, ev );
        return true;
      }
      return false;
    }

    // Handle only Ctrl+Shift+Tab here because Ctrl+Tab was already handled before
    if ( handleCtrlTab && keyevent->matches( QKeySequence::PreviousChild ) ) {
      QApplication::sendEvent( ui.tabWidget, ev );
      return true;
    }
  }

  if ( obj == translateLine ) {
    if ( ev->type() == QEvent::KeyPress ) {
      QKeyEvent * keyEvent = dynamic_cast< QKeyEvent * >( ev );

      if ( cfg.preferences.searchInDock ) {
        if ( keyEvent->matches( QKeySequence::MoveToNextLine ) && ui.wordList->count() ) {
          ui.wordList->setFocus( Qt::ShortcutFocusReason );
          ui.wordList->setCurrentRow( 0, QItemSelectionModel::ClearAndSelect );
          return true;
        }
      }
    }

    if ( ev->type() == QEvent::FocusIn ) {

      // select all on mouse click
      if ( const auto focusEvent = dynamic_cast< QFocusEvent * >( ev ); focusEvent->reason() == Qt::MouseFocusReason ) {
        QTimer::singleShot( 0, this, SLOT( focusTranslateLine() ) );
      }
      return false;
    }
  }
  else if ( obj == ui.wordList ) {
    if ( ev->type() == QEvent::KeyPress ) {
      QKeyEvent * keyEvent = dynamic_cast< QKeyEvent * >( ev );

      if ( keyEvent->matches( QKeySequence::MoveToPreviousLine ) && !ui.wordList->currentRow() ) {
        ui.wordList->setCurrentRow( 0, QItemSelectionModel::Clear );
        translateLine->setFocus( Qt::ShortcutFocusReason );
        return true;
      }

      if ( keyEvent->matches( QKeySequence::InsertParagraphSeparator ) && ui.wordList->selectedItems().size() ) {
        if ( cfg.preferences.searchInDock ) {
          if ( ui.searchPane->isFloating() ) {
            activateWindow();
          }
        }

        focusArticleView();

        return cfg.preferences.searchInDock;
      }
    }
  }

  if ( ev->type() == QEvent::KeyPress && obj != translateLine ) {

    if ( const auto key_event = dynamic_cast< QKeyEvent * >( ev ); key_event->modifiers() == Qt::NoModifier ) {
      const QString text = key_event->text();

      if ( Utils::ignoreKeyEvent( key_event ) || key_event->key() == Qt::Key_Return
           || key_event->key() == Qt::Key_Enter ) {
        return false; // Those key have other uses than to start typing
      }
      // or don't make sense
      if ( !text.isEmpty() ) {
        typingEvent( text );
        return true;
      }
    }
  }

  return QMainWindow::eventFilter( obj, ev );
}

void MainWindow::wordListItemActivated( QListWidgetItem * item )
{
  if ( wordListSelChanged ) {
    wordListSelChanged = false;
  }
  else {
    respondToTranslationRequest( item->text(), true );
  }
}

void MainWindow::wordListSelectionChanged()
{
  QList< QListWidgetItem * > selected = ui.wordList->selectedItems();

  if ( !selected.empty() ) {
    wordListSelChanged = true;
    showTranslationFor( selected.front()->text() );
  }
}

void MainWindow::dictsListItemActivated( QListWidgetItem * item )
{
  jumpToDictionary( item, true );
}

void MainWindow::dictsListSelectionChanged()
{
  QList< QListWidgetItem * > selected = ui.dictsList->selectedItems();
  if ( selected.size() ) {
    jumpToDictionary( selected.front() );
  }
}

void MainWindow::jumpToDictionary( QListWidgetItem * item, bool force )
{
  ArticleView * view = getCurrentArticleView();
  if ( view ) {
    view->jumpToDictionary( item->data( Qt::UserRole ).toString(), force );
  }
}

void MainWindow::openLinkInNewTab( QUrl const & url,
                                   QUrl const & referrer,
                                   QString const & fromArticle,
                                   Contexts const & contexts )
{
  createNewTab( !cfg.preferences.newTabsOpenInBackground, "" )->openLink( url, referrer, fromArticle, contexts );
}

void MainWindow::showDefinitionInNewTab( QString const & word,
                                         unsigned group,
                                         QString const & fromArticle,
                                         Contexts const & contexts )
{
  createNewTab( !cfg.preferences.newTabsOpenInBackground, word )->showDefinition( word, group, fromArticle, contexts );
}

void MainWindow::activeArticleChanged( ArticleView const * view, QString const & id )
{
  if ( view != getCurrentArticleView() ) {
    return; // It was background action
  }

  // select the row with the corresponding id
  for ( int i = 0; i < ui.dictsList->count(); ++i ) {
    QListWidgetItem * w = ui.dictsList->item( i );
    QString dictId      = w->data( Qt::UserRole ).toString();

    if ( dictId == id ) {
      // update the current row, but only if necessary
      if ( i != ui.dictsList->currentRow() ) {
        ui.dictsList->setCurrentRow( i );
      }
      return;
    }
  }
}

void MainWindow::typingEvent( QString const & t )
{
  if ( t == "\n" || t == "\r" ) {
    if ( translateLine->isEnabled() ) {
      focusTranslateLine();
    }
  }
  else {
    if ( ( cfg.preferences.searchInDock && ui.searchPane->isFloating() ) || ui.dictsPane->isFloating() ) {
      ui.searchPane->activateWindow();
    }

    if ( translateLine->isEnabled() ) {
      if ( navToolbar->isFloating() ) {
        navToolbar->activateWindow();
      }

      translateLine->clear();
      translateLine->setFocus();
      // Escaping the typed-in characters is the user's responsibility.
      setInputLineText( t, WildcardPolicy::WildcardsAreAlreadyEscaped, EnablePopup );
      translateLine->setCursorPosition( t.size() );
    }
  }
}

void MainWindow::mutedDictionariesChanged()
{
  if ( dictionaryBar.toggleViewAction()->isChecked() ) {
    applyMutedDictionariesState();
  }
}

void MainWindow::showHistoryItem( QString const & word )
{
  // qDebug() << "Showing history item" << word;

  history.enableAdd( false );

  setInputLineText( word, WildcardPolicy::EscapeWildcards, DisablePopup );
  showTranslationFor( word );

  history.enableAdd( cfg.preferences.storeHistory );
}

void MainWindow::showTranslationFor( QString const & word, unsigned inGroup, QString const & scrollTo )
{
  ArticleView * view = getCurrentArticleView();

  navPronounce->setEnabled( false );

  unsigned group = inGroup ? inGroup : ( groupInstances.empty() ? 0 : groupInstances[ groupList->currentIndex() ].id );

  view->showDefinition( word, group, scrollTo );

  //ui.tabWidget->setTabText( ui.tabWidget->indexOf(ui.tab), inWord.trimmed() );
}

void MainWindow::showTranslationForDicts( QString const & inWord,
                                          QStringList const & dictIDs,
                                          QRegularExpression const & searchRegExp,
                                          bool ignoreDiacritics )
{
  ArticleView * view = getCurrentArticleView();

  navPronounce->setEnabled( false );

  view->showDefinition( inWord,
                        dictIDs,
                        searchRegExp,
                        groupInstances[ groupList->currentIndex() ].id,
                        ignoreDiacritics );
}

void MainWindow::toggleMainWindow( bool ensureShow )
{
  bool shown = false;

  if ( !cfg.preferences.searchInDock ) {
    translateBox->setPopupEnabled( false );
  }

  if ( !isVisible() ) {
    show();

    activateWindow();
    raise();
    shown = true;
  }
  else if ( isMinimized() ) {
    if ( wasMaximized ) {
      showMaximized();
    }
    else {
      showNormal();
    }
    activateWindow();
    raise();
    shown = true;
  }
  else if ( !isActiveWindow() ) {
    activateWindow();
    if ( cfg.preferences.raiseWindowOnSearch ) {
      raise();
    }
    shown = true;
  }
  else if ( !ensureShow ) {

    // On Windows and Linux, a hidden window won't show a task bar icon
    // When trayicon is enabled, the duplication is unneeded

    // On macOS, a hidden window will still show on the Dock,
    // but click it won't bring it back, thus we can only minimize it.

#ifdef Q_OS_MAC
    if ( cfg.preferences.enableTrayIcon ) {
      showMinimized();
    }
#else
    if ( cfg.preferences.enableTrayIcon )
      hide();
    else
      showMinimized();
#endif


    if ( headwordsDlg ) {
      headwordsDlg->hide();
    }

    if ( ftsDlg ) {
      ftsDlg->hide();
    }
  }

  if ( shown ) {
    if ( headwordsDlg ) {
      headwordsDlg->show();
    }

    if ( ftsDlg ) {
      ftsDlg->show();
    }

    focusTranslateLine();
  }
}

void MainWindow::installHotKeys()
{
#if defined( Q_OS_UNIX ) && !defined( Q_OS_MACOS )
  if ( !qEnvironmentVariableIsEmpty( "GOLDENDICT_FORCE_WAYLAND" ) ) {
    return;
  }
#endif

  hotkeyWrapper.reset(); // Remove the old one

  if ( cfg.preferences.enableMainWindowHotkey || cfg.preferences.enableClipboardHotkey ) {
    try {
      hotkeyWrapper = std::make_shared< HotkeyWrapper >( this );
    }
    catch ( HotkeyWrapper::exInit & ) {
      QMessageBox::critical( this,
                             "GoldenDict",
                             tr( "Failed to initialize hotkeys monitoring mechanism.<br>"
                                 "Make sure your XServer has RECORD extension turned on." ) );

      return;
    }

    if ( cfg.preferences.enableMainWindowHotkey ) {
      hotkeyWrapper->setGlobalKey( cfg.preferences.mainWindowHotkey, 0 );
    }

    if ( cfg.preferences.enableClipboardHotkey ) {
      hotkeyWrapper->setGlobalKey( cfg.preferences.clipboardHotkey, 1 );
    }

    connect( hotkeyWrapper.get(),
             &HotkeyWrapper::hotkeyActivated,
             this,
             &MainWindow::hotKeyActivated,
             Qt::AutoConnection );
  }
}

void MainWindow::hotKeyActivated( int hk )
{
  if ( !hk ) {
    toggleMainWindow( false );
  }
  else if ( scanPopup ) {
#ifdef HAVE_X11
    // When the user requests translation with the Ctrl+C+C hotkey in certain apps
    // on some GNU/Linux systems, GoldenDict appears to handle Ctrl+C+C before the
    // active application finishes handling Ctrl+C. As a result, GoldenDict finds
    // the clipboard empty, silently cancels the translation request, and users report
    // that Ctrl+C+C is broken in these apps. Slightly delay handling the clipboard
    // hotkey to give the active application more time and thus work around the issue.
    QTimer::singleShot( 10, scanPopup, SLOT( translateWordFromClipboard() ) );
#else
    scanPopup->translateWordFromClipboard();
#endif
  }
}

void MainWindow::checkNewRelease()
{
  // Limit release check to 1 per day.
  if ( cfg.timeForNewReleaseCheck < QDateTime::currentDateTime().addDays( 1 ) ) {
    return;
  }

  QNetworkRequest github_release_api;
  github_release_api.setUrl( QUrl( "https://api.github.com/repos/xiaoyifang/goldendict-ng/releases/latest" ) );
  github_release_api.setRawHeader( QByteArray( "Accept" ), QByteArray( "application/vnd.github+json" ) );
  // github_release_api.setRawHeader( QByteArray( "Authorization" ), QByteArray( "" ) );
  github_release_api.setRawHeader( QByteArray( "X-GitHub-Api-Version" ), QByteArray( "2022-11-28" ) );

  auto * github_reply = dictNetMgr.get( github_release_api ); // will be marked as deleteLater when reply finished.

  QObject::connect( github_reply, &QNetworkReply::finished, [ github_reply, this ]() {
    if ( github_reply->error() != QNetworkReply::NoError ) {
      qWarning() << "Version check failed: " << github_reply->errorString();
    }
    else {
      auto latest_release = QJsonDocument::fromJson( github_reply->readAll() );
      if ( !latest_release.isNull() ) {
        const QJsonValue tag_name = latest_release[ "tag_name" ];
        const QJsonValue html_url = latest_release[ "html_url" ];

        if ( tag_name.isString() && html_url.isString() ) {
          QString latestVersion = tag_name.toString().mid( 1, 8 );
          QString downloadUrl   = html_url.toString();

          if ( latestVersion > PROGRAM_VERSION && latestVersion != cfg.skippedRelease ) {
            QMessageBox msg( QMessageBox::Information,
                             tr( "New Release Available" ),
                             tr( "Version <b>%1</b> of GoldenDict is now available for download.<br>"
                                 "Click <b>Download</b> to get to the download page." )
                               .arg( latestVersion ),
                             QMessageBox::NoButton,
                             this );

            QPushButton * dload = msg.addButton( tr( "Download" ), QMessageBox::AcceptRole );
            QPushButton * skip  = msg.addButton( tr( "Skip This Release" ), QMessageBox::DestructiveRole );
            msg.addButton( QMessageBox::Cancel );

            msg.exec();

            if ( msg.clickedButton() == dload ) {
              QDesktopServices::openUrl( QUrl( downloadUrl ) );
            }
            else if ( msg.clickedButton() == skip ) {
              cfg.skippedRelease = latestVersion;
            }
          }
        }
      }
    }

    cfg.timeForNewReleaseCheck = QDateTime::currentDateTime();

    Config::save( cfg );

    github_reply->deleteLater();
  } );
}

void MainWindow::trayIconActivated( QSystemTrayIcon::ActivationReason r )
{
  switch ( r ) {
    case QSystemTrayIcon::Trigger:
      // Left click toggles the visibility of main window
      toggleMainWindow( false );
      break;

    case QSystemTrayIcon::MiddleClick:
      // Middle mouse click on Tray translates selection
      // it is functional like as stardict
      scanPopup->translateWordFromSelection();
      break;
    default:
      break;
  }
}


void MainWindow::visitHomepage()
{
  QDesktopServices::openUrl( QApplication::organizationDomain() );
}

void MainWindow::openConfigFolder()
{
  QDesktopServices::openUrl( QUrl::fromLocalFile( Config::getConfigDir() ) );
}

void MainWindow::visitForum()
{
  QDesktopServices::openUrl( QUrl( "https://github.com/xiaoyifang/goldendict/discussions" ) );
}

void MainWindow::showAbout()
{
  About about( this, &dictionaries );

  about.show();
  about.exec();
}

void MainWindow::showDictBarNamesTriggered()
{
  bool show = showDictBarNamesAction.isChecked();

  dictionaryBar.setToolButtonStyle( show ? Qt::ToolButtonTextBesideIcon : Qt::ToolButtonIconOnly );
  cfg.showingDictBarNames = show;
}

int MainWindow::getIconSize()
{
  bool useLargeIcons = useLargeIconsInToolbarsAction.isChecked();
  int extent         = QApplication::style()->pixelMetric( QStyle::PM_ToolBarIconSize );
  if ( useLargeIcons ) {
    extent = QApplication::style()->pixelMetric( QStyle::PM_LargeIconSize );
  }
  else if ( useSmallIconsInToolbarsAction.isChecked() ) {
    extent = QApplication::style()->pixelMetric( QStyle::PM_SmallIconSize );
  }
  else {
    //empty
  }
  return extent;
}

void MainWindow::iconSizeActionTriggered( QAction * /*action*/ )
{
  //reset word zoom
  cfg.preferences.wordsZoomLevel = 0;
  wordsZoomBase->setEnabled( false );

  bool useLargeIcons = useLargeIconsInToolbarsAction.isChecked();
  int extent         = getIconSize();
  if ( useLargeIcons ) {
    cfg.usingToolbarsIconSize = Config::ToolbarsIconSize::Large;
  }
  else if ( useSmallIconsInToolbarsAction.isChecked() ) {
    cfg.usingToolbarsIconSize = Config::ToolbarsIconSize::Small;
  }
  else {
    cfg.usingToolbarsIconSize = Config::ToolbarsIconSize::Normal;
  }

  navToolbar->setIconSize( QSize( extent, extent ) );
  menuButton->setIconSize( QSize( extent, extent ) );

  updateDictionaryBar();

  scanPopup->setDictionaryIconSize();

  //adjust the font size as well
  auto font = translateBox->translateLine()->font();
  font.setWeight( QFont::Normal );
  //arbitrary value to make it look good
  font.setPixelSize( extent * 0.8 );
  //  translateBox->completerWidget()->setFont( font );
  //only set the font in toolbar
  translateBox->translateLine()->setFont( font );
}

void MainWindow::toggleMenuBarTriggered( bool announce )
{
  cfg.preferences.hideMenubar = !toggleMenuBarAction.isChecked();

  if ( announce ) {
    if ( cfg.preferences.hideMenubar ) {
      mainStatusBar->showMessage( tr( "You have chosen to hide a menubar. Use %1 to show it back." )
                                    .arg( QString( "<b>%1</b>" ) )
                                    .arg( tr( "Ctrl+M" ) ),
                                  10000,
                                  QPixmap( ":/icons/warning.png" ) );
    }
    else {
      mainStatusBar->clearMessage();
    }
  }

  // Obtain from the menubar all the actions with shortcuts
  // and either add them to the main window or remove them,
  // depending on the menubar state.

  QList< QMenu * > allMenus = menuBar()->findChildren< QMenu * >();
  QListIterator< QMenu * > menuIter( allMenus );
  while ( menuIter.hasNext() ) {
    QMenu * menu                      = menuIter.next();
    QList< QAction * > allMenuActions = menu->actions();
    QListIterator< QAction * > actionsIter( allMenuActions );
    while ( actionsIter.hasNext() ) {
      QAction * action = actionsIter.next();
      if ( !action->shortcut().isEmpty() ) {
        if ( cfg.preferences.hideMenubar ) {
          // add all menubar actions to the main window,
          // before we hide the menubar
          addAction( action );
        }
        else {
          // remove all menubar actions from the main window
          removeAction( action );
        }
      }
    }
  }

  menuBar()->setVisible( !cfg.preferences.hideMenubar );
  beforeOptionsSeparator->setVisible( cfg.preferences.hideMenubar );
  menuButtonAction->setVisible( cfg.preferences.hideMenubar );
}

void MainWindow::on_clearHistory_triggered()
{
  history.clear();
  history.save();
}

void MainWindow::on_newTab_triggered()
{
  addNewTab();
}

void MainWindow::setAutostart( bool autostart )
{
#if defined Q_OS_WIN32
  QSettings reg( R"(HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run)", QSettings::NativeFormat );
  if ( autostart ) {
    QString app_fname = QString( R"("%1")" ).arg( QCoreApplication::applicationFilePath() );
    app_fname.replace( "/", R"(\)" );
    reg.setValue( ApplicationSettingName, app_fname );
  }
  else {
    reg.remove( ApplicationSettingName );
  }
  reg.sync();
#elif defined HAVE_X11
  const QString destinationPath = QDir::homePath() + "/.config/autostart/goldendict-owned-by-preferences.desktop";
  if ( autostart == QFile::exists( destinationPath ) )
    return; // Nothing to do.
  if ( autostart ) {
    const QString sourcePath =
      Config::getProgramDataDir() + "../applications/io.github.xiaoyifang.goldendict_ng.desktop";
    QFile::copy( sourcePath, destinationPath );
  }
  else
    QFile::remove( destinationPath );
#endif
}

void MainWindow::on_actionCloseToTray_triggered()
{
  toggleMainWindow( !cfg.preferences.enableTrayIcon );
}

void MainWindow::on_pageSetup_triggered()
{
  if ( getPrinter().isValid() ) {
    QPageSetupDialog dialog( &getPrinter(), this );

    dialog.exec();
  }
  else {
    QMessageBox::critical( this, tr( "Page Setup" ), tr( "No printer is available. Please install one first." ) );
  }
}

void MainWindow::on_printPreview_triggered()
{
  QPrinter _printer;
  QPrintPreviewDialog dialog( &_printer, this );
  auto combox = dialog.findChild< QComboBox * >();
  int index   = combox->findText( "100%" );
  combox->setCurrentIndex( index );

  connect( &dialog, &QPrintPreviewDialog::paintRequested, this, &MainWindow::printPreviewPaintRequested );

  dialog.showMaximized();
  dialog.exec();
}

void MainWindow::on_print_triggered()
{
  QPrintDialog dialog( &getPrinter(), this );

  dialog.setWindowTitle( tr( "Print Article" ) );

  if ( dialog.exec() != QDialog::Accepted ) {
    return;
  }

  ArticleView * view = getCurrentArticleView();

  view->print( &getPrinter() );
}

void MainWindow::printPreviewPaintRequested( QPrinter * printer )
{
  ArticleView * view = getCurrentArticleView();

  view->print( printer );
}

static void filterAndCollectResources( QString & html,
                                       QRegularExpression & rx,
                                       const QString & sep,
                                       const QString & folder,
                                       set< QByteArray > & resourceIncluded,
                                       vector< pair< QUrl, QString > > & downloadResources )
{
  int pos = 0;

  auto match = rx.match( html, pos );
  while ( match.hasMatch() ) {
    pos = match.capturedStart();
    QUrl url( match.captured( 1 ) );
    QString host         = url.host();
    QString resourcePath = Utils::Url::path( url );

    if ( !host.startsWith( '/' ) ) {
      host.insert( 0, '/' );
    }
    if ( !resourcePath.startsWith( '/' ) ) {
      resourcePath.insert( 0, '/' );
    }

    QCryptographicHash hash( QCryptographicHash::Md5 );
    hash.addData( match.captured().toUtf8() );

    if ( resourceIncluded.insert( hash.result() ).second ) {
      // Gather resource information (url, filename) to be download later
      downloadResources.emplace_back( url, folder + host + resourcePath );
    }

    // Modify original url, set to the native one
    resourcePath   = QString::fromLatin1( QUrl::toPercentEncoding( resourcePath, "/" ) );
    QString newUrl = sep + QDir( folder ).dirName() + host + resourcePath + sep;
    html.replace( pos, match.captured().length(), newUrl );
    pos += newUrl.length();
    match = rx.match( html, pos );
  }
}

void MainWindow::on_saveArticle_triggered()
{
  ArticleView * view = getCurrentArticleView();

  QString fileName = view->getTitle().simplified();

  // Replace reserved filename characters
  QRegularExpression rxName( R"([/\\\?\*:\|<>])" );
  fileName.replace( rxName, "_" );

  fileName += ".html";
  QString savePath;

  if ( cfg.articleSavePath.isEmpty() ) {
    savePath = QDir::homePath();
  }
  else {
    savePath = QDir::fromNativeSeparators( cfg.articleSavePath );
    if ( !QDir( savePath ).exists() ) {
      savePath = QDir::homePath();
    }
  }

  QFileDialog::Options options = QFileDialog::HideNameFilterDetails;
  QString selectedFilter;
  QStringList filters;
  filters.push_back( tr( "Complete Html (*.html *.htm)" ) );
  filters.push_back( tr( "Single Html (*.html *.htm)" ) );
  filters.push_back( tr( "Pdf (*.pdf)" ) );
  filters.push_back( tr( "Mime Html (*.mhtml)" ) );

  fileName = savePath + "/" + fileName;
  fileName = QFileDialog::getSaveFileName( this,
                                           tr( "Save Article As" ),
                                           fileName,
                                           filters.join( ";;" ),
                                           &selectedFilter,
                                           options );

  qDebug() << "selected filter: " << selectedFilter;
  // The " (*.html)" part of filters[i] is absent from selectedFilter in Qt 5.
  bool const complete = filters.at( 0 ).startsWith( selectedFilter );

  if ( fileName.isEmpty() ) {
    return;
  }

  //Pdf
  if ( filters.at( 2 ).startsWith( selectedFilter ) ) {
    // Create a QWebEnginePage object
    QWebEnginePage * page = view->page();

    // Connect the printFinished signal to handle operations after printing is complete
    connect( page, &QWebEnginePage::pdfPrintingFinished, [ = ]( const QString & filePath, bool success ) {
      if ( success ) {
        qDebug() << "PDF exported successfully to:" << filePath;
      }
      else {
        qDebug() << "Failed to export PDF.";
      }
    } );

    // Print to PDF file
    page->printToPdf( fileName );

    return;
  }

  //mime html
  if ( filters.at( 3 ).startsWith( selectedFilter ) ) {
    // Create a QWebEnginePage object
    QWebEnginePage * page = view->page();
    page->save( fileName, QWebEngineDownloadRequest::MimeHtmlSaveFormat );

    return;
  }

  view->toHtml( [ = ]( QString & html ) mutable {
    QFile file( fileName );
    if ( !file.open( QIODevice::WriteOnly ) ) {
      QMessageBox::critical( this, tr( "Error" ), tr( "Can't save article: %1" ).arg( file.errorString() ) );
    }
    else {
      QFileInfo fi( fileName );
      cfg.articleSavePath = QDir::toNativeSeparators( fi.absoluteDir().absolutePath() );

      // Convert internal links

      static QRegularExpression rx3( R"lit(href="(bword:|gdlookup://localhost/)([^"]+)")lit" );
      int pos = 0;
      QRegularExpression anchorRx( "(g[0-9a-f]{32}_)[0-9a-f]+_" );
      auto match = rx3.match( html, pos );
      while ( match.hasMatch() ) {
        pos          = match.capturedStart();
        QString name = QUrl::fromPercentEncoding( match.captured( 2 ).simplified().toLatin1() );
        QString anchor;
        name.replace( "?gdanchor=", "#" );
        int n = name.indexOf( '#' );
        if ( n > 0 ) {
          anchor = name.mid( n );
          name.truncate( n );
          anchor.replace( anchorRx, "\\1" ); // MDict anchors
        }
        name.replace( rxName, "_" );
        name = QString( R"(href=")" ) + QUrl::toPercentEncoding( name ) + ".html" + anchor + "\"";
        html.replace( pos, match.captured().length(), name );
        pos += name.length();
        match = rx3.match( html, pos );
      }

      // MDict anchors
      QRegularExpression anchorLinkRe(
        R"((<\s*a\s+[^>]*\b(?:name|id)\b\s*=\s*["']*g[0-9a-f]{32}_)([0-9a-f]+_)(?=[^"']))",
        QRegularExpression::PatternOption::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption );
      html.replace( anchorLinkRe, "\\1" );

      if ( complete ) {
        QString folder = fi.absoluteDir().absolutePath() + "/" + fi.baseName() + "_files";
        static QRegularExpression rx1( R"lit("((?:bres|gico|gdau|qrcx|qrc|gdvideo)://[^"]+)")lit" );
        static QRegularExpression rx2( "'((?:bres|gico|gdau|qrcx|qrc|gdvideo)://[^']+)'" );
        set< QByteArray > resourceIncluded;
        vector< pair< QUrl, QString > > downloadResources;

        filterAndCollectResources( html, rx1, "\"", folder, resourceIncluded, downloadResources );
        filterAndCollectResources( html, rx2, "'", folder, resourceIncluded, downloadResources );

        auto * progressDialog = new ArticleSaveProgressDialog( this );
        // reserve '1' for saving main html file
        int maxVal = 1;

        // Pull and save resources to files
        for ( vector< pair< QUrl, QString > >::const_iterator i = downloadResources.begin();
              i != downloadResources.end();
              ++i ) {
          ResourceToSaveHandler * handler = view->saveResource( i->first, i->second );
          if ( !handler->isEmpty() ) {
            maxVal += 1;
            connect( handler, &ResourceToSaveHandler::done, progressDialog, &ArticleSaveProgressDialog::perform );
          }
        }

        progressDialog->setLabelText( tr( "Saving article..." ) );
        progressDialog->setRange( 0, maxVal );
        progressDialog->setValue( 0 );
        progressDialog->show();

        file.write( html.toUtf8() );
        progressDialog->perform();
      }
      else {
        file.write( html.toUtf8() );
      }

      mainStatusBar->showMessage( tr( "Save article complete" ), 5000 );
    }
  } );
}

void MainWindow::on_rescanFiles_triggered()
{
  hotkeyWrapper.reset(); // No hotkeys while we're editing dictionaries
  closeHeadwordsDialog();
  closeFullTextSearchDialog();

  ftsIndexing.stopIndexing();
  ftsIndexing.clearDictionaries();

  groupInstances.clear(); // Release all the dictionaries they hold
  dictionaries.clear();
  dictionariesUnmuted.clear();
  dictionaryBar.setDictionaries( dictionaries );

  loadDictionaries( this, cfg, dictionaries, dictNetMgr );
  dictMap = Dictionary::dictToMap( dictionaries );

  for ( const auto & dictionarie : dictionaries ) {
    dictionarie->setFTSParameters( cfg.preferences.fts );
    dictionarie->setSynonymSearchEnabled( cfg.preferences.synonymSearchEnabled );
  }

  ftsIndexing.setDictionaries( dictionaries );
  ftsIndexing.doIndexing();

  updateGroupList();


  scanPopup->refresh();
  installHotKeys();

  updateSuggestionList();
}

void MainWindow::on_alwaysOnTop_triggered( bool checked )
{
  cfg.preferences.alwaysOnTop = checked;

  bool wasVisible = isVisible();

  Qt::WindowFlags flags = this->windowFlags();
  if ( checked ) {
    setWindowFlags( flags | Qt::CustomizeWindowHint | Qt::WindowStaysOnTopHint );
    mainStatusBar->showMessage( tr( "The main window is set to be always on top." ),
                                10000,
                                QPixmap( ":/icons/warning.png" ) );
  }
  else {
    setWindowFlags( flags ^ ( Qt::CustomizeWindowHint | Qt::WindowStaysOnTopHint ) );
    mainStatusBar->clearMessage();
  }

  if ( wasVisible ) {
    show();
  }

  installHotKeys();
}

void MainWindow::zoomin()
{
  cfg.preferences.zoomFactor += 0.1;
  applyZoomFactor();
}

void MainWindow::zoomout()
{
  cfg.preferences.zoomFactor -= 0.1;
  applyZoomFactor();
}

void MainWindow::unzoom()
{
  cfg.preferences.zoomFactor = 1;
  applyZoomFactor();
}

void MainWindow::applyZoomFactor()
{
  // Always call this function synchronously to potentially disable a zoom action,
  // which is being repeatedly triggered. When the action is disabled, its
  // triggered() signal is no longer emitted, which in turn improves performance.
  adjustCurrentZoomFactor();

  // Scaling article views asynchronously dramatically improves performance when
  // a zoom action is triggered repeatedly while many or large articles are open
  // in the main window or in popup.
  // Multiple zoom action signals are processed before (often slow) article view
  // scaling is requested. Multiple scaling requests then ask for the same zoom factor,
  // so all of them except for the first one don't change anything and run very fast.
  // In effect, some intermediate zoom factors are skipped when scaling is slow.
  // The slower the scaling, the more steps are skipped.
  QTimer::singleShot( 0, this, &MainWindow::scaleArticlesByCurrentZoomFactor );
}

void MainWindow::adjustCurrentZoomFactor()
{
  if ( cfg.preferences.zoomFactor >= 5 ) {
    cfg.preferences.zoomFactor = 5;
  }
  else if ( cfg.preferences.zoomFactor <= 0.25 ) {
    cfg.preferences.zoomFactor = 0.25;
  }

  zoomIn->setEnabled( cfg.preferences.zoomFactor < 5 );
  zoomOut->setEnabled( cfg.preferences.zoomFactor > 0.25 );
  zoomBase->setEnabled( !qFuzzyCompare( cfg.preferences.zoomFactor, 1.0 ) );
}

void MainWindow::scaleArticlesByCurrentZoomFactor()
{
  for ( int i = 0; i < ui.tabWidget->count(); i++ ) {
    auto & view = dynamic_cast< ArticleView & >( *( ui.tabWidget->widget( i ) ) );
    view.setZoomFactor( cfg.preferences.zoomFactor );
  }

  scanPopup->applyZoomFactor();
}

void MainWindow::doWordsZoomIn()
{
  cfg.preferences.wordsZoomLevel = cfg.preferences.wordsZoomLevel + 2;

  applyWordsZoomLevel();
}

void MainWindow::doWordsZoomOut()
{
  cfg.preferences.wordsZoomLevel = cfg.preferences.wordsZoomLevel - 2;

  applyWordsZoomLevel();
}

void MainWindow::doWordsZoomBase()
{
  cfg.preferences.wordsZoomLevel = 0;

  applyWordsZoomLevel();
}

void MainWindow::applyWordsZoomLevel()
{
  QFont font = translateBox->translateLine()->font();

  int ps = getIconSize();

  ps += cfg.preferences.wordsZoomLevel;
  if ( ps < 12 ) {
    ps = 12;
  }

  font.setPixelSize( ps * 0.8 );
  font.setWeight( QFont::Normal );
  translateBox->translateLine()->setFont( font );
  //  translateBox->completerWidget()->setFont( font );
  wordsZoomBase->setEnabled( cfg.preferences.wordsZoomLevel != 0 );

  if ( !cfg.preferences.searchInDock ) {
    // Invalidating navToolbar's layout displays translateBoxWidget w/o the need to press the toolbar
    // extension button when Words Zoom level decreases enough for translateBoxWidget to fit in the toolbar.
    navToolbar->layout()->invalidate();
  }

  scanPopup->applyWordsZoomLevel();
}

void MainWindow::messageFromAnotherInstanceReceived( QString const & message )
{
  if ( message == "bringToFront" ) {
    toggleMainWindow( true );
    return;
  }

  if ( message == "toggleScanPopup" ) {
    enableScanningAction->trigger();
    return;
  }

  QString prefix = "window:";
  if ( message.left( prefix.size() ) == prefix ) {
    consoleWindowOnce = message.mid( prefix.size() );
  }

  if ( message.left( 15 ) == "translateWord: " ) {
    auto word = message.mid( 15 );
    if ( ( consoleWindowOnce == "popup" ) && scanPopup ) {
      scanPopup->translateWord( word );
    }
    else if ( consoleWindowOnce == "main" ) {
      wordReceived( word );
    }
    else {
      //default logic
      if ( scanPopup ) {
        scanPopup->translateWord( word );
      }
      else {
        wordReceived( word );
      }
    }

    consoleWindowOnce.clear();
  }
  else if ( message.left( 10 ) == "setGroup: " ) {
    setGroupByName( message.mid( 10 ), true );
  }
  else if ( message.left( 15 ) == "setPopupGroup: " ) {
    setGroupByName( message.mid( 15 ), false );
  }
  else {
    qWarning() << "Unknown message received from another instance: " << message;
  }
}

ArticleView * MainWindow::getCurrentArticleView()
{
  if ( QWidget * cw = ui.tabWidget->currentWidget() ) {
    return dynamic_cast< ArticleView * >( cw );
  }
  return nullptr;
}

void MainWindow::wordReceived( const QString & word )
{
  toggleMainWindow( true );
  setInputLineText( word, WildcardPolicy::EscapeWildcards, NoPopupChange );
  respondToTranslationRequest( word, false );
}

void MainWindow::updateFavoritesMenu()
{
  if ( ui.favoritesPane->isVisible() ) {
    ui.showHideFavorites->setText( tr( "&Hide" ) );
  }
  else {
    ui.showHideFavorites->setText( tr( "&Show" ) );
  }
}

void MainWindow::updateHistoryMenu()
{
  if ( ui.historyPane->isVisible() ) {
    ui.showHideHistory->setText( tr( "&Hide" ) );
  }
  else {
    ui.showHideHistory->setText( tr( "&Show" ) );
  }
}

void MainWindow::toggle_favoritesPane()
{
  if ( ui.favoritesPane->isVisible() ) {
    ui.favoritesPane->hide();
  }
  else {
    ui.favoritesPane->show();
    ui.favoritesPane->raise(); // useful when the Pane is tabbed.
  }
}

void MainWindow::toggle_historyPane()
{
  if ( ui.historyPane->isVisible() ) {
    ui.historyPane->hide();
  }
  else {
    ui.historyPane->show();
    ui.historyPane->raise();
  }
}

void MainWindow::on_exportHistory_triggered()
{
  QString exportPath;
  if ( cfg.historyExportPath.isEmpty() ) {
    exportPath = QDir::homePath();
  }
  else {
    exportPath = QDir::fromNativeSeparators( cfg.historyExportPath );
    if ( !QDir( exportPath ).exists() ) {
      exportPath = QDir::homePath();
    }
  }

  QString fileName = QFileDialog::getSaveFileName( this,
                                                   tr( "Export history to file" ),
                                                   exportPath,
                                                   tr( "Text files (*.txt);;All files (*.*)" ) );
  if ( fileName.size() == 0 ) {
    return;
  }

  cfg.historyExportPath = QDir::toNativeSeparators( QFileInfo( fileName ).absoluteDir().absolutePath() );
  QFile file( fileName );


  if ( !file.open( QFile::WriteOnly | QIODevice::Text ) ) {
    errorMessageOnStatusBar( QString( tr( "Export error: " ) ) + file.errorString() );
    return;
  }

  auto guard = qScopeGuard( [ &file ]() {
    file.close();
  } );
  Q_UNUSED( guard )

  // Write UTF-8 BOM
  QByteArray line;
  line.append( 0xEF ).append( 0xBB ).append( 0xBF );
  if ( file.write( line ) != line.size() ) {
    errorMessageOnStatusBar( QString( tr( "Export error: " ) ) + file.errorString() );
    return;
  }

  // Write history
  QList< History::Item > const & items = history.getItems();

  QList< History::Item >::const_iterator i;
  for ( i = items.constBegin(); i != items.constEnd(); ++i ) {
    line = i->word.toUtf8();

    line.replace( '\n', ' ' );
    line.replace( '\r', ' ' );

    line += "\n";

    if ( file.write( line ) != line.size() ) {
      errorMessageOnStatusBar( QString( tr( "Export error: " ) ) + file.errorString() );
      return;
    }
  }

  mainStatusBar->showMessage( tr( "History export complete" ), 5000 );
}

void MainWindow::on_importHistory_triggered()
{
  QString importPath;
  if ( cfg.historyExportPath.isEmpty() ) {
    importPath = QDir::homePath();
  }
  else {
    importPath = QDir::fromNativeSeparators( cfg.historyExportPath );
    if ( !QDir( importPath ).exists() ) {
      importPath = QDir::homePath();
    }
  }

  QString fileName = QFileDialog::getOpenFileName( this,
                                                   tr( "Import history from file" ),
                                                   importPath,
                                                   tr( "Text files (*.txt);;All files (*.*)" ) );
  if ( fileName.size() == 0 ) {
    return;
  }

  QFileInfo fileInfo( fileName );
  cfg.historyExportPath = QDir::toNativeSeparators( fileInfo.absoluteDir().absolutePath() );
  QString errStr;
  QFile file( fileName );

  if ( !file.open( QFile::ReadOnly | QIODevice::Text ) ) {
    errStr = QString( tr( "Import error: " ) ) + file.errorString();
    errorMessageOnStatusBar( errStr );
    return;
  }

  QTextStream fileStream( &file );
  QString itemStr, trimmedStr;
  QList< QString > itemList;

  do {
    itemStr = fileStream.readLine();
    if ( fileStream.status() >= QTextStream::ReadCorruptData ) {
      break;
    }

    trimmedStr = itemStr.trimmed();
    if ( trimmedStr.isEmpty() ) {
      continue;
    }

    if ( (unsigned)trimmedStr.size() <= history.getMaxItemLength() ) {
      itemList.prepend( trimmedStr );
    }

  } while ( !fileStream.atEnd() && itemList.size() < (int)history.getMaxSize() );

  history.enableAdd( true );

  for ( QList< QString >::const_iterator i = itemList.constBegin(); i != itemList.constEnd(); ++i ) {
    history.addItem( { *i } );
  }

  history.enableAdd( cfg.preferences.storeHistory );

  if ( file.error() != QFile::NoError ) {
    errStr = QString( tr( "Import error: " ) ) + file.errorString();
    errorMessageOnStatusBar( errStr );
    return;
  }

  if ( fileStream.status() >= QTextStream::ReadCorruptData ) {
    errStr = QString( tr( "Import error: invalid data in file" ) );
    errorMessageOnStatusBar( errStr );
    return;
  }
  //even without this line, the destructor of QFile will close the file as documented.
  file.close();
  mainStatusBar->showMessage( tr( "History import complete" ), 5000 );
}

void MainWindow::errorMessageOnStatusBar( const QString & errStr )
{
  this->mainStatusBar->showMessage( errStr, 10000, QPixmap( ":/icons/error.svg" ) );
}

void MainWindow::on_exportFavorites_triggered()
{
  QString exportPath;
  if ( cfg.historyExportPath.isEmpty() ) {
    exportPath = QDir::homePath();
  }
  else {
    exportPath = QDir::fromNativeSeparators( cfg.historyExportPath );
    if ( !QDir( exportPath ).exists() ) {
      exportPath = QDir::homePath();
    }
  }

  QString fileName = QFileDialog::getSaveFileName( this,
                                                   tr( "Export Favorites to file" ),
                                                   exportPath,
                                                   tr( "Text files (*.txt);;XML files (*.xml)" ) );
  if ( fileName.size() == 0 ) {
    return;
  }
  cfg.historyExportPath = QDir::toNativeSeparators( QFileInfo( fileName ).absoluteDir().absolutePath() );
  QFile file( fileName );
  if ( !file.open( QFile::WriteOnly | QIODevice::Text ) ) {
    errorMessageOnStatusBar( QString( tr( "Export error: " ) ) + file.errorString() );
    return;
  }
  if ( fileName.endsWith( ".xml", Qt::CaseInsensitive ) ) {
    QByteArray data;
    ui.favoritesPaneWidget->getDataInXml( data );

    if ( file.write( data ) != data.size() ) {
      errorMessageOnStatusBar( QString( tr( "Export error: " ) ) + file.errorString() );
      return;
    }
  }
  else {
    // Write UTF-8 BOM
    QByteArray line;
    line.append( 0xEF ).append( 0xBB ).append( 0xBF );
    if ( file.write( line ) != line.size() ) {
      errorMessageOnStatusBar( QString( tr( "Export error: " ) ) + file.errorString() );
      return;
    }

    // Write Favorites
    QString data;
    ui.favoritesPaneWidget->getDataInPlainText( data );

    line = data.toUtf8();
    if ( file.write( line ) != line.size() ) {
      errorMessageOnStatusBar( QString( tr( "Export error: " ) ) + file.errorString() );
      return;
    }
  }
  file.close();

  mainStatusBar->showMessage( tr( "Favorites export complete" ), 5000 );
}

void MainWindow::on_importFavorites_triggered()
{
  QString importPath;
  if ( cfg.historyExportPath.isEmpty() ) {
    importPath = QDir::homePath();
  }
  else {
    importPath = QDir::fromNativeSeparators( cfg.historyExportPath );
    if ( !QDir( importPath ).exists() ) {
      importPath = QDir::homePath();
    }
  }

  QString fileName = QFileDialog::getOpenFileName( this,
                                                   tr( "Import Favorites from file" ),
                                                   importPath,
                                                   tr( "Text and XML files (*.txt *.xml);;All files (*.*)" ) );
  if ( fileName.size() == 0 ) {
    return;
  }

  QFileInfo fileInfo( fileName );
  cfg.historyExportPath = QDir::toNativeSeparators( fileInfo.absoluteDir().absolutePath() );
  QString errStr;
  QFile file( fileName );

  if ( !file.open( QFile::ReadOnly | QIODevice::Text ) ) {
    errorMessageOnStatusBar( QString( tr( "Import error: " ) ) + file.errorString() );
    return;
  }

  QByteArray data = file.readAll();

  if ( fileInfo.suffix().compare( "txt", Qt::CaseInsensitive ) == 0 ) {
    if ( !ui.favoritesPaneWidget->setDataFromTxt( QString::fromUtf8( data.data(), data.size() ) ) ) {
      errorMessageOnStatusBar( QString( tr( "Data parsing error" ) ) );
      return;
    }
  }
  else {
    if ( !ui.favoritesPaneWidget->setDataFromXml( QString::fromUtf8( data.data(), data.size() ) ) ) {
      errorMessageOnStatusBar( QString( tr( "Data parsing error" ) ) );
      return;
    }
  }

  file.close();
  mainStatusBar->showMessage( tr( "Favorites import complete" ), 5000 );
}

void MainWindow::focusWordList()
{
  if ( ui.wordList->count() > 0 ) {
    ui.wordList->setFocus();
  }
}

void MainWindow::addWordToHistory( const QString & word )
{
  //    skip epwing reference link. epwing reference link has the pattern of r%dAt%d
  if ( QRegularExpressionMatch m = RX::Epwing::refWord.match( word ); m.hasMatch() ) {
    return;
  }
  history.addItem( { word.trimmed() } );
}

void MainWindow::forceAddWordToHistory( const QString & word )
{
  history.enableAdd( true );
  history.addItem( { word.trimmed() } );
  history.enableAdd( cfg.preferences.storeHistory );
}

void MainWindow::foundDictsPaneClicked( QListWidgetItem * item )
{
  Qt::KeyboardModifiers m = QApplication::keyboardModifiers();
  if ( ( m & ( Qt::ControlModifier | Qt::ShiftModifier ) ) || ( m == Qt::AltModifier ) ) {
    QString id = item->data( Qt::UserRole ).toString();
    emit clickOnDictPane( id );
  }

  jumpToDictionary( item, true );
}

void MainWindow::showDictionaryInfo( const QString & id )
{
  for ( unsigned x = 0; x < dictionaries.size(); x++ ) {
    if ( dictionaries[ x ]->getId() == id.toUtf8().data() ) {
      DictInfo infoMsg( cfg, this );
      infoMsg.showInfo( dictionaries[ x ] );
      int result = infoMsg.exec();

      if ( result == DictInfo::OPEN_FOLDER ) {
        openDictionaryFolder( id );
      }
      else if ( result == DictInfo::SHOW_HEADWORDS ) {
        showDictionaryHeadwords( dictionaries[ x ].get() );
      }

      break;
    }
  }
}

void MainWindow::showDictionaryHeadwords( Dictionary::Class * dict )
{

  QWidget * owner = qobject_cast< QWidget * >( sender() );

  // DictHeadwords internally check parent== mainwindow to know why it is requested.
  // If the DictHeadwords is requested by Edit->Dictionaries->ShowHeadWords, (owner = "EditDictionaries")
  // it will be a modal dialog. When click a word, that word will NOT be queried.
  // In all other cases, just set owner = mainwindow(this),

  if ( owner->objectName() == "EditDictionaries" ) {
    DictHeadwords headwords( owner, cfg, dict );
    headwords.exec();
  }
  else {
    if ( headwordsDlg == nullptr ) {
      headwordsDlg = new DictHeadwords( this, cfg, dict );
      addGlobalActionsToDialog( headwordsDlg );
      addGroupComboBoxActionsToDialog( headwordsDlg, groupList );
      connect( headwordsDlg,
               &DictHeadwords::headwordSelected,
               this,
               [ this ]( QString const & headword, QString const & dictID ) {
                 setInputLineText( headword, WildcardPolicy::EscapeWildcards, NoPopupChange );
                 respondToTranslationRequest( headword, false, ArticleView::scrollToFromDictionaryId( dictID ), false );
               } );
      connect( headwordsDlg,
               &DictHeadwords::closeDialog,
               this,
               &MainWindow::closeHeadwordsDialog,
               Qt::QueuedConnection );
    }
    else {
      headwordsDlg->setup( dict );
    }
    headwordsDlg->show();
  }
}


void MainWindow::closeHeadwordsDialog()
{
  if ( headwordsDlg ) {
    delete headwordsDlg;
    headwordsDlg = nullptr;
  }
}

void MainWindow::focusHeadwordsDialog()
{
  if ( headwordsDlg ) {
    headwordsDlg->activateWindow();
    if ( ftsDlg ) {
      ftsDlg->lower();
    }
  }
}

void MainWindow::focusArticleView()
{
  if ( ArticleView * view = getCurrentArticleView() ) {
    if ( !isActiveWindow() ) {
      activateWindow();
    }
    view->focus();
  }
}

void MainWindow::stopAudio()
{
  if ( ArticleView * view = getCurrentArticleView() ) {
    view->stopSound();
  }
}

void MainWindow::openDictionaryFolder( const QString & id )
{
  for ( unsigned x = 0; x < dictionaries.size(); x++ ) {
    if ( dictionaries[ x ]->getId() == id.toUtf8().data() ) {
      if ( !dictionaries[ x ]->getDictionaryFilenames().empty() ) {
        QDesktopServices::openUrl( QUrl::fromLocalFile( dictionaries[ x ]->getContainingFolder() ) );
      }
      break;
    }
  }
}

void MainWindow::foundDictsContextMenuRequested( const QPoint & pos )
{
  QListWidgetItem * item = ui.dictsList->itemAt( pos );
  if ( item ) {
    QString id                = item->data( Qt::UserRole ).toString();
    Dictionary::Class * pDict = nullptr;

    for ( unsigned i = 0; i < dictionaries.size(); i++ ) {
      if ( id.compare( dictionaries[ i ]->getId().c_str() ) == 0 ) {
        pDict = dictionaries[ i ].get();
        break;
      }
    }

    if ( pDict == nullptr ) {
      return;
    }

    if ( !pDict->isLocalDictionary() ) {
      if ( scanPopup ) {
        scanPopup->blockSignals( true );
      }
      showDictionaryInfo( id );
      if ( scanPopup ) {
        scanPopup->blockSignals( false );
      }
    }
    else {
      QMenu menu( ui.dictsList );
      QAction * infoAction = menu.addAction( tr( "Dictionary info" ) );

      QAction * headwordsAction = nullptr;
      if ( pDict->getWordCount() > 0 ) {
        headwordsAction = menu.addAction( tr( "Dictionary headwords" ) );
      }

      QAction * openDictFolderAction = menu.addAction( tr( "Open dictionary folder" ) );

      QAction * result = menu.exec( ui.dictsList->mapToGlobal( pos ) );

      if ( result && result == infoAction ) {
        if ( scanPopup ) {
          scanPopup->blockSignals( true );
        }
        showDictionaryInfo( id );
        if ( scanPopup ) {
          scanPopup->blockSignals( false );
        }
      }
      else if ( result && result == headwordsAction ) {
        if ( scanPopup ) {
          scanPopup->blockSignals( true );
        }
        showDictionaryHeadwords( pDict );
        if ( scanPopup ) {
          scanPopup->blockSignals( false );
        }
      }
      else if ( result && result == openDictFolderAction ) {
        openDictionaryFolder( id );
      }
    }
  }
}

void MainWindow::sendWordToInputLine( const QString & word )
{
  setInputLineText( word, WildcardPolicy::EscapeWildcards, NoPopupChange );
}

void MainWindow::storeResourceSavePath( const QString & newPath )
{
  cfg.resourceSavePath = newPath;
}

void MainWindow::proxyAuthentication( const QNetworkProxy &, QAuthenticator * authenticator )
{
  QNetworkProxy proxy = QNetworkProxy::applicationProxy();

  QString *userStr, *passwordStr;
  if ( cfg.preferences.proxyServer.useSystemProxy ) {
    userStr     = &cfg.preferences.proxyServer.systemProxyUser;
    passwordStr = &cfg.preferences.proxyServer.systemProxyPassword;
  }
  else {
    userStr     = &cfg.preferences.proxyServer.user;
    passwordStr = &cfg.preferences.proxyServer.password;
  }

  if ( proxy.user().isEmpty() && !userStr->isEmpty() ) {
    authenticator->setUser( *userStr );
    authenticator->setPassword( *passwordStr );

    proxy.setUser( *userStr );
    proxy.setPassword( *passwordStr );
    QNetworkProxy::setApplicationProxy( proxy );
  }
  else {
    QDialog dlg;
    Ui::Dialog ui;
    ui.setupUi( &dlg );
    dlg.adjustSize();

    ui.userEdit->setText( *userStr );
    ui.passwordEdit->setText( *passwordStr );

    if ( dlg.exec() == QDialog::Accepted ) {
      *userStr     = ui.userEdit->text();
      *passwordStr = ui.passwordEdit->text();

      authenticator->setUser( *userStr );
      authenticator->setPassword( *passwordStr );

      proxy.setUser( *userStr );
      proxy.setPassword( *passwordStr );
      QNetworkProxy::setApplicationProxy( proxy );
    }
  }
}

void MainWindow::showFullTextSearchDialog()
{
  if ( !ftsDlg ) {
    ftsDlg = new FTS::FullTextSearchDialog( this, cfg, dictionaries, groupInstances, ftsIndexing );
    // ftsDlg->setSearchText( translateLine->text() );

    addGlobalActionsToDialog( ftsDlg );
    addGroupComboBoxActionsToDialog( ftsDlg, groupList );

    connect( ftsDlg, &FTS::FullTextSearchDialog::showTranslationFor, this, &MainWindow::showTranslationForDicts );
    connect( ftsDlg,
             &FTS::FullTextSearchDialog::closeDialog,
             this,
             &MainWindow::closeFullTextSearchDialog,
             Qt::QueuedConnection );
    connect( &configEvents, SIGNAL( mutedDictionariesChanged() ), ftsDlg, SLOT( updateDictionaries() ) );

    unsigned group = groupInstances.empty() ? 0 : groupInstances[ groupList->currentIndex() ].id;
    ftsDlg->setCurrentGroup( group );
  }

  if ( !ftsDlg->isVisible() ) {
    ftsDlg->show();
  }
  else {
    ftsDlg->activateWindow();
    if ( headwordsDlg ) {
      headwordsDlg->lower();
    }
  }
}

void MainWindow::closeFullTextSearchDialog()
{
  if ( ftsDlg ) {
    ftsDlg->stopSearch();
    delete ftsDlg;
    ftsDlg = nullptr;
  }
}

void MainWindow::showFTSIndexingName( QString const & name )
{
  if ( name.isEmpty() ) {
    mainStatusBar->setBackgroundMessage( QString() );
  }
  else {
    mainStatusBar->setBackgroundMessage( tr( "Now indexing for full-text search: " ) + name );
  }
}

QString MainWindow::unescapeTabHeader( QString const & header )
{
  // Reset table header to original headword
  return Utils::unescapeAmps( header );
}

void MainWindow::addCurrentTabToFavorites()
{
  QString folder;
  Instances::Group const * igrp = groupInstances.findGroup( cfg.lastMainGroupId );
  if ( igrp ) {
    folder = igrp->favoritesFolder;
  }

  QString headword = ui.tabWidget->tabText( ui.tabWidget->currentIndex() );

  ui.favoritesPaneWidget->addHeadword( folder, unescapeTabHeader( headword ) );

  addToFavorites->setIcon( blueStarIcon );
  addToFavorites->setToolTip( tr( "Remove current tab from Favorites" ) );
}

void MainWindow::handleAddToFavoritesButton()
{
  QString folder;
  Instances::Group const * igrp = groupInstances.findGroup( cfg.lastMainGroupId );
  if ( igrp ) {
    folder = igrp->favoritesFolder;
  }
  QString headword = unescapeTabHeader( ui.tabWidget->tabText( ui.tabWidget->currentIndex() ) );

  if ( ui.favoritesPaneWidget->isHeadwordPresent( folder, headword ) ) {
    QMessageBox mb( QMessageBox::Question,
                    "GoldenDict",
                    tr( "Remove headword \"%1\" from Favorites?" ).arg( headword ),
                    QMessageBox::Yes | QMessageBox::No,
                    this );
    if ( mb.exec() == QMessageBox::Yes ) {
      if ( ui.favoritesPaneWidget->removeHeadword( folder, headword ) ) {
        addToFavorites->setIcon( starIcon );
        addToFavorites->setToolTip( tr( "Add current tab to Favorites" ) );
      }
    }
  }
  else {
    ui.favoritesPaneWidget->addHeadword( folder, headword );
    addToFavorites->setIcon( blueStarIcon );
    addToFavorites->setToolTip( tr( "Remove current tab from Favorites" ) );
  }
}

void MainWindow::addWordToFavorites( QString const & word, unsigned groupId, bool exist )
{
  QString folder;
  Instances::Group const * igrp = groupInstances.findGroup( groupId );
  if ( igrp ) {
    folder = igrp->favoritesFolder;
  }

  if ( !exist ) {
    ui.favoritesPaneWidget->addHeadword( folder, word );
  }
  else {
    ui.favoritesPaneWidget->removeHeadword( folder, word );
  }
}

void MainWindow::addBookmarkToFavorite( QString const & text )
{
  // get current tab word.
  QString word        = unescapeTabHeader( ui.tabWidget->tabText( ui.tabWidget->currentIndex() ) );
  const auto bookmark = QString( "%1~~~%2" ).arg( word, text );

  ui.favoritesPaneWidget->addHeadword( nullptr, bookmark );
}

void MainWindow::addAllTabsToFavorites()
{
  QString folder;
  Instances::Group const * igrp = groupInstances.findGroup( cfg.lastMainGroupId );
  if ( igrp ) {
    folder = igrp->favoritesFolder;
  }

  for ( int i = 0; i < ui.tabWidget->count(); i++ ) {
    QString headword = ui.tabWidget->tabText( i );
    ui.favoritesPaneWidget->addHeadword( folder, unescapeTabHeader( headword ) );
  }
  addToFavorites->setIcon( blueStarIcon );
  addToFavorites->setToolTip( tr( "Remove current tab from Favorites" ) );
}

bool MainWindow::isWordPresentedInFavorites( QString const & word, unsigned groupId )
{
  QString folder;
  Instances::Group const * igrp = groupInstances.findGroup( groupId );
  if ( igrp ) {
    folder = igrp->favoritesFolder;
  }

  return ui.favoritesPaneWidget->isHeadwordPresent( folder, word );
}

void MainWindow::setGroupByName( QString const & name, bool main_window )
{
  if ( main_window ) {
    int i;
    for ( i = 0; i < groupList->count(); i++ ) {
      if ( groupList->itemText( i ) == name ) {
        groupList->setCurrentIndex( i );
        break;
      }
    }
    if ( i >= groupList->count() ) {
      qWarning( "Group \"%s\" for main window is not found", name.toUtf8().data() );
    }
  }
  else {
    emit setPopupGroupByName( name );
  }
}

void MainWindow::headwordFromFavorites( QString const & headword, QString const & favoritesFolder )
{
  if ( !favoritesFolder.isEmpty() ) {
    // Find group by it Favorites folder
    for ( Instances::Groups::size_type i = 0; i < groupInstances.size(); i++ ) {
      if ( groupInstances[ i ].favoritesFolder == favoritesFolder ) {
        // Group found. Select it and stop search.
        if ( groupList->currentIndex() != (int)i ) {
          groupList->setCurrentIndex( i );

          // Restore focus on Favorites tree
          ui.favoritesPaneWidget->setFocusOnTree();
        }
        break;
      }
    }
  }

  // Show headword without lost of focus on Favorites tree
  // bookmark cases:   the favorite item may like this   "word~~~selectedtext"
  auto words = headword.split( "~~~" );

  setInputLineText( words[ 0 ], WildcardPolicy::EscapeWildcards, DisablePopup );

  //must be a bookmark.
  if ( words.size() > 1 ) {
    auto view = getCurrentArticleView();
    if ( view ) {
      view->setDelayedHighlightText( words[ 1 ] ); // findText( words[ 1 ], QWebEnginePage::FindCaseSensitively );
    }
  }

  showTranslationFor( words[ 0 ] );
}
