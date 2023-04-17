/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "scanpopup.hh"
#include "folding.hh"
#include <QCursor>
#include <QPixmap>
#include <QBitmap>
#include <QMenu>
#include <QMouseEvent>
#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
#include <QDesktopWidget>
#include <QStringList>
#endif
#include "gddebug.hh"
#include "gestures.hh"

#ifdef Q_OS_MAC
#include "macos/macmouseover.hh"
#define MouseOver MacMouseOver
#endif
#include "base_type.hh"

/// We use different window flags under Windows and X11 due to slight differences
/// in their behavior on those platforms.
static const Qt::WindowFlags defaultUnpinnedWindowFlags =

#if defined (Q_OS_WIN)
Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
#else
Qt::Popup
#endif
;

static const Qt::WindowFlags pinnedWindowFlags =
#ifdef HAVE_X11
/// With the Qt::Dialog flag, scan popup is always on top of the main window
/// on Linux/X11 with Qt 4, Qt 5 since version 5.12.1 (QTBUG-74309).
/// Qt::Window allows to use the scan popup and the main window independently.
Qt::Window
#else
Qt::Dialog
#endif
;

#ifdef HAVE_X11
static bool ownsClipboardMode( QClipboard::Mode mode )
{
  const QClipboard & clipboard = *QApplication::clipboard();
  switch( mode )
  {
    case QClipboard::Clipboard:
      return clipboard.ownsClipboard();
    case QClipboard::Selection:
      return clipboard.ownsSelection();
    case QClipboard::FindBuffer:
      return clipboard.ownsFindBuffer();
  }

  gdWarning( "Unknown clipboard mode: %d\n", static_cast< int >( mode ) );
  return false;
}
#endif

