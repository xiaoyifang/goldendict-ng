/* This file is (c) 2018 Igor Kushnir <igorkuo@gmail.com>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#ifdef MAKE_QTMULTIMEDIA_PLAYER

  #include <QByteArray>
  #include <QAudioDevice>
  #include "multimediaaudioplayer.hh"

  #include <QDebug>

MultimediaAudioPlayer::MultimediaAudioPlayer()
{
  audioOutput.setDevice( QMediaDevices::defaultAudioOutput() );
  player.setAudioOutput( &audioOutput );

  connect( &player, &QMediaPlayer::errorChanged, this, &MultimediaAudioPlayer::onMediaPlayerError );

  connect( &mediaDevices, &QMediaDevices::audioOutputsChanged, this, &MultimediaAudioPlayer::audioOutputChange );
}

void MultimediaAudioPlayer::audioOutputChange()
{
  qDebug() << "audio device changed";
  audioOutput.setDevice( QMediaDevices::defaultAudioOutput() );
}

QString MultimediaAudioPlayer::play( const char * data, int size )
{
  stop();
  audioBuffer.reset( new QBuffer() );
  audioBuffer->setData( data, size );
  if ( !audioBuffer->open( QIODevice::ReadOnly ) ) {
    return tr( "Couldn't open audio buffer for reading." );
  }
  player.setSourceDevice( audioBuffer.get() );

  player.play();
  return {};
}

void MultimediaAudioPlayer::stop()
{
  player.stop();

  if ( !audioBuffer.isNull() ) {
    audioBuffer->close();
    audioBuffer.reset( nullptr );
  }
}

void MultimediaAudioPlayer::onMediaPlayerError()
{
  emit error( player.errorString() );
}

#endif // MAKE_QTMULTIMEDIA_PLAYER
