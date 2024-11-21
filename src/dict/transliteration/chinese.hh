/* This file is (c) 2015 Zhe Wang <0x1997@gmail.com>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "dictionary.hh"

/// Chinese character conversion support.
namespace ChineseTranslit {

std::vector< sptr< Dictionary::Class > > makeDictionaries( Config::Chinese const & );
}
