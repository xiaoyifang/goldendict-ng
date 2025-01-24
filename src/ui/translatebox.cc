/* This file is (c) 2012 Tvangeste <i.4m.l33t@yandex.ru>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "translatebox.hh"

#include <QAction>
#include <QHBoxLayout>
#include <QEvent>
#include <QKeyEvent>
#include <QApplication>
#include <QModelIndex>
#include <QScrollBar>
#include <QStyle>
#include <QStringListModel>
#include <QTimer>

TranslateBox::TranslateBox( QWidget * parent ):
  QWidget( parent ),
  translate_line( new QLineEdit( this ) ),
  m_popupEnabled( false )
{
  completer = new QCompleter( words, this );
  resize( 200, 90 );
  QSizePolicy sizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
  setSizePolicy( sizePolicy );

  setFocusProxy( translate_line );
  translate_line->setObjectName( "translateLine" );
  translate_line->setPlaceholderText( tr( "Type a word or phrase to search dictionaries" ) );

  auto layout = new QHBoxLayout( this );
  setLayout( layout );
  layout->setContentsMargins( 0, 0, 0, 0 );
  layout->addWidget( translate_line );

  auto dropdown = new QAction( QIcon( ":/icons/1downarrow.svg" ), tr( "Drop-down" ), this );
  connect( dropdown, &QAction::triggered, this, &TranslateBox::rightButtonClicked );

  translate_line->addAction( dropdown, QLineEdit::TrailingPosition );
  translate_line->addAction( new QAction( QIcon( ":/icons/system-search.svg" ), "", this ),
                             QLineEdit::LeadingPosition );

  translate_line->setFocusPolicy( Qt::ClickFocus );

  translate_line->installEventFilter( this );

  translate_line->setCompleter( completer );
  completer->setCompletionMode( QCompleter::UnfilteredPopupCompletion );
  completer->setMaxVisibleItems( 16 );
  completer->popup()->setMinimumHeight( 256 );

  connect( translate_line, &QLineEdit::returnPressed, [ this ]() {
    emit returnPressed();
  } );
}

void TranslateBox::setText( const QString & text, bool showPopup )
{
  setPopupEnabled( showPopup );
  translate_line->setText( text );
}

void TranslateBox::setPopupEnabled( bool enable )
{
  m_popupEnabled = enable;
  showPopup();
}

void TranslateBox::setSizePolicy( QSizePolicy policy )
{
  QWidget::setSizePolicy( policy );
  if ( translate_line ) {
    translate_line->setSizePolicy( policy );
  }
}

void TranslateBox::setModel( QStringList & _words )
{
  disconnect( completer, QOverload< const QString & >::of( &QCompleter::activated ), translate_line, nullptr );
  const auto model = static_cast< QStringListModel * >( completer->model() );

  model->setStringList( _words );

  completer->popup()->scrollToTop();

  connect( completer,
           QOverload< const QString & >::of( &QCompleter::activated ),
           translate_line,
           [ & ]( const QString & text ) {
             translate_line->setText( text );
             emit returnPressed();
           } );
}

void TranslateBox::showPopup()
{
  if ( m_popupEnabled ) {
    completer->popup()->show();
    completer->complete();
  }
  else {
    completer->popup()->hide();
  }
}

QLineEdit * TranslateBox::translateLine()
{
  return translate_line;
}

QWidget * TranslateBox::completerWidget()
{
  return completer->popup();
}

void TranslateBox::rightButtonClicked()
{
  setPopupEnabled( !m_popupEnabled );
}
void TranslateBox::setSizePolicy( QSizePolicy::Policy hor, QSizePolicy::Policy ver )
{
  setSizePolicy( QSizePolicy( hor, ver ) );
}
