#pragma once

#include "dictionary.hh"

/// Support for the Slob dictionaries.
namespace Slob {

using std::vector;
using std::string;

vector< sptr< Dictionary::Class > > makeDictionaries( const vector< string > & fileNames,
                                                      const string & indicesDir,
                                                      Dictionary::Initializing &,
                                                      unsigned maxHeadwordsToExpand );

} // namespace Slob
