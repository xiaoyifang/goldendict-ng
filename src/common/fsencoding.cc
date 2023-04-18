/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "fsencoding.hh"
#include "wstring_qt.hh"
#include <QString>
#include <QDir>

namespace FsEncoding {

char separator()
{
  return QDir::separator().toLatin1();
}

string basename( string const & str )
{
  size_t x = str.rfind( separator() );

  if ( x == string::npos )
    return str;

  return string( str, x + 1 );
}

}
