/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "article_netmgr.hh"
#include "ui/articleview.hh"
#include "wordfinder.hh"
#include "keyboardstate.hh"
#include "config.hh"
#include "ui_scanpopup.h"
#include <QDialog>
#include <QClipboard>
#include <QToolBar>
#include "history.hh"
#include "dictionarybar.hh"
#include "mainstatusbar.hh"
#ifdef HAVE_X11
  #include "scanflag.hh"
#endif

#include <QActionGroup>

/// This is a popup dialog to show translations when clipboard scanning mode
/// is enabled.
class ScanPopup: public QMainWindow, KeyboardState
{
  Q_OBJECT

public:

  ScanPopup( QWidget * parent,
             Config::Class & cfg,
             ArticleNetworkAccessManager &,
             AudioPlayerPtr const &,
             std::vector< sptr< Dictionary::Class > > const & allDictionaries,
             Instances::Groups const &,
             History & );

  ~ScanPopup();

  // update dictionary bar, group data and possibly other data
  void refresh();

  /// Applies current zoom factor to the popup's view. Should be called when
  /// it's changed.
  void applyZoomFactor() const;
  void applyWordsZoomLevel();
  /// Translate the word
  void translateWord( QString const & word );

  void setDictionaryIconSize();

  void saveConfigData() const;

#ifdef HAVE_X11
  /// Interaction with scan flag window
  void showScanFlag();
  void hideScanFlag();

  QTimer selectionDelayTimer;
#endif

signals:

  /// Forwarded from the dictionary bar, so that main window could act on this.
  void editGroupRequest( unsigned id );
  /// Send word to main window
  void sendPhraseToMainWindow( QString const & word );
  /// Close opened menus when window hide
  void closeMenu();

  void inspectSignal( QWebEnginePage * page );
  /// Signal to switch expand optional parts mode
  void switchExpandMode();
  /// Signal to add word to history even if history is disabled
  void forceAddWordToHistory( const QString & word );
  /// Retranslate signal from dictionary bar
  void showDictionaryInfo( QString const & id );
  void openDictionaryFolder( QString const & id );
  /// Put translated word into history
  void sendWordToHistory( QString const & word );
  /// Put translated word into Favorites
  void sendWordToFavorites( QString const & word, unsigned groupId, bool );

#ifdef Q_OS_WIN32
  /// Ask for source window is current translate tab
  bool isGoldenDictWindow( HWND hwnd );
#endif

public slots:

  void inspectElementWhenPinned( QWebEnginePage * page );
  /// Translates the word from the clipboard, showing the window etc.
  void translateWordFromClipboard();
  /// Translates the word from the clipboard selection
  void translateWordFromSelection();
  /// From the dictionary bar.
  void editGroupRequested();

  void setGroupByName( QString const & name ) const;

#ifdef HAVE_X11
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

  void updateDictionaryBar();
  /// Check is word already presented in Favorites
  bool isWordPresentedInFavorites( QString const & word, unsigned groupId ) const;

  Config::Class & cfg;
  std::vector< sptr< Dictionary::Class > > const & allDictionaries;
  std::vector< sptr< Dictionary::Class > > dictionariesUnmuted;
  Instances::Groups const & groups;
  History & history;
  Ui::ScanPopup ui;
  ArticleView * definition;
  QAction escapeAction, switchExpandModeAction, focusTranslateLineAction;
  QAction stopAudioAction;
  QAction openSearchAction;
  QString pendingWord; // Word that is going to be translated
  WordFinder wordFinder;
  Config::Events configEvents;
  DictionaryBar dictionaryBar;
  QToolBar * toolbar;
  QActionGroup * actionGroup = nullptr;
  MainStatusBar * mainStatusBar;
  /// Fonts saved before words zooming is in effect, so it could be reset back.
  QFont wordListDefaultFont, translateLineDefaultFont, groupListDefaultFont;

#ifdef HAVE_X11
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

  void handleInputWord( QString const &, bool forcePopup = false );
  void engagePopup( bool forcePopup, bool giveFocus = false );

  vector< sptr< Dictionary::Class > > const & getActiveDicts();

  virtual bool eventFilter( QObject * watched, QEvent * event );

  /// Called from event filter or from mouseGrabPoll to handle mouse event
  /// while it is being intercepted.
  void reactOnMouseMove( QPointF const & p );

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

  void showTranslationFor( QString const & inputPhrase ) const;

  void updateSuggestionList();
  void updateSuggestionList( QString const & text );
private slots:
  void currentGroupChanged( int );
  void prefixMatchFinished();
  void on_pronounceButton_clicked() const;
  void pinButtonClicked( bool checked );
  void on_showDictionaryBar_clicked( bool checked );
  void showStatusBarMessage( QString const &, int, QPixmap const & ) const;
  void on_sendWordButton_clicked();
  void on_sendWordToFavoritesButton_clicked();
  void on_goBackButton_clicked() const;
  void on_goForwardButton_clicked() const;

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

  void translateInputChanged( QString const & text );
  void translateInputFinished();

  void focusTranslateLine();
  void stopAudio() const;

  void typingEvent( QString const & );

  void alwaysOnTopClicked( bool checked );

  void titleChanged( ArticleView *, QString const & title ) const;
  void updateFoundInDictsList();
  void onActionTriggered();
};
