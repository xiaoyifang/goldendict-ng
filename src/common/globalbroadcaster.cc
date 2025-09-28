#include "globalbroadcaster.hh"
#include <QGlobalStatic>
#include "utils.hh"

Q_GLOBAL_STATIC( GlobalBroadcaster, bdcaster )

GlobalBroadcaster::GlobalBroadcaster( QObject * parent ):
  QObject( parent )
{
  QStringList whiteUrlHosts = { "googleapis.com", "gstatic.com" };

  for ( auto & host : std::as_const( whiteUrlHosts ) ) {
    whitelist.insert( host );
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

Config::Preferences * GlobalBroadcaster::getPreference() const
{
  return preference;
}

void GlobalBroadcaster::addWhitelist( QString host )
{
  whitelist.insert( host );
}

bool GlobalBroadcaster::existedInWhitelist( QString host ) const
{
  for ( const QString & item : whitelist ) {
    // Exact match - e.g. "www.example.com" matches "www.example.com"
    if ( host == item ) {
      return true;
    }

    // Extract base domain from both host and item for comparison
    QString urlBaseDomain  = Utils::Url::extractBaseDomain( host );
    QString itemBaseDomain = Utils::Url::extractBaseDomain( item );

    // Compare base domains
    if ( urlBaseDomain == itemBaseDomain ) {
      return true;
    }
  }
  return false; // No match found
}


QString GlobalBroadcaster::getAbbrName( const QString & text )
{
  if ( text.isEmpty() ) {
    return {};
  }
  // remove whitespace,number,mark,puncuation,symbol
  QString simplified = text;
  simplified.remove(
    QRegularExpression( R"([\p{Z}\p{N}\p{M}\p{P}\p{S}])", QRegularExpression::UseUnicodePropertiesOption ) );

  if ( simplified.isEmpty() ) {
    return {};
  }

  return _icon_names.getIconName( simplified );
}
