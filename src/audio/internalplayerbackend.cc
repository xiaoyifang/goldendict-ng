#include "internalplayerbackend.hh"
#include "ffmpegaudioplayer.hh"
#include "multimediaaudioplayer.hh"

#ifdef MAKE_FFMPEG_PLAYER
constexpr auto ffmpeg = "FFmpeg";
#endif

#ifdef MAKE_QTMULTIMEDIA_PLAYER
constexpr auto qtmultimedia = "Qt Multimedia";
#endif

bool InternalPlayerBackend::anyAvailable()
{
#if defined( MAKE_FFMPEG_PLAYER ) || defined( MAKE_QTMULTIMEDIA_PLAYER )
  return true;
#else
  static_assert( false, "No audio player backend. Please enable one." );
  return false;
#endif
}

QStringList InternalPlayerBackend::availableBackends()
{
  QStringList result;
#ifdef MAKE_QTMULTIMEDIA_PLAYER
  result.push_back( qtmultimedia );
#endif
#ifdef MAKE_FFMPEG_PLAYER
  result.push_back( ffmpeg );
#endif
  return result;
}
AudioPlayerInterface * InternalPlayerBackend::getActualPlayer()
{
  // The one in user's config is not availiable,
  // fall back to the default one
  if ( name.isEmpty() || !availableBackends().contains( name ) ) {
    name = availableBackends().constFirst();
  }

#ifdef MAKE_FFMPEG_PLAYER
  if ( name == ffmpeg ) {
    return new Ffmpeg::AudioPlayer();
  };
#endif
#ifdef MAKE_QTMULTIMEDIA_PLAYER
  if ( name == qtmultimedia ) {
    return new MultimediaAudioPlayer();
  };
#endif
  qCritical( "Impossible situation. If ever reached, fix elsewhere. " );
  return nullptr;
}
