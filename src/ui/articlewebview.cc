/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "articlewebpage.hh"
#include "articlewebview.hh"
#include "version.hh"
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEnginePage>
#include <QWebEngineView>
#include <QApplication>
#include <QTimer>
#include <QDialog>
#include <QDesktopServices>
#include <QMainWindow>
#include <QSet>
#include <functional>
#include <memory>
#include <utility>
#ifdef Q_OS_WIN32
  #include <qt_windows.h>
#endif

namespace {

/// Schemes the Resource Viewer is meant to display: resources referenced by an
/// article (images, stylesheets, audio/video, built-in and local files).
bool isResourceViewerUrl( const QUrl & url )
{
  static const QSet< QString > schemes = { "bres", "gico", "qrc", "qrcx", "file", "data", "gdau", "gdvideo" };
  return schemes.contains( url.scheme() );
}

/// Page of the Resource Viewer windows. Only resource navigations are loaded;
/// anything else (dictionary links, web pages, ...) is reported back and
/// rejected, so a window opened from the inspector never just stays blank.
class ResourceViewerPage: public QWebEnginePage
{
public:
  using UnsupportedHandler = std::function< void( const QUrl & ) >;

  ResourceViewerPage( UnsupportedHandler handler, QObject * parent ):
    QWebEnginePage( parent ),
    unsupportedHandler( std::move( handler ) )
  {
  }

protected:
  bool acceptNavigationRequest( const QUrl & url, NavigationType, bool isMainFrame ) override
  {
    if ( !isMainFrame || isResourceViewerUrl( url ) || url.scheme().isEmpty() || url.scheme() == "about" ) {
      return true;
    }

    unsupportedHandler( url );
    return false;
  }

private:
  UnsupportedHandler unsupportedHandler;
};

} // namespace

ArticleWebView::ArticleWebView( QWidget * parent ):
  QWebEngineView( parent ),
  midButtonPressed( false ),
  selectionBySingleClick( false ),
  singleClickToDbClick( false ),
  dbClicked( false )
{
  auto page = new ArticleWebPage( this );
  connect( page, &ArticleWebPage::linkClicked, this, &ArticleWebView::linkClicked );
  connect( page, &QWebEnginePage::linkHovered, this, &ArticleWebView::linkHovered );
  this->setPage( page );

  QWebEngineScript script;
  script.setName( QStringLiteral( "GoldenDictInfo" ) );
  script.setSourceCode(
    QStringLiteral( "window.__DICT__ = { name: 'GoldenDict', version: '%1' };" ).arg( Version::version() ) );
  script.setInjectionPoint( QWebEngineScript::DocumentCreation );
  script.setWorldId( QWebEngineScript::MainWorld );
  script.setRunsOnSubFrames( true );
  page->scripts().insert( script );
}

void ArticleWebView::setUp( Config::Class * _cfg )
{
  this->cfg = _cfg;
  setZoomFactor( _cfg->preferences.zoomFactor );
}

void ArticleWebView::updatePreferredColorSchemeScript( bool isDark )
{
  static const QString name = QStringLiteral( "GdPreferredColorScheme" );
  // QWebEnginePage::scripts() returns a reference to the page's collection.
  QWebEngineScriptCollection & scripts = page()->scripts();

  // Remove any previously installed copy so the page never carries a stale
  // script across theme changes.
  for ( const auto & existing : scripts.find( name ) ) {
    scripts.remove( existing );
  }

  // In dark mode GoldenDict-ng already injects and force-enables Dark Reader
  // (which sets color-scheme:dark itself), so no override is needed there.
  // The script below is a light-mode-only fix: it forces the embedded view to
  // perceive a light scheme, overriding an OS-level dark appearance -- exactly
  // the scenario of issue #3077.
  if ( isDark ) {
    qDebug() << "PCS: isDark" << isDark << "-- script not registered";
    return;
  }

  qDebug() << "PCS: isDark" << isDark << "-- registering via sourceUrl";

  QWebEngineScript script;
  script.setName( name );
  // Refer to the script by URL instead of embedding its source: it stays a
  // standalone file under src/scripts/ (via qrc) and shows up as a named file
  // in DevTools.
  script.setSourceUrl( QUrl( "qrc:///scripts/gd-preferred-color-scheme.js" ) );
  script.setInjectionPoint( QWebEngineScript::DocumentCreation );
  script.setWorldId( QWebEngineScript::MainWorld );
  script.setRunsOnSubFrames( true );
  scripts.insert( script );
}

void ArticleWebView::setPopup( bool isPopup )
{
  if ( auto page = qobject_cast< ArticleWebPage * >( this->page() ) ) {
    page->setPopup( isPopup );
  }
}

