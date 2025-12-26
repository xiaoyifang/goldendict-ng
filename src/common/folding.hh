/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "text.hh"
#include <QString>

/// Folding provides means to translate several possible ways to write a
/// symbol into one. This facilitates searching. Here we currently perform
/// full case folding (everything gets translated to lowercase, ligatures
/// and complex letters are decomposed), diacritics folding (all diacritic
/// marks get removed) and whitespace/punctuation marks removal. These
/// transforms are done according to the Unicode standard and/or drafts. The
/// exact algorithms, lists and types of folding performed might get changed
/// in the future -- in this case, the Version field will be bumped up.

namespace Folding {


/// The algorithm's version.
enum {
  Version = 5
};

/// Applies the folding algorithm to each character in the given string,
/// making another one as a result.
std::u32string apply( const std::u32string &, bool preserveWildcards = false );
std::string applyForIndex( const QString & in );
/// Applies only simple case folding algorithm. Since many dictionaries have
/// different case style, we interpret words differing only by case as synonyms.
std::u32string applySimpleCaseOnly( const std::u32string & );
std::u32string applySimpleCaseOnly( const QString & in );
std::u32string applySimpleCaseOnly( const std::string & in );

/// Applies only full case folding algorithm. This includes simple case, but also
/// decomposing ligatures and complex letters.
std::u32string applyFullCaseOnly( const std::u32string & );

/// Applies only diacritics folding algorithm.
std::u32string applyDiacriticsOnly( const std::u32string & );

/// Applies only punctuation folding algorithm.
std::u32string applyPunctOnly( const std::u32string & );
QString applyPunctOnly( const QString & in );
/// Applies only whitespace folding algorithm.
std::u32string applyWhitespaceOnly( const std::u32string & );

/// Applies only whitespace&punctuation folding algorithm.
std::u32string applyWhitespaceAndPunctOnly( const std::u32string & );

/// Returns true if the given character is any form of whitespace, false
/// otherwise. Whitespace corresponds to Zl/Zp/Zs Unicode classes, and also
/// includes \n, \r and \t.
bool isWhitespace( char32_t ch );
bool isWhitespaceOrPunct( char32_t ch );

/// Returns true if the given character is any form of punctuation, false
/// otherwise. Punctuation corresponds to Pc/Pd/Pe/Pf/Pi/Po/Ps classes.
bool isPunct( char32_t ch );

/// Removes any whitespace or punctuation from the beginning and the end of
/// the word.
std::u32string trimWhitespaceOrPunct( const std::u32string & );
QString trimWhitespaceOrPunct( const QString & in );

/// Removes any whitespace from the beginning and the end of
/// the word.
std::u32string trimWhitespace( const std::u32string & );
QString trimWhitespace( const QString & in );

/// Same as apply( wstring ), but without any heap operations, therefore
/// preferable when there're many strings to process. Returns -1 if the
/// operation succeeded, or otherwise the minimum value of outSize required
/// to succeed.
/// Currently commented out, consider implementing it in case indices'
/// generation would be too slow.
//ssize_t apply( wchar const * in, wchar * out, size_t outSize );

/// Unescape all wildcard symbols (for exast search)
QString unescapeWildcardSymbols( const QString & );

/// Escape all wildcard symbols (for place word to input line)
QString escapeWildcardSymbols( const QString & );

/// Tests if the given char is one of the Unicode combining marks.
bool isCombiningMark( char32_t ch );

} // namespace Folding
