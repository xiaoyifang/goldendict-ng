/** @file
 * @brief Unicode and UTF-8 related classes and functions.
 */
/* Copyright (C) 2006,2007,2008,2009,2010,2011,2012,2013,2014,2015,2019 Olly Betts
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef XAPIAN_INCLUDED_UNICODE_H
#define XAPIAN_INCLUDED_UNICODE_H

#if !defined XAPIAN_IN_XAPIAN_H && !defined XAPIAN_LIB_BUILD
# error Never use <xapian/unicode.h> directly; include <xapian.h> instead.
#endif

#include <xapian/attributes.h>
#include <xapian/visibility.h>

#include <string>

namespace Xapian {

/** An iterator which returns Unicode character values from a UTF-8 encoded
 *  string.
 */
class XAPIAN_VISIBILITY_DEFAULT Utf8Iterator {
    const unsigned char* p;
    const unsigned char* end;
    mutable unsigned seqlen;

    bool XAPIAN_NOTHROW(calculate_sequence_length() const);

    unsigned get_char() const;

    Utf8Iterator(const unsigned char* p_,
		 const unsigned char* end_,
		 unsigned seqlen_)
	: p(p_), end(end_), seqlen(seqlen_) { }

  public:
    /** Return the raw const char* pointer for the current position. */
    const char* raw() const {
	return reinterpret_cast<const char*>(p ? p : end);
    }

    /** Return the number of bytes left in the iterator's buffer. */
    size_t left() const { return p ? end - p : 0; }

    /** Assign a new string to the iterator.
     *
     *  The iterator will forget the string it was iterating through, and
     *  return characters from the start of the new string when next called.
     *  The string is not copied into the iterator, so it must remain valid
     *  while the iteration is in progress.
     *
     *  @param p_ A pointer to the start of the string to read.
     *
     *  @param len The length of the string to read.
     */
    void assign(const char* p_, size_t len) {
	if (len) {
	    p = reinterpret_cast<const unsigned char*>(p_);
	    end = p + len;
	    seqlen = 0;
	} else {
	    p = NULL;
	}
    }

    /** Assign a new string to the iterator.
     *
     *  The iterator will forget the string it was iterating through, and
     *  return characters from the start of the new string when next called.
     *  The string is not copied into the iterator, so it must remain valid
     *  while the iteration is in progress.
     *
     *  @param s The string to read.  Must not be modified while the iteration
     *		 is in progress.
     */
    void assign(const std::string& s) { assign(s.data(), s.size()); }

    /** Create an iterator given a pointer to a null terminated string.
     *
     *  The iterator will return characters from the start of the string when
     *  next called.  The string is not copied into the iterator, so it must
     *  remain valid while the iteration is in progress.
     *
     *  @param p_ A pointer to the start of the null terminated string to read.
     */
    explicit Utf8Iterator(const char* p_);

    /** Create an iterator given a pointer and a length.
     *
     *  The iterator will return characters from the start of the string when
     *  next called.  The string is not copied into the iterator, so it must
     *  remain valid while the iteration is in progress.
     *
     *  @param p_ A pointer to the start of the string to read.
     *
     *  @param len The length of the string to read.
     */
    Utf8Iterator(const char* p_, size_t len) { assign(p_, len); }

    /** Create an iterator given a string.
     *
     *  The iterator will return characters from the start of the string when
     *  next called.  The string is not copied into the iterator, so it must
     *  remain valid while the iteration is in progress.
     *
     *  @param s The string to read.  Must not be modified while the iteration
     *		 is in progress.
     */
    Utf8Iterator(const std::string& s) { assign(s.data(), s.size()); }

    /** Create an iterator which is at the end of its iteration.
     *
     *  This can be compared to another iterator to check if the other iterator
     *  has reached its end.
     */
    XAPIAN_NOTHROW(Utf8Iterator())
	: p(NULL), end(0), seqlen(0) { }

