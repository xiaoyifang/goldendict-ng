#pragma once

#include <QString>

namespace Version {

const QLatin1String flags = QLatin1String(
#ifdef MAKE_ZIM_SUPPORT
  "ZIM "
#endif
#ifdef EPWING_SUPPORT
  "EPWING "
#endif
#ifdef MAKE_CHINESE_CONVERSION_SUPPORT
  "OPENCC "
#endif
#ifdef TTS_SUPPORT
  "TTS "
#endif
#ifdef MAKE_FFMPEG_PLAYER
  "FFMPEG "
#endif
);

const QLatin1String compiler = QLatin1String(
#if defined( Q_CC_MSVC )
  "MSVC " QT_STRINGIFY( _MSC_FULL_VER )
#elif defined( Q_CC_CLANG )
  "Clang " __clang_version__
#elif defined( Q_CC_GNU )
  "GCC " __VERSION__
#else
  "Unknown complier"
#endif
);
/// Version string from the version.txt
QString version();

/// Full report of version & various information
QString everything();

} // namespace Version
