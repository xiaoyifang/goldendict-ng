/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "dictionary.hh"

// Support for German transliteration
namespace GermanTranslit {

sptr< Dictionary::Class > makeDictionary();
}
