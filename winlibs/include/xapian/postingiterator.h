/** @file
 *  @brief Class for iterating over a list of document ids
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

#ifndef XAPIAN_INCLUDED_POSTINGITERATOR_H
#define XAPIAN_INCLUDED_POSTINGITERATOR_H

#if !defined XAPIAN_IN_XAPIAN_H && !defined XAPIAN_LIB_BUILD
# error Never use <xapian/postingiterator.h> directly; include <xapian.h> instead.
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
class XAPIAN_VISIBILITY_DEFAULT PostingIterator {
  public:
    /// Class representing the PostingIterator internals.
    class Internal;
    /// @private @internal Reference counted internals.
    Internal * internal;

    /// @private @internal Construct given internals.
    explicit PostingIterator(Internal *internal_);

    /// Copy constructor.
    PostingIterator(const PostingIterator & o);

    /// Assignment.
    PostingIterator & operator=(const PostingIterator & o);

#ifdef XAPIAN_MOVE_SEMANTICS
    /// Move constructor.
    PostingIterator(PostingIterator && o)
	: internal(o.internal) {
	o.internal = nullptr;
    }

    /// Move assignment operator.
    PostingIterator & operator=(PostingIterator && o) {
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
    XAPIAN_NOTHROW(PostingIterator())
	: internal(0) { }

    /// Destructor.
    ~PostingIterator() {
	if (internal) decref();
    }

    /// Return the document id at the current position.
    Xapian::docid operator*() const;

    /// Return the wdf for the document at the current position.
    Xapian::termcount get_wdf() const;

    /// Return the length of the document at the current position.
    Xapian::termcount get_doclength() const;

    /// Return the number of unique terms in the current document.
    Xapian::termcount get_unique_terms() const;

#if 0 // FIXME: TermIterator supports this, so PostingIterator really ought to.
    /// Return the length of the position list for the current position.
    Xapian::termcount positionlist_count() const;
#endif

    /// Return a PositionIterator for the current document.
    PositionIterator positionlist_begin() const;

    /// Return an end PositionIterator for the current document.
    PositionIterator XAPIAN_NOTHROW(positionlist_end() const) {
	return PositionIterator();
    }

    /// Advance the iterator to the next position.
    PostingIterator & operator++();

    /// Advance the iterator to the next position (postfix version).
    DerefWrapper_<Xapian::docid> operator++(int) {
	Xapian::docid did(**this);
	operator++();
	return DerefWrapper_<Xapian::docid>(did);
    }

    /** Advance the iterator to document @a did.
     *
     *  @param did	The document id to advance to.  If this document id
     *			isn't in the stream being iterated, then the iterator
     *			is moved to the next document id after it which is.
     */
    void skip_to(Xapian::docid did);

    /// Return a string describing this object.
    std::string get_description() const;

    /** @private @internal PostingIterator is what the C++ STL calls an
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
    typedef Xapian::docid value_type;
    /// @private
    typedef Xapian::doccount_diff difference_type;
    /// @private
    typedef Xapian::docid * pointer;
    /// @private
    typedef Xapian::docid & reference;
    // @}

  private:
    void decref();

    void post_advance(Internal * res);
};

bool
XAPIAN_NOTHROW(operator==(const PostingIterator &a, const PostingIterator &b));

/// Equality test for PostingIterator objects.
inline bool
operator==(const PostingIterator &a, const PostingIterator &b) XAPIAN_NOEXCEPT
{
    // Use a pointer comparison - this ensures both that (a == a) and correct
    // handling of end iterators (which we ensure have NULL internals).
    return a.internal == b.internal;
}

inline bool
XAPIAN_NOTHROW(operator!=(const PostingIterator &a, const PostingIterator &b));

/// Inequality test for PostingIterator objects.
inline bool
operator!=(const PostingIterator &a, const PostingIterator &b) XAPIAN_NOEXCEPT
{
    return !(a == b);
}

}

#endif // XAPIAN_INCLUDED_POSTINGITERATOR_H
