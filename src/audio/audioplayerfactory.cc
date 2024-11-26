/* This file is (c) 2018 Igor Kushnir <igorkuo@gmail.com>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include <QScopedPointer>
#include <QObject>
#include <utility>
#include "audioplayerfactory.hh"
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
    playerPtr.reset( internalPlayerBackend.getActualPlayer() );
    return;
  }

  std::unique_ptr< ExternalAudioPlayer > externalPlayer( new ExternalAudioPlayer );
  setAudioPlaybackProgram( *externalPlayer );
  playerPtr.reset( externalPlayer.release() );
}

void AudioPlayerFactory::setAudioPlaybackProgram( ExternalAudioPlayer & externalPlayer )
{
  externalPlayer.setPlayerCommandLine( audioPlaybackProgram.trimmed() );
}
