/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include <QSystemTrayIcon>
#include <functional>
#include "ui_mainwindow.h"
#include "config.hh"
#include "dict/dictionary.hh"
#include "article_netmgr.hh"
#include "audio/audioplayerfactory.hh"
#include "instances.hh"
#include "article_maker.hh"
#include "ui/articleview.hh"
#include "wordfinder.hh"
#include "dictionarybar.hh"
#include "history.hh"
#include "mainstatusbar.hh"
#include "mruqmenu.hh"
#include "translatebox.hh"
#include "dictheadwords.hh"
#include "fulltextsearch.hh"
#include "base_type.hh"
#include "hotkey/hotkeywrapper.hh"
#include "resourceschemehandler.hh"
#include "iframeschemehandler.hh"
#ifdef WITH_X11
  #include <fixx11h.h>
#endif
#include "scanpopup.hh"
#include "clipboard/clipboardlistener.hh"
//must place the qactiongroup after fixx11h.h, None in QActionGroup conflict with X.h's macro None.
#include <QActionGroup>

using std::string;
using std::vector;

class MainWindow: public QMainWindow
{
  Q_OBJECT

public:

  MainWindow( Config::Class & cfg );
  ~MainWindow();


  /// Set group for main/popup window
  void setGroupByName( const QString & name, bool main_window );

  enum class WildcardPolicy {
    EscapeWildcards,
    WildcardsAreAlreadyEscaped
  };
public slots:

  void messageFromAnotherInstanceReceived( const QString & );
  void showStatusBarMessage( const QString &, int, const QPixmap & );
  void wordReceived( const QString & );
  void headwordFromFavorites( const QString & word, const QString & favFolderFullPath );
  /// Save config and states...
  void commitData();
  void quitApp();

private:
  void addGlobalAction( QAction * action, const std::function< void() > & slotFunc );
  void addGlobalActionsToDialog( QDialog * dialog );
  void addGroupComboBoxActionsToDialog( QDialog * dialog, GroupComboBox * pGroupComboBox );
  void removeGroupComboBoxActionsFromDialog( QDialog * dialog, GroupComboBox * pGroupComboBox );


  QSystemTrayIcon * trayIcon;

  QScopedPointer< ArticleInspector > inspector;

  Ui::MainWindow ui;

  /// This widget is used as a title bar for the searchPane dock, and
  /// incorporates the next three objects inside
  QWidget searchPaneTitleBar;
  QHBoxLayout searchPaneTitleBarLayout;
  QLabel groupLabel;
  GroupComboBox *groupList, *groupListInToolbar, *groupListInDock;

  // Needed to be able to show/hide the translate box in the toolbar, since hiding
  // the list expilictily doesn't work, see docs for QToolBar::addWidget().
  QAction * translateBoxToolBarAction;

  QWidget dictsPaneTitleBar;
  QHBoxLayout dictsPaneTitleBarLayout;
  QLabel foundInDictsLabel;

  TranslateBox * translateBox;

  /// Fonts saved before words zooming is in effect, so it could be reset back.
  QFont wordListDefaultFont, translateLineDefaultFont, groupListDefaultFont;

  QAction escAction, focusTranslateLineAction, addTabAction, closeCurrentTabAction, closeAllTabAction,
    closeRestTabAction, switchToNextTabAction, switchToPrevTabAction, showDictBarNamesAction, toggleMenuBarAction,
    focusHeadwordsDlgAction, focusArticleViewAction, addAllTabToFavoritesAction;

  QAction useSmallIconsInToolbarsAction, useLargeIconsInToolbarsAction, useNormalIconsInToolbarsAction;

  QActionGroup * smallLargeIconGroup = new QActionGroup( this );

  QAction stopAudioAction;
  QToolBar * navToolbar;
  MainStatusBar * mainStatusBar;
  QAction *navBack, *navForward, *navPronounce, *enableScanningAction;
  QAction * beforeOptionsSeparator;
  QAction *zoomIn, *zoomOut, *zoomBase;
  QAction *addToFavorites, *beforeAddToFavoritesSeparator;
  QMenu trayIconMenu;
  QMenu * tabMenu;
  QAction * menuButtonAction;
  QToolButton * menuButton;
  MRUQMenu * tabListMenu;
  //List that contains indexes of tabs arranged in a most-recently-used order
  QList< QWidget * > mruList;
  QToolButton addTab, *tabListButton;
  Config::Class & cfg;
  Config::Events configEvents;
  History history;
  DictionaryBar dictionaryBar;
  vector< sptr< Dictionary::Class > > dictionaries;
  QMap< std::string, sptr< Dictionary::Class > > dictMap;
  /// Here we store unmuted dictionaries when the dictionary bar is active
  vector< sptr< Dictionary::Class > > dictionariesUnmuted;
  Instances::Groups groupInstances;
  ArticleMaker articleMaker;
  ArticleNetworkAccessManager articleNetMgr;
  QNetworkAccessManager dictNetMgr; // We give dictionaries a separate manager,
                                    // since their requests can be destroyed
                                    // in a separate thread
  AudioPlayerFactory audioPlayerFactory;

