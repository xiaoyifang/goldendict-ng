#include "tts/services/local_command.hh"
#include <QFile>
#include <QSaveFile>
#include <toml++/toml.hpp>

namespace TTS {


static const char * LocalCommandSaveFileName = "local_command.toml";


namespace {


/// @brief try read cmd or create a default one
/// @param path
/// @return if true ->  QString is wanted string, else errorString. (Poor man's std::expected)
std::tuple< bool, QString > getLocalCommandConfigFromFile( const QString & path ) noexcept
{
  if ( !QFileInfo::exists( path ) ) {
    QSaveFile f( path );
    f.open( QSaveFile::WriteOnly );

    auto tbl = toml::table{
      { "cmd",
        R"raw(pwsh.exe -Command $(New-Object System.Speech.Synthesis.SpeechSynthesizer).speak('%GDSENTENCE%'))raw" },
    };

    std::stringstream out;
    out << tbl;

    f.write( QByteArray::fromStdString( out.str() ) );
    if ( f.commit() == false ) {
      throw std::runtime_error( "Cannot write to file." );
    }
  }

  toml::table tbl;
  try {
    tbl = toml::parse_file( path.toStdString() );
  }
  catch ( const toml::parse_error & err ) {
    return { false, err.what() };
  }

  auto cmd = tbl[ "cmd" ].value< std::string >();

  if ( cmd.has_value() ) {
    return {
      true,
      QString::fromStdString( cmd.value() ),
    };
  }
  else {
    return { true, "" };
  }
};
} // namespace


LocalCommandService::LocalCommandService( const QDir & configRootPath )
{

  this->configFilePath = configRootPath.filePath( LocalCommandSaveFileName );
}

std::optional< std::string > LocalCommandService::loadCommandFromConfigFile()
{
  auto [ status, str ] = getLocalCommandConfigFromFile( this->configFilePath );
  if ( status == true ) {
    this->command = str;
    return {};
  }
  else {
    return { str.toStdString() };
  }
}

LocalCommandService::~LocalCommandService()
{
  process->disconnect( this ); // Prevent innocent error at program exit, which is considered as error.
}

void LocalCommandService::speak( QUtf8StringView s ) noexcept
{
  process.reset( new QProcess( this ) );
  QString cmd_to_be_executed = command;
  cmd_to_be_executed.replace( "%GDSENTENCE%", s.toString() );
  qDebug() << "local command speaking: " << cmd_to_be_executed;
  process->startCommand( cmd_to_be_executed );
  // TODO: handle errors of processes.
  connect( process.get(), &QProcess::errorOccurred, this, [ this ]( QProcess::ProcessError error ) {
    emit TTS::Service::errorOccured( "Process failed to execute due to QProcess::ProcessError" );
  } );
}

void LocalCommandService::stop() noexcept
{
  process.reset(); // deleter of QProcess also kills the process
}


LocalCommandConfigWidget::LocalCommandConfigWidget( QWidget * parent, const QDir & configRootPath ):
  TTS::ServiceConfigWidget( parent )
{
  auto * layout = new QVBoxLayout( this );
  layout->addWidget( new QLabel( R"(Set command)", parent ) );

  commandLineEdit = new QLineEdit( this );
  layout->addWidget( commandLineEdit );
  layout->addStretch();

  this->configFilePath = configRootPath.filePath( LocalCommandSaveFileName );

  auto [ status, str ] = getLocalCommandConfigFromFile( this->configFilePath );

  if ( status == true ) {
    commandLineEdit->setText( str );
  }
  else {
    // do nothing.
  }
}

std::optional< std::string > LocalCommandConfigWidget::save() noexcept
{

  QSaveFile f( this->configFilePath );
  f.open( QSaveFile::WriteOnly );

  auto tbl = toml::table{
    { "cmd", commandLineEdit->text().simplified().toStdString() },
  };

  std::stringstream out;
  out << tbl;

  f.write( QByteArray::fromStdString( out.str() ) );
  if ( f.commit() == false ) {
    return { "Cannot write to file." };
  }
  return {};
}

} // namespace TTS