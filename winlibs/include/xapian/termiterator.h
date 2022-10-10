/** @file
 *  @brief Class for iterating over a list of terms
 */
/* Copyright (C) 2007,2008,2009,2010,2011,2012,2013,2014,2015 Olly Betts
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#ifndef XAPIAN_INCLUDED_TERMITERATOR_H
#define XAPIAN_INCLUDED_TERMITERATOR_H

#if !defined XAPIAN_IN_XAPIAN_H && !defined XAPIAN_LIB_BUILD
# error Never use <xapian/termiterator.h> directly; include <xapian.h> instead.
#endif

#include <iterator>
#include <string>

#include <xapian/attributes.h>
#include <xapian/derefwrapper.h>
#include <xapian/positioniterator.h>
#include <xapian/types.h>
#include <xapian/visibility.h>

namespace Xapian {

/// Class for iterating over a list of terms.
class XAPIAN_VISIBILITY_DEFAULT TermIterator {
  public:
    /// Class representing the TermIterator internals.
    class Internal;
    /// @private @internal Reference counted internals.
    Internal * internal;

    /// @private @internal Construct given internals.
    explicit TermIterator(Internal *internal_);

    /// Copy constructor.
    TermIterator(const TermIterator & o);

    /// Assignment.
    TermIterator & operator=(const TermIterator & o);

#ifdef XAPIAN_MOVE_SEMANTICS
    /// Move constructor.
    TermIterator(TermIterator && o)
	: internal(o.internal) {
	o.internal = nullptr;
    }

    /// Move assignment operator.
    TermIterator & operator=(TermIterator && o) {
	if (this != &o) {
	    if (internal) decref();
	    internal = o.internal;
	    o.internal = nullptr;
	}
	return *this;
    }
#endif

    /** Default constructor.
     *
     *  Creates an uninitialised iterator, which can't be used before being
     *  assigned to, but is sometimes syntactically convenient.
     */
    XAPIAN_NOTHROW(TermIterator())
	: internal(0) { }

    /// Destructor.
    ~TermIterator() {
	if (internal) decref();
    }

    /// Return the term at the current position.
    std::string operator*() const;

    /// Return the wdf for the term at the current position.
    Xapian::termcount get_wdf() const;

    /// Return the term frequency for the term at the current position.
    Xapian::doccount get_termfreq() const;

    /// Return the length of the position list for the current position.
    Xapian::termcount positionlist_count() const;

    /// Return a PositionIterator for the current term.
    PositionIterator positionlist_begin() const;

    /// Return an end PositionIterator for the current term.
    PositionIterator XAPIAN_NOTHROW(positionlist_end() const) {
	return PositionIterator();
    }

    /// Advance the iterator to the next position.
    TermIterator & operator++();

    /// Advance the iterator to the next position (postfix version).
    DerefWrapper_<std::string> operator++(int) {
	const std::string & term(**this);
	operator++();
	return DerefWrapper_<std::string>(term);
    }

    /** Advance the iterator to term @a term.
     *
     *  If the iteration is over an unsorted list of terms, then this method
     *  will throw Xapian::InvalidOperationError.
     *
     *  @param term	The term to advance to.  If this term isn't in
     *			the stream being iterated, then the iterator is moved
     *			to the next term after it which is.
     */
    void skip_to(const std::string &term);

    /// Return a string describing this object.
    std::string get_description() const;

    /** @private @internal TermIterator is what the C++ STL calls an
     *  input_iterator.
     *
     *  The following typedefs allow std::iterator_traits<> to work so that
     *  this iterator can be used with the STL.
     *
     *  These are deliberately hidden from the Doxygen-generated docs, as the
     *  machinery here isn't interesting to API users.  They just need to know
     *  that Xapian iterator classes are compatible with the STL.
     */
    // @{
    /// @private
    typedef std::input_iterator_tag iterator_category;
    /// @private
    typedef std::string value_type;
    /// @private
    typedef Xapian::termcount_diff difference_type;
    /// @private
    typedef std::string * pointer;
    /// @private
    typedef std::string & reference;
    // @}

  private:
    void decref();

    void post_advance(Internal * res);
};

bool
XAPIAN_NOTHROW(operator==(const TermIterator &a, const TermIterator &b));

/// Equality test for TermIterator objects.
inline bool
operator==(const TermIterator &a, const TermIterator &b) XAPIAN_NOEXCEPT
{
    // Use a pointer comparison - this ensures both that (a == a) and correct
    // handling of end iterators (which we ensure have NULL internals).
    return a.internal == b.internal;
}

bool
XAPIAN_NOTHROW(operator!=(const TermIterator &a, const TermIterator &b));

/// Inequality test for TermIterator objects.
inline bool
operator!=(const TermIterator &a, const TermIterator &b) XAPIAN_NOEXCEPT
{
    return !(a == b);
}

}

#endif // XAPIAN_INCLUDED_TERMITERATOR_H
