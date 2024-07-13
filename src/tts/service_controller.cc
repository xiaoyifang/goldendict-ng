#include "tts/service_controller.hh"
#include "config_file_main.hh"
#include "tts/services/azure.hh"
#include "tts/services/dummy.hh"

TTS::ServiceController::ServiceController( const QString & configPath )
{
  configRootDir = QDir( configPath );
  configRootDir.mkpath( QStringLiteral( "ctts" ) );
  configRootDir.cd( QStringLiteral( "ctts" ) );
  currentService.reset();
}

void TTS::ServiceController::reload()
{
  QString service_name = get_service_name_from_path( this->configRootDir );
  if ( service_name == "azure" ) {
    currentService.reset( Azure::Service::Construct( this->configRootDir ) );
  }
  else {
    currentService.reset( new dummy::Service() );
  }
}

void TTS::ServiceController::speak( const QString & text )
{

  if ( currentService.isNull() ) {
    this->reload();
  }
  auto err = currentService->speak( text.toStdString() );
}