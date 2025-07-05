/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "articlewebpage.hh"
#include "articlewebview.hh"
#include <QMouseEvent>
#include <QWebEngineView>
#include <QApplication>
#include <QTimer>
#include <QDialog>
#include <QMainWindow>
#ifdef Q_OS_WIN32
  #include <qt_windows.h>
#endif

ArticleWebView::ArticleWebView( QWidget * parent ):
  QWebEngineView( parent ),
  midButtonPressed( false ),
  selectionBySingleClick( false )
{
  auto page = new ArticleWebPage( this );
  connect( page, &ArticleWebPage::linkClicked, this, &ArticleWebView::linkClicked );
  this->setPage( page );
}

void ArticleWebView::setUp( Config::Class * _cfg )
{
  this->cfg = _cfg;
  setZoomFactor( _cfg->preferences.zoomFactor );
}

QWebEngineView * ArticleWebView::createWindow( QWebEnginePage::WebWindowType type )
{
  if ( type == QWebEnginePage::WebWindowType::WebDialog ) {
    auto dlg  = new QMainWindow( this );
    auto view = new QWebEngineView( dlg );
    dlg->setCentralWidget( view );
    dlg->resize( 400, 400 );
    dlg->show();

    return view;
  }
  return QWebEngineView::createWindow( type );
}

bool ArticleWebView::event( QEvent * event )
{
  if ( event->type() == QEvent::ChildAdded ) {
    auto child_ev = dynamic_cast< QChildEvent * >( event );
    child_ev->child()->installEventFilter( this );
  }

  return QWebEngineView::event( event );
}

void ArticleWebView::linkClickedInHtml( const QUrl & )
{
  //disable single click to simulate dbclick action on the new loaded pages.
  singleClickToDbClick = false;
}

bool ArticleWebView::eventFilter( QObject * obj, QEvent * ev )
{
  if ( ev->type() == QEvent::MouseButtonDblClick ) {
    auto pe = dynamic_cast< QMouseEvent * >( ev );
    if ( Qt::MouseEventSynthesizedByApplication != pe->source() ) {
      singleClickToDbClick = false;
      dbClicked            = true;
    }
  }
  if ( ev->type() == QEvent::MouseMove ) {
    singleClickToDbClick = false;
  }
  if ( ev->type() == QEvent::MouseButtonPress ) {
    auto pe = dynamic_cast< QMouseEvent * >( ev );
    if ( pe->button() == Qt::LeftButton ) {
      singleClickToDbClick = true;
      dbClicked            = false;
      QTimer::singleShot( QApplication::doubleClickInterval(), this, [ = ]() {
        singleClickAction( pe );
      } );
    }
    if ( pe->buttons() & Qt::MiddleButton ) {
      midButtonPressed = true;
      QTimer::singleShot( 100, this, [ = ]() {
        sendCustomMouseEvent( QEvent::MouseButtonPress );
        sendCustomMouseEvent( QEvent::MouseButtonRelease );
      } );
      return false;
    }
  }
  if ( ev->type() == QEvent::MouseButtonRelease ) {
    auto pe = dynamic_cast< QMouseEvent * >( ev );
    mouseReleaseEvent( pe );
    if ( dbClicked ) {
      //emit the signal after button release.emit earlier(in MouseButtonDblClick event) can not get selected text;
      doubleClickAction( pe );
    }
  }
  if ( ev->type() == QEvent::Wheel ) {
    auto pe = dynamic_cast< QWheelEvent * >( ev );
    wheelEvent( pe );

    if ( pe->modifiers().testFlag( Qt::ControlModifier ) ) {
      return true;
    }
  }

  return QWebEngineView::eventFilter( obj, ev );
}

void ArticleWebView::singleClickAction( QMouseEvent * event )
{
  if ( !singleClickToDbClick ) {
    return;
  }

  if ( selectionBySingleClick ) {
    findText( "" ); // clear the selection first, if any
    //send dbl click event twice? send one time seems not work .weird really.  need further investigate.
    sendCustomMouseEvent( QEvent::MouseButtonDblClick );
    sendCustomMouseEvent( QEvent::MouseButtonDblClick );
  }
}

void ArticleWebView::sendCustomMouseEvent( QEvent::Type type )
{
  QPoint pt = mapFromGlobal( QCursor::pos() );
  QMouseEvent ev( type,
                  pt,
                  pt,
                  QCursor::pos(),
                  Qt::LeftButton,
                  Qt::LeftButton,
                  Qt::NoModifier,
                  Qt::MouseEventSynthesizedByApplication );

  auto childrens = this->children();
  for ( auto child : childrens ) {
    QApplication::sendEvent( child, &ev );
  }
}

void ArticleWebView::doubleClickAction( QMouseEvent * event )
{
  if ( Qt::MouseEventSynthesizedByApplication != event->source() ) {
    emit doubleClicked( event->pos() );
  }
}

void ArticleWebView::wheelEvent( QWheelEvent * ev )
{
#ifdef Q_OS_WIN32

  // Avoid wrong mouse wheel handling in QWebEngineView
  // if system preferences is set to "scroll by page"

  if ( ev->modifiers() == Qt::NoModifier ) {
    unsigned nLines;
    SystemParametersInfo( SPI_GETWHEELSCROLLLINES, 0, &nLines, 0 );
    if ( nLines == WHEEL_PAGESCROLL ) {
      QKeyEvent kev( QEvent::KeyPress, ev->angleDelta().y() > 0 ? Qt::Key_PageUp : Qt::Key_PageDown, Qt::NoModifier );
      auto childrens = this->children();
      for ( auto child : childrens ) {
        QApplication::sendEvent( child, &kev );
      }

      ev->accept();
      return;
    }
  }
#endif

  if ( ev->modifiers().testFlag( Qt::ControlModifier ) ) {
    ev->ignore();
  }
  else {
    QWebEngineView::wheelEvent( ev );
  }
}
