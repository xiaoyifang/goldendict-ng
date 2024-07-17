#include "tts/single_service_controller.hh"
#include "config_file_main.hh"
#include "tts/services/azure.hh"
#include "tts/services/dummy.hh"
#include "tts/services/local_command.hh"
#include "tts/error_dialog.hh"

namespace TTS {

SingleServiceController::SingleServiceController( const QString & configPath )
{
  configRootDir = QDir( configPath );
  configRootDir.mkpath( QStringLiteral( "ctts" ) );
  configRootDir.cd( QStringLiteral( "ctts" ) );
  currentService.reset();
}

void SingleServiceController::reload()
{
  QString service_name = get_service_name_from_path( this->configRootDir );
  if ( service_name == "azure" ) {
    currentService.reset( TTS::AzureService::Construct( this->configRootDir ) );
  }
  else if ( service_name == "local_command" ) {
    auto * s = new TTS::LocalCommandService( this->configRootDir );
    s->loadCommandFromConfigFile(); // TODO:: error unhandled.
    currentService.reset( s );
  }
  else {
    currentService.reset( new TTS::DummyService() );
  }

  connect( currentService.get(), &Service::errorOccured, []( const QString & errorString ) {
    TTS::reportError( errorString );
  } );
}

void SingleServiceController::speak( const QString & text )
{

  if ( !currentService ) {
    this->reload();
  }
  currentService->speak( text.toStdString() );
}

} // namespace TTS