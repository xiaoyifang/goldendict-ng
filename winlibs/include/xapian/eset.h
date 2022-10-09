/** @file
 *  @brief Class representing a list of query expansion terms
 */
/* Copyright (C) 2015,2016 Olly Betts
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

#ifndef XAPIAN_INCLUDED_ESET_H
#define XAPIAN_INCLUDED_ESET_H

#if !defined XAPIAN_IN_XAPIAN_H && !defined XAPIAN_LIB_BUILD
# error Never use <xapian/eset.h> directly; include <xapian.h> instead.
#endif

#include <iterator>
#include <string>

#include <xapian/attributes.h>
#include <xapian/intrusive_ptr.h>
#include <xapian/stem.h>
#include <xapian/types.h>
#include <xapian/visibility.h>

namespace Xapian {

class ESetIterator;

/// Class representing a list of search results.
class XAPIAN_VISIBILITY_DEFAULT ESet {
    friend class ESetIterator;

  public:
    /// Class representing the ESet internals.
    class Internal;
    /// @private @internal Reference counted internals.
    Xapian::Internal::intrusive_ptr<Internal> internal;

    /** Copying is allowed.
     *
     *  The internals are reference counted, so copying is cheap.
     */
    ESet(const ESet & o);

    /** Copying is allowed.
     *
     *  The internals are reference counted, so assignment is cheap.
     */
    ESet & operator=(const ESet & o);

#ifdef XAPIAN_MOVE_SEMANTICS
    /// Move constructor.
    ESet(ESet && o);

    /// Move assignment operator.
    ESet & operator=(ESet && o);
#endif

    /** Default constructor.
     *
     *  Creates an empty ESet, mostly useful as a placeholder.
     */
    ESet();

    /// Destructor.
    ~ESet();

    /** Return number of items in this ESet object. */
    Xapian::doccount size() const;

    /** Return true if this ESet object is empty. */
    bool empty() const { return size() == 0; }

    /** Return a bound on the full size of this ESet object.
     *
     *  This is a bound on size() if get_eset() had been called with
     *  maxitems set high enough that all results were returned.
     */
    Xapian::termcount get_ebound() const;

    /** Efficiently swap this ESet object with another. */
    void swap(ESet & o) { internal.swap(o.internal); }

    /** Return iterator pointing to the first item in this ESet. */
    ESetIterator begin() const;

    /** Return iterator pointing to just after the last item in this ESet. */
    ESetIterator end() const;

    /** Return iterator pointing to the i-th object in this ESet. */
    ESetIterator operator[](Xapian::doccount i) const;

    /** Return iterator pointing to the last object in this ESet. */
    ESetIterator back() const;

    /// Return a string describing this object.
    std::string get_description() const;

    /** @private @internal ESet is what the C++ STL calls a container.
     *
     *  The following typedefs allow the class to be used in templates in the
     *  same way the standard containers can be.
     *
     *  These are deliberately hidden from the Doxygen-generated docs, as the
     *  machinery here isn't interesting to API users.  They just need to know
     *  that Xapian container classes are compatible with the STL.
     *
     *  See "The C++ Programming Language", 3rd ed. section 16.3.1:
     */
    // @{
    /// @private
    typedef Xapian::ESetIterator value_type;
    /// @private
    typedef Xapian::termcount size_type;
    /// @private
    typedef Xapian::termcount_diff difference_type;
    /// @private
    typedef Xapian::ESetIterator iterator;
    /// @private
    typedef Xapian::ESetIterator const_iterator;
    /// @private
    typedef value_type * pointer;
    /// @private
    typedef const value_type * const_pointer;
    /// @private
    typedef value_type & reference;
    /// @private
    typedef const value_type & const_reference;
    // @}
    //
    /** @private @internal ESet is what the C++ STL calls a container.
     *
     *  The following methods allow the class to be used in templates in the
     *  same way the standard containers can be.
     *
     *  These are deliberately hidden from the Doxygen-generated docs, as the
     *  machinery here isn't interesting to API users.  They just need to know
     *  that Xapian container classes are compatible with the STL.
     */
    // @{
    // The size is fixed once created.
    Xapian::doccount max_size() const { return size(); }
    // @}
};

/// Iterator over a Xapian::ESet.
class XAPIAN_VISIBILITY_DEFAULT ESetIterator {
    friend class ESet;

    ESetIterator(const Xapian::ESet & eset_, Xapian::doccount off_from_end_)
	: eset(eset_), off_from_end(off_from_end_) { }

  public:
    /** @private @internal The ESet we are iterating over. */
    Xapian::ESet eset;

    /** @private @internal The current position of the iterator.
     *
     *  We store the offset from the end of @a eset, since that means
     *  ESet::end() just needs to set this member to 0.
     */
    Xapian::ESet::size_type off_from_end;

    /** Create an unpositioned ESetIterator. */
    ESetIterator() : off_from_end(0) { }

