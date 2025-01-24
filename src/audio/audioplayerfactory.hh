/* This file is (c) 2018 Igor Kushnir <igorkuo@gmail.com>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "audioplayerinterface.hh"
#include "internalplayerbackend.hh"

class ExternalAudioPlayer;

class AudioPlayerFactory
{
  Q_DISABLE_COPY( AudioPlayerFactory )

public:
  explicit AudioPlayerFactory( bool useInternalPlayer,
                               InternalPlayerBackend internalPlayerBackend,
                               QString audioPlaybackProgram );
  void setPreferences( bool new_useInternalPlayer,
                       const InternalPlayerBackend & new_internalPlayerBackend,
                       const QString & new_audioPlaybackProgram );
  /// The returned reference to a smart pointer is valid as long as this object
  /// exists. The pointer to the owned AudioPlayerInterface may change after the
  /// call to setPreferences(), but it is guaranteed to never be null.
  AudioPlayerPtr const & player() const
  {
    return playerPtr;
  }

private:
  void reset();
  void setAudioPlaybackProgram( ExternalAudioPlayer & externalPlayer );

  bool useInternalPlayer;
  InternalPlayerBackend internalPlayerBackend;
  QString audioPlaybackProgram;
  AudioPlayerPtr playerPtr;
};
