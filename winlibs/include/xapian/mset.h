/** @file
 *  @brief Class representing a list of search results
 */
/* Copyright (C) 2015,2016,2019 Olly Betts
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

#ifndef XAPIAN_INCLUDED_MSET_H
#define XAPIAN_INCLUDED_MSET_H

#if !defined XAPIAN_IN_XAPIAN_H && !defined XAPIAN_LIB_BUILD
# error Never use <xapian/mset.h> directly; include <xapian.h> instead.
#endif

#include <iterator>
#include <string>

#include <xapian/attributes.h>
#include <xapian/document.h>
#include <xapian/intrusive_ptr.h>
#include <xapian/stem.h>
#include <xapian/types.h>
#include <xapian/visibility.h>

namespace Xapian {

class MSetIterator;

/// Class representing a list of search results.
class XAPIAN_VISIBILITY_DEFAULT MSet {
    friend class MSetIterator;

    // Helper function for fetch() methods.
    void fetch_(Xapian::doccount first, Xapian::doccount last) const;

  public:
    /// Class representing the MSet internals.
    class Internal;
    /// @private @internal Reference counted internals.
    Xapian::Internal::intrusive_ptr<Internal> internal;

    /** Copying is allowed.
     *
     *  The internals are reference counted, so copying is cheap.
     */
    MSet(const MSet & o);

    /** Copying is allowed.
     *
     *  The internals are reference counted, so assignment is cheap.
     */
    MSet & operator=(const MSet & o);

#ifdef XAPIAN_MOVE_SEMANTICS
    /// Move constructor.
    MSet(MSet && o);

    /// Move assignment operator.
    MSet & operator=(MSet && o);
