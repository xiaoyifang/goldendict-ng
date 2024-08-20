#include "state_manager.hh"
#include <QTimer>
#include <QSettings>

StateManager::StateManager( QObject * parent, Config::Class & cfg ):
  QObject( parent ),
  _cfg( cfg ),
  _settings( Config::getStateFileName(), QSettings::IniFormat, parent )
{
  _state    = _cfg.mainWindowState;
  _geometry = _cfg.mainWindowGeometry;
  _dirty    = false;
}


void StateManager::setState( const QByteArray & state, const QByteArray & geometry )
{
  if ( _dirty )
    return;

  if ( state == _state && _geometry == geometry ) {
    return;
  }
  _dirty = true;
  //delay execution
  QTimer::singleShot( 3000, this, [ = ]() {
    saveConfigData( state, geometry );

    _dirty = false;
  } );
}


void StateManager::saveConfigData( QByteArray state, QByteArray geometry )
{
  try {
    // Save MainWindow state and geometry
    _cfg.mainWindowState    = state;
    _cfg.mainWindowGeometry = geometry;

    // Config::save( _cfg );

    _settings.beginGroup( "mainwindow" );
    _settings.setValue( "state", state );
    _settings.setValue( "geometry", geometry );
    _settings.endGroup();

    _state    = state;
    _geometry = geometry;
  }
  catch ( std::exception & e ) {
    qWarning() << "state manager save config data failed: " << e.what();
  }
}