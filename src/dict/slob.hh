#pragma once

#include "dictionary.hh"

/// Support for the Slob dictionaries.
namespace Slob {

using std::vector;
using std::string;

vector< sptr< Dictionary::Class > > makeDictionaries( vector< string > const & fileNames,
                                                      string const & indicesDir,
                                                      Dictionary::Initializing &,
                                                      unsigned maxHeadwordsToExpand );

} // namespace Slob