ScanPopup::ScanPopup( QWidget * parent,
                      Config::Class & cfg_,
                      ArticleNetworkAccessManager & articleNetMgr,
                      AudioPlayerPtr const & audioPlayer_,
                      std::vector< sptr< Dictionary::Class > > const & allDictionaries_,
                      Instances::Groups const & groups_,
                      History & history_ ):
  QMainWindow( parent ),
  cfg( cfg_ ),
  allDictionaries( allDictionaries_ ),
  groups( groups_ ),
  history( history_ ),
  escapeAction( this ),
  switchExpandModeAction( this ),
  focusTranslateLineAction( this ),
  openSearchAction( this ),
  wordFinder( this ),
  dictionaryBar( this, configEvents, cfg.editDictionaryCommandLine, cfg.preferences.maxDictionaryRefsInContextMenu ),
  mouseEnteredOnce( false ),
  mouseIntercepted( false ),
  hideTimer( this ),
  starIcon( ":/icons/star.svg" ),
  blueStarIcon( ":/icons/star_blue.svg" )
{
  ui.setupUi( this );

  openSearchAction.setShortcut( QKeySequence( "Ctrl+F" ) );

  if( layoutDirection() == Qt::RightToLeft )
  {
    // Adjust button icons for Right-To-Left layout
    ui.goBackButton->setIcon( QIcon( ":/icons/next.svg" ) );
    ui.goForwardButton->setIcon( QIcon( ":/icons/previous.svg" ) );
  }

  mainStatusBar = new MainStatusBar( this );

  ui.queryError->hide();

  definition = new ArticleView( ui.outerFrame, articleNetMgr, audioPlayer_,
                                allDictionaries, groups, true, cfg,
                                openSearchAction,
                                ui.translateBox->translateLine(),
                                dictionaryBar.toggleViewAction()
                                );

  connect( definition, &ArticleView::inspectSignal, this, &ScanPopup::inspectElementWhenPinned );
  connect( definition, &ArticleView::forceAddWordToHistory, this, &ScanPopup::forceAddWordToHistory );
  connect( this, &ScanPopup::closeMenu, definition, &ArticleView::closePopupMenu );
  connect( definition, &ArticleView::sendWordToHistory, this, &ScanPopup::sendWordToHistory );
  connect( definition, &ArticleView::typingEvent, this, &ScanPopup::typingEvent );

  wordListDefaultFont      = ui.translateBox->wordList()->font();
  translateLineDefaultFont = ui.translateBox->font();
  groupListDefaultFont = ui.groupList->font();

  ui.mainLayout->addWidget( definition );

  ui.translateBox->wordList()->attachFinder( &wordFinder );
  ui.translateBox->wordList()->setFocusPolicy(Qt::ClickFocus);
  ui.translateBox->translateLine()->installEventFilter( this );
  definition->installEventFilter(this);
  this->installEventFilter(this);

  connect( ui.translateBox->translateLine(), &QLineEdit::textChanged, this, &ScanPopup::translateInputChanged );

  connect( ui.translateBox->translateLine(), &QLineEdit::returnPressed, this, &ScanPopup::translateInputFinished );

  connect( ui.translateBox->wordList(), &QListWidget::itemClicked, this, &ScanPopup::wordListItemActivated );

  connect( ui.translateBox->wordList(),
    SIGNAL( itemDoubleClicked( QListWidgetItem * ) ),
    this,
    SLOT( wordListItemActivated( QListWidgetItem * ) ) );

  connect( ui.translateBox->wordList(), &WordList::statusBarMessage, this, &ScanPopup::showStatusBarMessage );

  ui.pronounceButton->hide();

  ui.groupList->fill( groups );
  ui.groupList->setCurrentGroup( cfg.lastPopupGroupId );

  dictionaryBar.setFloatable( false );

  Instances::Group const * igrp = groups.findGroup( cfg.lastPopupGroupId );
  if( cfg.lastPopupGroupId == Instances::Group::AllGroupId )
  {
    if( igrp )
      igrp->checkMutedDictionaries( &cfg.popupMutedDictionaries );
    dictionaryBar.setMutedDictionaries( &cfg.popupMutedDictionaries );
  }
  else
  {
    Config::Group * grp = cfg.getGroup( cfg.lastPopupGroupId );
    if( igrp && grp )
      igrp->checkMutedDictionaries( &grp->popupMutedDictionaries );
    dictionaryBar.setMutedDictionaries( grp ? &grp->popupMutedDictionaries : 0 );
  }

  addToolBar( Qt::RightToolBarArea, &dictionaryBar );

  connect( &dictionaryBar, SIGNAL(editGroupRequested()),
           this, SLOT(editGroupRequested()) );
  connect( this, &ScanPopup::closeMenu, &dictionaryBar, &DictionaryBar::closePopupMenu );
  connect( &dictionaryBar, &DictionaryBar::showDictionaryInfo, this, &ScanPopup::showDictionaryInfo );
  connect( &dictionaryBar, &DictionaryBar::openDictionaryFolder, this, &ScanPopup::openDictionaryFolder );

  pinnedGeometry = cfg.popupWindowGeometry;
  if( cfg.popupWindowGeometry.size() )
    restoreGeometry( cfg.popupWindowGeometry );

  if ( cfg.popupWindowState.size() )
    restoreState( cfg.popupWindowState, 1 );

  ui.onTopButton->setChecked( cfg.popupWindowAlwaysOnTop );
  ui.onTopButton->setVisible( cfg.pinPopupWindow );
  connect( ui.onTopButton, &QAbstractButton::clicked, this, &ScanPopup::alwaysOnTopClicked );

  ui.pinButton->setChecked( cfg.pinPopupWindow );

  if ( cfg.pinPopupWindow )
  {
    dictionaryBar.setMovable( true );
    Qt::WindowFlags flags = pinnedWindowFlags;
    if( cfg.popupWindowAlwaysOnTop )
      flags |= Qt::WindowStaysOnTopHint;
    setWindowFlags( flags );
#ifdef Q_OS_MACOS
    setAttribute(Qt::WA_MacAlwaysShowToolWindow);
#endif
  }
  else
  {
    dictionaryBar.setMovable( false );
    setWindowFlags( unpinnedWindowFlags() );
#ifdef Q_OS_MACOS
    setAttribute(Qt::WA_MacAlwaysShowToolWindow, false);
#endif
  }

  connect( &configEvents, &Config::Events::mutedDictionariesChanged, this, &ScanPopup::mutedDictionariesChanged );

  definition->focus();

  #if 0 // Experimental code to give window a non-rectangular shape (i.e.
        // balloon) using a colorkey mask.
  QPixmap pixMask( size() );
  render( &pixMask );

  setMask( pixMask.createMaskFromColor( QColor( 255, 0, 0 ) ) );

  // This helps against flickering
  setAttribute( Qt::WA_NoSystemBackground );
  #endif

  escapeAction.setShortcut( QKeySequence( "Esc" ) );
  addAction( &escapeAction );
  connect( &escapeAction, &QAction::triggered, this, &ScanPopup::escapePressed );

  focusTranslateLineAction.setShortcutContext( Qt::WidgetWithChildrenShortcut );
  addAction( &focusTranslateLineAction );
  focusTranslateLineAction.setShortcuts( QList< QKeySequence >() <<
                                         QKeySequence( "Alt+D" ) <<
                                         QKeySequence( "Ctrl+L" ) );

  connect( &focusTranslateLineAction, &QAction::triggered, this, &ScanPopup::focusTranslateLine );

  QAction * const focusArticleViewAction = new QAction( this );
  focusArticleViewAction->setShortcutContext( Qt::WidgetWithChildrenShortcut );
  focusArticleViewAction->setShortcut( QKeySequence( "Ctrl+N" ) );
  addAction( focusArticleViewAction );
  connect( focusArticleViewAction, &QAction::triggered, definition, &ArticleView::focus );

  switchExpandModeAction.setShortcuts( QList< QKeySequence >() <<
                                       QKeySequence( Qt::CTRL | Qt::Key_8 ) <<
                                       QKeySequence( Qt::CTRL | Qt::Key_Asterisk ) <<
                                       QKeySequence( Qt::CTRL | Qt::SHIFT | Qt::Key_8 ) );

  addAction( &switchExpandModeAction );
  connect( &switchExpandModeAction, &QAction::triggered, this, &ScanPopup::switchExpandOptionalPartsMode );

  connect( ui.groupList, &QComboBox::currentIndexChanged,
           this, &ScanPopup::currentGroupChanged);

  connect( &wordFinder, &WordFinder::finished, this, &ScanPopup::prefixMatchFinished );

  connect( ui.pinButton, &QAbstractButton::clicked, this, &ScanPopup::pinButtonClicked );

  connect( definition, &ArticleView::pageLoaded, this, &ScanPopup::pageLoaded );

  connect( definition, &ArticleView::statusBarMessage, this, &ScanPopup::showStatusBarMessage );

  connect( definition, &ArticleView::titleChanged, this, &ScanPopup::titleChanged );

#ifdef Q_OS_MAC
  connect( &MouseOver::instance(),
    SIGNAL( hovered( QString const &, bool ) ),
    this,
    SLOT( mouseHovered( QString const &, bool ) ) );
#endif

  hideTimer.setSingleShot( true );
  hideTimer.setInterval( 400 );

  connect( &hideTimer, &QTimer::timeout, this, &ScanPopup::hideTimerExpired );

  mouseGrabPollTimer.setSingleShot( false );
  mouseGrabPollTimer.setInterval( 10 );
  connect( &mouseGrabPollTimer, &QTimer::timeout, this, &ScanPopup::mouseGrabPoll );
#ifdef Q_OS_MAC
  MouseOver::instance().setPreferencesPtr( &( cfg.preferences ) );
#endif
  ui.goBackButton->setEnabled( false );
  ui.goForwardButton->setEnabled( false );

  grabGesture( Gestures::GDPinchGestureType );
  grabGesture( Gestures::GDSwipeGestureType );

#ifdef HAVE_X11
  scanFlag = new ScanFlag( this );

  connect( scanFlag, &ScanFlag::requestScanPopup,
    this, [=]{
    translateWordFromSelection();
  });

  // Use delay show to prevent multiple popups while selection in progress
  selectionDelayTimer.setSingleShot( true );
  selectionDelayTimer.setInterval( 200 );

  connect( &selectionDelayTimer, &QTimer::timeout, this, &ScanPopup::translateWordFromSelection );
#endif

  applyZoomFactor();
  applyWordsZoomLevel();
}

