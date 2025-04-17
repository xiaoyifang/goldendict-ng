#pragma once

#include <QObject>
#include <QMap>
#include <QMutex>


enum class PronounceState {
  AVAILABLE,
  OCCUPIED
};

class PronounceEngine: public QObject
{
  Q_OBJECT
  PronounceState state = PronounceState::AVAILABLE;
  QMutex mutex;

  QMap< std::string, QList< QString > > dictAudioMap;

  QString firstAudioLink;

public:
  explicit PronounceEngine( QObject * parent = nullptr );
  void reset();
  QString getFirstAudioLink();

  void sendAudio( const std::string & dictId, const QString & audioLink );
  void finishDictionary( std::string dictId );
signals:
  void emitAudio( QString audioLink );
};