  //current active translateLine;
  QLineEdit * translateLine;

  WordFinder wordFinder;

  ScanPopup * scanPopup = nullptr;

  //only used once, when used ,reset to empty.
  QString consoleWindowOnce;

  sptr< HotkeyWrapper > hotkeyWrapper;

  sptr< QPrinter > printer; // The printer we use for all printing operations

  bool wordListSelChanged;

  bool wasMaximized; // Window state before minimization

  QPrinter & getPrinter(); // Creates a printer if it's not there and returns it

  DictHeadwords * headwordsDlg;

  FTS::FtsIndexing ftsIndexing;

  FTS::FullTextSearchDialog * ftsDlg;

  QIcon starIcon, blueStarIcon;

  LocalSchemeHandler * localSchemeHandler;
  IframeSchemeHandler * iframeSchemeHandler;
  ResourceSchemeHandler * resourceSchemeHandler;

  BaseClipboardListener * clipboardListener;

#if !defined( Q_OS_WIN )
  // On Linux, this will be the style before getting overriden by custom styles
  // On macOS, this will be just Fusion.
  QString defaultInterfaceStyle;
#endif
  /// Applies Qt stylesheets, use Windows dark palette etc....
  void updateAppearances( const QString & addonStyle,
                          const QString & displayStyle,
                          Config::Dark darkMode
#if !defined( Q_OS_WIN )
                          ,
                          const QString & interfaceStyle
#endif
  );

  /// Creates, destroys or otherwise updates tray icon, according to the
  /// current configuration and situation.
  void trayIconUpdateOrInit();

  void wheelEvent( QWheelEvent * );
  void closeEvent( QCloseEvent * );

  void applyProxySettings();
  void setupNetworkCache( int maxSize );
  void makeDictionaries();
  void updateStatusLine();
  void updateGroupList( bool reload = true );

  void updatePronounceAvailability();

  void updateBackForwardButtons();

  void updateWindowTitle();

  /// Updates word search request and active article view in response to
  /// muting or unmuting dictionaries, or showing/hiding dictionary bar.
  void applyMutedDictionariesState();

  virtual bool eventFilter( QObject *, QEvent * );

  /// Returns the reference to dictionaries stored in the currently active
  /// group, or to all dictionaries if there are no groups.
  const vector< sptr< Dictionary::Class > > & getActiveDicts();

  /// @param ensureShow only ensure the window will be shown and no "toggling"
  void toggleMainWindow( bool ensureShow );

  /// Creates hotkeyWrapper and hooks the currently set keys for it
  void installHotKeys();

  void applyZoomFactor();
  void adjustCurrentZoomFactor();

  void mousePressEvent( QMouseEvent * event );


  /// Handles backward and forward mouse buttons and
  /// returns true if the event is handled.
  bool handleBackForwardMouseButtons( QMouseEvent * ev );

  ArticleView * getCurrentArticleView();
  void ctrlTabPressed();

  void respondToTranslationRequest( const QString & word,
                                    bool checkModifiers,
                                    const QString & scrollTo = QString(),
                                    bool focus               = true );

  void updateSuggestionList();
  void updateSuggestionList( const QString & text );

  enum TranslateBoxPopup {
    NoPopupChange,
    EnablePopup,
    DisablePopup
  };


  /// Change the text of translateLine (Input line in the dock) or TranslateBox (Input line in toolbar)
  void setInputLineText( QString text, WildcardPolicy wildcardPolicy, TranslateBoxPopup popupAction );

  void changeWebEngineViewFont() const;

  void errorMessageOnStatusBar( const QString & errStr );
  int getIconSize();
  DictionaryBar::IconSize getIconSizeLogical();


  bool updateFavIcon( const QString & word );

private slots:
  void updateFavIconSlot();

  /// Try check new release, popup a dialog, and update the check time & skippedRelease version
  void checkNewRelease();

  void hotKeyActivated( int );

  void updateFoundInDictsList();

  /// Receive click on "Found in:" pane
  void foundDictsPaneClicked( QListWidgetItem * item );

  /// Receive right click on "Found in:" pane
  void foundDictsContextMenuRequested( const QPoint & pos );

  void showDictionaryInfo( const QString & id );

  void showDictionaryHeadwords( Dictionary::Class * dict );

  void openDictionaryFolder( const QString & id );

  void showFTSIndexingName( const QString & name );
  void openWebsiteInNewTab( QString name, QString url, QString dictId );

  void handleAddToFavoritesButton();

  void addCurrentTabToFavorites();

  void addAllTabsToFavorites();

  // Executed in response to a user click on an 'add tab' tool button
  void addNewTab();
  // Executed in response to a user click on an 'close' button on a tab
  void tabCloseRequested( int );
  // Closes current tab.
  void closeCurrentTab();
  void closeAllTabs();
  void closeRestTabs();
  void switchToNextTab();
  void switchToPrevTab();

  // Handling of active tab list
  void createTabList();
  void fillWindowsMenu();
  void switchToWindow( QAction * act );

  /// Triggered by the actions in the nav toolbar
  void backClicked();
  void forwardClicked();

