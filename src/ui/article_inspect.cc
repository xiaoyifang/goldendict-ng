#include "article_inspect.hh"
#include <QCloseEvent>
#include <QWebEngineContextMenuRequest>
ArticleInspector::ArticleInspector( QWidget * parent ):
  QWidget( parent, Qt::WindowType::Window )
{
  setWindowTitle( tr( "Inspect" ) );
  setAttribute( Qt::WidgetAttribute::WA_DeleteOnClose, false );
  QVBoxLayout * v = new QVBoxLayout( this );
  v->setSpacing( 0 );
  v->setContentsMargins( 0, 0, 0, 0 );
  viewContainer = new QWebEngineView( this );
  v->addWidget( viewContainer );

  setInspectPage( nullptr );
  resize( 800, 600 );
}

void ArticleInspector::setInspectPage( QWebEnginePage * page )
{
  viewContainer->page()->setInspectedPage( page );

  if ( !page ) {
    qDebug() << "reset inspector";
    return;
  }

  raise();
  show();
}

void ArticleInspector::triggerAction( QWebEnginePage * page )
{
  setInspectPage( page );

  if ( !page ) {
    return;
  }

  page->triggerAction( QWebEnginePage::InspectElement );
}

void ArticleInspector::closeEvent( QCloseEvent * )
{
  viewContainer->page()->setInspectedPage( nullptr );
}
