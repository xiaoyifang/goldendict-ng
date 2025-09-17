/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "dictionary.hh"

/// Support for the ABBYY Lingvo .DSL files.
namespace Dsl {

using std::vector;
using std::string;

vector< sptr< Dictionary::Class > > makeDictionaries( const vector< string > & fileNames,
                                                      const string & indicesDir,
                                                      Dictionary::Initializing &,
                                                      unsigned int maxHeadwordSize );

} // namespace Dsl
