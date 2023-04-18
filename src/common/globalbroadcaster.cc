#include "globalbroadcaster.hh"
#include <QGlobalStatic>
#include "utils.hh"

Q_GLOBAL_STATIC( GlobalBroadcaster, bdcaster )
GlobalBroadcaster::GlobalBroadcaster( QObject * parent ) : QObject( parent )
{
  QStringList whiteUrlHosts = { "ajax.googleapis.com" };
 
  for( auto host : whiteUrlHosts )
  {
    addWhitelist( host );
  }
}

GlobalBroadcaster * GlobalBroadcaster::instance()
{
  return bdcaster;
}
void GlobalBroadcaster::setPreference( Config::Preferences * p )
{
  preference = p;
}
Config::Preferences * GlobalBroadcaster::getPreference()
{
  return preference;
}

void GlobalBroadcaster::addWhitelist( QString url )
{
  whitelist.insert( url );
  auto baseUrl = Utils::Url::getHostBase( url );
  whitelist.insert( baseUrl );
}

bool GlobalBroadcaster::existedInWhitelist( QString url )
{
  return whitelist.contains(url);
}
// namespace global
