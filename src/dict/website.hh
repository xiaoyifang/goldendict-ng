/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "dictionary.hh"
#include "config.hh"
#include <QNetworkReply>

/// Support for any web sites via a templated url.
namespace WebSite {

using std::vector;

vector< sptr< Dictionary::Class > > makeDictionaries( const Config::WebSites &, QNetworkAccessManager & );


} // namespace WebSite