void ScanPopup::refresh() {

  // TODO: GroupCombox's update should be moved inside GroupCombox

  // currentIndexChanged() signal is very trigger-happy. To avoid triggering
  // it, we disconnect it while we're clearing and filling back groups.
  disconnect( ui.groupList, &GroupComboBox::currentIndexChanged,
    this, &ScanPopup::currentGroupChanged );
  ui.groupList->clear();
  ui.groupList->fill(groups);
  ui.groupList->setCurrentGroup(0); // user edited group list, force reset to default
  ui.groupList->setVisible(!cfg.groups.empty());

  updateDictionaryBar();

  connect( ui.groupList, &GroupComboBox::currentIndexChanged,
    this, &ScanPopup::currentGroupChanged );
}


ScanPopup::~ScanPopup()
{
  saveConfigData();

  ungrabGesture( Gestures::GDPinchGestureType );
  ungrabGesture( Gestures::GDSwipeGestureType );
}

void ScanPopup::saveConfigData()
{
  // Save state, geometry and pin status
  cfg.popupWindowState = saveState( 1 );
  cfg.popupWindowGeometry = saveGeometry();
  cfg.pinPopupWindow = ui.pinButton->isChecked();
  cfg.popupWindowAlwaysOnTop = ui.onTopButton->isChecked();
}

void ScanPopup::inspectElementWhenPinned( QWebEnginePage * page ){
  if(cfg.pinPopupWindow)
    emit inspectSignal(page);
}

void ScanPopup::applyZoomFactor()
{
  definition->setZoomFactor( cfg.preferences.zoomFactor );
}

void ScanPopup::applyWordsZoomLevel()
{
  QFont font( wordListDefaultFont );
  int ps = font.pointSize();

  if ( cfg.preferences.wordsZoomLevel != 0 )
  {
    ps += cfg.preferences.wordsZoomLevel;
    if ( ps < 1 )
      ps = 1;
    font.setPointSize( ps );
  }

  if ( ui.translateBox->wordList()->font().pointSize() != ps )
    ui.translateBox->wordList()->setFont( font );

  font = translateLineDefaultFont;
  ps = font.pointSize();

  if ( cfg.preferences.wordsZoomLevel != 0 )
  {
    ps += cfg.preferences.wordsZoomLevel;
    if ( ps < 1 )
      ps = 1;
    font.setPointSize( ps );
  }

  if ( ui.translateBox->translateLine()->font().pointSize() != ps )
    ui.translateBox->translateLine()->setFont( font );

  font = groupListDefaultFont;
  ps = font.pointSize();

  if ( cfg.preferences.wordsZoomLevel != 0 )
  {
    ps += cfg.preferences.wordsZoomLevel;
    if ( ps < 1 )
      ps = 1;
    font.setPointSize( ps );
  }

  if ( ui.groupList->font().pointSize() != ps )
  {
    disconnect( ui.groupList, &GroupComboBox::currentIndexChanged,
                this, &ScanPopup::currentGroupChanged );
    int n = ui.groupList->currentIndex();
    ui.groupList->clear();
    ui.groupList->setFont( font );
    ui.groupList->fill( groups );
    ui.groupList->setCurrentIndex( n );
    connect( ui.groupList, &GroupComboBox::currentIndexChanged,
                this, &ScanPopup::currentGroupChanged );
  }

  ui.outerFrame->layout()->activate();
}

