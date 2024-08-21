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
  if ( state == _state && _geometry == geometry ) {
    return;
  }
  _state    = state;
  _geometry = geometry;
  //in the save state process already.
  if ( _dirty )
    return;
  _dirty = true;

  //delay execution
  QTimer::singleShot( 3000, this, [ this ]() {
    saveConfigData( _state, _geometry );

    _dirty = false;
  } );
}


void StateManager::saveConfigData( QByteArray state, QByteArray geometry )
{
  _settings.beginGroup( "mainwindow" );
  _settings.setValue( "state", state );
  _settings.setValue( "geometry", geometry );
  _settings.setValue( "searchInDock", _cfg.preferences.searchInDock );
  _settings.endGroup();
}