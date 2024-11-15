#pragma once

#include "dictionary.hh"

/// Support for the Dabilon source .GLS files.
namespace Gls {

using std::vector;
using std::string;

vector< sptr< Dictionary::Class > >
makeDictionaries( vector< string > const & fileNames, string const & indicesDir, Dictionary::Initializing & );

} // namespace Gls
