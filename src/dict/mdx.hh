/* This file is (c) 2013 Timon Wong <timon86.wang AT gmail DOT com>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "dictionary.hh"

namespace Mdx {

using std::vector;
using std::string;

vector< sptr< Dictionary::Class > >
makeDictionaries( vector< string > const & fileNames, string const & indicesDir, Dictionary::Initializing & );

} // namespace Mdx

