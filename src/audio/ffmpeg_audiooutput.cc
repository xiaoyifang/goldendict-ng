#include "ffmpeg_audiooutput.hh"

#include <QtConcurrentRun>
#include <QFuture>
#include <QWaitCondition>
#include <QCoreApplication>
#include <QThreadPool>
#include <QAudioSink>
#include <QtGlobal>
#include <QBuffer>

// fixes #2272 issue's error on use of qWarning for compiling against qt 6.9.0
#include <QDebug>

// take reference from this file (https://github.com/valbok/QtAVPlayer/blob/6cc30e484b354d59511c9a60fabced4cb7c57c8e/src/QtAVPlayer/qavaudiooutput.cpp)
// and make some changes.

static QAudioFormat format( int sampleRate, int channelCount )
{
  QAudioFormat out;

  out.setSampleRate( sampleRate );
  out.setChannelCount( channelCount );
  out.setSampleFormat( QAudioFormat::Int16 );

  return out;
}

class AudioOutputPrivate: public QIODevice
{
public:
  AudioOutputPrivate()
  {
    open( QIODevice::ReadOnly );
    threadPool.setMaxThreadCount( 1 );
  }

  QFuture< void > audioPlayFuture;

  QAudioSink * qAudioSink = nullptr;
  QByteArray buffer;
  qint64 offset = 0;
  bool quit     = false;
  QMutex mutex;
  QWaitCondition cond;
  QThreadPool threadPool;
  int sampleRate = 0;
  int channels   = 0;

  void setAudioFormat( int _sampleRate, int _channels )
  {
    sampleRate = _sampleRate;
    channels   = _channels;
  }

  qint64 readData( char * data, qint64 len ) override
  {
    if ( !len ) {
      return 0;
    }

    QMutexLocker locker( &mutex );
    qint64 bytesWritten = 0;
    while ( len && !quit ) {
      if ( buffer.isEmpty() ) {
        // Wait for more frames
        if ( bytesWritten == 0 ) {
          cond.wait( &mutex );
        }
        if ( buffer.isEmpty() ) {
          break;
        }
      }

      auto sampleData   = buffer.data();
      const int toWrite = qMin( (qint64)buffer.size(), len );
      memcpy( &data[ bytesWritten ], sampleData, toWrite );
      buffer.remove( 0, toWrite );
      bytesWritten += toWrite;
      len -= toWrite;
    }

    return bytesWritten;
  }

  qint64 writeData( const char *, qint64 ) override
  {
    return 0;
  }
  qint64 size() const override
  {
    return buffer.size();
  }
  qint64 bytesAvailable() const override
  {
    return buffer.size();
  }
  bool isSequential() const override
  {
    return true;
  }
  bool atEnd() const override
  {
    return buffer.isEmpty();
  }

  void init( const QAudioFormat & fmt )
  {
    if ( !qAudioSink || ( fmt.isValid() && qAudioSink->format() != fmt )
         || qAudioSink->state() == QAudio::StoppedState ) {
      if ( qAudioSink ) {
        delete qAudioSink;
        qAudioSink = nullptr;
      }
      qAudioSink = new QAudioSink( fmt );
      QObject::connect( qAudioSink, &QAudioSink::stateChanged, qAudioSink, [ & ]( QAudio::State state ) {
        switch ( state ) {
          case QAudio::StoppedState:
            quit = true;

            if ( qAudioSink->error() != QAudio::NoError ) {
              qWarning() << "QAudioOutput stopped:" << qAudioSink->error();
            }
            break;
          default:
            break;
        }
      } );

      qAudioSink->start( this );
      if ( qAudioSink && qAudioSink->state() == QAudio::StoppedState ) {
        quit = true;
      }
    }

    //    audioOutput->setVolume(volume);
  }

  void doPlayAudio()
  {
    while ( !quit ) {
      QMutexLocker locker( &mutex );
      cond.wait( &mutex, 10 );
      auto fmt = sampleRate == 0 ? QAudioFormat() : format( sampleRate, channels );
      locker.unlock();
      if ( fmt.isValid() ) {
        init( fmt );
      }
      QCoreApplication::processEvents();
    }
    if ( qAudioSink ) {
      qAudioSink->stop();
      delete qAudioSink;
    }
    qAudioSink = nullptr;
  }

  void stop()
  {
    quit = true;
    cond.wakeAll();
    audioPlayFuture.waitForFinished();
  }
};

void AudioOutput::stop()
{
  d_ptr->stop();
}

AudioOutput::AudioOutput( QObject * parent ):
  QObject( parent ),
  d_ptr( new AudioOutputPrivate )
{
  d_ptr->audioPlayFuture = QtConcurrent::run( &d_ptr->threadPool, &AudioOutputPrivate::doPlayAudio, d_ptr.data() );
}

void AudioOutput::setAudioFormat( int sampleRate, int channels )
{
  d_ptr->setAudioFormat( sampleRate, channels );
}

AudioOutput::~AudioOutput()
{
  d_ptr->stop();
}

bool AudioOutput::play( const uint8_t * data, qint64 len )
{
  if ( d_ptr->quit ) {
    return false;
  }

  QMutexLocker locker( &d_ptr->mutex );
  auto * cuint = const_cast< uint8_t * >( data );
  auto * cptr  = reinterpret_cast< char * >( cuint );
  d_ptr->buffer.append( cptr, len );
  d_ptr->cond.wakeAll();

  return true;
}
