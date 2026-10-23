#pragma once

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
  void stop();

private:
  // named "d" to avoid shadowing QObject's own d_ptr
  QScopedPointer< AudioOutputPrivate > d;

  Q_DISABLE_COPY( AudioOutput )
};
