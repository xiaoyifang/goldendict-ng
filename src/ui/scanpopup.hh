/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "article_netmgr.hh"
#include "ui/articleview.hh"
#include "wordfinder.hh"
#include "keyboardstate.hh"
#include "config.hh"
#include "ui_scanpopup_toolbar.h"
#include <QClipboard>
#include "history.hh"
#include "dictionarybar.hh"
#include "mainstatusbar.hh"
#include <QMainWindow>
#include <QActionGroup>
#include "groupcombobox.hh"
#include "translatebox.hh"
#ifdef WITH_X11
  #include "scanflag.hh"
#endif


/// This is a popup dialog to show translations when clipboard scanning mode
/// is enabled.
class ScanPopup: public QMainWindow, KeyboardState
{
  Q_OBJECT

public:

  ScanPopup( QWidget * parent,
             Config::Class & cfg,
             ArticleNetworkAccessManager &,
             const AudioPlayerPtr &,
             const std::vector< sptr< Dictionary::Class > > & allDictionaries,
             const Instances::Groups &,
             History & );

  ~ScanPopup();

  // update dictionary bar, group data and possibly other data
  void refresh();

  /// Applies current zoom factor to the popup's view. Should be called when
  /// it's changed.
  void applyZoomFactor() const;
  /// Translate the word
  void translateWord( const QString & word );

  void setDictionaryIconSize();

  void saveConfigData() const;

#ifdef WITH_X11
  /// Interaction with scan flag window
  void showScanFlag();
  void hideScanFlag();

  QTimer selectionDelayTimer;
#endif

signals:

  /// Forwarded from the dictionary bar, so that main window could act on this.
  void editGroupRequest( unsigned id );
  /// Send word to main window
  void sendPhraseToMainWindow( const QString & word );
  /// Close opened menus when window hide
  void closeMenu();

  void inspectSignal( QWebEnginePage * page );
  /// Signal to switch expand optional parts mode
  void switchExpandMode();
  /// Signal to add word to history even if history is disabled
  void forceAddWordToHistory( const QString & word );
  /// Retranslate signal from dictionary bar
  void showDictionaryInfo( const QString & id );
  void openDictionaryFolder( const QString & id );
  /// Put translated word into history
  void sendWordToHistory( const QString & word );
  /// Put translated word into Favorites
  void sendWordToFavorites( const QString & word );

#ifdef Q_OS_WIN32
  /// Ask for source window is current translate tab
  bool isGoldenDictWindow( HWND hwnd );
#endif

public slots:

  void inspectElementWhenPinned( QWebEnginePage * page );
  /// Translates the word from the clipboard, showing the window etc.
  void translateWordFromPrimaryClipboard();
  /// Translates the word from the clipboard selection
  void translateWordFromSelection();
  /// From the dictionary bar.
  void editGroupRequested();

  void setGroupByName( const QString & name ) const;

#ifdef WITH_X11
  void showEngagePopup();
#endif
  void openSearch();


private:

  Qt::WindowFlags unpinnedWindowFlags() const;

  // Translates the word from the clipboard or the clipboard selection
  void translateWordFromClipboard( QClipboard::Mode m );

  // Hides the popup window, effectively closing it.
  void hideWindow();

  // Grabs mouse and installs global event filter to track it thoroughly.
  void interceptMouse();
  // Ungrabs mouse and uninstalls global event filter.
  void uninterceptMouse();

  /// Check is word already presented in Favorites
  bool isWordPresentedInFavorites( const QString & word ) const;

  Config::Class & cfg;
  const std::vector< sptr< Dictionary::Class > > & allDictionaries;
  std::vector< sptr< Dictionary::Class > > dictionariesUnmuted;
  const Instances::Groups & groups;
  History & history;
  Ui::ScanPopupToolBar ui;
  TranslateBox * translateBox;
  GroupComboBox * groupList;
  QAction * groupListAction; // for hiding it in QToolbar
  ArticleView * definition;
  QAction escapeAction, switchExpandModeAction, focusTranslateLineAction;
  QAction stopAudioAction;
  QAction openSearchAction;
  QString pendingWord; // Word that is going to be translated
  WordFinder wordFinder;
  Config::Events configEvents;
  DictionaryBar dictionaryBar;
  QToolBar * foundBar;
  QActionGroup * actionGroup = nullptr;
  MainStatusBar * mainStatusBar;
  /// Fonts saved before words zooming is in effect, so it could be reset back.
  QFont wordListDefaultFont, translateLineDefaultFont, groupListDefaultFont;

#ifdef WITH_X11
  ScanFlag * scanFlag;
#endif

  bool mouseEnteredOnce = false;
  bool mouseIntercepted = false;

  QPointF startPos; // For window moving
  QByteArray pinnedGeometry;

  QTimer hideTimer; // When mouse leaves the window, a grace period is
                    // given for it to return back. If it doesn't before
                    // this timer expires, the window gets hidden.

  QTimer mouseGrabPollTimer;

  QIcon starIcon     = QIcon( ":/icons/star.svg" );
  QIcon blueStarIcon = QIcon( ":/icons/star_blue.svg" );

  void handleInputWord( const QString &, bool forcePopup = false );
  void engagePopup( bool forcePopup, bool giveFocus = false );

  const vector< sptr< Dictionary::Class > > & getActiveDicts();

  virtual bool eventFilter( QObject * watched, QEvent * event );

  /// Called from event filter or from mouseGrabPoll to handle mouse event
  /// while it is being intercepted.
  void reactOnMouseMove( const QPointF & p );

  virtual void mousePressEvent( QMouseEvent * );
  virtual void mouseMoveEvent( QMouseEvent * );
  virtual void mouseReleaseEvent( QMouseEvent * );
  virtual void leaveEvent( QEvent * event );
  virtual void enterEvent( QEnterEvent * event );
  virtual void showEvent( QShowEvent * );
  virtual void closeEvent( QCloseEvent * );
  virtual void moveEvent( QMoveEvent * );

  /// Returns inputWord, chopped with appended ... if it's too long/
  QString elideInputWord();

  void updateBackForwardButtons() const;

  void showTranslationFor( const QString & inputPhrase ) const;

  void updateSuggestionList();
  void updateSuggestionList( const QString & text );
private slots:
  void currentGroupChanged( int );
  void prefixMatchFinished();
  void pinButtonClicked( bool checked );
  void dictionaryBar_visibility_changed( bool visible );
  /// Status bar message slot function, used to handle status bar message signals sent by the dictionary bar
  void showStatusBarMessage( const QString & message, int timeout = 3000, const QPixmap & icon = QPixmap() );

  void pronounceButton_clicked() const;
  void saveArticleButton_clicked();
  void sendWordButton_clicked();
  void sendWordToFavoritesButton_clicked();
  void goBackButton_clicked() const;
  void goForwardButton_clicked() const;

  void hideTimerExpired();

  /// Called repeatedly once the popup is initially engaged and we monitor the
  /// mouse as it may move away from the window. This simulates mouse grab, in
  /// essence, but seems more reliable. Once the mouse enters the window, the
  /// polling stops.
  void mouseGrabPoll();

  void pageLoaded( ArticleView * ) const;

  void escapePressed();

  void mutedDictionariesChanged();

  void switchExpandOptionalPartsMode();

  void translateInputChanged( const QString & text );
  void translateInputFinished();

  void focusTranslateLine();
  void stopAudio() const;

  void typingEvent( const QString & );

  void alwaysOnTopClicked( bool checked );

  void titleChanged( ArticleView *, const QString & title ) const;
  void updateFoundInDictsList();
  void onActionTriggered();
};