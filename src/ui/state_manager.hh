#ifndef STATE_MANAGER_HH
#define STATE_MANAGER_HH

#include <config.hh>
#include <QSettings>

class StateManager: public QObject
{
  Q_OBJECT

public:
  StateManager( QObject * parent, Config::Class & cfg );

  void setState( const QByteArray & state, const QByteArray & geometry );

private:
  bool _dirty;
  Config::Class & _cfg;
  QByteArray _state;
  QByteArray _geometry;
  QSettings _settings;


  void saveConfigData( QByteArray state, QByteArray geometry );
};

#endif // STATE_MANAGER_HH