Qt::WindowFlags ScanPopup::unpinnedWindowFlags() const
{
  return defaultUnpinnedWindowFlags;
}

void ScanPopup::translateWordFromClipboard()
{
	return translateWordFromClipboard(QClipboard::Clipboard);
}

void ScanPopup::translateWordFromSelection()
{
	return translateWordFromClipboard(QClipboard::Selection);
}

void ScanPopup::editGroupRequested()
{
  emit editGroupRequested( ui.groupList->getCurrentGroup() );
}

void ScanPopup::translateWordFromClipboard(QClipboard::Mode m)
{
  qDebug()<< "translating from clipboard or selection" ;

  QString subtype = "plain";

  QString str = QApplication::clipboard()->text( subtype, m);
  qDebug() << "clipboard data:" << str;
  translateWord( str );
}

void ScanPopup::translateWord( QString const & word )
{
  pendingInputPhrase = cfg.preferences.sanitizeInputPhrase( word );

  if ( !pendingInputPhrase.isValid() )
    return; // Nothing there

#ifdef HAVE_X11
  emit hideScanFlag();
#endif

  inputPhrase = pendingInputPhrase;
  engagePopup( false,
#ifdef Q_OS_WIN
      true // We only focus popup under Windows when activated via Ctrl+C+C
           // -- on Linux it already has an implicit focus
#else
      false
#endif
      );
}

void ScanPopup::mouseHovered( QString const & str, bool forcePopup )
{
  handleInputWord( str, forcePopup );
}

[[deprecated]]
void ScanPopup::handleInputWord( QString const & str, bool forcePopup )
{
  Config::InputPhrase sanitizedPhrase = cfg.preferences.sanitizeInputPhrase( str );

  if ( isVisible() && sanitizedPhrase == inputPhrase )
  {
    // Attempt to translate the same word we already have shown in scan popup.
    // Ignore it, as it is probably a spurious mouseover event.
    return;
  }

  pendingInputPhrase = sanitizedPhrase;


#ifdef HAVE_X11
  if ( cfg.preferences.showScanFlag ) {
    inputPhrase = pendingInputPhrase;
    emit showScanFlag();
    return;
  }
#endif

  inputPhrase = pendingInputPhrase;
  engagePopup( forcePopup );
}

#ifdef HAVE_X11
void ScanPopup::showEngagePopup()
{
  engagePopup(false);
}
#endif

void ScanPopup::engagePopup( bool forcePopup, bool giveFocus )
{
  if( cfg.preferences.scanToMainWindow && !forcePopup )
  {
    // Send translated word to main window istead of show popup
    emit sendPhraseToMainWindow( inputPhrase );
    return;
  }

  definition->setSelectionBySingleClick( cfg.preferences.selectWordBySingleClick );

  if ( !isVisible() )
  {
    // Need to show the window

    if ( !ui.pinButton->isChecked() )
    {
      // Decide where should the window land

      QPoint currentPos = QCursor::pos();

      QRect desktop = QGuiApplication::primaryScreen()->geometry();

      QSize windowSize = geometry().size();

      int x, y;

      /// Try the to-the-right placement
      if ( currentPos.x() + 4 + windowSize.width() <= desktop.topRight().x() )
        x = currentPos.x() + 4;
      else
      /// Try the to-the-left placement
      if ( currentPos.x() - 4 - windowSize.width() >= desktop.x() )
        x = currentPos.x() - 4 - windowSize.width();
      else
      // Center it
        x = desktop.x() + ( desktop.width() - windowSize.width() ) / 2;

      /// Try the to-the-bottom placement
      if ( currentPos.y() + 15 + windowSize.height() <= desktop.bottomLeft().y() )
        y = currentPos.y() + 15;
      else
      /// Try the to-the-top placement
      if ( currentPos.y() - 15 - windowSize.height() >= desktop.y() )
        y = currentPos.y() - 15 - windowSize.height();
      else
      // Center it
        y = desktop.y() + ( desktop.height() - windowSize.height() ) / 2;

      move( x, y );
    }
    else
    {
      if( pinnedGeometry.size() > 0 )
        restoreGeometry( pinnedGeometry );
    }

    show();

#if defined( HAVE_X11 )
    // Ensure that the window always has focus on X11 with Qt::Tool flag.
    // This also often prevents the window from disappearing prematurely with Qt::Popup flag,
    // especially when combined with Qt::X11BypassWindowManagerHint flag.
    if ( !ui.pinButton->isChecked() )
      giveFocus = true;
#endif

    if ( giveFocus )
    {
      activateWindow();
      raise();
    }

    if ( !ui.pinButton->isChecked() )
    {
      mouseEnteredOnce = false;
      // Need to monitor the mouse so we know when to hide the window
      interceptMouse();
    }

    // This produced some funky mouse grip-related bugs so we commented it out
    //QApplication::processEvents(); // Make window appear immediately no matter what
  }
  else if( ui.pinButton->isChecked() ) {
    // Pinned-down window isn't always on top, so we need to raise it
    show();
    if( cfg.preferences.raiseWindowOnSearch ) {
      activateWindow();
      raise();
    }
  }
#if defined( HAVE_X11 )
  else if( ( windowFlags() & Qt::Tool ) == Qt::Tool && cfg.preferences.raiseWindowOnSearch ) {
    // Ensure that the window with Qt::Tool flag always has focus on X11.
    activateWindow();
    raise();
  }
#endif

  if ( ui.pinButton->isChecked() )
       setWindowTitle( tr( "%1 - %2" ).arg( elideInputWord(), "GoldenDict" ) );

  /// Too large strings make window expand which is probably not what user
  /// wants
  ui.translateBox->setText( Folding::escapeWildcardSymbols( inputPhrase.phrase ), false );
  translateBoxSuffix = inputPhrase.punctuationSuffix;

  showTranslationFor( inputPhrase );
}

