#ifndef MRUQMENU_HH
#define MRUQMENU_HH

#include <QMenu>
#include <QEvent>

// Detail: http://goldendict.org/forum/viewtopic.php?f=4&t=1176
// When ctrl during ctrl+tab released, a request to change current tab will be emitted.
class MRUQMenu: public QMenu
{
  Q_OBJECT

public:
  explicit MRUQMenu( const QString title, QWidget * parent = 0 );

private:
  void keyReleaseEvent( QKeyEvent * kev ) override;

signals:
  void requestTabChange( int index );
};


#endif // MRUQMENU_HH
