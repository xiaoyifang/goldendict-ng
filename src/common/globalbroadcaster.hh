#pragma once

#include "config.hh"
#include "pronounceengine.hh"
#include "dictionary_icon_name.hh"

struct ActiveDictIds
{
  unsigned groupId;
  QString word;
  QStringList dictIds;

  operator QString() const
  {
    return QString( "groupId:%1,word:%2,dictId:%3" ).arg( QString::number( groupId ), word, dictIds.join( "," ) );
  }
};


class GlobalBroadcaster: public QObject
{
  Q_OBJECT

  Config::Class * config;
  QSet< QString > whitelist;
  Icons::DictionaryIconName _icon_names;

public:
  void setConfig( Config::Class * _config );
  Config::Class * getConfig() const;
  // For backward compatibility
  Config::Preferences * getPreference() const;
  GlobalBroadcaster( QObject * parent = nullptr );
  /// \brief Add a host to whitelist.
  ///
  /// The host should be a full domain. For subdomain matching, add the base domain
  /// (e.g. "example.com"). For special TLDs, add the appropriate form
  /// (e.g. "example.com.uk" for UK sites).
  ///
  /// \param host The host to add to whitelist
  void addWhitelist( QString host );

  /// \brief Check if a host exists in the whitelist
  ///
  /// This method checks for exact matches and base domain matches:
  /// 1. Direct string matching - e.g. "www.example.com" matches "www.example.com"
  /// 2. Base domain matching using Utils::Url::extractBaseDomain() - e.g. "example.com" matches "www.example.com"
  ///
  /// Generic pattern handling for TLDs like .com.xx, .co.xx, .org.xx:
  /// - For "www.example.com.jp", the base domain is "example.com"
  /// - For "api.service.org.uk", the base domain is "service.org"
  ///
  /// Cross-TLD matching requires explicit entries:
  /// - To match both ".com" and ".com.xx" domains, both "example.com" and "example.com.xx"
  ///   need to be added to the whitelist separately
  ///
  /// \param host The host to check
  /// \return true if the host is in the whitelist, false otherwise
  bool existedInWhitelist( QString host ) const;
  static GlobalBroadcaster * instance();
  unsigned currentGroupId;
  QString translateLineText{};
  //hold the dictionary id;
  QSet< QString > collapsedDicts;

  std::function< bool( const QString & ) > isWordPresentedInFavorites;

  PronounceEngine pronounce_engine;
  QString getAbbrName( const QString & text );

  /// Check if dark mode is enabled
  /// @return true if dark mode is enabled, false otherwise
  bool isDarkModeEnabled() const;

signals:
  void dictionaryChanges( ActiveDictIds ad );
  void dictionaryClear( ActiveDictIds ad );

  void indexingDictionary( QString );

  void websiteDictionarySignal( QString, QString, QString );
};
