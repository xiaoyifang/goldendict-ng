#ifdef MAKE_FFMPEG_PLAYER

  #include "audiooutput.hh"
  #include "ffmpegaudio.hh"

  #include <errno.h>

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
  #include "gddebug.hh"
  #include "utils.hh"

using std::vector;

namespace Ffmpeg {

QMutex DecoderThread::deviceMutex_;

static inline QString avErrorString( int errnum )
{
  char buf[ 64 ];
  av_strerror( errnum, buf, 64 );
  return QString::fromLatin1( buf );
}

AudioService & AudioService::instance()
{
  static AudioService a;
  return a;
}


AudioService::~AudioService()
{
  emit cancelPlaying( true );
}

void AudioService::playMemory( const char * ptr, int size )
{
  emit cancelPlaying( false );
  if ( !thread.isNull() ) {
    thread->wait();
  }
  QByteArray audioData( ptr, size );
  thread.reset( new DecoderThread( audioData, this ) );
  connect( this, &AudioService::cancelPlaying, thread.get(), &DecoderThread::cancel );
  thread->start();
}

void AudioService::stop()
{
  emit cancelPlaying( false );
}


DecoderContext::DecoderContext( QByteArray const & audioData, QAtomicInt & isCancelled ):
  isCancelled_( isCancelled ),
  audioData_( audioData ),
  audioDataStream_( audioData_ ),
  formatContext_( nullptr ),
  codec_( nullptr ),
  codecContext_( nullptr ),
  avioContext_( nullptr ),
  audioStream_( nullptr ),
  audioOutput( new AudioOutput ),
  avformatOpened_( false ),
  swr_( nullptr )
{
}

DecoderContext::~DecoderContext()
{
  closeOutputDevice();
  closeCodec();
}

static int readAudioData( void * opaque, unsigned char * buffer, int bufferSize )
{
  QDataStream * pStream = (QDataStream *)opaque;
  // This function is passed as the read_packet callback into avio_alloc_context().
  // The documentation for this callback parameter states:
  // For stream protocols, must never return 0 but rather a proper AVERROR code.
  if ( pStream->atEnd() )
    return AVERROR_EOF;
  const int bytesRead = pStream->readRawData( (char *)buffer, bufferSize );
  // QDataStream::readRawData() returns 0 at EOF => return AVERROR_EOF in this case.
  // An error is unlikely here, so just print a warning and return AVERROR_EOF too.
  if ( bytesRead < 0 )
    gdWarning( "readAudioData: error while reading raw data." );
  return bytesRead > 0 ? bytesRead : AVERROR_EOF;
}

bool DecoderContext::openCodec( QString & errorString )
{
  formatContext_ = avformat_alloc_context();
  if ( !formatContext_ ) {
    errorString = "avformat_alloc_context() failed.";
    return false;
  }

  unsigned char * avioBuffer = (unsigned char *)av_malloc( kBufferSize + AV_INPUT_BUFFER_PADDING_SIZE );

  if ( !avioBuffer ) {
    errorString = "av_malloc() failed.";
    return false;
  }

  // Don't free buffer allocated here (if succeeded), it will be cleaned up automatically.
  avioContext_ = avio_alloc_context( avioBuffer, kBufferSize, 0, &audioDataStream_, readAudioData, nullptr, nullptr );
  if ( !avioContext_ ) {
    av_free( avioBuffer );
    errorString = "avio_alloc_context() failed.";
    return false;
  }

  avioContext_->seekable   = 0;
  avioContext_->write_flag = 0;

  // If pb not set, avformat_open_input() simply crash.
  formatContext_->pb = avioContext_;
  formatContext_->flags |= AVFMT_FLAG_CUSTOM_IO;

  int ret         = 0;
  avformatOpened_ = true;

  ret = avformat_open_input( &formatContext_, nullptr, nullptr, nullptr );
  if ( ret < 0 ) {
    errorString = QString( "avformat_open_input() failed: %1." ).arg( avErrorString( ret ) );
    return false;
  }

  ret = avformat_find_stream_info( formatContext_, nullptr );
  if ( ret < 0 ) {
    errorString = QString( "avformat_find_stream_info() failed: %1." ).arg( avErrorString( ret ) );
    return false;
  }

  // Find audio stream, use the first audio stream if available
  for ( unsigned i = 0; i < formatContext_->nb_streams; i++ ) {
    if ( formatContext_->streams[ i ]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO ) {
      audioStream_ = formatContext_->streams[ i ];
      break;
    }
  }
  if ( !audioStream_ ) {
    errorString = QString( "Could not find audio stream." );
    return false;
  }

  codec_ = avcodec_find_decoder( audioStream_->codecpar->codec_id );
  if ( !codec_ ) {
    errorString = QString( "Codec [id: %1] not found." ).arg( audioStream_->codecpar->codec_id );
    return false;
  }
  codecContext_ = avcodec_alloc_context3( codec_ );
  if ( !codecContext_ ) {
    errorString = QString( "avcodec_alloc_context3() failed." );
    return false;
  }
  avcodec_parameters_to_context( codecContext_, audioStream_->codecpar );

  ret = avcodec_open2( codecContext_, codec_, nullptr );
  if ( ret < 0 ) {
    errorString = QString( "avcodec_open2() failed: %1." ).arg( avErrorString( ret ) );
    return false;
  }

  gdDebug( "Codec open: %s: channels: %d, rate: %d, format: %s\n",
           codec_->long_name,
           codecContext_->channels,
           codecContext_->sample_rate,
           av_get_sample_fmt_name( codecContext_->sample_fmt ) );

  auto layout = codecContext_->channel_layout;
  if ( !layout ) {
    layout                        = av_get_default_channel_layout( codecContext_->channels );
    codecContext_->channel_layout = layout;
  }

  swr_ = swr_alloc_set_opts( nullptr,
                             layout,
                             AV_SAMPLE_FMT_S16,
                             44100,
                             layout,
                             codecContext_->sample_fmt,
                             codecContext_->sample_rate,
                             0,
                             nullptr );

  if ( !swr_ || swr_init( swr_ ) < 0 ) {
    av_log( nullptr, AV_LOG_ERROR, "Cannot create sample rate converter \n" );
    swr_free( &swr_ );
    return false;
  }

  return true;
}

void DecoderContext::closeCodec()
{
  if ( swr_ ) {
    swr_free( &swr_ );
  }

  if ( !formatContext_ ) {
    if ( avioContext_ ) {
      av_free( avioContext_->buffer );
      avioContext_ = nullptr;
    }
    return;
  }

  // avformat_open_input() is not called, just free the buffer associated with
  // the AVIOContext, and the AVFormatContext
  if ( !avformatOpened_ ) {
    if ( formatContext_ ) {
      avformat_free_context( formatContext_ );
      formatContext_ = nullptr;
    }

    if ( avioContext_ ) {
      av_free( avioContext_->buffer );
      avioContext_ = nullptr;
    }
    return;
  }

  avformatOpened_ = false;

  // Closing a codec context without prior avcodec_open2() will result in
  // a crash in ffmpeg
  if ( audioStream_ && codecContext_ && codec_ ) {
    audioStream_->discard = AVDISCARD_ALL;
    avcodec_close( codecContext_ );
    avcodec_free_context( &codecContext_ );
  }

  avformat_close_input( &formatContext_ );
  av_free( avioContext_->buffer );
}

bool DecoderContext::openOutputDevice( QString & errorString )
{
  // only check device when qt version is greater than 6.2
  #if ( QT_VERSION >= QT_VERSION_CHECK( 6, 2, 0 ) )
  QAudioDevice m_outputDevice = QMediaDevices::defaultAudioOutput();
  if ( m_outputDevice.isNull() ) {
    errorString += QString( "Can not found default audio output device" );
    return false;
  }
  #endif

  if ( audioOutput == nullptr ) {
    errorString += QStringLiteral( "Failed to create audioOutput." );
    return false;
  }

  audioOutput->setAudioFormat( 44100, codecContext_->channels );
  return true;
}

void DecoderContext::closeOutputDevice() {}

bool DecoderContext::play( QString & errorString )
{
  AVFrame * frame = av_frame_alloc();
  if ( !frame ) {
    errorString = QString( "avcodec_alloc_frame() failed." );
    return false;
  }

  AVPacket * packet = av_packet_alloc();

  while ( !Utils::AtomicInt::loadAcquire( isCancelled_ ) && av_read_frame( formatContext_, packet ) >= 0 ) {
    if ( packet->stream_index == audioStream_->index ) {
      int ret = avcodec_send_packet( codecContext_, packet );
      /* read all the output frames (in general there may be any number of them) */
      while ( ret >= 0 ) {
        ret = avcodec_receive_frame( codecContext_, frame );

        if ( Utils::AtomicInt::loadAcquire( isCancelled_ ) || ret < 0 )
          break;

        playFrame( frame );
      }
    }

    av_packet_unref( packet );
  }

  /* flush the decoder */
  packet->data = nullptr;
  packet->size = 0;
  int ret      = avcodec_send_packet( codecContext_, packet );
  while ( ret >= 0 ) {
    ret = avcodec_receive_frame( codecContext_, frame );
    if ( Utils::AtomicInt::loadAcquire( isCancelled_ ) || ret < 0 )
      break;
    playFrame( frame );
  }
  av_frame_free( &frame );
  av_packet_free( &packet );
  return true;
}

void DecoderContext::stop()
{
  if ( audioOutput ) {
    audioOutput->stop();
    audioOutput->deleteLater();
    audioOutput = nullptr;
  }
}

bool DecoderContext::normalizeAudio( AVFrame * frame, vector< uint8_t > & samples )
{
  auto dst_freq     = 44100;
  auto dst_channels = codecContext_->channels;
  int out_count     = (int64_t)frame->nb_samples * dst_freq / frame->sample_rate + 256;
  int out_size      = av_samples_get_buffer_size( nullptr, dst_channels, out_count, AV_SAMPLE_FMT_S16, 1 );
  samples.resize( out_size );
  uint8_t * data[ 2 ] = { nullptr };
  data[ 0 ]           = &samples.front();

  auto out_nb_samples = swr_convert( swr_, data, out_count, (const uint8_t **)frame->extended_data, frame->nb_samples );

  if ( out_nb_samples < 0 ) {
    av_log( nullptr, AV_LOG_ERROR, "converte fail \n" );
    return false;
  }
  else {
    //    qDebug( "out_count:%d, out_nb_samples:%d, frame->nb_samples:%d \n", out_count, out_nb_samples, frame->nb_samples );
  }

  int actual_size = av_samples_get_buffer_size( nullptr, dst_channels, out_nb_samples, AV_SAMPLE_FMT_S16, 1 );
  samples.resize( actual_size );
  return true;
}

void DecoderContext::playFrame( AVFrame * frame )
{
  if ( !frame )
    return;

  vector< uint8_t > samples;
  if ( normalizeAudio( frame, samples ) ) {
    audioOutput->play( &samples.front(), samples.size() );
  }
}

DecoderThread::DecoderThread( QByteArray const & audioData, QObject * parent ):
  QThread( parent ),
  isCancelled_( 0 ),
  audioData_( audioData ),
  d( audioData_, isCancelled_ )
{
}

DecoderThread::~DecoderThread()
{
  isCancelled_.ref();
  d.stop();
}

void DecoderThread::run()
{
  QString errorString;

  if ( !d.openCodec( errorString ) ) {
    emit error( errorString );
    return;
  }

  while ( !deviceMutex_.tryLock( 100 ) ) {
    if ( Utils::AtomicInt::loadAcquire( isCancelled_ ) )
      return;
  }

  if ( !d.openOutputDevice( errorString ) )
    emit error( errorString );
  else if ( !d.play( errorString ) )
    emit error( errorString );

  d.closeOutputDevice();
  deviceMutex_.unlock();
}

void DecoderThread::cancel( bool waitUntilFinished )
{
  isCancelled_.ref();
  d.stop();
  if ( waitUntilFinished )
    this->wait();
}

} // namespace Ffmpeg

#endif // MAKE_FFMPEG_PLAYER
