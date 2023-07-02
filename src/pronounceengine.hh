#ifndef PRONOUNCEENGINE_HH
#define PRONOUNCEENGINE_HH

#include <QObject>
#include <QMutexLocker>
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

public:
  explicit PronounceEngine( QObject * parent = nullptr );
  void reset();
  void sendAudio( QString audioLink );

signals:
  void emitAudio( QString audioLink );
};

#endif // PRONOUNCEENGINE_HH