QString ScanPopup::elideInputWord()
{
  QString const & inputWord = inputPhrase.phrase;
  return inputWord.size() > 32 ? inputWord.mid( 0, 32 ) + "..." : inputWord;
}
void ScanPopup::currentGroupChanged( int )
{
    cfg.lastPopupGroupId = ui.groupList->getCurrentGroup();
    Instances::Group const * igrp = groups.findGroup( cfg.lastPopupGroupId );
    if( cfg.lastPopupGroupId == Instances::Group::AllGroupId )
    {
      if( igrp )
        igrp->checkMutedDictionaries( &cfg.popupMutedDictionaries );
      dictionaryBar.setMutedDictionaries( &cfg.popupMutedDictionaries );
    }
    else
    {
      Config::Group * grp = cfg.getGroup( cfg.lastPopupGroupId );
      if( grp )
      {
        if( igrp )
          igrp->checkMutedDictionaries( &grp->popupMutedDictionaries );
        dictionaryBar.setMutedDictionaries( &grp->popupMutedDictionaries );
      }
      else
        dictionaryBar.setMutedDictionaries( 0 );
    }

  updateDictionaryBar();

  if ( isVisible() )
  {
    updateSuggestionList();
    translateInputFinished();
  }

  cfg.lastPopupGroupId = ui.groupList->getCurrentGroup();
}

void ScanPopup::wordListItemActivated( QListWidgetItem * item )
{
  showTranslationFor( Config::InputPhrase::fromPhrase( item->text() ) );
}

void ScanPopup::translateInputChanged( QString const & text )
{
  updateSuggestionList( text );
  translateBoxSuffix = QString();
}

void ScanPopup::updateSuggestionList()
{
  updateSuggestionList( ui.translateBox->translateLine()->text() );
}

void ScanPopup::updateSuggestionList( QString const & text )
{
  mainStatusBar->clearMessage();
  ui.translateBox->wordList()->setCurrentItem( 0, QItemSelectionModel::Clear );

  QString req = text.trimmed();

  if ( !req.size() )
  {
    // An empty request always results in an empty result
    wordFinder.cancel();
    ui.translateBox->wordList()->clear();
    ui.translateBox->wordList()->unsetCursor();

    // Reset the noResults mark if it's on right now
    if ( ui.translateBox->translateLine()->property( "noResults" ).toBool() )
    {
      auto translateLine=ui.translateBox->translateLine();
      translateLine->setProperty( "noResults", false );

      Utils::Widget::setNoResultColor( translateLine, false );
    }
    return;
  }

  ui.translateBox->wordList()->setCursor( Qt::WaitCursor );

  wordFinder.prefixMatch( req, getActiveDicts() );
}

void ScanPopup::translateInputFinished()
{
  inputPhrase.phrase = Folding::unescapeWildcardSymbols( ui.translateBox->translateLine()->text().trimmed() );
  inputPhrase.punctuationSuffix = translateBoxSuffix;
  showTranslationFor( inputPhrase );
}

void ScanPopup::showTranslationFor( Config::InputPhrase const & inputPhrase )
{
  ui.pronounceButton->hide();

  unsigned groupId = ui.groupList->getCurrentGroup();
  definition->showDefinition( inputPhrase, groupId );
  definition->focus();
}

