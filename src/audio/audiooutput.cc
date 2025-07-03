#include "audiooutput.hh"

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

  using AudioOutput = QAudioSink;

  AudioOutput * audioOutput = nullptr;
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
    if ( !audioOutput || ( fmt.isValid() && audioOutput->format() != fmt )
         || audioOutput->state() == QAudio::StoppedState ) {
      if ( audioOutput ) {
        delete audioOutput;
        audioOutput = nullptr;
      }
      audioOutput = new AudioOutput( fmt );
      QObject::connect( audioOutput, &AudioOutput::stateChanged, audioOutput, [ & ]( QAudio::State state ) {
        switch ( state ) {
          case QAudio::StoppedState:
            quit = true;

            if ( audioOutput->error() != QAudio::NoError ) {
              qWarning() << "QAudioOutput stopped:" << audioOutput->error();
            }
            break;
          default:
            break;
        }
      } );

      audioOutput->start( this );
      if ( audioOutput && audioOutput->state() == QAudio::StoppedState ) {
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
    if ( audioOutput ) {
      audioOutput->stop();
      delete audioOutput;
    }
    audioOutput = nullptr;
  }
};

void AudioOutput::stop()
{
  Q_D( AudioOutput );
  d->quit = true;
  d->cond.wakeAll();
  d->audioPlayFuture.waitForFinished();
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
  Q_D( AudioOutput );
  d->quit = true;
  d->cond.wakeAll();
  d->audioPlayFuture.waitForFinished();
}

bool AudioOutput::play( const uint8_t * data, qint64 len )
{
  Q_D( AudioOutput );
  if ( d->quit ) {
    return false;
  }

  QMutexLocker locker( &d->mutex );
  auto cuint = const_cast< uint8_t * >( data );
  auto cptr  = reinterpret_cast< char * >( cuint );
  d->buffer.append( cptr, len );
  d->cond.wakeAll();

  return true;
}
