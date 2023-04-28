#include "searchpanel.hh"
#include <QLabel>
#include <QVBoxLayout>

SearchPanel::SearchPanel( QWidget * parent ): QWidget( parent )
{
  lineEdit = new QLineEdit( this );

  close = new QPushButton( this );
  close->setIcon( QIcon( ":/icons/closetab.svg" ) );

  previous = new QPushButton( this );
  previous->setIcon( QIcon( ":/icons/previous.svg" ) );
  previous->setText( tr( "&Previous" ) );
  previous->setShortcut( QKeySequence( tr( "Ctrl+Shift+G" ) ) );

  next = new QPushButton( this );
  next->setIcon( QIcon( ":/icons/next.svg" ) );
  next->setText( tr( "&Next" ) );
  next->setShortcut( QKeySequence( tr( "Ctrl+G" ) ) );

  highlightAll = new QCheckBox( this );
  highlightAll->setIcon( QIcon( ":/icons/highlighter.png" ) );
  highlightAll->setText( tr( "Highlight &all" ) );
  highlightAll->setChecked( true );

  caseSensitive = new QCheckBox( this );
  caseSensitive->setText( tr( "&Case Sensitive" ) );

  auto * searchLabel = new QLabel( tr( "Find:" ) );

  auto * editRow = new QHBoxLayout(); // parent will be set in layout->addLayout.
  editRow->addWidget( searchLabel );
  editRow->addWidget( lineEdit );
  editRow->addWidget( close );

  auto * buttonsRow = new QHBoxLayout();
  buttonsRow->addWidget( previous );
  buttonsRow->addWidget( next );
  buttonsRow->addWidget( highlightAll );
  buttonsRow->addWidget( caseSensitive );
  buttonsRow->addStretch();

  auto * layout = new QVBoxLayout( this );
  layout->addLayout( editRow );
  layout->addLayout( buttonsRow );
}
