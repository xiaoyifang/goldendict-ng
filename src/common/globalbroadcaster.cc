#include "globalbroadcaster.hh"
#include "dict/dictionary.hh"
#include "instances.hh"
#include "audio/audioplayerinterface.hh"
#include <QGlobalStatic>
#include <QGuiApplication>
#include <QStyleHints>
#include <Qt>
#include "utils.hh"
#include <QDir>
#include <QDebug>
#include <QFile>
#include <QTextStream>

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
  hostWhitelist.insert( host );
}

void GlobalBroadcaster::addRefererWhitelist( QString host )
{
  refererWhitelist.insert( host );
}

bool existedInWhitelistInternal( const QSet< QString > & whitelist, QString host )
{
  for ( const QString & item : whitelist ) {
    if ( host == item ) {
      return true;
    }
    QString urlBaseDomain  = Utils::Url::extractBaseDomain( host );
    QString itemBaseDomain = Utils::Url::extractBaseDomain( item );
    if ( urlBaseDomain == itemBaseDomain ) {
      return true;
    }
  }
  return false;
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
      qDebug() << "Whitelisted host from config/whitelist:" << line;
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

bool GlobalBroadcaster::isDarkModeEnabled() const
{
  if ( !config ) {
    return false;
  }

  bool darkModeEnabled = ( config->preferences.darkReaderMode == Config::Dark::On );

#if QT_VERSION >= QT_VERSION_CHECK( 6, 5, 0 )
  if ( config->preferences.darkReaderMode == Config::Dark::Auto
  #if !defined( Q_OS_WINDOWS )
       // For macOS & Linux, uses "System's style hint". There is no darkMode setting in GD for them.
       && QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark
  #else
       // For Windows, uses the setting in GD
       && config->preferences.darkMode == Config::Dark::On
  #endif
  ) {
    darkModeEnabled = true;
  }
#endif
  return darkModeEnabled;
}