#endif

    /** Default constructor.
     *
     *  Creates an empty MSet, mostly useful as a placeholder.
     */
    MSet();

    /// Destructor.
    ~MSet();

    /** Convert a weight to a percentage.
     *
     *  The matching document with the highest weight will get 100% if it
     *  matches all the weighted query terms, and proportionally less if it
     *  only matches some, and other weights are scaled by the same factor.
     *
     *  Documents with a non-zero score will always score at least 1%.
     *
     *  Note that these generally aren't percentages of anything meaningful
     *  (unless you use a custom weighting formula where they are!)
     */
    int convert_to_percent(double weight) const;

    /** Convert the weight of the current iterator position to a percentage.
     *
     *  The matching document with the highest weight will get 100% if it
     *  matches all the weighted query terms, and proportionally less if it
     *  only matches some, and other weights are scaled by the same factor.
     *
     *  Documents with a non-zero score will always score at least 1%.
     *
     *  Note that these generally aren't percentages of anything meaningful
     *  (unless you use a custom weighting formula where they are!)
     */
    int convert_to_percent(const MSetIterator & it) const;

    /** Get the termfreq of a term.
     *
     *  @return The number of documents which @a term occurs in.  This
     *		considers all documents in the database being searched, so
     *		gives the same answer as <code>db.get_termfreq(term)</code>
     *		(but is more efficient for query terms as it returns a
     *		value cached during the search.)
     */
    Xapian::doccount get_termfreq(const std::string & term) const;

    /** Get the term weight of a term.
     *
     *  @return	The maximum weight that @a term could have contributed to a
     *		document.
     */
    double get_termweight(const std::string & term) const;

    /** Rank of first item in this MSet.
     *
     *  This is the parameter `first` passed to Xapian::Enquire::get_mset().
     */
    Xapian::doccount get_firstitem() const;

    /** Lower bound on the total number of matching documents. */
    Xapian::doccount get_matches_lower_bound() const;
    /** Estimate of the total number of matching documents. */
    Xapian::doccount get_matches_estimated() const;
    /** Upper bound on the total number of matching documents. */
    Xapian::doccount get_matches_upper_bound() const;

    /** Lower bound on the total number of matching documents before collapsing.
     *
     *  Conceptually the same as get_matches_lower_bound() for the same query
     *  without any collapse part (though the actual value may differ).
     */
    Xapian::doccount get_uncollapsed_matches_lower_bound() const;
    /** Estimate of the total number of matching documents before collapsing.
     *
     *  Conceptually the same as get_matches_estimated() for the same query
     *  without any collapse part (though the actual value may differ).
     */
    Xapian::doccount get_uncollapsed_matches_estimated() const;
    /** Upper bound on the total number of matching documents before collapsing.
     *
     *  Conceptually the same as get_matches_upper_bound() for the same query
     *  without any collapse part (though the actual value may differ).
     */
    Xapian::doccount get_uncollapsed_matches_upper_bound() const;

    /** The maximum weight attained by any document. */
    double get_max_attained() const;
    /** The maximum possible weight any document could achieve. */
    double get_max_possible() const;

    enum {
	/** Model the relevancy of non-query terms in MSet::snippet().
	 *
	 *  Non-query terms will be assigned a small weight, and the snippet
	 *  will tend to prefer snippets which contain a more interesting
	 *  background (where the query term content is equivalent).
	 */
	SNIPPET_BACKGROUND_MODEL = 1,
	/** Exhaustively evaluate candidate snippets in MSet::snippet().
	 *
	 *  Without this flag, snippet generation will stop once it thinks
	 *  it has found a "good enough" snippet, which will generally reduce
	 *  the time taken to generate a snippet.
	 */
	SNIPPET_EXHAUSTIVE = 2,
	/** Return the empty string if no term got matched.
	 *
	 *  If enabled, snippet() returns an empty string if not a single match
	 *  was found in text. If not enabled, snippet() returns a (sub)string
	 *  of text without any highlighted terms.
	 */
	SNIPPET_EMPTY_WITHOUT_MATCH = 4,

	/** Enable generation of n-grams from CJK text.
	 *
	 *  This option highlights CJK searches made using the QueryParser
	 *  FLAG_CJK_NGRAM flag.  Non-CJK characters are split into words as
	 *  normal.
	 *
	 *  The TermGenerator FLAG_CJK_NGRAM flag needs to have been used at
	 *  index time.
	 *
	 *  This mode can also be enabled by setting environment variable
	 *  XAPIAN_CJK_NGRAM to a non-empty value (but doing so was deprecated
	 *  in 1.4.11).
	 *
	 *  @since Added in Xapian 1.4.11.
	 */
	SNIPPET_CJK_NGRAM = 2048
    };

    /** Generate a snippet.
     *
     *  This method selects a continuous run of words from @a text, based
     *  mainly on where the query matches (currently terms, exact phrases and
     *  wildcards are taken into account).  If flag SNIPPET_BACKGROUND_MODEL is
     *  used (which it is by default) then the selection algorithm also
     *  considers the non-query terms in the text with the aim of showing
     *  a context which provides more useful information.
     *
     *  The size of the text selected can be controlled by the @a length
     *  parameter, which specifies a number of bytes of text to aim to select.
     *  However slightly more text may be selected.  Also the size of any
     *  escaping, highlighting or omission markers is not considered.
     *
     *  The returned text is escaped to make it suitable for use in HTML
     *  (though beware that in upstream releases 1.4.5 and earlier this
     *  escaping was sometimes incomplete), and matches with the query will be
     *  highlighted using @a hi_start and @a hi_end.
     *
     *  If the snippet seems to start or end mid-sentence, then @a omit is
     *  prepended or append (respectively) to indicate this.
     *
     *  The same stemming algorithm which was used to build the query should be
     *  specified in @a stemmer.
     *
     *  And @a flags contains flags controlling behaviour.
     *
     *  Added in 1.3.5.
     */
    std::string snippet(const std::string & text,
			size_t length = 500,
			const Xapian::Stem & stemmer = Xapian::Stem(),
			unsigned flags = SNIPPET_BACKGROUND_MODEL|SNIPPET_EXHAUSTIVE,
			const std::string & hi_start = "<b>",
			const std::string & hi_end = "</b>",
			const std::string & omit = "...") const;

    /** Prefetch hint a range of items.
     *
     *  For a remote database, this may start a pipelined fetch of the
     *  requested documents from the remote server.
     *
     *  For a disk-based database, this may send prefetch hints to the
     *  operating system such that the disk blocks the requested documents
     *  are stored in are more likely to be in the cache when we come to
     *  actually read them.
     */
    void fetch(const MSetIterator &begin, const MSetIterator &end) const;

    /** Prefetch hint a single MSet item.
     *
     *  For a remote database, this may start a pipelined fetch of the
     *  requested documents from the remote server.
     *
     *  For a disk-based database, this may send prefetch hints to the
     *  operating system such that the disk blocks the requested documents
     *  are stored in are more likely to be in the cache when we come to
     *  actually read them.
     */
    void fetch(const MSetIterator &item) const;

    /** Prefetch hint the whole MSet.
     *
     *  For a remote database, this may start a pipelined fetch of the
     *  requested documents from the remote server.
     *
     *  For a disk-based database, this may send prefetch hints to the
     *  operating system such that the disk blocks the requested documents
     *  are stored in are more likely to be in the cache when we come to
     *  actually read them.
     */
    void fetch() const { fetch_(0, Xapian::doccount(-1)); }

    /** Return number of items in this MSet object. */
    Xapian::doccount size() const;

    /** Return true if this MSet object is empty. */
    bool empty() const { return size() == 0; }

    /** Efficiently swap this MSet object with another. */
    void swap(MSet & o) { internal.swap(o.internal); }

    /** Return iterator pointing to the first item in this MSet. */
    MSetIterator begin() const;

    /** Return iterator pointing to just after the last item in this MSet. */
    MSetIterator end() const;

    /** Return iterator pointing to the i-th object in this MSet. */
    MSetIterator operator[](Xapian::doccount i) const;

    /** Return iterator pointing to the last object in this MSet. */
    MSetIterator back() const;

    /// Return a string describing this object.
    std::string get_description() const;

    /** @private @internal MSet is what the C++ STL calls a container.
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
    typedef Xapian::MSetIterator value_type;
    /// @private
    typedef Xapian::doccount size_type;
    /// @private
    typedef Xapian::doccount_diff difference_type;
    /// @private
    typedef Xapian::MSetIterator iterator;
    /// @private
    typedef Xapian::MSetIterator const_iterator;
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
    /** @private @internal MSet is what the C++ STL calls a container.
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

/// Iterator over a Xapian::MSet.
class XAPIAN_VISIBILITY_DEFAULT MSetIterator {
    friend class MSet;

    MSetIterator(const Xapian::MSet & mset_, Xapian::doccount off_from_end_)
	: mset(mset_), off_from_end(off_from_end_) { }

  public:
    /** @private @internal The MSet we are iterating over. */
    Xapian::MSet mset;

    /** @private @internal The current position of the iterator.
     *
     *  We store the offset from the end of @a mset, since that means
     *  MSet::end() just needs to set this member to 0.
     */
    Xapian::MSet::size_type off_from_end;

    /** Create an unpositioned MSetIterator. */
    MSetIterator() : off_from_end(0) { }

    /** Get the numeric document id for the current position. */
    Xapian::docid operator*() const;

    /// Advance the iterator to the next position.
    MSetIterator & operator++() {
	--off_from_end;
	return *this;
    }

    /// Advance the iterator to the next position (postfix version).
    MSetIterator operator++(int) {
	MSetIterator retval = *this;
	--off_from_end;
	return retval;
    }

    /// Move the iterator to the previous position.
    MSetIterator & operator--() {
	++off_from_end;
	return *this;
    }

    /// Move the iterator to the previous position (postfix version).
    MSetIterator operator--(int) {
	MSetIterator retval = *this;
	++off_from_end;
	return retval;
    }

    /** @private @internal MSetIterator is what the C++ STL calls an
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
    MSetIterator & operator+=(difference_type n) {
	off_from_end -= n;
	return *this;
    }

    /// Move the iterator back by n positions.
    MSetIterator & operator-=(difference_type n) {
	off_from_end += n;
	return *this;
    }

    /** Return the iterator incremented by @a n positions.
     *
     *  If @a n is negative, decrements by (-n) positions.
     */
    MSetIterator operator+(difference_type n) const {
	return MSetIterator(mset, off_from_end - n);
    }

    /** Return the iterator decremented by @a n positions.
     *
     *  If @a n is negative, increments by (-n) positions.
     */
    MSetIterator operator-(difference_type n) const {
	return MSetIterator(mset, off_from_end + n);
    }

    /** Return the number of positions between @a o and this iterator. */
    difference_type operator-(const MSetIterator& o) const {
	return difference_type(o.off_from_end) - difference_type(off_from_end);
    }

    /** Return the MSet rank for the current position.
     *
     *  The rank of mset[0] is mset.get_firstitem().
     */
    Xapian::doccount get_rank() const {
	return mset.get_firstitem() + (mset.size() - off_from_end);
    }

    /** Get the Document object for the current position. */
    Xapian::Document get_document() const;

    /** Get the weight for the current position. */
    double get_weight() const;

    /** Return the collapse key for the current position.
     *
     *  If collapsing isn't in use, an empty string will be returned.
     */
    std::string get_collapse_key() const;

    /** Return a count of the number of collapses done onto the current key.
     *
     *  This starts at 0, and is incremented each time an item is eliminated
     *  because its key is the same as that of the current item (as returned
     *  by get_collapse_key()).
     *
     *  Note that this is NOT necessarily one less than the total number of
     *  matching documents with this collapse key due to various optimisations
     *  implemented in the matcher - for example, it can skip documents
     *  completely if it can prove their weight wouldn't be enough to make the
     *  result set.
     *
     *  You can say is that if get_collapse_count() > 0 then there are
     *  >= get_collapse_count() other documents with the current collapse
     *  key.  But if get_collapse_count() == 0 then there may or may not be
     *  other such documents.
     */
    Xapian::doccount get_collapse_count() const;

    /** Return the sort key for the current position.
     *
     *  If sorting didn't use a key then an empty string will be returned.
     *
     *  @since Added in Xapian 1.4.6.
     */
    std::string get_sort_key() const;

    /** Convert the weight of the current iterator position to a percentage.
     *
     *  The matching document with the highest weight will get 100% if it
     *  matches all the weighted query terms, and proportionally less if it
     *  only matches some, and other weights are scaled by the same factor.
     *
     *  Documents with a non-zero score will always score at least 1%.
     *
     *  Note that these generally aren't percentages of anything meaningful
     *  (unless you use a custom weighting formula where they are!)
     */
    int get_percent() const {
	return mset.convert_to_percent(get_weight());
    }

    /// Return a string describing this object.
    std::string get_description() const;
};

