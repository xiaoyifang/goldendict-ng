/* This file is (c) 2014 Abs62
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "epwing_charmap.hh"

namespace Epwing {

EpwingCharmap & EpwingCharmap::instance()
{
  static EpwingCharmap ec;
  return ec;
}

QByteArray EpwingCharmap::mapToUtf8( QString const & code )
{
  if ( charMap.contains( code ) )
    return QString( charMap[ code ] ).toUtf8();

  return QByteArray();
}

void EpwingCharmap::addEntry( QString const & code, int ch )
{
  charMap[ code ] = QChar( ch );
}

} // namespace Epwing
