#include "globalbroadcaster.hh"
#include "dict/dictionary.hh"
#include "instances.hh"
#include "audio/audioplayerinterface.hh"
#include <QGlobalStatic>
#include <QGuiApplication>
#include <QStyleHints>
#include "utils.hh"
#include <QDir>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QOperatingSystemVersion>
#include <QSettings>

Q_GLOBAL_STATIC( GlobalBroadcaster, bdcaster )

static std::vector< sptr< Dictionary::Class > > emptyDicts;
static Instances::Groups emptyGroups;
static AudioPlayerPtr emptyAudioPlayer;

GlobalBroadcaster::GlobalBroadcaster( QObject * parent ):
  QObject( parent )
{
  QStringList whiteUrlHosts = { "googleapis.com", "gstatic.com" };

  for ( auto & host : std::as_const( whiteUrlHosts ) ) {
    hostWhitelist.insert( host );
  }
}

GlobalBroadcaster * GlobalBroadcaster::instance()
{
  return bdcaster;
}

void GlobalBroadcaster::setConfig( Config::Class * _config )
{
  config = _config;
  loadWhitelist();
}

Config::Class * GlobalBroadcaster::getConfig() const
{
  return config;
}

Config::Preferences * GlobalBroadcaster::getPreference() const
{
  return config ? &config->preferences : nullptr;
}

void GlobalBroadcaster::addHostWhitelist( QString host )
{
  if ( host.isEmpty() ) {
    return;
  }
  bool isNegated  = host.startsWith( '-' );
  QString pattern = ( isNegated ? host.mid( 1 ).trimmed() : host.trimmed() ).toLower();
  QString base    = Utils::Url::extractBaseDomain( pattern );
  hostWhitelist.insert( isNegated ? "-" + base : base );
}

void GlobalBroadcaster::addRefererWhitelist( QString host )
{
  if ( host.isEmpty() ) {
    return;
  }
  bool isNegated  = host.startsWith( '-' );
  QString pattern = ( isNegated ? host.mid( 1 ).trimmed() : host.trimmed() ).toLower();
  QString base    = Utils::Url::extractBaseDomain( pattern );
  refererWhitelist.insert( isNegated ? "-" + base : base );
}

bool existedInWhitelistInternal( const QSet< QString > & whitelist, QString host )
{
  if ( host.isEmpty() ) {
    return false;
  }

  // Hostnames are case-insensitive
  QString lowerHost = host.toLower();
  QString baseHost  = Utils::Url::extractBaseDomain( lowerHost );
  bool whitelisted  = false;

  for ( const QString & item : whitelist ) {
    bool isNegated  = item.startsWith( '-' );
    QString pattern = ( isNegated ? item.mid( 1 ).trimmed() : item.trimmed() ).toLower();

    if ( pattern.isEmpty() ) {
      continue;
    }

    // Normalize pattern: remove leading dot if present
    if ( pattern.startsWith( '.' ) ) {
      pattern = pattern.mid( 1 );
    }

    // Match exact host, base domain, or any subdomain
    if ( lowerHost == pattern || baseHost == pattern || lowerHost.endsWith( "." + pattern ) ) {
      if ( isNegated ) {
        return false; // Blacklisted/negated items have the highest priority, directly rejecting.
      }
      whitelisted = true;
    }
  }
  return whitelisted;
}

bool GlobalBroadcaster::existedInHostWhitelist( QString host ) const
{
  return existedInWhitelistInternal( hostWhitelist, host );
}

bool GlobalBroadcaster::existedInRefererWhitelist( QString host ) const
{
  return existedInWhitelistInternal( refererWhitelist, host );
}


QString GlobalBroadcaster::getAbbrName( const QString & text, const QString & key )
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

  QString cacheKey = key.isEmpty() ? simplified : key;
  return _icon_names.getIconName( cacheKey, simplified );
}