bool
XAPIAN_NOTHROW(operator==(const MSetIterator &a, const MSetIterator &b));

/// Equality test for MSetIterator objects.
inline bool
operator==(const MSetIterator &a, const MSetIterator &b) XAPIAN_NOEXCEPT
{
    return a.off_from_end == b.off_from_end;
}

inline bool
XAPIAN_NOTHROW(operator!=(const MSetIterator &a, const MSetIterator &b));

/// Inequality test for MSetIterator objects.
inline bool
operator!=(const MSetIterator &a, const MSetIterator &b) XAPIAN_NOEXCEPT
{
    return !(a == b);
}

bool
XAPIAN_NOTHROW(operator<(const MSetIterator &a, const MSetIterator &b));

/// Inequality test for MSetIterator objects.
inline bool
operator<(const MSetIterator &a, const MSetIterator &b) XAPIAN_NOEXCEPT
{
    return a.off_from_end > b.off_from_end;
}

inline bool
XAPIAN_NOTHROW(operator>(const MSetIterator &a, const MSetIterator &b));

/// Inequality test for MSetIterator objects.
inline bool
operator>(const MSetIterator &a, const MSetIterator &b) XAPIAN_NOEXCEPT
{
    return b < a;
}

inline bool
XAPIAN_NOTHROW(operator>=(const MSetIterator &a, const MSetIterator &b));

