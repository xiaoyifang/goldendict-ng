#include "version.hh"
#include <QFile>

namespace Version {
QString version()
{
  QFile versionFile( ":/version.txt" );

  if ( !versionFile.open( QFile::ReadOnly ) ) {
    return QStringLiteral( "[Unknown Version]" );
  }
  else {
    return versionFile.readAll().trimmed();
  }
}

QString everything()
{
  return QStringLiteral( "Goldendict-ng " ) + Version::version() + "\n" + "Qt " + QLatin1String( qVersion() ) + " "
    + Version::compiler + QSysInfo::productType() + " " + QSysInfo::kernelType() + " " + QSysInfo::kernelVersion() + " "
    + QSysInfo::buildAbi() + "\n" + "Flags:" + flags;
}

} // namespace Version
