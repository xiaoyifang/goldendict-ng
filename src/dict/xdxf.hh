/* This file is (c) 2008-2009 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "dictionary.hh"

/// Support for the XDXF (.xdxf(.dz)) files.
namespace Xdxf {

using std::vector;
using std::string;

quint32 getLanguageId( const QString & lang );

vector< sptr< Dictionary::Class > >
makeDictionaries( const vector< string > & fileNames, const string & indicesDir, Dictionary::Initializing & );

} // namespace Xdxf