/// Inequality test for MSetIterator objects.
inline bool
operator>=(const MSetIterator &a, const MSetIterator &b) XAPIAN_NOEXCEPT
{
    return !(a < b);
}

inline bool
XAPIAN_NOTHROW(operator<=(const MSetIterator &a, const MSetIterator &b));

/// Inequality test for MSetIterator objects.
inline bool
operator<=(const MSetIterator &a, const MSetIterator &b) XAPIAN_NOEXCEPT
{
    return !(b < a);
}

/** Return MSetIterator @a it incremented by @a n positions.
 *
 *  If @a n is negative, decrements by (-n) positions.
 */
inline MSetIterator
operator+(MSetIterator::difference_type n, const MSetIterator& it)
{
    return it + n;
}

// Inlined methods of MSet which need MSetIterator to have been defined:

inline void
MSet::fetch(const MSetIterator &begin_it, const MSetIterator &end_it) const
{
    fetch_(begin_it.off_from_end, end_it.off_from_end);
}

inline void
MSet::fetch(const MSetIterator &item) const
{
    fetch_(item.off_from_end, item.off_from_end);
}

inline MSetIterator
MSet::begin() const {
    return MSetIterator(*this, size());
}

inline MSetIterator
MSet::end() const {
    // Decrementing the result of end() needs to work, so we must pass in
    // *this here.
    return MSetIterator(*this, 0);
}

inline MSetIterator
MSet::operator[](Xapian::doccount i) const {
    return MSetIterator(*this, size() - i);
}

inline MSetIterator
MSet::back() const {
    return MSetIterator(*this, 1);
}

inline int
MSet::convert_to_percent(const MSetIterator & it) const {
    return convert_to_percent(it.get_weight());
}

}

#endif // XAPIAN_INCLUDED_MSET_H
