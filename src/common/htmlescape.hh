/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include <QString>
#include <string>

namespace Html {
enum class HtmlOption {
  Strip,
  Keep
};
using std::string;

// Replaces &, <, > and " by their html entity equivalents
// The " is not really required to be escaped in html, but is replaced anyway
// to make the result suitable for inserting as attributes' values.
string escape( const string & );
QString escapeQString( const QString & );

// Converts the given preformatted text to html. Each end of line is replaced by
// <br>, each leading space is converted to &nbsp;.
string preformat( const string &, bool baseRightToLeft = false );

// Escapes the given string to be included in JavaScript.
string escapeForJavaScript( const string & );
QString stripHtml( QString tmp );
// Replace html entities
QString unescape( QString str, HtmlOption option = HtmlOption::Strip );

QString fromHtmlEscaped( const QString & str );
string unescapeUtf8( const string & str, HtmlOption option = HtmlOption::Strip );

} // namespace Html
