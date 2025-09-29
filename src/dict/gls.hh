#pragma once

#include "dictionary.hh"

/// Support for the Dabilon source .GLS files.
namespace Gls {

using std::vector;
using std::string;

vector< sptr< Dictionary::Class > >
makeDictionaries( const vector< string > & fileNames, const string & indicesDir, Dictionary::Initializing & );

} // namespace Gls
