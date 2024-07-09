#ifndef __FFMPEGAUDIO_HH_INCLUDED__
#define __FFMPEGAUDIO_HH_INCLUDED__

#ifdef MAKE_FFMPEG_PLAYER
  #include "audiooutput.hh"
  #include <QObject>
  #include <QMutex>
  #include <QByteArray>
  #include <QThread>
extern "C" {
  #include <libavcodec/avcodec.h>
  #include <libavformat/avformat.h>
  #include <libavutil/avutil.h>
  #include "libswresample/swresample.h"
}

  #include <QString>
  #include <QDataStream>

  #include <vector>
  #if ( QT_VERSION >= QT_VERSION_CHECK( 6, 2, 0 ) )
    #include <QMediaDevices>

    #include <QAudioDevice>
  #endif
using std::vector;
namespace Ffmpeg {
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
  void error( QString const & message );

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

  DecoderContext( QByteArray const & audioData, QAtomicInt & isCancelled );
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
  DecoderThread( QByteArray const & audioData, QObject * parent );
  virtual ~DecoderThread();

public slots:
  void run();
  void cancel( bool waitUntilFinished );

signals:
  void error( QString const & message );
};

} // namespace Ffmpeg

#endif // MAKE_FFMPEG_PLAYER

#endif // __FFMPEGAUDIO_HH_INCLUDED__
