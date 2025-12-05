/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "text.hh"
#include <vector>
#include <algorithm>
#include <QByteArray>
#include <QList>

namespace Text {

const char * getEncodingNameFor( Encoding e )
{
  switch ( e ) {
    case Encoding::Utf32LE:
      return utf32_le;
    case Encoding::Utf32BE:
      return utf32_be;
    case Encoding::Utf32:
      return utf32;
    case Encoding::Utf16LE:
      return utf16_le;
    case Encoding::Utf16BE:
      return utf16_be;
    case Encoding::Windows1252:
      return windows_1252;
    case Encoding::Windows1251:
      return windows_1251;
    case Encoding::Windows1250:
      return windows_1250;
    case Encoding::Utf8:
    default:
      return utf8;
  }
}

Encoding getEncodingForName( const QByteArray & name )
{
  const auto n = name.toUpper();
  if ( n == utf32_le ) {
    return Encoding::Utf32LE;
  }
  if ( n == utf32_be ) {
    return Encoding::Utf32BE;
  }
  if ( n == utf32 ) {
    return Encoding::Utf32;
  }
  if ( n == utf16_le ) {
    return Encoding::Utf16LE;
  }
  if ( n == utf16_be ) {
    return Encoding::Utf16BE;
  }
  if ( n == windows_1252 ) {
    return Encoding::Windows1252;
  }
  if ( n == windows_1251 ) {
    return Encoding::Windows1251;
  }
  if ( n == windows_1250 ) {
    return Encoding::Windows1250;
  }
  return Encoding::Utf8;
}

/// Encodes the given UTF-32 into UTF-8. The inSize specifies the number
/// of wide characters the 'in' pointer points to. The 'out' buffer must be
/// at least inSize * 4 bytes long. The function returns the number of chars
/// stored in the 'out' buffer. The result is not 0-terminated.
size_t encode( const char32_t * in, size_t inSize, char * out_ )
{
  unsigned char * out = (unsigned char *)out_;

  while ( inSize-- ) {
    if ( *in < 0x80 ) {
      *out++ = *in++;
    }
    else if ( *in < 0x800 ) {
      *out++ = 0xC0 | ( *in >> 6 );
      *out++ = 0x80 | ( *in++ & 0x3F );
    }
    else if ( *in < 0x10000 ) {
      *out++ = 0xE0 | ( *in >> 12 );
      *out++ = 0x80 | ( ( *in >> 6 ) & 0x3F );
      *out++ = 0x80 | ( *in++ & 0x3F );
    }
    else {
      *out++ = 0xF0 | ( *in >> 18 );
      *out++ = 0x80 | ( ( *in >> 12 ) & 0x3F );
      *out++ = 0x80 | ( ( *in >> 6 ) & 0x3F );
      *out++ = 0x80 | ( *in++ & 0x3F );
    }
  }

  return out - (unsigned char *)out_;
}

/// Decodes the given UTF-8 into UTF-32. The inSize specifies the number
/// of bytes the 'in' pointer points to. The 'out' buffer must be at least
/// inSize wide characters long. If the given UTF-8 is invalid, the decode
/// function returns -1, otherwise it returns the number of wide characters
/// stored in the 'out' buffer. The result is not 0-terminated.
long decode( const char * in_, size_t inSize, char32_t * out_ )
{
  const unsigned char * in = (const unsigned char *)in_;
  char32_t * out           = out_;

  while ( inSize-- ) {
    char32_t result;

    if ( *in & 0x80 ) {
      if ( *in & 0x40 ) {
        if ( *in & 0x20 ) {
          if ( *in & 0x10 ) {
            // Four-byte sequence
            if ( *in & 8 ) {
              // This can't be
              return -1;
            }

            if ( inSize < 3 ) {
              return -1;
            }

            inSize -= 3;

            result = ( (char32_t)*in++ & 7 ) << 18;

            if ( ( *in & 0xC0 ) != 0x80 ) {
              return -1;
            }
            result |= ( (char32_t)*in++ & 0x3F ) << 12;

            if ( ( *in & 0xC0 ) != 0x80 ) {
              return -1;
            }
            result |= ( (char32_t)*in++ & 0x3F ) << 6;

            if ( ( *in & 0xC0 ) != 0x80 ) {
              return -1;
            }
            result |= (char32_t)*in++ & 0x3F;
          }
          else {
            // Three-byte sequence

            if ( inSize < 2 ) {
              return -1;
            }

            inSize -= 2;

            result = ( (char32_t)*in++ & 0xF ) << 12;

            if ( ( *in & 0xC0 ) != 0x80 ) {
              return -1;
            }
            result |= ( (char32_t)*in++ & 0x3F ) << 6;

            if ( ( *in & 0xC0 ) != 0x80 ) {
              return -1;
            }
            result |= (char32_t)*in++ & 0x3F;
          }
        }
        else {
          // Two-byte sequence
          if ( !inSize ) {
            return -1;
          }

          --inSize;

          result = ( (char32_t)*in++ & 0x1F ) << 6;

          if ( ( *in & 0xC0 ) != 0x80 ) {
            return -1;
          }
          result |= (char32_t)*in++ & 0x3F;
        }
      }
      else {
        // This char is from the middle of encoding, it can't be leading
        return -1;
      }
    }
    else {
      // One-byte encoding
      result = *in++;
    }

    *out++ = result;
  }

  return out - out_;
}

std::string toUtf8( const std::u32string & in ) noexcept
{
  if ( in.empty() ) {
    return {};
  }

  std::vector< char > buffer( in.size() * 4 );

  return { &buffer.front(), encode( in.data(), in.size(), &buffer.front() ) };
}

std::u32string toUtf32( const std::string & in )
{
  if ( in.empty() ) {
    return {};
  }

  std::vector< char32_t > buffer( in.size() );

  long result = decode( in.data(), in.size(), &buffer.front() );

  if ( result < 0 ) {
    throw exCantDecode( in );
  }

  return std::u32string( &buffer.front(), result );
}

bool isspace( int c )
{
  switch ( c ) {
    case ' ':
    case '\f':
    case '\n':
    case '\r':
    case '\t':
    case '\v':
      return true;

    default:
      return false;
  }
}

//get the first line in string s1. -1 if not found
int findFirstLinePosition( char * s1, int s1length, const char * s2, int s2length )
{
  char * pos = std::search( s1, s1 + s1length, s2, s2 + s2length );

  if ( pos == s1 + s1length ) {
    return pos - s1;
  }

  //the line size.
  return pos - s1 + s2length;
}


LineFeed initLineFeed( const Encoding e )
{
  LineFeed lf{};
  switch ( e ) {
    case Encoding::Utf32LE:
      lf.lineFeed = new char[ 4 ]{ 0x0A, 0, 0, 0 };
      lf.length   = 4;
      break;
    case Encoding::Utf32BE:
      lf.lineFeed = new char[ 4 ]{ 0, 0, 0, 0x0A };
      lf.length   = 4;
      break;
    case Encoding::Utf16LE:
      lf.lineFeed = new char[ 2 ]{ 0x0A, 0 };
      lf.length   = 2;
      break;
    case Encoding::Utf16BE:
      lf.lineFeed = new char[ 2 ]{ 0, 0x0A };
      lf.length   = 2;
      break;
    case Encoding::Windows1252:
    case Encoding::Windows1251:
    case Encoding::Windows1250:
    case Encoding::Utf8:
    default:
      lf.length   = 1;
      lf.lineFeed = new char[ 1 ]{ 0x0A };
  }
  return lf;
}

// When convert non-BMP characters to wstring,the ending char maybe \0 .This method remove the tailing \0 from the wstring
// as \0 is sensitive in the index.  This method will be only used with index related operations like store/query.
std::u32string removeTrailingZero( const std::u32string & v )
{
  int n = v.size();
  while ( n > 0 && v[ n - 1 ] == 0 ) {
    n--;
  }
  return std::u32string( v.data(), n );
}

std::u32string removeTrailingZero( const QString & in )
{
  QList< unsigned int > v = in.toUcs4();

  int n = v.size();
  while ( n > 0 && v[ n - 1 ] == 0 ) {
    n--;
  }
  if ( n != v.size() ) {
    v.resize( n );
  }

  return std::u32string( (const char32_t *)v.constData(), v.size() );
}

std::u32string normalize( const std::u32string & str )
{
  return QString::fromStdU32String( str ).normalized( QString::NormalizationForm_C ).toStdU32String();
}


std::string detectEncodingFromBom( const char * data, size_t size )
{
  if ( !data || size == 0 ) {
    return {};
  }

  const unsigned char * bytes = reinterpret_cast< const unsigned char * >( data );

  // Check for UTF-32 BOMs (4 bytes) - must check before UTF-16
  if ( size >= 4 ) {
    // UTF-32 LE: FF FE 00 00
    if ( bytes[ 0 ] == 0xFF && bytes[ 1 ] == 0xFE && bytes[ 2 ] == 0x00 && bytes[ 3 ] == 0x00 ) {
      return utf32_le;
    }
    // UTF-32 BE: 00 00 FE FF
    if ( bytes[ 0 ] == 0x00 && bytes[ 1 ] == 0x00 && bytes[ 2 ] == 0xFE && bytes[ 3 ] == 0xFF ) {
      return utf32_be;
    }
  }

  // Check for UTF-8 BOM (3 bytes)
  if ( size >= 3 ) {
    // UTF-8: EF BB BF
    if ( bytes[ 0 ] == 0xEF && bytes[ 1 ] == 0xBB && bytes[ 2 ] == 0xBF ) {
      return utf8;
    }
  }

  // Check for UTF-16 BOMs (2 bytes)
  if ( size >= 2 ) {
    // UTF-16 LE: FF FE
    if ( bytes[ 0 ] == 0xFF && bytes[ 1 ] == 0xFE ) {
      return utf16_le;
    }
    // UTF-16 BE: FE FF
    if ( bytes[ 0 ] == 0xFE && bytes[ 1 ] == 0xFF ) {
      return utf16_be;
    }
  }

  // No BOM found
  return {};
}

bool isValidUtf8( const char * data, size_t size )
{
  const unsigned char * bytes = reinterpret_cast< const unsigned char * >( data );
  size_t i                    = 0;

  while ( i < size ) {
    unsigned char byte = bytes[ i ];

    // ASCII (0x00-0x7F): single byte
    if ( byte <= 0x7F ) {
      i++;
      continue;
    }

    // Multi-byte sequence
    int extraBytes = 0;

    // 2-byte: 110xxxxx 10xxxxxx
    if ( ( byte & 0xE0 ) == 0xC0 ) {
      extraBytes = 1;
    }
    // 3-byte: 1110xxxx 10xxxxxx 10xxxxxx
    else if ( ( byte & 0xF0 ) == 0xE0 ) {
      extraBytes = 2;
    }
    // 4-byte: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    else if ( ( byte & 0xF8 ) == 0xF0 ) {
      extraBytes = 3;
    }
    else {
      // Invalid UTF-8 start byte
      return false;
    }

    // Check if we have enough bytes
    if ( i + extraBytes >= size ) {
      return false;
    }

    // Validate continuation bytes (must be 10xxxxxx)
    for ( int j = 1; j <= extraBytes; j++ ) {
      if ( ( bytes[ i + j ] & 0xC0 ) != 0x80 ) {
        return false;
      }
    }

    i += extraBytes + 1;
  }

  return true;
}

} // namespace Text