vector< sptr< Dictionary::Class > > const & ScanPopup::getActiveDicts()
{
  int current = ui.groupList->currentIndex();

  Q_ASSERT(0 <= current || current <= groups.size());

  Config::MutedDictionaries const * mutedDictionaries = dictionaryBar.getMutedDictionaries();

  if ( !dictionaryBar.toggleViewAction()->isChecked() || mutedDictionaries == nullptr ){
    return groups[current].dictionaries;
  }

  vector< sptr< Dictionary::Class > > const & activeDicts =
  groups[current].dictionaries;

  // Populate the special dictionariesUnmuted array with only unmuted
  // dictionaries

  dictionariesUnmuted.clear();
  dictionariesUnmuted.reserve( activeDicts.size() );

  for(const auto & activeDict : activeDicts){
    if ( !mutedDictionaries->contains(QString::fromStdString( activeDict->getId() ) ) ){
      dictionariesUnmuted.push_back( activeDict );
    }
  }

  return dictionariesUnmuted;
}

void ScanPopup::typingEvent( QString const & t )
{
  if ( t == "\n" || t == "\r" ) {
    focusTranslateLine();
  }
  else {
    ui.translateBox->translateLine()->clear();
    ui.translateBox->translateLine()->setFocus();
    ui.translateBox->setText( t, true );
    ui.translateBox->translateLine()->setCursorPosition( t.size() );
  }
}

bool ScanPopup::eventFilter( QObject * watched, QEvent * event )
{
  if ( watched == ui.translateBox->translateLine() && event->type() == QEvent::FocusIn )
  {
    const QFocusEvent * focusEvent = static_cast< QFocusEvent * >( event );

    // select all on mouse click
    if ( focusEvent->reason() == Qt::MouseFocusReason ) {
      QTimer::singleShot( 0, this, &ScanPopup::focusTranslateLine );
    }
    return false;
  }

  if ( mouseIntercepted )
  {
    // We're only interested in our events

    if ( event->type() == QEvent::MouseMove )
    {
      QMouseEvent * mouseEvent = ( QMouseEvent * ) event;
      reactOnMouseMove( mouseEvent->globalPos() );
    }
  }

  if ( event->type() == QEvent::KeyPress && watched != ui.translateBox->translateLine() ) {

    if ( const auto key_event = dynamic_cast< QKeyEvent * >(event);  key_event->modifiers() == Qt::NoModifier ) {
      const QString text = key_event->text();

      if ( Utils::ignoreKeyEvent( key_event ) ||
           key_event->key() == Qt::Key_Return ||
           key_event->key() == Qt::Key_Enter )
          return false; // Those key have other uses than to start typing
      // or don't make sense
      if ( !text.isEmpty() ) {
          typingEvent( text );
          return true;
      }
    }

  }

  return QMainWindow::eventFilter( watched, event );
}

void ScanPopup::reactOnMouseMove( QPoint const & p )
{
  if ( geometry().contains( p ) )
  {
//        GD_DPRINTF( "got inside\n" );

    hideTimer.stop();
    mouseEnteredOnce = true;
    uninterceptMouse();
  }
  else
  {
//        GD_DPRINTF( "outside\n" );
    // We're in grab mode and outside the window - calculate the
    // distance from it. We might want to hide it.

    // When the mouse has entered once, we don't allow it stayng outside,
    // but we give a grace period for it to return.
    int proximity = mouseEnteredOnce ? 0 : 60;

    // Note: watched == this ensures no other child objects popping out are
    // receiving this event, meaning there's basically nothing under the
    // cursor.
    if ( /*watched == this &&*/
         !frameGeometry().adjusted( -proximity, -proximity, proximity, proximity ).
         contains( p ) )
    {
      // We've way too far from the window -- hide the popup

      // If the mouse never entered the popup, hide the window instantly --
      // the user just moved the cursor further away from the window.

      if ( !mouseEnteredOnce )
        hideWindow();
      else
        hideTimer.start();
    }
  }
}

void ScanPopup::mousePressEvent( QMouseEvent * ev )
{
  // With mouse grabs, the press can occur anywhere on the screen, which
  // might mean hiding the window.

  if ( !frameGeometry().contains( ev->globalPos() ) )
  {
    hideWindow();

    return;
  }

  if ( ev->button() == Qt::LeftButton )
  {
    startPos = ev->globalPos();
    setCursor( Qt::ClosedHandCursor );
  }

  QMainWindow::mousePressEvent( ev );
}

void ScanPopup::mouseMoveEvent( QMouseEvent * event )
{
  if ( event->buttons() && cursor().shape() == Qt::ClosedHandCursor )
  {
    QPoint newPos = event->globalPos();

    QPoint delta = newPos - startPos;

    startPos = newPos;

    // Move the window

    move( pos() + delta );
  }

  QMainWindow::mouseMoveEvent( event );
}

void ScanPopup::mouseReleaseEvent( QMouseEvent * ev )
{
  unsetCursor();
  QMainWindow::mouseReleaseEvent( ev );
}