    /** Get the current Unicode character value pointed to by the iterator.
     *
     *  If an invalid UTF-8 sequence is encountered, then the byte values
     *  comprising it are returned until valid UTF-8 or the end of the input is
     *  reached.
     *
     *  Returns unsigned(-1) if the iterator has reached the end of its buffer.
     */
    unsigned XAPIAN_NOTHROW(operator*() const) XAPIAN_PURE_FUNCTION;

    /** @private @internal Get the current Unicode character
     *  value pointed to by the iterator.
     *
     *  If an invalid UTF-8 sequence is encountered, then the byte values
     *  comprising it are returned with the top bit set (so the caller can
     *  differentiate these from the same values arising from valid UTF-8)
     *  until valid UTF-8 or the end of the input is reached.
     *
     *  Returns unsigned(-1) if the iterator has reached the end of its buffer.
     */
    unsigned XAPIAN_NOTHROW(strict_deref() const) XAPIAN_PURE_FUNCTION;

    /** Move forward to the next Unicode character.
     *
     *  @return An iterator pointing to the position before the move.
     */
    Utf8Iterator operator++(int) {
	// If we've not calculated seqlen yet, do so.
	if (seqlen == 0) calculate_sequence_length();
	const unsigned char* old_p = p;
	unsigned old_seqlen = seqlen;
	p += seqlen;
	if (p == end) p = NULL;
	seqlen = 0;
	return Utf8Iterator(old_p, end, old_seqlen);
    }

    /** Move forward to the next Unicode character.
     *
     *  @return A reference to this object.
     */
    Utf8Iterator& operator++() {
	if (seqlen == 0) calculate_sequence_length();
	p += seqlen;
	if (p == end) p = NULL;
	seqlen = 0;
	return *this;
    }

    /** Test two Utf8Iterators for equality.
     *
     *  @param other	The Utf8Iterator to compare this one with.
     *  @return true iff the iterators point to the same position.
     */
    bool XAPIAN_NOTHROW(operator==(const Utf8Iterator& other) const) {
	return p == other.p;
    }

    /** Test two Utf8Iterators for inequality.
     *
     *  @param other	The Utf8Iterator to compare this one with.
     *  @return true iff the iterators do not point to the same position.
     */
    bool XAPIAN_NOTHROW(operator!=(const Utf8Iterator& other) const) {
	return p != other.p;
    }

    /// We implement the semantics of an STL input_iterator.
    //@{
    typedef std::input_iterator_tag iterator_category;
    typedef unsigned value_type;
    typedef size_t difference_type;
    typedef const unsigned* pointer;
    typedef const unsigned& reference;
    //@}
};

