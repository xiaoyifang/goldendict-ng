#pragma once

#ifdef MAKE_FFMPEG_PLAYER

extern "C" {
  #include <libavcodec/avcodec.h>
  #include <libavformat/avformat.h>
  #include <libavutil/avutil.h>
  #include <libswresample/swresample.h>
}
  #include "ffmpeg_audiooutput.hh"
  #include <QObject>
  #include <QMutex>
  #include <QByteArray>
  #include <QThread>
  #include <QAudioDevice>
  #include <QDataStream>
  #include <QMediaDevices>
  #include <QString>
  #include <vector>

namespace Ffmpeg {
using std::vector;
class DecoderThread;
class AudioService: public QObject
{
  Q_OBJECT
  QScopedPointer< DecoderThread > thread;

public:
  static AudioService & instance();
  void playMemory( const char * ptr, int size );
  void stop();

signals:
  void cancelPlaying( bool waitUntilFinished );
  void error( const QString & message );

private:
  AudioService() = default;
  ~AudioService();
};

struct DecoderContext
{
  enum {
    kBufferSize = 32768
  };

  static QMutex deviceMutex_;
  QAtomicInt & isCancelled_;
  QByteArray audioData_;
  QDataStream audioDataStream_;
  AVFormatContext * formatContext_;
  #if LIBAVCODEC_VERSION_MAJOR < 59
  AVCodec * codec_;
  #else
  const AVCodec * codec_;
  #endif
  AVCodecContext * codecContext_;
  AVIOContext * avioContext_;
  AVStream * audioStream_;
  AudioOutput * audioOutput;
  bool avformatOpened_;

  SwrContext * swr_;

  DecoderContext( const QByteArray & audioData, QAtomicInt & isCancelled );
  ~DecoderContext();

  bool openCodec( QString & errorString );
  void closeCodec();
  bool openOutputDevice( QString & errorString );
  void closeOutputDevice();
  bool play( QString & errorString );
  void stop();
  bool normalizeAudio( AVFrame * frame, vector< uint8_t > & samples );
  void playFrame( AVFrame * frame );
};

class DecoderThread: public QThread
{
  Q_OBJECT

  static QMutex deviceMutex_;
  QAtomicInt isCancelled_;
  QByteArray audioData_;
  DecoderContext d;

public:
  DecoderThread( const QByteArray & audioData, QObject * parent );
  virtual ~DecoderThread();

public slots:
  void run();
  void cancel( bool waitUntilFinished );

signals:
  void error( const QString & message );
};

} // namespace Ffmpeg

#endif // MAKE_FFMPEG_PLAYER
