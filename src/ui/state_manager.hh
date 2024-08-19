#ifndef STATE_MANAGER_HH
#define STATE_MANAGER_HH

#include <config.hh>

class StateManager: public QObject
{
  Q_OBJECT

public:
  StateManager( QWidget * parent, Config::Class & cfg );

  void setState( QByteArray state, QByteArray geometry );

private:
  bool _dirty;
  Config::Class & _cfg;
  QByteArray _state;
  QByteArray _geometry;
  void saveConfigData( QByteArray state, QByteArray geometry );
};

#endif // STATE_MANAGER_HH
