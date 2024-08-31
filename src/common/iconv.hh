/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#ifndef __ICONV_HH_INCLUDED__
#define __ICONV_HH_INCLUDED__

#include <QString>

#include "wstring.hh"
#include "ex.hh"

#include <iconv.h>


/// "Internationalization conversion" for char encoding conversion, currently implemented with iconv()
/// Only supports converting from a known "from" to UTF8
class Iconv
{
  iconv_t state;

public:

  DEF_EX( Ex, "Iconv exception", std::exception )
  DEF_EX_STR( exCantInit, "Can't initialize iconv conversion:", Ex )

  // Some predefined character sets' names

  static char const * const GdWchar;
  static char const * const Utf16Le;
  static char const * const Utf8;

  explicit Iconv( char const * from );

  ~Iconv();

  QString convert( void const *& inBuf, size_t & inBytesLeft );

  // Converts a given block of data from the given encoding to a wide string.
  static gd::wstring toWstring( char const * fromEncoding, void const * fromData, size_t dataSize );

  // Converts a given block of data from the given encoding to an utf8-encoded
  // string.
  static std::string toUtf8( char const * fromEncoding, void const * fromData, size_t dataSize );

  static QString toQString( char const * fromEncoding, void const * fromData, size_t dataSize );

  // Copying/assigning isn't supported
  Q_DISABLE_COPY_MOVE( Iconv );
};

#endif
