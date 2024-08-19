#include "state_manager.hh"
#include <QTimer>

StateManager::StateManager( QWidget * parent, Config::Class & cfg ):
  _cfg( cfg )
{
  _state    = cfg.mainWindowState;
  _geometry = cfg.mainWindowGeometry;
  _dirty    = false;
}


void StateManager::setState( QString state, QString geometry )
{
  if ( _dirty )
    return;


  if ( state == _state && _geometry == geometry ) {
    return;
  }
  _dirty = true;

  //delay execution
  QTimer::singleShot( 1000, &this, [ this, state, geometry ]() {
    saveConfigData( state, geometry );

    _dirty = false;
  } );
}


void StateManager::saveConfigData( QString state, QString geometry )
{
  try {
    // Save MainWindow state and geometry
    _cfg.mainWindowState    = state;
    _cfg.mainWindowGeometry = geometry;

    Config::save( _cfg );

    _state    = state;
    _geometry = geometry;
  }
  catch ( std::exception & e ) {
    gdWarning( "save config data failed, error: %s\n", e.what() );
  }
}