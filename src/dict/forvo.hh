/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "dictionary.hh"
#include "config.hh"
#include <QNetworkAccessManager>

/// Support for Forvo pronunciations, based on its API.
namespace Forvo {

std::vector< sptr< Dictionary::Class > >
makeDictionaries( Dictionary::Initializing &, const Config::Forvo &, QNetworkAccessManager & );

} // namespace Forvo
