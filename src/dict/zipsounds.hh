/* This file is part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "dictionary.hh"

/// Support for compresses audio packs (.zips).
namespace ZipSounds {

using std::vector;
using std::string;

vector< sptr< Dictionary::Class > >
makeDictionaries( const vector< string > & fileNames, const string & indicesDir, Dictionary::Initializing & );

} // namespace ZipSounds
