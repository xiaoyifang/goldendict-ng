/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include <string>

/// Utilities to guess file types based on their names.
namespace Filetype {

using std::string;

/// Removes any trailing or leading spaces and may lowercases the string.
string simplifyString( const string & str, bool lowercase = true );
/// Returns true if the name resembles the one of a sound file (i.e. ends
/// with .wav, .ogg and such).
bool isNameOfSound( const string & );
/// Returns true if the name resembles the one of a video file (i.e. ends
/// with .mpg, .ogv and such).
bool isNameOfVideo( const string & );
/// Returns true if the name resembles the one of a picture file (i.e. ends
/// with .jpg, .png and such).
bool isNameOfPicture( const string & );
/// Returns true if the name resembles the one of a .tiff file (i.e. ends
/// with .tif or tiff). We have this one separately since we need to reconvert
bool isNameOfTiff( const string & );
/// Returns true if the name resembles the one of a .css file
bool isNameOfCSS( const string & );
/// Returns true if the name resembles the one of a .svg file
bool isNameOfSvg( const string & name );

} // namespace Filetype
