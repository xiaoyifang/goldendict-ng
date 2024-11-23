/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */
#pragma once

#include <cstdio>
#include <QByteArray>
#include <string>
#include "ex.hh"

/// Facilities to process Text, focusing on Unicode
namespace Text {
DEF_EX_STR( exCantDecode, "Can't decode the given string from Utf8:", std::exception )

// Those are possible encodings for .dsl files
enum Encoding {
  Utf16LE,
  Utf16BE,
  Windows1252,
  Windows1251,
  Windows1250,
  Utf8,
  Utf32BE,
  Utf32LE,
};

std::string toUtf8( std::u32string const & ) noexcept;
std::u32string toUtf32( std::string const & );

/// Since the standard isspace() is locale-specific, we need something
/// that would never mess up our utf8 input. The stock one worked fine under
/// Linux but was messing up strings under Windows.
bool isspace( int c );

//get the first line in string s1. -1 if not found
int findFirstLinePosition( char * s1, int s1length, const char * s2, int s2length );
char const * getEncodingNameFor( Encoding e );
Encoding getEncodingForName( const QByteArray & name );

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
