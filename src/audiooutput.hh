#ifndef AUDIOOUTPUT_H
#define AUDIOOUTPUT_H

#include <QObject>
#include <QScopedPointer>

class AudioOutputPrivate;
class AudioOutput: public QObject
{
public:
  AudioOutput( QObject * parent = nullptr );
  ~AudioOutput();

  bool play( const uint8_t * data, qint64 len );
  void setAudioFormat( int sampleRate, int channels );

protected:
  QScopedPointer< AudioOutputPrivate > d_ptr;

private:
  Q_DISABLE_COPY( AudioOutput )
  Q_DECLARE_PRIVATE( AudioOutput )
};


#endif // AUDIOOUTPUT_H
