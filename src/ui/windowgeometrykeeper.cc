/* This file is part of GoldenDict.
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "windowgeometrykeeper.hh"

#include <QApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QWindow>
#include <QWidget>
#include <utility>

WindowGeometryKeeper::WindowGeometryKeeper( QWidget * window, SizeConstraint sizeConstraint, PostRestore postRestore ):
  QObject( window ),
  window( window ),
  sizeConstraint( std::move( sizeConstraint ) ),
  postRestore( std::move( postRestore ) )
{
  lastStableNormalSize = window->normalGeometry().size();
  if ( lastStableNormalSize.isEmpty() ) {
    lastStableNormalSize = window->size();
  }

  normalSizeTimer.setSingleShot( true );
  normalSizeTimer.setInterval( 200 );
  screenChangeTimer.setSingleShot( true );
  screenChangeTimer.setInterval( 150 );

  connect( &normalSizeTimer, &QTimer::timeout, this, &WindowGeometryKeeper::commitNormalSize );
  connect( &screenChangeTimer, &QTimer::timeout, this, &WindowGeometryKeeper::restoreNormalSizeAfterScreenChange );
  connect( qApp, &QGuiApplication::screenAdded, this, [ this ]( QScreen * screen ) {
    watchScreen( screen );
    scheduleScreenGeometryRestore();
  } );
  connect( qApp, &QGuiApplication::screenRemoved, this, &WindowGeometryKeeper::scheduleScreenGeometryRestore );

  for ( auto * screen : QGuiApplication::screens() ) {
    watchScreen( screen );
  }

  window->installEventFilter( this );
  connectWindowScreenChange();

  // Fit the window to the screen it was restored onto before any resize can be
  // mistaken for a user chosen size.
  handlingScreenChange = true;
  restoreNormalSizeAfterScreenChange();
}

void WindowGeometryKeeper::prepareForSave()
{
  if ( handlingScreenChange ) {
    screenChangeTimer.stop();
    restoreNormalSizeAfterScreenChange();
  }
}

bool WindowGeometryKeeper::eventFilter( QObject * watched, QEvent * event )
{
  if ( watched == window ) {
    switch ( event->type() ) {
      case QEvent::Show:
        connectWindowScreenChange();
        scheduleScreenGeometryRestore();
        break;
      case QEvent::Resize:
        rememberNormalSize();
        break;
      case QEvent::WindowStateChange:
        if ( !window->isMinimized() && !window->isMaximized() && !window->isFullScreen() ) {
          scheduleScreenGeometryRestore();
        }
        break;
      default:
        break;
    }
  }

  return QObject::eventFilter( watched, event );
}

void WindowGeometryKeeper::watchScreen( QScreen * screen )
{
  // UniqueConnection keeps re-added screens from stacking duplicate handlers.
  connect( screen,
           &QScreen::availableGeometryChanged,
           this,
           &WindowGeometryKeeper::scheduleScreenGeometryRestore,
           Qt::UniqueConnection );
}

void WindowGeometryKeeper::connectWindowScreenChange()
{
  if ( window->windowHandle() ) {
    // Show is emitted more than once and the handle may be recreated, so only
    // keep a single connection to the current handle.
    connect( window->windowHandle(),
             &QWindow::screenChanged,
             this,
             &WindowGeometryKeeper::scheduleScreenGeometryRestore,
             Qt::UniqueConnection );
  }
}

void WindowGeometryKeeper::rememberNormalSize()
{
  if ( handlingScreenChange || window->isMinimized() || window->isMaximized() || window->isFullScreen()
       || window->size().isEmpty() ) {
    return;
  }

  pendingNormalSize = window->size();
  normalSizeTimer.start();
}

void WindowGeometryKeeper::commitNormalSize()
{
  // pendingNormalSize is only recorded in normal state, so it stays valid even
  // if the timer fires after the window has been maximized or minimized.
  if ( !handlingScreenChange && !pendingNormalSize.isEmpty() ) {
    lastStableNormalSize = pendingNormalSize;
  }
  pendingNormalSize = QSize();
}

void WindowGeometryKeeper::scheduleScreenGeometryRestore()
{
  normalSizeTimer.stop();
  pendingNormalSize    = QSize();
  handlingScreenChange = true;
  screenChangeTimer.start();
}

void WindowGeometryKeeper::restoreNormalSizeAfterScreenChange()
{
  normalSizeTimer.stop();
  pendingNormalSize = QSize();

  // Applying a size while minimized, maximized or full screen would drop that
  // state, so wait until the window is back to normal.
  if ( window->isMinimized() || window->isMaximized() || window->isFullScreen() ) {
    handlingScreenChange = false;
    return;
  }

  QSize targetSize = lastStableNormalSize;
  QScreen * screen = window->windowHandle() ? window->windowHandle()->screen() : QGuiApplication::primaryScreen();

  if ( sizeConstraint ) {
    targetSize = sizeConstraint( targetSize, screen );
  }
  else if ( screen ) {
    targetSize = targetSize.boundedTo( screen->availableGeometry().size() );
  }

  if ( !targetSize.isEmpty() && targetSize != window->size() ) {
    window->resize( targetSize );
  }

  // Run even without a resize: the size constraint may already have changed
  // the window, leaving it partly outside the available area.
  if ( postRestore ) {
    postRestore( screen );
  }

  handlingScreenChange = false;
}
