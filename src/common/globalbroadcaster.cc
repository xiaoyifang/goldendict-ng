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

void GlobalBroadcaster::addWhitelist( QString url )
{
  whitelist.insert( url );
}

bool GlobalBroadcaster::existedInWhitelist( QString url ) const
{
  for ( const QString & item : whitelist ) {
    // Exact match - e.g. "www.example.com" matches "www.example.com"
    if ( url == item ) {
      return true;
    }

    // Extract base domain from both url and item for comparison
    QString urlBaseDomain  = extractBaseDomain( url );
    QString itemBaseDomain = extractBaseDomain( item );

    // Compare base domains
    if ( urlBaseDomain == itemBaseDomain ) {
      return true;
    }
  }
  return false; // No match found
}

QString GlobalBroadcaster::extractBaseDomain( const QString & domain ) const
{
  // More generic approach for detecting two-part TLDs
  // Look for patterns like com.xx, co.xx, org.xx, gov.xx, net.xx, edu.xx
  QStringList parts = domain.split( '.' );
  if ( parts.size() >= 3 ) {
    QString secondLevel = parts[ parts.size() - 2 ];
    QString topLevel    = parts[ parts.size() - 1 ];

    // Check if the second level is a common second-level domain indicator
    // and the top level is a standard TLD (2-3 characters)
    if ( ( secondLevel == "com" || secondLevel == "co" || secondLevel == "org" || secondLevel == "gov"
           || secondLevel == "net" || secondLevel == "edu" )
         && ( topLevel.length() == 2 || topLevel.length() == 3 ) ) {
      // Extract the registrable domain (e.g., "example.com" from "www.example.com.jp")
      if ( parts.size() >= 3 ) {
        return parts[ parts.size() - 3 ] + "." + secondLevel;
      }
      return secondLevel + "." + topLevel;
    }
  }

  // For domains with multiple parts, extract the last two parts as base domain
  int dotCount = domain.count( '.' );
  if ( dotCount >= 2 ) {
    if ( parts.isEmpty() ) {
      parts = domain.split( '.' );
    }
    if ( parts.size() >= 2 ) {
      return parts[ parts.size() - 2 ] + "." + parts[ parts.size() - 1 ];
    }
  }

  // For domains with one or no dots, return as is
  return domain;
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
