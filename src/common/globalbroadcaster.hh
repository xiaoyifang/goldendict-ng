#pragma once

#include <QObject>
#include <vector>
#include "config.hh"
#include "pronounceengine.hh"
#include <QCache>
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

  Config::Preferences * preference;
  QSet< QString > whitelist;
  Icons::DictionaryIconName _icon_names;

public:
  void setPreference( Config::Preferences * _pre );
  Config::Preferences * getPreference() const;
  GlobalBroadcaster( QObject * parent = nullptr );
  void addWhitelist( QString host );
  bool existedInWhitelist( QString host ) const;
  static GlobalBroadcaster * instance();
  unsigned currentGroupId;
  QString translateLineText{};
  //hold the dictionary id;
  QSet< QString > collapsedDicts;
  QSet< QString > favoriteWords;
  //map group id to favorite folder
  QMap< unsigned, QString > groupFavoriteMap;
  PronounceEngine pronounce_engine;
  QString getAbbrName( QString const & text );
signals:
  void dictionaryChanges( ActiveDictIds ad );
  void dictionaryClear( ActiveDictIds ad );

  void indexingDictionary( QString );
};