void ScanPopup::leaveEvent( QEvent * event )
{
  QMainWindow::leaveEvent( event );

  // We hide the popup when the mouse leaves it.

  // Combo-boxes seem to generate leave events for their parents when
  // unfolded, so we check coordinates as well.
  // If the dialog is pinned, we don't hide the popup.
  // If some mouse buttons are pressed, we don't hide the popup either,
  // since it indicates the move operation is underway.
  if ( !ui.pinButton->isChecked() && !geometry().contains( QCursor::pos() ) &&
       QApplication::mouseButtons() == Qt::NoButton )
  {
    hideTimer.start();
  }
}

#if (QT_VERSION >= QT_VERSION_CHECK(6,0,0))
void ScanPopup::enterEvent( QEnterEvent * event )
#else
void ScanPopup::enterEvent( QEvent * event )
#endif
{
  QMainWindow::enterEvent( event );

  if ( mouseEnteredOnce )
  {
    // We "enter" first time via our event filter. This seems to evade some
    // unexpected behavior under Windows.

    // If there was a countdown to hide the window, stop it.
    hideTimer.stop();
  }
}

void ScanPopup::requestWindowFocus()
{
  // One of the rare, actually working workarounds for requesting a user keyboard focus on X11,
  // works for Qt::Popup windows, exactly like our Scan Popup (in unpinned state).
  // Modern window managers actively resist to automatically focus pop-up windows.

}

void ScanPopup::showEvent( QShowEvent * ev )
{
  QMainWindow::showEvent( ev );

  QTimer::singleShot( 100, this, &ScanPopup::requestWindowFocus );

  if ( groups.size() <= 1 ) // Only the default group? Hide then.
    ui.groupList->hide();

  if ( ui.showDictionaryBar->isChecked() != dictionaryBar.isVisible() )
  {
    ui.showDictionaryBar->setChecked( dictionaryBar.isVisible() );
    updateDictionaryBar();
  }
}

void ScanPopup::closeEvent( QCloseEvent * ev )
{
  if( isVisible() && ui.pinButton->isChecked() )
    pinnedGeometry = saveGeometry();

  QMainWindow::closeEvent( ev );
}

void ScanPopup::moveEvent( QMoveEvent * ev )
{
  if( isVisible() && ui.pinButton->isChecked() )
    pinnedGeometry = saveGeometry();

  QMainWindow::moveEvent( ev );
}

void ScanPopup::prefixMatchFinished()
{
  // Check that there's a window there at all
  if ( isVisible() )
  {
    if ( wordFinder.getErrorString().size() )
    {
      ui.queryError->setToolTip( wordFinder.getErrorString() );
      ui.queryError->show();
    }
    else
      ui.queryError->hide();
  }
}

void ScanPopup::on_pronounceButton_clicked()
{
  definition->playSound();
}

void ScanPopup::pinButtonClicked( bool checked )
{
  if ( checked )
  {
    uninterceptMouse();

    ui.onTopButton->setVisible( true );
    Qt::WindowFlags flags = pinnedWindowFlags;
    if( ui.onTopButton->isChecked() )
      flags |= Qt::WindowStaysOnTopHint;
    setWindowFlags( flags );

#ifdef Q_OS_MACOS
    setAttribute(Qt::WA_MacAlwaysShowToolWindow);
#endif

    setWindowTitle( tr( "%1 - %2" ).arg( elideInputWord(), "GoldenDict" ) );
    dictionaryBar.setMovable( true );
    hideTimer.stop();
  }
  else
  {
    ui.onTopButton->setVisible( false );
    dictionaryBar.setMovable( false );
    setWindowFlags( unpinnedWindowFlags() );

#ifdef Q_OS_MACOS
    setAttribute(Qt::WA_MacAlwaysShowToolWindow, false);
#endif

    mouseEnteredOnce = true;
  }
  cfg.pinPopupWindow = checked;

  show();

  if( checked )
    pinnedGeometry = saveGeometry();
}

void ScanPopup::focusTranslateLine()
{
  if ( !isActiveWindow() )
    activateWindow();

  ui.translateBox->translateLine()->setFocus();
  ui.translateBox->translateLine()->selectAll();
}

void ScanPopup::on_showDictionaryBar_clicked( bool checked )
{
  dictionaryBar.setVisible( checked );
  updateDictionaryBar();
  definition->updateMutedContents();
}

void ScanPopup::hideTimerExpired()
{
  if ( isVisible() )
    hideWindow();
}

void ScanPopup::pageLoaded( ArticleView * )
{
  if( !isVisible() )
    return;
  auto pronounceBtn = ui.pronounceButton;
  definition->hasSound(
    [ pronounceBtn ]( bool has )
    {
      if( pronounceBtn )
        pronounceBtn->setVisible( has );
    } );

  updateBackForwardButtons();

  if ( cfg.preferences.pronounceOnLoadPopup )
    definition->playSound();
}

void ScanPopup::showStatusBarMessage( QString const & message, int timeout, QPixmap const & icon )
{
  mainStatusBar->showMessage( message, timeout, icon );
}

