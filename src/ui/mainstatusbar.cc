/* This file is (c) 2012 Tvangeste <i.4m.l33t@yandex.ru>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "mainstatusbar.hh"

#include <QFrame>
#include <QVBoxLayout>
#include <QApplication>
#include <QShowEvent>

MainStatusBar::MainStatusBar( QWidget * parent ):
  QWidget( parent ),
  timer( new QTimer( this ) )
{
  textWidget = new QLabel( QString(), this );
  textWidget->setObjectName( "text" );
  textWidget->setFont( QApplication::font( "QStatusBar" ) );
  textWidget->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );

  picWidget = new QLabel( QString(), this );
  picWidget->setObjectName( "icon" );
  picWidget->setPixmap( QPixmap() );
  picWidget->setScaledContents( true );
  picWidget->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Ignored );


  timer->setSingleShot( true );

  // layout
  const auto layout = new QHBoxLayout;
  layout->setSpacing( 4 );
  layout->setSizeConstraint( QLayout::SetFixedSize );
  layout->setAlignment( Qt::AlignLeft | Qt::AlignVCenter );
  layout->setContentsMargins( 5, 5, 5, 5 );
  layout->addWidget( picWidget );
  layout->addWidget( textWidget );
  setLayout( layout );

  if ( parentWidget() ) {
    parentWidget()->installEventFilter( this );
  }

  connect( timer, &QTimer::timeout, this, &MainStatusBar::clearMessage );

  setAutoFillBackground( true );

  hide();
}

void MainStatusBar::updatePosition()
{
  if ( parentWidget() ) {
    raise();
    int offsetX = 2;
    int offsetY = 2;
    move( offsetX, parentWidget()->height() - height() - offsetY );
  }
}

void MainStatusBar::showEvent( QShowEvent * event )
{
  QWidget::showEvent( event );
  updatePosition();
}

void MainStatusBar::clearMessage()
{
  message.clear();
  textWidget->setText( backgroundMessage );
  picWidget->setPixmap( QPixmap() );
  timer->stop();
  hide();
}

QString MainStatusBar::currentMessage() const
{
  return message;
}

void MainStatusBar::setBackgroundMessage( const QString & bkg_message )
{
  show();
  backgroundMessage = bkg_message;
  if ( message.isEmpty() ) {
    textWidget->setText( backgroundMessage );
  }
}

void MainStatusBar::showMessage( const QString & str, int timeout, const QPixmap & pixmap )
{
  show();
  textWidget->setText( message = str );
  picWidget->setPixmap( pixmap );

  if ( !picWidget->pixmap().isNull() ) {
    int iconSize = fontMetrics().height() - 2;
    picWidget->setFixedSize( iconSize, iconSize );
  }
  else {
    picWidget->setFixedSize( 0, 0 );
  }

  if ( timeout > 0 ) {
    timer->start( timeout );
  }

  updatePosition();
}

void MainStatusBar::mousePressEvent( QMouseEvent * )
{
  clearMessage();
}

bool MainStatusBar::eventFilter( QObject *, QEvent * e )
{
  switch ( e->type() ) {
    case QEvent::Resize:
      if ( isVisible() ) {
        updatePosition();
      }
      break;
    default:
      break;
  }

  return false;
}
