#include "version.hh"
#include <QFile>
#include <QRegularExpression>

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
  return QStringLiteral( R"(Version: %1
Qt %2 %3
%4 %5 %6 %7
Flags: %8)" )
    .arg( Version::version() )
    .arg( QLatin1String( qVersion() ), Version::compiler )
    .arg( QSysInfo::productType(), QSysInfo::kernelType(), QSysInfo::kernelVersion(), QSysInfo::buildAbi() )
    .arg( flags );
}

QString getVersionTag()
{
  QString ver = version();
  
  QRegularExpression re( R"(^([\d.]+)-([a-f0-9]+)\s*\([^)]+\)\s+at)" );
  QRegularExpressionMatch match = re.match( ver );
  
  if ( match.hasMatch() ) {
    QString baseVersion = match.captured( 1 );
    QString gitHash = match.captured( 2 );
    
    return QStringLiteral( "v%1_alpha.%2" ).arg( baseVersion, gitHash );
  }
  
  QRegularExpression reSimple( R"(^([\d.]+)\s+at)" );
  QRegularExpressionMatch matchSimple = reSimple.match( ver );
  
  if ( matchSimple.hasMatch() ) {
    return QStringLiteral( "v%1" ).arg( matchSimple.captured( 1 ) );
  }
  
  QRegularExpression reNoAt( R"(^([\d.]+))" );
  QRegularExpressionMatch matchNoAt = reNoAt.match( ver );
  
  if ( matchNoAt.hasMatch() ) {
    return QStringLiteral( "v%1" ).arg( matchNoAt.captured( 1 ) );
  }
  
  return QString();
}

QString getReleaseUrl()
{
  QString tag = getVersionTag();
  if ( tag.isEmpty() ) {
    return QStringLiteral( "https://github.com/xiaoyifang/goldendict-ng/releases" );
  }
  return QStringLiteral( "https://github.com/xiaoyifang/goldendict-ng/releases/tag/%1" ).arg( tag );
}

} // namespace Version
