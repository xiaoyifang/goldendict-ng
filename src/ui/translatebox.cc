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

namespace
{
#define MAX_POPUP_ROWS 17
}


TranslateBox::TranslateBox( QWidget * parent ):
  QWidget( parent ),
  translate_line( new QLineEdit( this ) ),
  m_popupEnabled( false )
{

  completer = new QCompleter( words, this );
  resize(200, 90);
  QSizePolicy sizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
  sizePolicy.setHorizontalStretch(0);
  sizePolicy.setVerticalStretch(0);
  setSizePolicy(sizePolicy);
  // setMinimumSize(QSize(800, 0));

  setFocusProxy(translate_line);
  translate_line->setObjectName("translateLine");
  translate_line->setPlaceholderText( tr( "Type a word or phrase to search dictionaries" ) );


  QHBoxLayout *layout = new QHBoxLayout(this);
  setLayout(layout);
  layout->setContentsMargins(0,0,0,0);
  layout->addWidget(translate_line);

  QAction * dropdown = new QAction( QIcon(":/icons/1downarrow.svg"), tr("Drop-down"),this);
  connect( dropdown,&QAction::triggered,this, &TranslateBox::rightButtonClicked );

  translate_line->addAction( dropdown,QLineEdit::TrailingPosition);
  translate_line->addAction( new QAction(QIcon(":/icons/system-search.svg"),"",this),QLineEdit::LeadingPosition);

  translate_line->setFocusPolicy(Qt::ClickFocus);

  translate_line->installEventFilter( this );
  installEventFilter( this );

  translate_line->setCompleter( completer );
  completer->setCompletionMode( QCompleter::UnfilteredPopupCompletion );

  connect( completer, QOverload< const QString & >::of( &QCompleter::activated ), [ = ]( const QString & text ) {
    emit translate_line->returnPressed();
  } );
}

void TranslateBox::setText( QString text, bool showPopup )
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
  if ( translate_line )
    translate_line->setSizePolicy( policy );
}

void TranslateBox::setModel( QStringList & words )
{
  QStringListModel * model = (QStringListModel *)( completer->model() );

  model->setStringList( words );
}

void TranslateBox::showPopup()
{
  //todo,hide ,or show the popup.
}

QLineEdit * TranslateBox::translateLine()
{
  return translate_line;
}

QWidget * TranslateBox::completerWidget()
{
  return completer->widget();
}

void TranslateBox::rightButtonClicked()
{
  setPopupEnabled( !m_popupEnabled );
}

void TranslateBox::onTextEdit()
{
  if ( translate_line->hasFocus() )
    setPopupEnabled( true );
}
