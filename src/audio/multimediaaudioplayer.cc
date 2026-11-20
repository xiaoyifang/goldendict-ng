/* This file is (c) 2018 Igor Kushnir <igorkuo@gmail.com>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#ifdef MAKE_QTMULTIMEDIA_PLAYER

  #include <QAudioDevice>
  #include <QAudioOutput>
  #include <QBuffer>
  #include <QMediaDevices>
  #include <QMediaPlayer>
  #include "multimediaaudioplayer.hh"

  #include <QDebug>

MultimediaAudioPlayer::MultimediaAudioPlayer()
{
  releaseTimer.setSingleShot( true );
  releaseTimer.setInterval( 0 );
  connect( &releaseTimer, &QTimer::timeout, this, &MultimediaAudioPlayer::destroyPlayer );
}

MultimediaAudioPlayer::~MultimediaAudioPlayer()
{
  releaseTimer.stop();
  destroyPlayer();
}

void MultimediaAudioPlayer::createPlayer()
{
  if ( player ) {
    return;
  }

  audioOutput.reset( new QAudioOutput );
  audioOutput->setDevice( QMediaDevices::defaultAudioOutput() );

  player.reset( new QMediaPlayer );
  player->setAudioOutput( audioOutput.data() );
  connect( player.data(), &QMediaPlayer::errorChanged, this, &MultimediaAudioPlayer::onMediaPlayerError );
  connect( player.data(),
           &QMediaPlayer::mediaStatusChanged,
           this,
           [ this ]( QMediaPlayer::MediaStatus status ) {
             if ( status == QMediaPlayer::EndOfMedia ) {
               // Release the audio device once playback ends, but never destroy
               // the player from within its own signal handler.
               releaseTimer.start();
             }
           } );

  mediaDevices.reset( new QMediaDevices );
  connect( mediaDevices.data(), &QMediaDevices::audioOutputsChanged, this, &MultimediaAudioPlayer::audioOutputChange );
}

void MultimediaAudioPlayer::destroyPlayer()
{
  if ( player ) {
    player->disconnect( this );
    player->stop();
    player->setSourceDevice( nullptr );
    player->setSource( QUrl() );
    player->setAudioOutput( nullptr );
  }
  if ( !audioBuffer.isNull() ) {
    audioBuffer->close();
    audioBuffer.reset( nullptr );
  }
  player.reset( nullptr );
  audioOutput.reset( nullptr );
  mediaDevices.reset( nullptr );
}

void MultimediaAudioPlayer::audioOutputChange()
{
  const QAudioDevice newDevice = QMediaDevices::defaultAudioOutput();
  if ( !audioOutput || newDevice == audioOutput->device() ) {
    // OS may emit several device notifications for a single plug/unplug
    // event; only react when the default output actually changes.
    return;
  }
  qDebug() << "audio device changed";
  audioOutput->setDevice( newDevice );
}

QString MultimediaAudioPlayer::play( const char * data, int size )
{
  stop();
  createPlayer();

  audioBuffer.reset( new QBuffer() );
  audioBuffer->setData( data, size );
  if ( !audioBuffer->open( QIODevice::ReadOnly ) ) {
    destroyPlayer();
    return tr( "Couldn't open audio buffer for reading." );
  }
  player->setSourceDevice( audioBuffer.data() );

  player->play();
  return {};
}

void MultimediaAudioPlayer::stop()
{
  releaseTimer.stop();
  destroyPlayer();
}

void MultimediaAudioPlayer::onMediaPlayerError()
{
  if ( player->error() == QMediaPlayer::NoError ) {
    return;
  }
  emit error( player->errorString() );
  // Do not keep the audio device busy after a failed playback.
  releaseTimer.start();
}

#endif // MAKE_QTMULTIMEDIA_PLAYER
