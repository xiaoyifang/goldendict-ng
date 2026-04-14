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
  // Disconnect previous connections
  if ( currentPage ) {
    disconnect( currentPage, nullptr, this, nullptr );
  }

  viewContainer->page()->setInspectedPage( page );
  currentPage = page;

  if ( !page ) {
    qDebug() << "reset inspector";
    return;
  }

  // Connect to page destroyed signal
  connect( page, &QObject::destroyed, this, [this]() {
    qDebug() << "Inspected page destroyed, closing inspector";
    close();
  });

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
  // Disconnect from the page
  if ( currentPage ) {
    disconnect( currentPage, nullptr, this, nullptr );
    currentPage = nullptr;
  }
  viewContainer->page()->setInspectedPage( nullptr );
}