QWebEngineView * ArticleWebView::createWindow( QWebEnginePage::WebWindowType type )
{
  if ( type == QWebEnginePage::WebWindowType::WebDialog || type == QWebEnginePage::WebWindowType::WebBrowserTab
       || type == QWebEnginePage::WebWindowType::WebBrowserWindow ) {
    auto dlg  = new QMainWindow( this );
    auto view = new QWebEngineView( dlg );
    dlg->setAttribute( Qt::WA_DeleteOnClose );
    dlg->setObjectName( "ResourceViewer" );
    dlg->setCentralWidget( view );
    dlg->resize( 800, 600 );
    dlg->setWindowTitle( tr( "GoldenDict Resource Viewer" ) );

    auto dispatched = std::make_shared< bool >( false );

    // Navigation the viewer cannot display. Dictionary links are handled as if
    // they were clicked inside an article, everything else goes to the desktop
    // handler. The (still empty) window is discarded either way.
    auto handleUnsupported = [ this, dlg, dispatched ]( const QUrl & url ) {
      if ( *dispatched || url.scheme().isEmpty() || url.scheme() == "about" ) {
        return;
      }
      *dispatched = true;
      dlg->deleteLater();

      if ( url.scheme() == "gdlookup" || url.scheme() == "bword" || url.scheme() == "entry" ) {
        // Emit asynchronously: the article view may rebuild its tabs in
        // response, and we are still inside a navigation callback here.
        QTimer::singleShot( 0, this, [ this, url ]() { emit linkClicked( url ); } );
      }
      else if ( url.scheme() == "http" || url.scheme() == "https" || url.scheme() == "ftp"
                 || url.scheme() == "mailto" ) {
        QDesktopServices::openUrl( url );
      }
    };

    auto page = new ResourceViewerPage( handleUnsupported, view );
    view->setPage( page );

    // Only reveal the window once a resource actually loaded, so rejected or
    // failed navigations never leave a blank window on screen.
    connect( view, &QWebEngineView::loadFinished, dlg,
             [ view, dlg, dispatched, handleUnsupported ]( bool ok ) {
               if ( *dispatched ) {
                 return;
               }
               const QUrl loaded = view->url();
               if ( isResourceViewerUrl( loaded ) ) {
                 if ( ok ) {
                   dlg->show();
                 }
                 else {
                   dlg->deleteLater();
                 }
                 return;
               }
               if ( loaded.scheme().isEmpty() || loaded.scheme() == "about" ) {
                 dlg->deleteLater();
                 return;
               }
               handleUnsupported( loaded );
             } );

    // When Qt adopts the target URL directly instead of asking
    // acceptNavigationRequest (the initial navigation of a new window), the
    // scheme still has to be checked here.
    connect( view, &QWebEngineView::urlChanged, dlg, [ handleUnsupported ]( const QUrl & url ) {
      if ( !isResourceViewerUrl( url ) ) {
        handleUnsupported( url );
      }
    } );

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

void ArticleWebView::linkHovered( const QString & link )
{
  hoveredLink = link;
}

bool ArticleWebView::eventFilter( QObject * obj, QEvent * ev )
{
  // Return/Enter with a non-empty selection looks the selected text up. With no
  // selection the key keeps its normal browser behaviour (activating a focused
  // link, submitting a form field, ...). ShortcutOverride is handled as well so
  // that a parent QAction cannot steal the key before the KeyPress arrives.
  if ( ev->type() == QEvent::ShortcutOverride || ev->type() == QEvent::KeyPress ) {
    auto * keyEvent = static_cast< QKeyEvent * >( ev );
    // Check the key first -- selectedText() is a synchronous call into the
    // renderer, so only pay for it when the key is actually of interest.
    if ( ( keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter )
         && !selectedText().trimmed().isEmpty() ) {
      const Qt::KeyboardModifiers mods = keyEvent->modifiers();
      // Only bare Enter and Ctrl+Enter belong to us. Shift/Alt/Meta variants
      // are left alone (Qt::Key_Enter from the keypad carries KeypadModifier).
      if ( !mods.testFlag( Qt::ShiftModifier ) && !mods.testFlag( Qt::AltModifier )
           && !mods.testFlag( Qt::MetaModifier ) ) {
        if ( ev->type() == QEvent::KeyPress ) {
          emit searchSelectedText( mods.testFlag( Qt::ControlModifier ) );
        }
        ev->accept();
        return true;
      }
    }
  }

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
      dbClicked = false;
      // A press on a link is a plain link click, not the start of a
      // single-click-to-select-word action.
      if ( !hoveredLink.isEmpty() ) {
        singleClickToDbClick = false;
      }
      else {
        singleClickToDbClick = true;
        QTimer::singleShot( QApplication::doubleClickInterval(), this, [ = ]() {
          singleClickAction();
        } );
      }
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

void ArticleWebView::singleClickAction()
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
  for ( auto * child : childrens ) {
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
    unsigned nLines = 0;
    SystemParametersInfo( SPI_GETWHEELSCROLLLINES, 0, &nLines, 0 );
    if ( nLines == WHEEL_PAGESCROLL ) {
      QKeyEvent kev( QEvent::KeyPress, ev->angleDelta().y() > 0 ? Qt::Key_PageUp : Qt::Key_PageDown, Qt::NoModifier );
      auto childrens = this->children();
      for ( auto * child : childrens ) {
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
