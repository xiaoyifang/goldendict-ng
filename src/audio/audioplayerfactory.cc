/* This file is (c) 2018 Igor Kushnir <igorkuo@gmail.com>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include <QScopedPointer>
#include <QObject>
#include <utility>
#include "audioplayerfactory.hh"
#include "ffmpegaudioplayer.hh"
#include "multimediaaudioplayer.hh"
#include "externalaudioplayer.hh"

AudioPlayerFactory::AudioPlayerFactory( bool useInternalPlayer,
                                        InternalPlayerBackend internalPlayerBackend,
                                        QString audioPlaybackProgram ):
  useInternalPlayer( useInternalPlayer ),
  internalPlayerBackend( std::move( internalPlayerBackend ) ),
  audioPlaybackProgram( std::move( audioPlaybackProgram ) )
{
  reset();
}

void AudioPlayerFactory::setPreferences( bool new_useInternalPlayer,
                                         const InternalPlayerBackend & new_internalPlayerBackend,
                                         const QString & new_audioPlaybackProgram )
{
  if ( useInternalPlayer != new_useInternalPlayer ) {
    useInternalPlayer     = new_useInternalPlayer;
    internalPlayerBackend = new_internalPlayerBackend;
    audioPlaybackProgram  = new_audioPlaybackProgram;
    reset();
  }
  else if ( useInternalPlayer && internalPlayerBackend != new_internalPlayerBackend ) {
    internalPlayerBackend = new_internalPlayerBackend;
    reset();
  }
  else if ( !useInternalPlayer && new_audioPlaybackProgram != audioPlaybackProgram ) {
    audioPlaybackProgram                       = new_audioPlaybackProgram;
    ExternalAudioPlayer * const externalPlayer = qobject_cast< ExternalAudioPlayer * >( playerPtr.data() );
    if ( externalPlayer ) {
      setAudioPlaybackProgram( *externalPlayer );
    }
    else {
      qWarning( "External player was expected, but it does not exist." );
    }
  }
}

void AudioPlayerFactory::reset()
{
  if ( useInternalPlayer ) {
    // qobject_cast checks below account for the case when an unsupported backend
    // is stored in config. After this backend is replaced with the default one
    // upon preferences saving, the code below does not reset playerPtr with
    // another object of the same type.

#ifdef MAKE_FFMPEG_PLAYER
    Q_ASSERT( InternalPlayerBackend::defaultBackend().isFfmpeg()
              && "Adjust the code below after changing the default backend." );

    if ( !internalPlayerBackend.isQtmultimedia() ) {
      if ( !playerPtr || !qobject_cast< Ffmpeg::AudioPlayer * >( playerPtr.data() ) ) {
        playerPtr.reset( new Ffmpeg::AudioPlayer );
      }
      return;
    }
#endif

#ifdef MAKE_QTMULTIMEDIA_PLAYER
    if ( !playerPtr || !qobject_cast< MultimediaAudioPlayer * >( playerPtr.data() ) ) {
      playerPtr.reset( new MultimediaAudioPlayer );
    }
    return;
#endif
  }

  std::unique_ptr< ExternalAudioPlayer > externalPlayer( new ExternalAudioPlayer );
  setAudioPlaybackProgram( *externalPlayer );
  playerPtr.reset( externalPlayer.release() );
}

void AudioPlayerFactory::setAudioPlaybackProgram( ExternalAudioPlayer & externalPlayer )
{
  externalPlayer.setPlayerCommandLine( audioPlaybackProgram.trimmed() );
}
