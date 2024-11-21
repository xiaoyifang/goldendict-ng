/* This file is (c) 2013 Maksim Tamkovicz <quendimax@gmail.com>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include <vector>
#include "dictionary.hh"

// Support for Belarusian transliteration
namespace BelarusianTranslit {

std::vector< sptr< Dictionary::Class > > makeDictionaries();
}
