/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "scanpopup.hh"
#include "folding.hh"
#include <QCursor>
#include <QPixmap>
#include <QBitmap>
#include <QMenu>
#include <QMouseEvent>
#include "gestures.hh"

#ifdef Q_OS_MAC
  #include "macos/macmouseover.hh"
  #define MouseOver MacMouseOver
#endif
#include "base_type.hh"


static const Qt::WindowFlags defaultUnpinnedWindowFlags =

#if defined( Q_OS_WIN )
  Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
#else
  Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
#endif
  ;

static const Qt::WindowFlags pinnedWindowFlags =
#ifdef HAVE_X11
  /// With the Qt::Dialog flag, popup is always on top of the main window
  /// on Linux/X11 with Qt 4, Qt 5 since version 5.12.1 (QTBUG-74309).
  /// Qt::Window allows to use the popup and the main window independently.
  Qt::Window
#else
  Qt::Dialog
#endif
  ;

#ifdef HAVE_X11
static bool ownsClipboardMode( QClipboard::Mode mode )
{
  const QClipboard & clipboard = *QApplication::clipboard();
  switch ( mode ) {
    case QClipboard::Clipboard:
      return clipboard.ownsClipboard();
    case QClipboard::Selection:
      return clipboard.ownsSelection();
    case QClipboard::FindBuffer:
      return clipboard.ownsFindBuffer();
  }

  qWarning( "Unknown clipboard mode: %d", static_cast< int >( mode ) );
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
  stopAudioAction( this ),
  openSearchAction( this ),
  wordFinder( this ),
  dictionaryBar( this, configEvents, cfg.preferences.maxDictionaryRefsInContextMenu ),
  hideTimer( this )
{
  ui.setupUi( this );
  toolbar = new QToolBar( "Found Dictionary", this );

  if ( layoutDirection() == Qt::RightToLeft ) {
    // Adjust button icons for Right-To-Left layout
    ui.goBackButton->setIcon( QIcon( ":/icons/next.svg" ) );
    ui.goForwardButton->setIcon( QIcon( ":/icons/previous.svg" ) );
  }

  mainStatusBar = new MainStatusBar( this );

  ui.queryError->hide();

  definition = new ArticleView( ui.outerFrame,
                                articleNetMgr,
                                audioPlayer_,
                                allDictionaries,
                                groups,
                                true,
                                cfg,
                                ui.translateBox->translateLine(),
                                dictionaryBar.toggleViewAction(),
                                cfg.lastPopupGroupId );

  connect( definition, &ArticleView::inspectSignal, this, &ScanPopup::inspectElementWhenPinned );
  connect( definition, &ArticleView::forceAddWordToHistory, this, &ScanPopup::forceAddWordToHistory );
  connect( this, &ScanPopup::closeMenu, definition, &ArticleView::closePopupMenu );
  connect( definition, &ArticleView::sendWordToHistory, this, &ScanPopup::sendWordToHistory );
  connect( definition, &ArticleView::typingEvent, this, &ScanPopup::typingEvent );
  connect( definition, &ArticleView::updateFoundInDictsList, this, &ScanPopup::updateFoundInDictsList );

  openSearchAction.setShortcut( QKeySequence( "Ctrl+F" ) );
  openSearchAction.setShortcutContext( Qt::WidgetWithChildrenShortcut );
  addAction( &openSearchAction );
  connect( &openSearchAction, &QAction::triggered, definition, &ArticleView::openSearch );

  wordListDefaultFont      = ui.translateBox->completerWidget()->font();
  translateLineDefaultFont = ui.translateBox->font();
  groupListDefaultFont     = ui.groupList->font();

  ui.mainLayout->addWidget( definition );

  ui.translateBox->translateLine()->installEventFilter( this );
  definition->installEventFilter( this );
  this->installEventFilter( this );

  connect( ui.translateBox->translateLine(), &QLineEdit::textEdited, this, &ScanPopup::translateInputChanged );

  connect( ui.translateBox, &TranslateBox::returnPressed, this, &ScanPopup::translateInputFinished );

  ui.pronounceButton->setDisabled( true );

  ui.groupList->fill( groups );
  ui.groupList->setCurrentGroup( cfg.lastPopupGroupId );

  definition->setCurrentGroupId( ui.groupList->getCurrentGroup() );
  definition->setSelectionBySingleClick( cfg.preferences.selectWordBySingleClick );
  dictionaryBar.setFloatable( false );

  Instances::Group const * igrp = groups.findGroup( cfg.lastPopupGroupId );
  if ( cfg.lastPopupGroupId == GroupId::AllGroupId ) {
    if ( igrp ) {
      igrp->checkMutedDictionaries( &cfg.popupMutedDictionaries );
    }
    dictionaryBar.setMutedDictionaries( &cfg.popupMutedDictionaries );
  }
  else {
    Config::Group * grp = cfg.getGroup( cfg.lastPopupGroupId );
    if ( igrp && grp ) {
      igrp->checkMutedDictionaries( &grp->popupMutedDictionaries );
    }
    dictionaryBar.setMutedDictionaries( grp ? &grp->popupMutedDictionaries : nullptr );
  }

  addToolBar( Qt::RightToolBarArea, toolbar );

  connect( &dictionaryBar, &DictionaryBar::editGroupRequested, this, &ScanPopup::editGroupRequested );
  connect( this, &ScanPopup::closeMenu, &dictionaryBar, &DictionaryBar::closePopupMenu );
  connect( &dictionaryBar, &DictionaryBar::showDictionaryInfo, this, &ScanPopup::showDictionaryInfo );
  connect( &dictionaryBar, &DictionaryBar::openDictionaryFolder, this, &ScanPopup::openDictionaryFolder );

  connect( &GlobalBroadcaster::instance()->pronounce_engine,
           &PronounceEngine::emitAudio,
           this,
           [ this ]( auto audioUrl ) {
             definition->setAudioLink( audioUrl );
             if ( !isActiveWindow() ) {
               return;
             }
             if ( cfg.preferences.pronounceOnLoadPopup ) {
               definition->playAudio( QUrl::fromEncoded( audioUrl.toUtf8() ) );
             }
           } );
  pinnedGeometry = cfg.popupWindowGeometry;
  if ( cfg.popupWindowGeometry.size() ) {
    restoreGeometry( cfg.popupWindowGeometry );
  }

  if ( cfg.popupWindowState.size() ) {
    restoreState( cfg.popupWindowState );
  }

  //fix this toolbar
  addToolBar( Qt::TopToolBarArea, &dictionaryBar );

  ui.onTopButton->setChecked( cfg.popupWindowAlwaysOnTop );
  ui.onTopButton->setVisible( cfg.pinPopupWindow );
  connect( ui.onTopButton, &QAbstractButton::clicked, this, &ScanPopup::alwaysOnTopClicked );

  ui.pinButton->setChecked( cfg.pinPopupWindow );

  if ( cfg.pinPopupWindow ) {
    dictionaryBar.setMovable( false );
    Qt::WindowFlags flags = pinnedWindowFlags;
    if ( cfg.popupWindowAlwaysOnTop ) {
      flags |= Qt::WindowStaysOnTopHint;
    }
    setWindowFlags( flags );
#ifdef Q_OS_MACOS
    setAttribute( Qt::WA_MacAlwaysShowToolWindow );
#endif
  }
  else {
    dictionaryBar.setMovable( false );
    setWindowFlags( unpinnedWindowFlags() );
#ifdef Q_OS_MACOS
    setAttribute( Qt::WA_MacAlwaysShowToolWindow, false );
#endif
  }

  connect( &configEvents, &Config::Events::mutedDictionariesChanged, this, &ScanPopup::mutedDictionariesChanged );

  definition->focus();

  escapeAction.setShortcut( QKeySequence( "Esc" ) );
  addAction( &escapeAction );
  connect( &escapeAction, &QAction::triggered, this, &ScanPopup::escapePressed );

  focusTranslateLineAction.setShortcutContext( Qt::WidgetWithChildrenShortcut );
  addAction( &focusTranslateLineAction );
  focusTranslateLineAction.setShortcuts( QList< QKeySequence >()
                                         << QKeySequence( "Alt+D" ) << QKeySequence( "Ctrl+L" ) );

  connect( &focusTranslateLineAction, &QAction::triggered, this, &ScanPopup::focusTranslateLine );

  stopAudioAction.setShortcutContext( Qt::WidgetWithChildrenShortcut );
  addAction( &stopAudioAction );
  stopAudioAction.setShortcut( QKeySequence( "Ctrl+Shift+S" ) );

  connect( &stopAudioAction, &QAction::triggered, this, &ScanPopup::stopAudio );

  QAction * const focusArticleViewAction = new QAction( this );
  focusArticleViewAction->setShortcutContext( Qt::WidgetWithChildrenShortcut );
  focusArticleViewAction->setShortcut( QKeySequence( "Ctrl+N" ) );
  addAction( focusArticleViewAction );
  connect( focusArticleViewAction, &QAction::triggered, definition, &ArticleView::focus );

  switchExpandModeAction.setShortcuts( QList< QKeySequence >() << QKeySequence( Qt::CTRL | Qt::Key_8 )
                                                               << QKeySequence( Qt::CTRL | Qt::Key_Asterisk )
                                                               << QKeySequence( Qt::CTRL | Qt::SHIFT | Qt::Key_8 ) );

  addAction( &switchExpandModeAction );
  connect( &switchExpandModeAction, &QAction::triggered, this, &ScanPopup::switchExpandOptionalPartsMode );

  connect( ui.groupList, &QComboBox::currentIndexChanged, this, &ScanPopup::currentGroupChanged );

  connect( &wordFinder, &WordFinder::finished, this, &ScanPopup::prefixMatchFinished );

  connect( ui.pinButton, &QAbstractButton::clicked, this, &ScanPopup::pinButtonClicked );

  connect( definition, &ArticleView::pageLoaded, this, &ScanPopup::pageLoaded );

  connect( definition, &ArticleView::statusBarMessage, this, &ScanPopup::showStatusBarMessage );

  connect( definition, &ArticleView::titleChanged, this, &ScanPopup::titleChanged );

#ifdef Q_OS_MAC
  connect( &MouseOver::instance(), &MouseOver::hovered, this, &ScanPopup::handleInputWord );
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

#ifndef Q_OS_MACOS
  grabGesture( Gestures::GDPinchGestureType );
  grabGesture( Gestures::GDSwipeGestureType );
#endif

#ifdef HAVE_X11
  scanFlag = new ScanFlag( this );

  connect( scanFlag, &ScanFlag::requestScanPopup, this, [ this ] {
    translateWordFromSelection();
  } );

  // Use delay show to prevent popup from showing up while selection is still in progress
  // Only certain software has this problem (e.g. Chrome)
  selectionDelayTimer.setSingleShot( true );
  selectionDelayTimer.setInterval( cfg.preferences.selectionChangeDelayTimer );

  connect( &selectionDelayTimer, &QTimer::timeout, this, &ScanPopup::translateWordFromSelection );
#endif

  applyZoomFactor();
  applyWordsZoomLevel();
}

void ScanPopup::onActionTriggered()
{
  QAction * action = qobject_cast< QAction * >( sender() );
  if ( action != nullptr ) {
    auto dictId = action->data().toString();
    qDebug() << "Action triggered:" << dictId;
    definition->jumpToDictionary( dictId, true );
  }
}

void ScanPopup::updateFoundInDictsList()
{
  if ( !toolbar->isVisible() ) {
    // nothing to do, the list is not visible
    return;
  }
  toolbar->setUpdatesEnabled( false );

  unsigned currentId           = ui.groupList->getCurrentGroup();
  Instances::Group const * grp = groups.findGroup( currentId );

  auto dictionaries = grp ? grp->dictionaries : allDictionaries;
  QStringList ids   = definition->getArticlesList();
  QString activeId  = definition->getActiveArticleId();
  toolbar->clear();
  if ( actionGroup != nullptr ) {
    actionGroup->deleteLater();
  }
  actionGroup = new QActionGroup( this );
  actionGroup->setExclusive( true );
  for ( QStringList::const_iterator i = ids.constBegin(); i != ids.constEnd(); ++i ) {
    // Find this dictionary

    for ( unsigned x = dictionaries.size(); x--; ) {
      if ( dictionaries[ x ]->getId() == i->toUtf8().data() ) {

        auto dictionary  = dictionaries[ x ];
        QIcon icon       = dictionary->getIcon();
        QString dictName = QString::fromUtf8( dictionary->getName().c_str() );
        QAction * action = new QAction( dictName, this );
        action->setIcon( icon );
        QString id = QString::fromStdString( dictionary->getId() );
        action->setData( id );
        action->setCheckable( true );
        if ( id == activeId ) {
          action->setChecked( true );
        }
        connect( action, &QAction::triggered, this, &ScanPopup::onActionTriggered );
        toolbar->addAction( action );
        actionGroup->addAction( action );
        break;
      }
    }
  }

  toolbar->setUpdatesEnabled( true );
}

void ScanPopup::refresh()
{
  // currentIndexChanged() signal is very trigger-happy. To avoid triggering
  // it, we disconnect it while we're clearing and filling back groups.
  disconnect( ui.groupList, &GroupComboBox::currentIndexChanged, this, &ScanPopup::currentGroupChanged );

  auto OldGroupID = ui.groupList->getCurrentGroup();

  // repopulate
  ui.groupList->clear();
  ui.groupList->fill( groups );

  ui.groupList->setCurrentGroup( OldGroupID ); // This does nothing if OldGroupID doesn't exist;

  ui.groupList->setVisible( !cfg.groups.empty() );

  updateDictionaryBar();

  definition->syncBackgroundColorWithCfgDarkReader();

  connect( ui.groupList, &GroupComboBox::currentIndexChanged, this, &ScanPopup::currentGroupChanged );
#ifdef HAVE_X11
  selectionDelayTimer.setInterval( cfg.preferences.selectionChangeDelayTimer );
#endif
}


ScanPopup::~ScanPopup()
{
  saveConfigData();
#ifndef Q_OS_MACOS
  ungrabGesture( Gestures::GDPinchGestureType );
  ungrabGesture( Gestures::GDSwipeGestureType );
#endif
}

void ScanPopup::saveConfigData() const
{
  // Save state, geometry and pin status
  cfg.popupWindowState       = saveState();
  cfg.popupWindowGeometry    = saveGeometry();
  cfg.pinPopupWindow         = ui.pinButton->isChecked();
  cfg.popupWindowAlwaysOnTop = ui.onTopButton->isChecked();
}

void ScanPopup::inspectElementWhenPinned( QWebEnginePage * page )
{
  if ( cfg.pinPopupWindow ) {
    emit inspectSignal( page );
  }
}

void ScanPopup::applyZoomFactor() const
{
  definition->setZoomFactor( cfg.preferences.zoomFactor );
}

void ScanPopup::applyWordsZoomLevel()
{
  QFont font = ui.translateBox->translateLine()->font();

  int ps = dictionaryBar.iconSize().height();

  if ( cfg.preferences.wordsZoomLevel != 0 ) {
    ps += cfg.preferences.wordsZoomLevel;
    if ( ps < 12 ) {
      ps = 12;
    }
    font.setPixelSize( ps * 0.8 );
  }
  ui.translateBox->completerWidget()->setFont( font );
  //  ui.translateBox->translateLine()->setFont( font );

  ui.outerFrame->layout()->activate();
}

Qt::WindowFlags ScanPopup::unpinnedWindowFlags() const
{
  return defaultUnpinnedWindowFlags;
}

void ScanPopup::translateWordFromClipboard()
{
  return translateWordFromClipboard( QClipboard::Clipboard );
}

void ScanPopup::translateWordFromSelection()
{
  return translateWordFromClipboard( QClipboard::Selection );
}

void ScanPopup::editGroupRequested()
{
  emit editGroupRequest( ui.groupList->getCurrentGroup() );
}

void ScanPopup::translateWordFromClipboard( QClipboard::Mode m )
{
  qDebug() << "translating from clipboard or selection";

  QString subtype = "plain";

  QString str = QApplication::clipboard()->text( subtype, m );
  qDebug() << "clipboard data:" << str;
  translateWord( str );
}

void ScanPopup::translateWord( QString const & word )
{
  pendingWord = cfg.preferences.sanitizeInputPhrase( word );

  if ( pendingWord.isEmpty() ) {
    return; // Nothing there
  }

#ifdef HAVE_X11
  emit hideScanFlag();
#endif

  engagePopup( false, true );
}

#ifdef HAVE_X11
void ScanPopup::showEngagePopup()
{
  engagePopup( false );
}
#endif

[[deprecated]] void ScanPopup::handleInputWord( QString const & str, bool forcePopup )
{
  auto sanitizedPhrase = cfg.preferences.sanitizeInputPhrase( str );

  if ( isVisible() && sanitizedPhrase == pendingWord ) {
    // Attempt to translate the same word we already have shown in popup.
    // Ignore it, as it is probably a spurious mouseover event.
    return;
  }

  pendingWord = sanitizedPhrase;

#ifdef HAVE_X11
  if ( cfg.preferences.showScanFlag ) {
    emit showScanFlag();
    return;
  }
#endif

  engagePopup( forcePopup );
}

void ScanPopup::engagePopup( bool forcePopup, bool giveFocus )
{
  if ( cfg.preferences.scanToMainWindow && !forcePopup ) {
    // Send translated word to main window istead of show popup
    emit sendPhraseToMainWindow( pendingWord );
    return;
  }

  if ( !isVisible() ) {
    // Need to show the window

    if ( !ui.pinButton->isChecked() ) {
      // Decide where should the window land

      QPoint currentPos = QCursor::pos();

      auto screen = QGuiApplication::screenAt( currentPos );
      if ( !screen ) {
        return;
      }

      QRect desktop = screen->geometry();

      QSize windowSize = geometry().size();

      int x, y;

      /// Try the to-the-right placement
      if ( currentPos.x() + 4 + windowSize.width() <= desktop.topRight().x() ) {
        x = currentPos.x() + 4;
      }
      else
        /// Try the to-the-left placement
        if ( currentPos.x() - 4 - windowSize.width() >= desktop.x() ) {
          x = currentPos.x() - 4 - windowSize.width();
        }
        else {
          // Center it
          x = desktop.x() + ( desktop.width() - windowSize.width() ) / 2;
        }

      /// Try the to-the-bottom placement
      if ( currentPos.y() + 15 + windowSize.height() <= desktop.bottomLeft().y() ) {
        y = currentPos.y() + 15;
      }
      else
        /// Try the to-the-top placement
        if ( currentPos.y() - 15 - windowSize.height() >= desktop.y() ) {
          y = currentPos.y() - 15 - windowSize.height();
        }
        else {
          // Center it
          y = desktop.y() + ( desktop.height() - windowSize.height() ) / 2;
        }

      move( x, y );
    }
    else {
      if ( pinnedGeometry.size() > 0 ) {
        restoreGeometry( pinnedGeometry );
      }
    }

    show();

    if ( giveFocus ) {
      activateWindow();
      raise();
    }

    if ( !ui.pinButton->isChecked() ) {
      mouseEnteredOnce = false;
      // Need to monitor the mouse so we know when to hide the window
      interceptMouse();
    }

    // This produced some funky mouse grip-related bugs so we commented it out
    //QApplication::processEvents(); // Make window appear immediately no matter what
  }
  else {
    // Pinned-down window isn't always on top, so we need to raise it
    show();
    if ( cfg.preferences.raiseWindowOnSearch ) {
      activateWindow();
      raise();
    }
  }

  if ( ui.pinButton->isChecked() ) {
    setWindowTitle( QString( "%1 - GoldenDict-ng" ).arg( elideInputWord() ) );
  }

  /// Too large strings make window expand which is probably not what user
  /// wants
  ui.translateBox->setText( Folding::escapeWildcardSymbols( pendingWord ), false );

  showTranslationFor( pendingWord );
}

QString ScanPopup::elideInputWord()
{
  return pendingWord.size() > 32 ? pendingWord.mid( 0, 32 ) + "..." : pendingWord;
}

void ScanPopup::currentGroupChanged( int )
{
  cfg.lastPopupGroupId          = ui.groupList->getCurrentGroup();
  Instances::Group const * igrp = groups.findGroup( cfg.lastPopupGroupId );
  if ( cfg.lastPopupGroupId == GroupId::AllGroupId ) {
    if ( igrp ) {
      igrp->checkMutedDictionaries( &cfg.popupMutedDictionaries );
    }
    dictionaryBar.setMutedDictionaries( &cfg.popupMutedDictionaries );
  }
  else {
    Config::Group * grp = cfg.getGroup( cfg.lastPopupGroupId );
    if ( grp ) {
      if ( igrp ) {
        igrp->checkMutedDictionaries( &grp->popupMutedDictionaries );
      }
      dictionaryBar.setMutedDictionaries( &grp->popupMutedDictionaries );
    }
    else {
      dictionaryBar.setMutedDictionaries( nullptr );
    }
  }

  updateDictionaryBar();

  definition->setCurrentGroupId( cfg.lastPopupGroupId );

  if ( isVisible() ) {
    updateSuggestionList();
    QString word = Folding::unescapeWildcardSymbols( definition->getWord() );
    showTranslationFor( word );
  }

  cfg.lastPopupGroupId = ui.groupList->getCurrentGroup();
}

void ScanPopup::translateInputChanged( QString const & text )
{
  updateSuggestionList( text );
  GlobalBroadcaster::instance()->translateLineText = text;
}

void ScanPopup::updateSuggestionList()
{
  updateSuggestionList( ui.translateBox->translateLine()->text() );
}

void ScanPopup::updateSuggestionList( QString const & text )
{
  mainStatusBar->clearMessage();

  QString req = text.trimmed();

  if ( !req.size() ) {
    // An empty request always results in an empty result
    wordFinder.cancel();


    return;
  }

  wordFinder.prefixMatch( req, getActiveDicts() );
}

void ScanPopup::translateInputFinished()
{
  pendingWord = Folding::unescapeWildcardSymbols( ui.translateBox->translateLine()->text().trimmed() );
  showTranslationFor( pendingWord );
}

void ScanPopup::showTranslationFor( QString const & word ) const
{
  ui.pronounceButton->setDisabled( true );

  unsigned groupId = ui.groupList->getCurrentGroup();
  definition->showDefinition( word, groupId );
  definition->focus();
}

vector< sptr< Dictionary::Class > > const & ScanPopup::getActiveDicts()
{
  int current = ui.groupList->currentIndex();

  Q_ASSERT( 0 <= current || current <= groups.size() );

  Config::MutedDictionaries const * mutedDictionaries = dictionaryBar.getMutedDictionaries();

  if ( !dictionaryBar.toggleViewAction()->isChecked() || mutedDictionaries == nullptr ) {
    return groups[ current ].dictionaries;
  }

  vector< sptr< Dictionary::Class > > const & activeDicts = groups[ current ].dictionaries;

  // Populate the special dictionariesUnmuted array with only unmuted
  // dictionaries

  dictionariesUnmuted.clear();
  dictionariesUnmuted.reserve( activeDicts.size() );

  for ( const auto & activeDict : activeDicts ) {
    if ( !mutedDictionaries->contains( QString::fromStdString( activeDict->getId() ) ) ) {
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

  updateSuggestionList();
}

bool ScanPopup::eventFilter( QObject * watched, QEvent * event )
{
  if ( watched == ui.translateBox->translateLine() && event->type() == QEvent::FocusIn ) {
    const QFocusEvent * focusEvent = static_cast< QFocusEvent * >( event );

    // select all on mouse click
    if ( focusEvent->reason() == Qt::MouseFocusReason ) {
      QTimer::singleShot( 0, this, &ScanPopup::focusTranslateLine );
    }
    return false;
  }

  if ( mouseIntercepted ) {
    // We're only interested in our events

    if ( event->type() == QEvent::MouseMove ) {
      QMouseEvent * mouseEvent = (QMouseEvent *)event;
      reactOnMouseMove( mouseEvent->globalPosition() );
    }
  }

  if ( event->type() == QEvent::KeyPress && watched != ui.translateBox->translateLine() ) {

    if ( const auto key_event = dynamic_cast< QKeyEvent * >( event ); key_event->modifiers() == Qt::NoModifier ) {
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

  return QMainWindow::eventFilter( watched, event );
}

void ScanPopup::reactOnMouseMove( QPointF const & p )
{
  if ( geometry().contains( p.toPoint() ) ) {
    //        qDebug( "got inside" );

    hideTimer.stop();
    mouseEnteredOnce = true;
    uninterceptMouse();
  }
  else {
    //        qDebug( "outside" );
    // We're in grab mode and outside the window - calculate the
    // distance from it. We might want to hide it.

    // When the mouse has entered once, we don't allow it stayng outside,
    // but we give a grace period for it to return.
    int proximity = mouseEnteredOnce ? 0 : 60;

    // Note: watched == this ensures no other child objects popping out are
    // receiving this event, meaning there's basically nothing under the
    // cursor.
    if ( /*watched == this &&*/
         !frameGeometry().adjusted( -proximity, -proximity, proximity, proximity ).contains( p.toPoint() ) ) {
      // We've way too far from the window -- hide the popup

      // If the mouse never entered the popup, hide the window instantly --
      // the user just moved the cursor further away from the window.

      if ( !mouseEnteredOnce ) {
        hideWindow();
      }
      else {
        hideTimer.start();
      }
    }
  }
}

void ScanPopup::mousePressEvent( QMouseEvent * ev )
{
  // With mouse grabs, the press can occur anywhere on the screen, which
  // might mean hiding the window.

  if ( !frameGeometry().contains( ev->globalPosition().toPoint() ) ) {
    hideWindow();

    return;
  }

  if ( ev->button() == Qt::LeftButton ) {
    startPos = ev->globalPosition();
    setCursor( Qt::ClosedHandCursor );
  }

  QMainWindow::mousePressEvent( ev );
}

void ScanPopup::mouseMoveEvent( QMouseEvent * event )
{
  if ( event->buttons() && cursor().shape() == Qt::ClosedHandCursor ) {
    QPointF newPos = event->globalPosition();
    QPointF delta  = newPos - startPos;

    startPos = newPos;

    // Move the window
    move( ( pos() + delta ).toPoint() );
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
  if ( !ui.pinButton->isChecked() && !geometry().contains( QCursor::pos() )
       && QApplication::mouseButtons() == Qt::NoButton ) {
    hideTimer.start();
  }
}

void ScanPopup::enterEvent( QEnterEvent * event )
{
  QMainWindow::enterEvent( event );

  if ( mouseEnteredOnce ) {
    // We "enter" first time via our event filter. This seems to evade some
    // unexpected behavior under Windows.

    // If there was a countdown to hide the window, stop it.
    hideTimer.stop();
  }
}

void ScanPopup::showEvent( QShowEvent * ev )
{
  QMainWindow::showEvent( ev );

  if ( groups.size() <= 1 ) { // Only the default group? Hide then.
    ui.groupList->hide();
  }

  if ( ui.showDictionaryBar->isChecked() != dictionaryBar.isVisible() ) {
    ui.showDictionaryBar->setChecked( dictionaryBar.isVisible() );
    updateDictionaryBar();
  }
}

void ScanPopup::closeEvent( QCloseEvent * ev )
{
  if ( isVisible() && ui.pinButton->isChecked() ) {
    pinnedGeometry = saveGeometry();
  }

  QMainWindow::closeEvent( ev );
}

void ScanPopup::moveEvent( QMoveEvent * ev )
{
  if ( isVisible() && ui.pinButton->isChecked() ) {
    pinnedGeometry = saveGeometry();
  }

  QMainWindow::moveEvent( ev );
}

void ScanPopup::prefixMatchFinished()
{
  // Check that there's a window there at all
  if ( isVisible() ) {
    if ( wordFinder.getErrorString().size() ) {
      ui.queryError->setToolTip( wordFinder.getErrorString() );
      ui.queryError->show();
      showStatusBarMessage( tr( "WARNING: %1" ).arg( wordFinder.getErrorString() ),
                            20000,
                            QPixmap( ":/icons/error.svg" ) );
    }
    else {
      ui.queryError->hide();
      auto results = wordFinder.getResults();
      QStringList _results;
      for ( auto const & [ fst, snd ] : results ) {
        _results << fst;
      }

      // Reset the noResults mark if it's on right now
      auto translateLine = ui.translateBox->translateLine();

      Utils::Widget::setNoResultColor( translateLine, _results.isEmpty() );
      ui.translateBox->setModel( _results );
    }
  }
}

void ScanPopup::on_pronounceButton_clicked() const
{
  definition->playSound();
}

void ScanPopup::pinButtonClicked( bool checked )
{
  if ( checked ) {
    uninterceptMouse();

    ui.onTopButton->setVisible( true );
    Qt::WindowFlags flags = pinnedWindowFlags;
    if ( ui.onTopButton->isChecked() ) {
      flags |= Qt::WindowStaysOnTopHint;
    }
    setWindowFlags( flags );

#ifdef Q_OS_MACOS
    setAttribute( Qt::WA_MacAlwaysShowToolWindow );
#endif

    setWindowTitle( QString( "%1 - GoldenDict-ng" ).arg( elideInputWord() ) );
    dictionaryBar.setMovable( false );
    hideTimer.stop();
  }
  else {
    ui.onTopButton->setVisible( false );
    dictionaryBar.setMovable( false );
    setWindowFlags( unpinnedWindowFlags() );

#ifdef Q_OS_MACOS
    setAttribute( Qt::WA_MacAlwaysShowToolWindow, false );
#endif

    mouseEnteredOnce = true;
  }
  cfg.pinPopupWindow = checked;

  show();

  if ( checked ) {
    pinnedGeometry = saveGeometry();
  }
}

void ScanPopup::focusTranslateLine()
{
  if ( !isActiveWindow() ) {
    activateWindow();
  }

  ui.translateBox->translateLine()->setFocus();
  ui.translateBox->translateLine()->selectAll();
}

void ScanPopup::stopAudio() const
{
  definition->stopSound();
}

void ScanPopup::on_showDictionaryBar_clicked( bool checked )
{
  dictionaryBar.setVisible( checked );
  updateDictionaryBar();
  definition->updateMutedContents();
}

void ScanPopup::hideTimerExpired()
{
  if ( isVisible() ) {
    hideWindow();
  }
}

void ScanPopup::pageLoaded( ArticleView * ) const
{
  if ( !isVisible() ) {
    return;
  }
  auto pronounceBtn = ui.pronounceButton;
  definition->hasSound( [ pronounceBtn ]( bool has ) {
    if ( pronounceBtn ) {
      pronounceBtn->setDisabled( !has );
    }
  } );

  updateBackForwardButtons();
}

void ScanPopup::showStatusBarMessage( QString const & message, int timeout, QPixmap const & icon ) const
{
  mainStatusBar->showMessage( message, timeout, icon );
}

void ScanPopup::escapePressed()
{
  if ( !definition->closeSearch() ) {
    hideWindow();
  }
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
  definition->clearContent();
}

void ScanPopup::interceptMouse()
{
  if ( !mouseIntercepted ) {
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
  if ( mouseIntercepted ) {
    reactOnMouseMove( QCursor::pos() );
  }
}

void ScanPopup::uninterceptMouse()
{
  if ( mouseIntercepted ) {
    qApp->removeEventFilter( this );
    mouseGrabPollTimer.stop();
    //    releaseMouse();

    mouseIntercepted = false;
  }
}

void ScanPopup::updateDictionaryBar()
{
  if ( !dictionaryBar.toggleViewAction()->isChecked() ) {
    return; // It's not enabled, therefore hidden -- don't waste time
  }

  unsigned currentId           = ui.groupList->getCurrentGroup();
  Instances::Group const * grp = groups.findGroup( currentId );

  if ( grp ) { // Should always be !0, but check as a safeguard
    dictionaryBar.setDictionaries( grp->dictionaries );
  }

  if ( currentId == GroupId::AllGroupId ) {
    dictionaryBar.setMutedDictionaries( &cfg.popupMutedDictionaries );
  }
  else {
    Config::Group * group = cfg.getGroup( currentId );
    dictionaryBar.setMutedDictionaries( group ? &group->popupMutedDictionaries : nullptr );
  }

  setDictionaryIconSize();
}

void ScanPopup::mutedDictionariesChanged()
{
  updateSuggestionList();
  if ( dictionaryBar.toggleViewAction()->isChecked() ) {
    definition->updateMutedContents();
  }
}

void ScanPopup::on_sendWordButton_clicked()
{
  if ( !isVisible() ) {
    return;
  }
  if ( !ui.pinButton->isChecked() ) {
    definition->closeSearch();
    hideWindow();
  }
  emit sendPhraseToMainWindow( definition->getWord() );
}

void ScanPopup::on_sendWordToFavoritesButton_clicked()
{
  if ( !isVisible() ) {
    return;
  }
  unsigned groupId   = ui.groupList->getCurrentGroup();
  auto current_exist = isWordPresentedInFavorites( definition->getTitle(), groupId );
  //if current_exist=false( not exist ),  after click ,the word should be in the favorite which is blueStar
  ui.sendWordToFavoritesButton->setIcon( !current_exist ? blueStarIcon : starIcon );
  emit sendWordToFavorites( definition->getTitle(), cfg.lastPopupGroupId, current_exist );
}

void ScanPopup::switchExpandOptionalPartsMode()
{
  if ( isVisible() ) {
    emit switchExpandMode();
  }
}

void ScanPopup::updateBackForwardButtons() const
{
  ui.goBackButton->setEnabled( definition->canGoBack() );
  ui.goForwardButton->setEnabled( definition->canGoForward() );
}

void ScanPopup::on_goBackButton_clicked() const
{
  definition->back();
}

void ScanPopup::on_goForwardButton_clicked() const
{
  definition->forward();
}

void ScanPopup::setDictionaryIconSize()
{
  if ( cfg.usingToolbarsIconSize == Config::ToolbarsIconSize::Small ) {
    dictionaryBar.setDictionaryIconSize( DictionaryBar::IconSize::Small );
  }
  else if ( cfg.usingToolbarsIconSize == Config::ToolbarsIconSize::Normal ) {
    dictionaryBar.setDictionaryIconSize( DictionaryBar::IconSize::Normal );
  }
  else if ( cfg.usingToolbarsIconSize == Config::ToolbarsIconSize::Large ) {
    dictionaryBar.setDictionaryIconSize( DictionaryBar::IconSize::Large );
  }

  applyWordsZoomLevel();
}


void ScanPopup::setGroupByName( QString const & name ) const
{
  int i;
  for ( i = 0; i < ui.groupList->count(); i++ ) {
    if ( ui.groupList->itemText( i ) == name ) {
      ui.groupList->setCurrentIndex( i );
      break;
    }
  }
  if ( i >= ui.groupList->count() ) {
    qWarning( "Group \"%s\" for popup window is not found", name.toUtf8().data() );
  }
}

void ScanPopup::openSearch()
{
  definition->openSearch();
}

void ScanPopup::alwaysOnTopClicked( bool checked )
{
  bool wasVisible = isVisible();
  if ( ui.pinButton->isChecked() ) {
    Qt::WindowFlags flags = this->windowFlags();
    if ( checked ) {
      setWindowFlags( flags | Qt::WindowStaysOnTopHint );
    }
    else {
      setWindowFlags( flags ^ Qt::WindowStaysOnTopHint );
    }
    if ( wasVisible ) {
      show();
    }
  }
}

void ScanPopup::titleChanged( ArticleView *, QString const & title ) const
{
  unsigned groupId = ui.groupList->getCurrentGroup();

  // Set icon for "Add to Favorites" button
  ui.sendWordToFavoritesButton->setIcon( isWordPresentedInFavorites( title, groupId ) ? blueStarIcon : starIcon );
}

bool ScanPopup::isWordPresentedInFavorites( QString const & word, unsigned groupId ) const
{
  QString folder = GlobalBroadcaster::instance()->groupFolderMap[ groupId ];

  return GlobalBroadcaster::instance()->folderFavoritesMap[ folder ].contains( word );
}

#ifdef HAVE_X11
void ScanPopup::showScanFlag()
{
  scanFlag->showScanFlag();
}

void ScanPopup::hideScanFlag()
{
  scanFlag->hideWindow();
}
#endif
