/* This file is (c) 2014 Abs62
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#ifdef EPWING_SUPPORT

  #include "epwing_charmap.hh"

namespace Epwing {

EpwingCharmap & EpwingCharmap::instance()
{
  static EpwingCharmap ec;
  return ec;
}

QByteArray EpwingCharmap::mapToUtf8( const QString & code )
{
  if ( charMap.contains( code ) )
    return QString( charMap[ code ] ).toUtf8();

  return QByteArray();
}

void EpwingCharmap::addEntry( const QString & code, int ch )
{
  charMap[ code ] = QChar( ch );
}

} // namespace Epwing

#endif
