/* This file is (c) 2012 Tvangeste <i.4m.l33t@yandex.ru>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "mainstatusbar.hh"

#include <QApplication>

MainStatusBar::MainStatusBar( QWidget *parent ) : QStatusBar( parent )
{
  textWidget = new QLabel( QString(), this );
  textWidget->setObjectName( "text" );
  textWidget->setFont( QApplication::font( "QStatusBar" ) );
  textWidget->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
  textWidget->setWindowFlag( Qt::FramelessWindowHint ); // No frame
  textWidget->setAttribute( Qt::WA_NoSystemBackground ); // No background
  textWidget->setAttribute( Qt::WA_TranslucentBackground );

  picWidget = new QLabel( QString(), this );
  picWidget->setObjectName( "icon" );
  picWidget->setPixmap( QPixmap() );
  picWidget->setScaledContents( true );
  picWidget->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Ignored );
  picWidget->setFixedSize( textWidget->height(), textWidget->height() );
  picWidget->setWindowFlag( Qt::FramelessWindowHint ); // No frame
  picWidget->setAttribute( Qt::WA_NoSystemBackground ); // No background
  picWidget->setAttribute( Qt::WA_TranslucentBackground );
  addWidget( picWidget, 1 );
  addWidget( textWidget, 1 );
  timer = new QTimer( this );
  timer->setSingleShot( true );

  connect( timer, &QTimer::timeout, this, &MainStatusBar::clearMessage );
}

void MainStatusBar::clearMessage()
{
  textWidget->setText( backgroungMessage );
  removeWidget( picWidget );
  timer->stop();
  QStatusBar::clearMessage();
}

void MainStatusBar::setBackgroundMessage( const QString & bkg_message )
{
  backgroungMessage = bkg_message;
  showMessage( bkg_message );
}

void MainStatusBar::showMessage( const QString & str, int timeout, const QPixmap & pixmap )
{
  textWidget->setText( str );
  if( pixmap.isNull() )
  {
    removeWidget( picWidget );
  }
  else
  {
    picWidget->setPixmap( pixmap );
    insertWidget( 0, picWidget, 1 );
    picWidget->show();
  }

  //  QStatusBar::showMessage(str,timeout);
  if ( timeout > 0 )
  {
    timer->start( timeout );
  }
}