/// Functions associated with handling Unicode characters.
namespace Unicode {

/** Each Unicode character is in exactly one of these categories.
 *
 * The Unicode standard calls this the "General Category", and uses a
 * "Major, minor" convention to derive a two letter code.
 */
typedef enum {
    UNASSIGNED,                         /**< Other, not assigned (Cn) */
    UPPERCASE_LETTER,                   /**< Letter, uppercase (Lu) */
    LOWERCASE_LETTER,                   /**< Letter, lowercase (Ll) */
    TITLECASE_LETTER,                   /**< Letter, titlecase (Lt) */
    MODIFIER_LETTER,                    /**< Letter, modifier (Lm) */
    OTHER_LETTER,                       /**< Letter, other (Lo) */
    NON_SPACING_MARK,                   /**< Mark, nonspacing (Mn) */
    ENCLOSING_MARK,                     /**< Mark, enclosing (Me) */
    COMBINING_SPACING_MARK,             /**< Mark, spacing combining (Mc) */
    DECIMAL_DIGIT_NUMBER,               /**< Number, decimal digit (Nd) */
    LETTER_NUMBER,                      /**< Number, letter (Nl) */
    OTHER_NUMBER,                       /**< Number, other (No) */
    SPACE_SEPARATOR,                    /**< Separator, space (Zs) */
    LINE_SEPARATOR,                     /**< Separator, line (Zl) */
    PARAGRAPH_SEPARATOR,                /**< Separator, paragraph (Zp) */
    CONTROL,                            /**< Other, control (Cc) */
    FORMAT,                             /**< Other, format (Cf) */
    PRIVATE_USE,                        /**< Other, private use (Co) */
    SURROGATE,                          /**< Other, surrogate (Cs) */
    CONNECTOR_PUNCTUATION,              /**< Punctuation, connector (Pc) */
    DASH_PUNCTUATION,                   /**< Punctuation, dash (Pd) */
    OPEN_PUNCTUATION,                   /**< Punctuation, open (Ps) */
    CLOSE_PUNCTUATION,                  /**< Punctuation, close (Pe) */
    INITIAL_QUOTE_PUNCTUATION,          /**< Punctuation, initial quote (Pi) */
    FINAL_QUOTE_PUNCTUATION,            /**< Punctuation, final quote (Pf) */
    OTHER_PUNCTUATION,                  /**< Punctuation, other (Po) */
    MATH_SYMBOL,                        /**< Symbol, math (Sm) */
    CURRENCY_SYMBOL,                    /**< Symbol, currency (Sc) */
    MODIFIER_SYMBOL,                    /**< Symbol, modified (Sk) */
    OTHER_SYMBOL                        /**< Symbol, other (So) */
} category;

namespace Internal {
    /** @private @internal Extract the information about a character from the
     *  Unicode character tables.
     *
     *  Characters outside of the Unicode range (i.e. ch >= 0x110000) are
     *  treated as UNASSIGNED with no case variants.
     */
    XAPIAN_VISIBILITY_DEFAULT
    int XAPIAN_NOTHROW(get_character_info(unsigned ch)) XAPIAN_CONST_FUNCTION;

    /** @private @internal Extract how to convert the case of a Unicode
     *  character from its info.
     */
    inline int get_case_type(int info) { return ((info & 0xe0) >> 5); }

    /** @private @internal Extract the category of a Unicode character from its
     *  info.
     */
    inline category get_category(int info) {
	return static_cast<category>(info & 0x1f);
    }

