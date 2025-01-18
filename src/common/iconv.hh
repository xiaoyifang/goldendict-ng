/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "ex.hh"
#include "text.hh"
#include <QString>
#include <QStringList>
#include <iconv.h>

/// "Internationalization conversion" for char encoding conversion, currently implemented with iconv()
/// Only supports converting from a known "from" to UTF8
class Iconv
{
  iconv_t state;

public:

  DEF_EX( Ex, "Iconv exception", std::exception )
  DEF_EX_STR( exCantInit, "Can't initialize iconv conversion:", Ex )

  explicit Iconv( char const * from );

  ~Iconv();
  static QByteArray fromUnicode( const QString & input, const char * toEncoding );

  QString convert( void const *& inBuf, size_t & inBytesLeft );

  // Converts a given block of data from the given encoding to a wide string.
  static std::u32string toWstring( char const * fromEncoding, void const * fromData, size_t dataSize );

  // Converts a given block of data from the given encoding to an utf8-encoded
  // string.
  static std::string toUtf8( char const * fromEncoding, void const * fromData, size_t dataSize );
  static std::string toUtf8( char const * fromEncoding, std::u32string_view str );

  static QString toQString( char const * fromEncoding, void const * fromData, size_t dataSize );
  // tries to find a valid encoding from the given list of encodings.
  static QString findValidEncoding( const QStringList & encodings );
  // Copying/assigning isn't supported
  Q_DISABLE_COPY_MOVE( Iconv );
};
