/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "iconv.hh"
#include <vector>
#include <errno.h>
#include <string.h>
#include "wstring_qt.hh"

char const * const Iconv::GdWchar = "UTF-32LE";
char const * const Iconv::Utf16Le = "UTF-16LE";
char const * const Iconv::Utf8    = "UTF-8";

Iconv::Iconv( char const * from ):
  state( iconv_open( Utf8, from ) )
{
  if ( state == (iconv_t)-1 )
    throw exCantInit( strerror( errno ) );
}

Iconv::~Iconv()
{
  iconv_close( state );
}

QString Iconv::convert( void const *& inBuf, size_t & inBytesLeft )
{
  size_t dsz = inBytesLeft;
  //avoid most realloc
  std::vector< char > outBuf( dsz + 32 );

  void * outBufPtr = &outBuf.front();

  size_t outBufLeft = outBuf.size();
  size_t result;
  while ( inBytesLeft > 0 ) {
    result = iconv( state, (char **)&inBuf, &inBytesLeft, (char **)&outBufPtr, &outBufLeft );
    if ( result == (size_t)-1 ) {
      if ( errno == E2BIG || outBufLeft == 0 ) {

        if ( inBytesLeft > 0 ) {
          // Grow the buffer and retry
          // The pointer may get invalidated so we save the diff and restore it
          size_t offset = (char *)outBufPtr - &outBuf.front();
          outBuf.resize( outBuf.size() + dsz );
          outBufPtr = &outBuf.front() + offset;
          outBufLeft += dsz;
          continue;
        }
      }
      break;
    }
  }

  //flush output
  if ( result != (size_t)( -1 ) ) {
    /* flush the shift-out sequences */
    for ( ;; ) {
      result = iconv( state, NULL, NULL, (char **)&outBufPtr, &outBufLeft );

      if ( result != (size_t)( -1 ) ) {
        break;
      }

      if ( errno == E2BIG ) {
        size_t offset = (char *)outBufPtr - &outBuf.front();
        outBuf.resize( outBuf.size() + 256 );
        outBufPtr = &outBuf.front() + offset;
        outBufLeft += 256;
      }
      else {
        break;
      }
    }
  }

  size_t datasize = outBuf.size() - outBufLeft;
  //  QByteArray ba( &outBuf.front(), datasize );
  return QString::fromUtf8( &outBuf.front(), datasize );
}

gd::wstring Iconv::toWstring( char const * fromEncoding, void const * fromData, size_t dataSize )

{
  /// Special-case the dataSize == 0 to avoid any kind of iconv-specific
  /// behaviour in that regard.

  if ( dataSize == 0 ) {
    return {};
  }

  Iconv ic( fromEncoding );

  QString outStr = ic.convert( fromData, dataSize );
  return gd::toWString( outStr );
}

std::string Iconv::toUtf8( char const * fromEncoding, void const * fromData, size_t dataSize )

{
  // Similar to toWstring

  if ( dataSize == 0 ) {
    return {};
  }

  Iconv ic( fromEncoding );

  const QString outStr = ic.convert( fromData, dataSize );
  return outStr.toStdString();
}

QString Iconv::toQString( char const * fromEncoding, void const * fromData, size_t dataSize )
{
  if ( dataSize == 0 ) {
    return {};
  }

  Iconv ic( fromEncoding );
  return ic.convert( fromData, dataSize );
}