    /** @private @internal Extract the delta to use for case conversion of a
     *  character from its info.
     */
    inline int get_delta(int info) {
	/* It's implementation defined if sign extension happens when right
	 * shifting a signed int, although in practice sign extension is what
	 * most compilers implement.
	 *
	 * Some compilers are smart enough to spot common idioms for sign
	 * extension, but not all (e.g. GCC < 7 doesn't spot the one used
	 * below), so check what the implementation-defined behaviour is with
	 * a constant conditional which should get optimised away.
	 *
	 * We use the ternary operator here to avoid various compiler
	 * warnings which writing this as an `if` results in.
	 */
	return ((-1 >> 1) == -1 ?
		// Right shift sign-extends.
		info >> 8 :
		// Right shift shifts in zeros so bitwise-not before and after
		// the shift for negative values.
		(info >= 0) ? (info >> 8) : (~(~info >> 8)));
    }
}

/** Convert a single non-ASCII Unicode character to UTF-8.
 *
 *  This is intended mainly as a helper method for to_utf8().
 *
 *  @param ch	The character (which must be > 128) to write to @a buf.
 *  @param buf	The buffer to write the character to - it must have
 *		space for (at least) 4 bytes.
 *
 *  @return	The length of the resultant UTF-8 character in bytes.
 */
XAPIAN_VISIBILITY_DEFAULT
unsigned nonascii_to_utf8(unsigned ch, char* buf);

/** Convert a single Unicode character to UTF-8.
 *
 *  @param ch	The character to write to @a buf.
 *  @param buf	The buffer to write the character to - it must have
 *		space for (at least) 4 bytes.
 *
 *  @return	The length of the resultant UTF-8 character in bytes.
 */
inline unsigned to_utf8(unsigned ch, char* buf) {
    if (ch < 128) {
	*buf = static_cast<unsigned char>(ch);
	return 1;
    }
    return Xapian::Unicode::nonascii_to_utf8(ch, buf);
}

/** Append the UTF-8 representation of a single Unicode character to a
 *  std::string.
 */
inline void append_utf8(std::string& s, unsigned ch) {
    char buf[4];
    s.append(buf, to_utf8(ch, buf));
}

/// Return the category which a given Unicode character falls into.
inline category get_category(unsigned ch) {
    return Internal::get_category(Internal::get_character_info(ch));
}

/// Test if a given Unicode character is "word character".
inline bool is_wordchar(unsigned ch) {
    const unsigned int WORDCHAR_MASK =
	    (1 << Xapian::Unicode::UPPERCASE_LETTER) |
	    (1 << Xapian::Unicode::LOWERCASE_LETTER) |
	    (1 << Xapian::Unicode::TITLECASE_LETTER) |
	    (1 << Xapian::Unicode::MODIFIER_LETTER) |
	    (1 << Xapian::Unicode::OTHER_LETTER) |
	    (1 << Xapian::Unicode::NON_SPACING_MARK) |
	    (1 << Xapian::Unicode::ENCLOSING_MARK) |
	    (1 << Xapian::Unicode::COMBINING_SPACING_MARK) |
	    (1 << Xapian::Unicode::DECIMAL_DIGIT_NUMBER) |
	    (1 << Xapian::Unicode::LETTER_NUMBER) |
	    (1 << Xapian::Unicode::OTHER_NUMBER) |
	    (1 << Xapian::Unicode::CONNECTOR_PUNCTUATION);
    return ((WORDCHAR_MASK >> get_category(ch)) & 1);
}

/// Test if a given Unicode character is a whitespace character.
inline bool is_whitespace(unsigned ch) {
    const unsigned int WHITESPACE_MASK =
	    (1 << Xapian::Unicode::CONTROL) | // For TAB, CR, LF, FF.
	    (1 << Xapian::Unicode::SPACE_SEPARATOR) |
	    (1 << Xapian::Unicode::LINE_SEPARATOR) |
	    (1 << Xapian::Unicode::PARAGRAPH_SEPARATOR);
    return ((WHITESPACE_MASK >> get_category(ch)) & 1);
}

/// Test if a given Unicode character is a currency symbol.
inline bool is_currency(unsigned ch) {
    return (get_category(ch) == Xapian::Unicode::CURRENCY_SYMBOL);
}

/// Convert a Unicode character to lowercase.
inline unsigned tolower(unsigned ch) {
    int info = Xapian::Unicode::Internal::get_character_info(ch);
    if (!(Internal::get_case_type(info) & 2))
	return ch;
    return ch + Internal::get_delta(info);
}

/// Convert a Unicode character to uppercase.
inline unsigned toupper(unsigned ch) {
    int info = Xapian::Unicode::Internal::get_character_info(ch);
    if (!(Internal::get_case_type(info) & 4))
	return ch;
    return ch - Internal::get_delta(info);
}

/// Convert a UTF-8 std::string to lowercase.
inline std::string
tolower(const std::string& term)
{
    std::string result;
    result.reserve(term.size());
    for (Utf8Iterator i(term); i != Utf8Iterator(); ++i) {
	append_utf8(result, tolower(*i));
    }
    return result;
}

/// Convert a UTF-8 std::string to uppercase.
inline std::string
toupper(const std::string& term)
{
    std::string result;
    result.reserve(term.size());
    for (Utf8Iterator i(term); i != Utf8Iterator(); ++i) {
	append_utf8(result, toupper(*i));
    }
    return result;
}

}

}

#endif // XAPIAN_INCLUDED_UNICODE_H