  /// ArticleView's title has changed
  void titleChanged( ArticleView *, const QString & );
  /// ArticleView's icon has changed
  void iconChanged( ArticleView *, const QIcon & );

  void pageLoaded( ArticleView * );
  void tabSwitched( int );
  void tabMenuRequested( QPoint pos );

  void dictionaryBarToggled( bool checked );

  void zoomin();
  void zoomout();
  void unzoom();

  void scaleArticlesByCurrentZoomFactor();

  /// If editDictionaryGroup is specified, the dialog positions on that group
  /// initially.
  void editDictionaries( unsigned editDictionaryGroup = GroupId::NoGroupId );
  /// Edits current group when triggered from the dictionary bar.
  void editCurrentGroup();
  void editPreferences();

  void currentGroupChanged( int );
  void translateInputChanged( const QString & );
  void translateInputFinished( bool checkModifiers );

  /// Closes any opened search in the article view, and focuses the translateLine/close main window to tray.
  void handleEsc();

  /// Gives the keyboard focus to the translateLine and selects all the text
  /// it has.
  void focusTranslateLine();

  void wordListItemActivated( QListWidgetItem * );
  void wordListSelectionChanged();

  void dictsListItemActivated( QListWidgetItem * );
  void dictsListSelectionChanged();

  void jumpToDictionary( QListWidgetItem *, bool force = false );

  void showDictsPane();
  void dictsPaneVisibilityChanged( bool );

  /// Creates a new tab, which is to be populated then with some content.
  ArticleView * createNewTab( bool switchToIt, const QString & name );

  /// Finds an existing ArticleView that has loaded a website with the given host
  ArticleView * findArticleViewByHost( const QString & host );
  ArticleView * findArticleViewByDictId( const QString & dictId );

  void openLinkInNewTab( const QUrl &, const QUrl &, const QString &, const Contexts & contexts );
  void showDefinitionInNewTab( const QString & word,
                               unsigned group,
                               const QString & fromArticle,
                               const Contexts & contexts );
  void typingEvent( const QString & );

  void activeArticleChanged( const ArticleView *, const QString & id );

  void mutedDictionariesChanged();

  void showTranslationFor( const QString &, unsigned inGroup = 0, const QString & scrollTo = QString() );

  void showTranslationForDicts( const QString &,
                                const QStringList & dictIDs,
                                const QRegularExpression & searchRegExp,
                                bool ignoreDiacritics );

  void showHistoryItem( const QString & );

  void trayIconActivated( QSystemTrayIcon::ActivationReason );

  void setAutostart( bool );

  void visitHomepage();
  void visitForum();
  void openConfigFolder();
  void showAbout();

  void showDictBarNamesTriggered();
  void iconSizeActionTriggered( QAction * action );
  void toggleMenuBarTriggered( bool announce = true );

  void on_clearHistory_triggered();

  void on_newTab_triggered();

  void on_actionCloseToTray_triggered();

  void on_pageSetup_triggered();
  void on_printPreview_triggered();
  void on_print_triggered();
  void printPreviewPaintRequested( QPrinter * );

  void on_saveArticle_triggered();

  void on_rescanFiles_triggered();

  void toggle_favoritesPane();
  void toggle_historyPane(); // Toggling visibility
  void on_exportHistory_triggered();
  void on_importHistory_triggered();
  void on_alwaysOnTop_triggered( bool checked );
  void focusWordList();

  void on_exportFavorites_triggered();
  void on_importFavorites_triggered();

  void updateSearchPaneAndBar( bool searchInDock );

  void updateFavoritesMenu();
  void updateHistoryMenu();

  /// Add word to history
  void addWordToHistory( const QString & word );
  /// Add word to history even if history is disabled in options
  void forceAddWordToHistory( const QString & word );


  void addBookmarkToFavorite( const QString & text );

  void sendWordToInputLine( const QString & word );

  void storeResourceSavePath( const QString & );

  void closeHeadwordsDialog();

  void focusHeadwordsDialog();

  void focusArticleView();
  void stopAudio();

  void proxyAuthentication( const QNetworkProxy & proxy, QAuthenticator * authenticator );

  void showFullTextSearchDialog();
  void closeFullTextSearchDialog();

  void clipboardChange( QClipboard::Mode m );

  void inspectElement( QWebEnginePage * );
  void prefixMatchUpdated();
  void prefixMatchFinished();
  void updateMatchResults( bool finished );
  void refreshTranslateLine();
signals:
  /// Retranslate Ctrl(Shift) + Click on dictionary pane to dictionary toolbar
  void clickOnDictPane( const QString & id );

  /// Set group for popup window
  void setPopupGroupByName( const QString & name );
};

class ArticleSaveProgressDialog: public QProgressDialog
{
  Q_OBJECT

public:
  explicit ArticleSaveProgressDialog( QWidget * parent = nullptr, Qt::WindowFlags f = Qt::Widget ):
    QProgressDialog( parent, f )
  {
    setAutoReset( false );
    setAutoClose( false );
  }

public slots:
  void perform()
  {
    int progress = value() + 1;
    if ( progress == maximum() ) {
      emit close();
      deleteLater();
    }
    setValue( progress );
  }
};
