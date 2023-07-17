#ifndef PRONOUNCEENGINE_HH
#define PRONOUNCEENGINE_HH

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

public:
  explicit PronounceEngine( QObject * parent = nullptr );
  void reset();
  void sendAudio( std::string dictId, QString audioLink );
  void finishDictionary( std::string dictId );
signals:
  void emitAudio( QString audioLink );
};

#endif // PRONOUNCEENGINE_HH
