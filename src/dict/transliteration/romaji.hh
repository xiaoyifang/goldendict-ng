/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "transliteration.hh"

/// Japanese romanization (Romaji) support.
namespace RomajiTranslit {

std::vector< sptr< Dictionary::Class > > makeDictionaries( const Config::Romaji & );
} // namespace RomajiTranslit
