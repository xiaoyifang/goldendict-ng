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

using gd::wchar;

Iconv::Iconv( char const * from )
#ifdef USE_ICONV
  // the to encoding must be UTF8
  :
  state( iconv_open( Utf8, from ) )
#endif
{
#ifdef USE_ICONV
  if ( state == (iconv_t)-1 )
    throw exCantInit( strerror( errno ) );
#else
  codec = QTextCodec::codecForName( from );
#endif
}

Iconv::~Iconv()
{
#ifdef USE_ICONV
  iconv_close( state );
#endif
}

QString Iconv::convert( void const *& inBuf, size_t & inBytesLeft )
{
#ifdef USE_ICONV
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
#else
  if ( codec )
    return codec->toUnicode( static_cast< const char * >( inBuf ), inBytesLeft );
  QByteArray ba( static_cast< const char * >( inBuf ), inBytesLeft );
  return QString( ba );
#endif
}

gd::wstring Iconv::toWstring( char const * fromEncoding, void const * fromData, size_t dataSize )

{
  /// Special-case the dataSize == 0 to avoid any kind of iconv-specific
  /// behaviour in that regard.

  if ( !dataSize )
    return {};

  Iconv ic( fromEncoding );

  QString outStr = ic.convert( fromData, dataSize );
  return gd::toWString( outStr );
}

std::string Iconv::toUtf8( char const * fromEncoding, void const * fromData, size_t dataSize )

{
  // Similar to toWstring

  if ( !dataSize )
    return {};

  Iconv ic( fromEncoding );

  const QString outStr = ic.convert( fromData, dataSize );
  return outStr.toStdString();
}
