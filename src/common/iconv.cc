/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "iconv.hh"
#include <vector>
#include <errno.h>
#include <string.h>

Iconv::Iconv( char const * from ):
  state( iconv_open( Text::utf8, from ) )
{
  if ( state == (iconv_t)-1 ) {
    throw exCantInit( strerror( errno ) );
  }
}

Iconv::~Iconv()
{
  iconv_close( state );
}

QByteArray Iconv::fromUnicode( const QString & input, const char * toEncoding )
{
  // Convert QString to UTF-8
  QByteArray utf8Data = input.toUtf8();
  const char * inBuf  = utf8Data.constData();
  size_t inBytesLeft  = utf8Data.size();

  // Initialize iconv
  iconv_t cd = iconv_open( toEncoding, "UTF-8" );
  if ( cd == (iconv_t)-1 ) {
    throw std::runtime_error( "iconv_open failed" );
  }

  // Prepare output buffer
  size_t outBytesLeft = inBytesLeft * 4; // Allocate enough space
  std::vector< char > outBuf( outBytesLeft );
  char * outBufPtr = outBuf.data();

  // Perform conversion
  while ( inBytesLeft > 0 ) {
    size_t result = iconv( cd, const_cast< char ** >( &inBuf ), &inBytesLeft, &outBufPtr, &outBytesLeft );
    if ( result == (size_t)-1 ) {
      if ( errno == E2BIG ) {
        // Grow the buffer and retry
        size_t offset = outBufPtr - outBuf.data();
        outBuf.resize( outBuf.size() + inBytesLeft * 4 );
        outBufPtr = outBuf.data() + offset;
        outBytesLeft += inBytesLeft * 4;
      }
      else {
        iconv_close( cd );
        throw std::runtime_error( "iconv conversion failed" );
      }
    }
  }

  // Clean up
  iconv_close( cd );

  // Resize output buffer to actual size
  outBuf.resize( outBuf.size() - outBytesLeft );
  return QByteArray( outBuf.data(), outBuf.size() );
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

std::u32string Iconv::toWstring( char const * fromEncoding, void const * fromData, size_t dataSize )

{
  /// Special-case the dataSize == 0 to avoid any kind of iconv-specific
  /// behaviour in that regard.

  if ( dataSize == 0 ) {
    return {};
  }

  Iconv ic( fromEncoding );

  QString outStr = ic.convert( fromData, dataSize );
  return outStr.toStdU32String();
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

std::string Iconv::toUtf8( char const * fromEncoding, std::u32string_view str )
{
  // u32string::size -> returns the number of char32_t instead of the length of bytes
  return toUtf8( fromEncoding, str.data(), str.size() * sizeof( char32_t ) );
}

QString Iconv::toQString( char const * fromEncoding, void const * fromData, size_t dataSize )
{
  if ( dataSize == 0 ) {
    return {};
  }

  Iconv ic( fromEncoding );
  return ic.convert( fromData, dataSize );
}
QString Iconv::findValidEncoding( const QStringList & encodings )
{
  for ( const QString & encoding : encodings ) {
    iconv_t cd = iconv_open( "UTF-8", encoding.toUtf8().constData() );
    if ( cd != (iconv_t)-1 ) {
      iconv_close( cd );
      return encoding;
    }
  }
  return {};
}
