#include "globalbroadcaster.hh"
#include <QGlobalStatic>
#include "utils.hh"
#include "service_locator.hh"

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

Config::Preferences * GlobalBroadcaster::getPreference() const
{
  return const_cast< Config::Preferences * >( &ServiceLocator::instance().getConfig().preferences );
}

void GlobalBroadcaster::addWhitelist( QString url )
{
  whitelist.insert( url );
}

bool GlobalBroadcaster::existedInWhitelist( QString url ) const
{
  for ( const QString & item : whitelist ) {
    if ( url.endsWith( item ) ) {
      return true; // Match found
    }
  }
  return false; // No match found
}


QString GlobalBroadcaster::getAbbrName( const QString & text )
{
  if ( text.isEmpty() ) {
    return {};
  }
  //remove whitespace,number,mark,puncuation,symbol
  QString simplified = text;
  simplified.remove(
    QRegularExpression( R"([\p{Z}\p{N}\p{M}\p{P}\p{S}])", QRegularExpression::UseUnicodePropertiesOption ) );

  if ( simplified.isEmpty() ) {
    return {};
  }

  return _icon_names.getIconName( simplified );
}
// namespace global
