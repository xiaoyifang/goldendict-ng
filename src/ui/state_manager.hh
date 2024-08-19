#ifndef STATE_MANAGER_HH
#define STATE_MANAGER_HH

#include <config.hh>

class StateManager: public QObject
{
  Q_OBJECT

public:
  StateManager( QWidget * parent, Config::Class & cfg );

  void setState( QString state, QString geometry );

private:
  boolean _dirty;
  Config::Class & _cfg;
  QString _state;
  QString _geometry;
  void saveConfigData( QString state, QString geometry );
};

#endif // STATE_MANAGER_HH
