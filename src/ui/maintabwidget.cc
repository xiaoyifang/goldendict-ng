/* This file is (c) 2012 Tvangeste <i.4m.l33t@yandex.ru>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "maintabwidget.hh"
#include <QMouseEvent>

MainTabWidget::MainTabWidget( QWidget * parent ):
  QTabWidget( parent )
{
  hideSingleTab = false;
  installEventFilter( this );
  tabBar()->installEventFilter( this );
}

void MainTabWidget::setHideSingleTab( bool hide )
{
  hideSingleTab = hide;
  updateTabBarVisibility();
}

void MainTabWidget::tabInserted( int index )
{
  (void)index;
  updateTabBarVisibility();

  // Avoid bug in Qt 4.8.0
  setUsesScrollButtons( count() > 10 );
}

void MainTabWidget::tabRemoved( int index )
{
  (void)index;
  updateTabBarVisibility();

  // Avoid bug in Qt 4.8.0
  setUsesScrollButtons( count() > 10 );
}

void MainTabWidget::updateTabBarVisibility()
{
  tabBar()->setVisible( !hideSingleTab || tabBar()->count() > 1 );
}

bool MainTabWidget::eventFilter( QObject * obj, QEvent * ev )
{
  if ( obj == tabBar() && ev->type() == QEvent::MouseButtonPress ) {
    QMouseEvent * mev = static_cast< QMouseEvent * >( ev );
    if ( mev->button() == Qt::MiddleButton ) {
      emit tabCloseRequested( tabBar()->tabAt( mev->pos() ) );
      return true;
    }
  }

  return QTabWidget::eventFilter( obj, ev );
}
