/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */
#pragma once

#include "ex.hh"
#include <QByteArray>
#include <string>

/// Facilities to process Text, focusing on Unicode
namespace Text {
DEF_EX_STR( exCantDecode, "Can't decode the given string from Utf8:", std::exception )

/// Encoding names. Ref -> IANA's encoding names https://www.iana.org/assignments/character-sets/character-sets.xhtml
/// Notice: The ordering must not be changed before Utf32LE. The current .dsl format index file depends on it.
enum class Encoding {
  Utf16LE = 0,
  Utf16BE,
  Windows1252,
  Windows1251,
  Windows1250,
  Utf8,
  Utf32BE,
  Utf32LE,
  Utf32,
};

inline constexpr auto utf16_be     = "UTF-16BE";
inline constexpr auto utf16_le     = "UTF-16LE";
inline constexpr auto utf32        = "UTF-32";
inline constexpr auto utf32_be     = "UTF-32BE";
inline constexpr auto utf32_le     = "UTF-32LE";
inline constexpr auto utf8         = "UTF-8";
inline constexpr auto windows_1250 = "WINDOWS-1250";
inline constexpr auto windows_1251 = "WINDOWS-1251";
inline constexpr auto windows_1252 = "WINDOWS-1252";

const char * getEncodingNameFor( Encoding e );
Encoding getEncodingForName( const QByteArray & name );

/// utf32 -> utf8
std::string toUtf8( std::u32string const & ) noexcept;
/// utf8 -> utf32
std::u32string toUtf32( std::string const & );

/// Since the standard isspace() is locale-specific, we need something
/// that would never mess up our utf8 input. The stock one worked fine under
/// Linux but was messing up strings under Windows.
bool isspace( int c );

//get the first line in string s1. -1 if not found
int findFirstLinePosition( char * s1, int s1length, const char * s2, int s2length );

struct LineFeed
{
  int length;
  char * lineFeed;
};

LineFeed initLineFeed( Encoding e );

std::u32string removeTrailingZero( std::u32string const & v );
std::u32string removeTrailingZero( QString const & in );
std::u32string normalize( std::u32string const & );
} // namespace Text