void ScanPopup::escapePressed()
{
  if ( !definition->closeSearch() )
    hideWindow();
}

void ScanPopup::hideWindow()
{
  uninterceptMouse();

  emit closeMenu();
  hideTimer.stop();
  unsetCursor();
  ui.translateBox->setPopupEnabled( false );
  ui.translateBox->translateLine()->deselect();
  hide();
}

void ScanPopup::interceptMouse()
{
  if ( !mouseIntercepted )
  {
    // We used to grab the mouse -- but this doesn't always work reliably
    // (e.g. doesn't work at all in Windows 7 for some reason). Therefore
    // we use a polling timer now.

//    grabMouse();
    mouseGrabPollTimer.start();

    qApp->installEventFilter( this );

    mouseIntercepted = true;
  }
}

void ScanPopup::mouseGrabPoll()
{
  if ( mouseIntercepted )
    reactOnMouseMove( QCursor::pos() );
}

void ScanPopup::uninterceptMouse()
{
  if ( mouseIntercepted )
  {
    qApp->removeEventFilter( this );
    mouseGrabPollTimer.stop();
//    releaseMouse();

    mouseIntercepted = false;
  }
}

void ScanPopup::updateDictionaryBar()
{
  if ( !dictionaryBar.toggleViewAction()->isChecked() )
    return; // It's not enabled, therefore hidden -- don't waste time

  unsigned currentId = ui.groupList->getCurrentGroup();
  Instances::Group const * grp = groups.findGroup( currentId );

  if ( grp ) // Should always be !0, but check as a safeguard
    dictionaryBar.setDictionaries( grp->dictionaries );

  if( currentId == Instances::Group::AllGroupId )
    dictionaryBar.setMutedDictionaries( &cfg.popupMutedDictionaries );
  else
  {
    Config::Group * grp = cfg.getGroup( currentId );
    dictionaryBar.setMutedDictionaries( grp ? &grp->popupMutedDictionaries : 0 );
  }

  setDictionaryIconSize();
}

void ScanPopup::mutedDictionariesChanged()
{
  updateSuggestionList();
  if ( dictionaryBar.toggleViewAction()->isChecked() )
    definition->updateMutedContents();
}

void ScanPopup::on_sendWordButton_clicked()
{
  if ( !isVisible() )
    return;
  if( !ui.pinButton->isChecked() )
  {
    definition->closeSearch();
    hideWindow();
  }
  emit sendPhraseToMainWindow( definition->getPhrase() );
}

void ScanPopup::on_sendWordToFavoritesButton_clicked()
{
  if ( !isVisible() )
    return;
  emit sendWordToFavorites( definition->getTitle(), cfg.lastPopupGroupId );

  ui.sendWordToFavoritesButton->setIcon( blueStarIcon );
}

void ScanPopup::switchExpandOptionalPartsMode()
{
  if( isVisible() )
    emit switchExpandMode();
}

void ScanPopup::updateBackForwardButtons()
{
  ui.goBackButton->setEnabled(definition->canGoBack());
  ui.goForwardButton->setEnabled(definition->canGoForward());
}

void ScanPopup::on_goBackButton_clicked()
{
  definition->back();
}

void ScanPopup::on_goForwardButton_clicked()
{
  definition->forward();
}

void ScanPopup::setDictionaryIconSize()
{
  int extent = cfg.usingSmallIconsInToolbars ?
               QApplication::style()->pixelMetric( QStyle::PM_SmallIconSize ) :
               QApplication::style()->pixelMetric(QStyle::PM_ToolBarIconSize);
  dictionaryBar.setDictionaryIconSize( extent );
}

void ScanPopup::setGroupByName( QString const & name )
{
  int i;
  for( i = 0; i < ui.groupList->count(); i++ )
  {
    if( ui.groupList->itemText( i ) == name )
    {
      ui.groupList->setCurrentIndex( i );
      break;
    }
  }
  if( i >= ui.groupList->count() )
    gdWarning( "Group \"%s\" for popup window is not found\n", name.toUtf8().data() );
}

void ScanPopup::alwaysOnTopClicked( bool checked )
{
  bool wasVisible = isVisible();
  if( ui.pinButton->isChecked() )
  {
    Qt::WindowFlags flags = this->windowFlags();
    if( checked )
      setWindowFlags(flags | Qt::WindowStaysOnTopHint );
    else
      setWindowFlags(flags ^ Qt::WindowStaysOnTopHint );
    if( wasVisible )
      show();
  }
}

void ScanPopup::titleChanged( ArticleView *, QString const & title )
{
  unsigned groupId = ui.groupList->getCurrentGroup();

  // Set icon for "Add to Favorites" button
  ui.sendWordToFavoritesButton->setIcon( isWordPresentedInFavorites( title, groupId ) ?
                                         blueStarIcon : starIcon );
}

#ifdef HAVE_X11
void ScanPopup::showScanFlag(){
  scanFlag->showScanFlag();
}

void ScanPopup::hideScanFlag(){
  scanFlag->hideWindow();
}
#endif
