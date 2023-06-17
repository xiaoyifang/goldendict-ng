#pragma once

#include <QString>

namespace Version {

const QLatin1String flags = QLatin1String(
  "USE_XAPIAN"
#ifdef MAKE_ZIM_SUPPORT
  " MAKE_ZIM_SUPPORT"
#endif
#ifdef NO_EPWING_SUPPORT
  " NO_EPWING_SUPPORT"
#endif
#ifdef USE_ICONV
  " USE_ICONV"
#endif
#ifdef MAKE_CHINESE_CONVERSION_SUPPORT
  " MAKE_CHINESE_CONVERSION_SUPPORT"
#endif
);

const QLatin1String compiler = QLatin1String(
#if defined( Q_CC_MSVC )
  "Visual C++ Compiler " QT_STRINGIFY( _MSC_FULL_VER )
#elif  defined( Q_CC_CLANG )
  "Clang " __clang_version__
#elif  defined( Q_CC_GNU )
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
