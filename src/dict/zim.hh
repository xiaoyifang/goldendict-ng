#pragma once

#ifdef MAKE_ZIM_SUPPORT

  #include "dictionary.hh"

/// Support for the Zim dictionaries.
namespace Zim {

using std::vector;
using std::string;

vector< sptr< Dictionary::Class > > makeDictionaries( const vector< string > & fileNames,
                                                      const string & indicesDir,
                                                      Dictionary::Initializing &,
                                                      unsigned maxHeadwordsToExpand );

} // namespace Zim

#endif
