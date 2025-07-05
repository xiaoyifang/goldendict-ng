/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "dictionary.hh"
#include "config.hh"
#include <QNetworkAccessManager>

/// Support for MediaWiki-based wikis, based on its public API.
namespace MediaWiki {

using std::vector;

vector< sptr< Dictionary::Class > >
makeDictionaries( Dictionary::Initializing &, const Config::MediaWikis & wikis, QNetworkAccessManager & );


} // namespace MediaWiki
