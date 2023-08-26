#ifndef GLOBAL_GLOBALBROADCASTER_H
#define GLOBAL_GLOBALBROADCASTER_H

#include <QObject>
#include <vector>
#include "config.hh"
#include "pronounceengine.hh"
#include <QCache>

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

public:
  void setPreference( Config::Preferences * _pre );
  Config::Preferences * getPreference();
  GlobalBroadcaster( QObject * parent = nullptr );
  void addWhitelist( QString host );
  bool existedInWhitelist( QString host );
  static GlobalBroadcaster * instance();
  unsigned currentGroupId;
  QString translateLineText{};
  //hold the dictionary id;
  QSet< QString > collapsedDicts;
  QMap< QString, QSet< QString > > folderFavoritesMap;
  QMap< unsigned, QString > groupFolderMap;
  PronounceEngine pronounce_engine;
  QCache< QString, QByteArray > cache;

  void insertCache( const QString & , QByteArray * );

signals:
  void dictionaryChanges( ActiveDictIds ad );
  void dictionaryClear( ActiveDictIds ad );

  void indexingDictionary( QString );
};

#endif // GLOBAL_GLOBALBROADCASTER_H
