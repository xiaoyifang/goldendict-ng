/* This file is (c) 2012 Tvangeste <i.4m.l33t@yandex.ru>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include <QTabWidget>
#include <QTabBar>

/// An extension of QTabWidget that allows to better control
/// the tabbar visibility.
class MainTabWidget: public QTabWidget
{
  Q_OBJECT
  Q_PROPERTY( bool hideSingleTab READ isHideSingleTab WRITE setHideSingleTab )

public:
  MainTabWidget( QWidget * parent = 0 );

  bool isHideSingleTab() const
  {
    return hideSingleTab;
  }
  void setHideSingleTab( bool hide );

private:
  virtual void tabInserted( int index );
  virtual void tabRemoved( int index );
  void updateTabBarVisibility();
  virtual bool eventFilter( QObject * obj, QEvent * ev );

  bool hideSingleTab;
};
