#include "internalplayerbackend.hh"

bool InternalPlayerBackend::anyAvailable()
{
#if defined( MAKE_FFMPEG_PLAYER ) || defined( MAKE_QTMULTIMEDIA_PLAYER )
  return true;
#else
  return false;
#endif
}

InternalPlayerBackend InternalPlayerBackend::defaultBackend()
{
#if defined( MAKE_FFMPEG_PLAYER )
  return ffmpeg();
#elif defined( MAKE_QTMULTIMEDIA_PLAYER )
  return qtmultimedia();
#else
  return InternalPlayerBackend( QString() );
#endif
}

QStringList InternalPlayerBackend::nameList()
{
  QStringList result;
#ifdef MAKE_FFMPEG_PLAYER
  result.push_back( ffmpeg().uiName() );
#endif
#ifdef MAKE_QTMULTIMEDIA_PLAYER
  result.push_back( qtmultimedia().uiName() );
#endif
  return result;
}

bool InternalPlayerBackend::isFfmpeg() const
{
#ifdef MAKE_FFMPEG_PLAYER
  return *this == ffmpeg();
#else
  return false;
#endif
}

bool InternalPlayerBackend::isQtmultimedia() const
{
#ifdef MAKE_QTMULTIMEDIA_PLAYER
  return *this == qtmultimedia();
#else
  return false;
#endif
}
