#include "config_file_main.hh"

#include <QFileInfo>
#include <QSaveFile>


namespace TTS {

auto current_service_txt = "current_service.txt";

QString get_service_name_from_path( const QDir & configPath )
{
  qDebug() << configPath;
  if ( !QFileInfo::exists( configPath.absoluteFilePath( current_service_txt ) ) ) {
    save_service_name_to_path( configPath, "azure" );
  }
  QFile f( configPath.filePath( current_service_txt ) );
  if ( !f.open( QFile::ReadOnly ) ) {
    throw std::runtime_error( "cannot open service name" ); // TODO
  }
  QString ret = f.readAll();
  f.close();
  return ret;
}

void save_service_name_to_path( const QDir & configPath, QUtf8StringView serviceName )
{
  QSaveFile f( configPath.absoluteFilePath( current_service_txt ) );
  if ( !f.open( QFile::WriteOnly ) ) {
    throw std::runtime_error( "Cannot write service name" );
  }
  f.write( serviceName.data(), serviceName.length() );
  f.commit();
};
} // namespace TTS