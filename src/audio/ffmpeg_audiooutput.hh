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
  QScopedPointer< AudioOutputPrivate > d_ptr;

  Q_DISABLE_COPY( AudioOutput )
  Q_DECLARE_PRIVATE( AudioOutput )
};
