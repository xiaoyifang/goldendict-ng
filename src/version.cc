#include "build_config.hh"
#include "version.hh"
#include <QFile>

namespace Version {

QString everything()
{
  return QStringLiteral( "Version: " ) + GD_VERSION_STRING + "\n" + "Qt " + QLatin1String( qVersion() ) + " "
    + Version::compiler + "\n" + QSysInfo::productType() + " " + QSysInfo::kernelType() + " "
    + QSysInfo::kernelVersion() + " " + QSysInfo::buildAbi() + "\nFlags: " + flags;
}

} // namespace Version
