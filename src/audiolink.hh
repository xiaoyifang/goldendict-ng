/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once


#include <QString>
#include <string>

/// Adds a piece of javascript to save the given audiolink to a special
/// javascript variable. Embed this into article's html to enable the
/// 'say sound' functionality.
/// The url should be escaped and surrounded by quotes.
/// The dictionary id is used to make active dictionary feature work.
std::string addAudioLink( const std::string & url, const std::string & dictionaryId );
std::string addAudioLink( const QString & url, const std::string & dictionaryId );
