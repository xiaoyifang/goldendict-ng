#include "ftssearchpanel.hh"
#include <QHBoxLayout>

FtsSearchPanel::FtsSearchPanel( QWidget * parent ):
  QWidget( parent )
{

  auto * layout = new QHBoxLayout( this );
  previous      = new QPushButton( this );
  next          = new QPushButton( this );
  statusLabel   = new QLabel( this );

  layout->addWidget( previous );
  layout->addWidget( next );
  layout->addWidget( statusLabel );

  previous->setIcon( QIcon( ":/icons/previous.svg" ) );
  next->setIcon( QIcon( ":/icons/next.svg" ) );

  previous->setText( tr( "&Previous" ) );
  next->setText( tr( "&Next" ) );

  layout->addStretch();
}
