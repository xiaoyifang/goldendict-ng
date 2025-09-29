#include "globalbroadcaster.hh"
#include <QGlobalStatic>
#include <QGuiApplication>
#include <QStyleHints>
#include <Qt>
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

bool GlobalBroadcaster::isDarkModeEnabled() const
{
  if ( !preference ) {
    return false;
  }

  bool darkModeEnabled = ( preference->darkReaderMode == Config::Dark::On );

#if QT_VERSION >= QT_VERSION_CHECK( 6, 5, 0 )
  if ( preference->darkReaderMode == Config::Dark::Auto
  #if !defined( Q_OS_WINDOWS )
       // For macOS & Linux, uses "System's style hint". There is no darkMode setting in GD for them.
       && QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark
  #else
       // For Windows, uses the setting in GD
       && preference->darkMode == Config::Dark::On
  #endif
  ) {
    darkModeEnabled = true;
  }
#endif
  return darkModeEnabled;
}
