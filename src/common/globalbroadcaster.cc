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
    if ( url.endsWith( item ) ) {
      return true; // Match found
    }
  }
  return false; // No match found
}


QString GlobalBroadcaster::getAbbrName( QString const & text )
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
