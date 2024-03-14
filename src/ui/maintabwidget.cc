/* This file is (c) 2012 Tvangeste <i.4m.l33t@yandex.ru>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "maintabwidget.hh"
#include <QEvent>
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
  // QTabBar::mouseDoubleClickEvent is different from QWidget::mouseDoubleClickEvent
  // The former only got emitted when an actual tab is clicked, here we want to detect click on empty sapce of the tabbar
  if ( ev->type() == QEvent::MouseButtonDblClick ) {
    QMouseEvent * mev = static_cast< QMouseEvent * >( ev );
    if ( tabBar()->rect().contains( mev->position().toPoint() ) && tabBar()->tabAt( mev->position().toPoint() ) == -1 ) {
      emit doubleClicked();
      return true;
    }
  }

  if ( obj == tabBar() && ev->type() == QEvent::MouseButtonPress ) {
    QMouseEvent * mev = static_cast< QMouseEvent * >( ev );
    if ( mev->button() == Qt::MiddleButton ) {
      emit tabCloseRequested( tabBar()->tabAt( mev->pos() ) );
      return true;
    }
  }

  return QTabWidget::eventFilter( obj, ev );
}