void GlobalBroadcaster::loadWhitelist()
{
  QString whitelistFile = Config::getConfigDir() + "whitelist";
  QFile file( whitelistFile );
  if ( !file.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
    return;
  }

  QTextStream in( &file );
  while ( !in.atEnd() ) {
    QString line = in.readLine().trimmed();
    if ( !line.isEmpty() && !line.startsWith( "#" ) ) {
      addHostWhitelist( line );
      if ( line.startsWith( '-' ) ) {
        qDebug() << "Blacklisted (negated) host from config/whitelist:" << line.mid( 1 );
      }
      else {
        qDebug() << "Whitelisted host from config/whitelist:" << line;
      }
    }
  }
}

void GlobalBroadcaster::setAudioPlayer( const AudioPlayerPtr * _audioPlayer )
{
  audioPlayer = _audioPlayer;
}

const AudioPlayerPtr * GlobalBroadcaster::getAudioPlayer() const
{
  return audioPlayer ? audioPlayer : &emptyAudioPlayer;
}

void GlobalBroadcaster::setAllDictionaries( std::vector< sptr< Dictionary::Class > > * _allDictionaries )
{
  allDictionaries = _allDictionaries;
}

const std::vector< sptr< Dictionary::Class > > * GlobalBroadcaster::getAllDictionaries() const
{
  return allDictionaries ? allDictionaries : &emptyDicts;
}

sptr< Dictionary::Class > GlobalBroadcaster::getDictionaryById( const QString & dictId )
{
  if ( dictMap.empty() ) {
    if ( allDictionaries != nullptr ) {
      for ( const auto & dict : *allDictionaries ) {
        dictMap.insert( QString::fromStdString( dict->getId() ), dict );
      }
    }
  }
  return dictMap.value( dictId );
}

void GlobalBroadcaster::clearDictMap()
{
  dictMap.clear();
}

void GlobalBroadcaster::setGroups( Instances::Groups * _groups )
{
  groups = _groups;
}

const Instances::Groups * GlobalBroadcaster::getGroups() const
{
  return groups ? groups : &emptyGroups;
}

void GlobalBroadcaster::addLsaDictMapping( const QString & dictId, const QString & path )
{
  auto nativePath = QDir::toNativeSeparators( path );
  lsaIdToPathMap.insert( dictId, nativePath );
  lsaPathToIdMap.insert( nativePath, dictId );
}

QString GlobalBroadcaster::getLsaPathFromId( const QString & dictId ) const
{
  return lsaIdToPathMap.value( dictId );
}

QString GlobalBroadcaster::getLsaIdFromPath( const QString & path ) const
{
  auto nativePath = QDir::toNativeSeparators( path );
  return lsaPathToIdMap.value( nativePath );
}

bool GlobalBroadcaster::isDarkReaderModeEnabled() const
{
  if ( !config ) {
    return false;
  }

  bool darkModeEnabled = ( config->preferences.darkReaderMode == Config::Dark::On );

  if ( config->preferences.darkReaderMode == Config::Dark::Auto ) {
    darkModeEnabled = isSystemDarkTheme();
  }

  return darkModeEnabled;
}

bool GlobalBroadcaster::isSystemDarkTheme()
{
#if QT_VERSION >= QT_VERSION_CHECK( 6, 5, 0 )
  #if defined( Q_OS_WINDOWS )
  // For all Windows versions (10 and 11), use registry check as it's more reliable
  // than Qt's colorScheme() which may not work consistently on Windows 11
  QSettings settings( "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                      QSettings::NativeFormat );
  
  // AppsUseLightTheme: 0 = Dark mode, 1 = Light mode
  // If the key doesn't exist, default to light mode (return false)
  if ( settings.contains( "AppsUseLightTheme" ) ) {
    return settings.value( "AppsUseLightTheme" ).toInt() == 0;
  }
  
  // Fallback: Also check SystemUsesLightTheme for system-wide theme
  if ( settings.contains( "SystemUsesLightTheme" ) ) {
    return settings.value( "SystemUsesLightTheme" ).toInt() == 0;
  }
  
  // Default to light mode if no registry keys found
  return false;
  #else
  // Other platforms: Use Qt's colorScheme
  return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
  #endif
#else
  return false;
#endif
}