    /** Get the term at the current position. */
    std::string operator*() const;

    /// Advance the iterator to the next position.
    ESetIterator & operator++() {
	--off_from_end;
	return *this;
    }

    /// Advance the iterator to the next position (postfix version).
    ESetIterator operator++(int) {
	ESetIterator retval = *this;
	--off_from_end;
	return retval;
    }

    /// Move the iterator to the previous position.
    ESetIterator & operator--() {
	++off_from_end;
	return *this;
    }

    /// Move the iterator to the previous position (postfix version).
    ESetIterator operator--(int) {
	ESetIterator retval = *this;
	++off_from_end;
	return retval;
    }

    /** @private @internal ESetIterator is what the C++ STL calls an
     *  random_access_iterator.
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
    typedef std::random_access_iterator_tag iterator_category;
    /// @private
    typedef std::string value_type;
    /// @private
    typedef Xapian::termcount_diff difference_type;
    /// @private
    typedef std::string * pointer;
    /// @private
    typedef std::string & reference;
    // @}

    /// Move the iterator forwards by n positions.
    ESetIterator & operator+=(difference_type n) {
	off_from_end -= n;
	return *this;
    }

    /// Move the iterator back by n positions.
    ESetIterator & operator-=(difference_type n) {
	off_from_end += n;
	return *this;
    }

    /** Return the iterator incremented by @a n positions.
     *
     *  If @a n is negative, decrements by (-n) positions.
     */
    ESetIterator operator+(difference_type n) const {
	return ESetIterator(eset, off_from_end - n);
    }

    /** Return the iterator decremented by @a n positions.
     *
     *  If @a n is negative, increments by (-n) positions.
     */
    ESetIterator operator-(difference_type n) const {
	return ESetIterator(eset, off_from_end + n);
    }

    /** Return the number of positions between @a o and this iterator. */
    difference_type operator-(const ESetIterator& o) const {
	return difference_type(o.off_from_end) - difference_type(off_from_end);
    }

    /** Get the weight for the current position. */
    double get_weight() const;

    /// Return a string describing this object.
    std::string get_description() const;
};

bool
XAPIAN_NOTHROW(operator==(const ESetIterator &a, const ESetIterator &b));

/// Equality test for ESetIterator objects.
inline bool
operator==(const ESetIterator &a, const ESetIterator &b) XAPIAN_NOEXCEPT
{
    return a.off_from_end == b.off_from_end;
}

inline bool
XAPIAN_NOTHROW(operator!=(const ESetIterator &a, const ESetIterator &b));

/// Inequality test for ESetIterator objects.
inline bool
operator!=(const ESetIterator &a, const ESetIterator &b) XAPIAN_NOEXCEPT
{
    return !(a == b);
}

bool
XAPIAN_NOTHROW(operator<(const ESetIterator &a, const ESetIterator &b));

/// Inequality test for ESetIterator objects.
inline bool
operator<(const ESetIterator &a, const ESetIterator &b) XAPIAN_NOEXCEPT
{
    return a.off_from_end > b.off_from_end;
}

inline bool
XAPIAN_NOTHROW(operator>(const ESetIterator &a, const ESetIterator &b));

/// Inequality test for ESetIterator objects.
inline bool
operator>(const ESetIterator &a, const ESetIterator &b) XAPIAN_NOEXCEPT
{
    return b < a;
}

inline bool
XAPIAN_NOTHROW(operator>=(const ESetIterator &a, const ESetIterator &b));

/// Inequality test for ESetIterator objects.
inline bool
operator>=(const ESetIterator &a, const ESetIterator &b) XAPIAN_NOEXCEPT
{
    return !(a < b);
}

inline bool
XAPIAN_NOTHROW(operator<=(const ESetIterator &a, const ESetIterator &b));

/// Inequality test for ESetIterator objects.
inline bool
operator<=(const ESetIterator &a, const ESetIterator &b) XAPIAN_NOEXCEPT
{
    return !(b < a);
}

/** Return ESetIterator @a it incremented by @a n positions.
 *
 *  If @a n is negative, decrements by (-n) positions.
 */
inline ESetIterator
operator+(ESetIterator::difference_type n, const ESetIterator& it)
{
    return it + n;
}

// Inlined methods of ESet which need ESetIterator to have been defined:

inline ESetIterator
ESet::begin() const {
    return ESetIterator(*this, size());
}

inline ESetIterator
ESet::end() const {
    // Decrementing the result of end() needs to work, so we must pass in
    // *this here.
    return ESetIterator(*this, 0);
}

inline ESetIterator
ESet::operator[](Xapian::doccount i) const {
    return ESetIterator(*this, size() - i);
}

inline ESetIterator
ESet::back() const {
    return ESetIterator(*this, 1);
}

}

#endif // XAPIAN_INCLUDED_ESET_H
