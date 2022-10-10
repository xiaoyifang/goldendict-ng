/** @file
 * @brief Xapian::Query API class
 */
/* Copyright (C) 2011,2012,2013,2014,2015,2016,2017,2018,2019 Olly Betts
 * Copyright (C) 2008 Richard Boulton
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef XAPIAN_INCLUDED_QUERY_H
#define XAPIAN_INCLUDED_QUERY_H

#if !defined XAPIAN_IN_XAPIAN_H && !defined XAPIAN_LIB_BUILD
# error Never use <xapian/query.h> directly; include <xapian.h> instead.
#endif

#include <string>

#include <xapian/attributes.h>
#include <xapian/intrusive_ptr.h>
#include <xapian/postingiterator.h>
#include <xapian/registry.h>
#include <xapian/termiterator.h>
#include <xapian/types.h>
#include <xapian/visibility.h>

class QueryOptimiser; // FIXME

namespace Xapian {

class PostingSource;

/// Class representing a query.
class XAPIAN_VISIBILITY_DEFAULT Query {
  public:
    /// Class representing the query internals.
    class Internal;
    /// @private @internal Reference counted internals.
    Xapian::Internal::intrusive_ptr<Internal> internal;

    /** A query matching no documents.
     *
     *  This is a static instance of a default-constructed Xapian::Query
     *  object.  It is safe to use concurrently from different threads,
     *  unlike @a MatchAll (this is because MatchNothing has a NULL
     *  internal object so there's no reference counting happening).
     *
     *  When combined with other Query objects using the various supported
     *  operators, MatchNothing works like @c false in boolean logic, so
     *  <code>MatchNothing & q</code> is @c MatchNothing, while
     *  <code>MatchNothing | q</code> is @c q.
     */
    static const Xapian::Query MatchNothing;

    /** A query matching all documents.
     *
     *  This is a static instance of <code>Xapian::Query(std::string())</code>.
     *  If you are constructing Query objects which use @a MatchAll in
     *  different threads then the reference counting of the static object can
     *  get messed up by concurrent access so you should instead use
     *  <code>Xapian::Query(std::string())</code> directly.
     */
    static const Xapian::Query MatchAll;

    /** Query operators. */
    enum op {
	/** Match only documents which all subqueries match.
	 *
	 *  When used in a weighted context, the weight is the sum of the
	 *  weights for all the subqueries.
	 */
	OP_AND = 0,

	/** Match documents which at least one subquery matches.
	 *
	 *  When used in a weighted context, the weight is the sum of the
	 *  weights for matching subqueries (so additional matching subqueries
	 *  will mean a higher weight).
	 */
	OP_OR = 1,

	/** Match documents which the first subquery matches but no others do.
	 *
	 *  When used in a weighted context, the weight is just the weight of
	 *  the first subquery.
	 */
	OP_AND_NOT = 2,

	/** Match documents which an odd number of subqueries match.
	 *
	 *  When used in a weighted context, the weight is the sum of the
	 *  weights for matching subqueries (so additional matching subqueries
	 *  will mean a higher weight).
	 */
	OP_XOR = 3,

	/** Match the first subquery taking extra weight from other subqueries.
	 *
	 *  When used in a weighted context, the weight is the sum of the
	 *  weights for matching subqueries (so additional matching subqueries
	 *  will mean a higher weight).
	 *
	 *  Because only the first subquery determines which documents are
	 *  matched, in a non-weighted context only the first subquery matters.
	 */
	OP_AND_MAYBE = 4,

	/** Match like OP_AND but only taking weight from the first subquery.
	 *
	 *  When used in a non-weighted context, OP_FILTER and OP_AND are
	 *  equivalent.
	 *
	 *  In older 1.4.x, the third and subsequent subqueries were ignored
	 *  in some situations.  This was fixed in 1.4.15.
	 */
	OP_FILTER = 5,

	/** Match only documents where all subqueries match near each other.
	 *
	 *  The subqueries must match at term positions within the specified
	 *  window size, in any order.
	 *
	 *  Currently subqueries must be terms or terms composed with OP_OR.
	 *
	 *  When used in a weighted context, the weight is the sum of the
	 *  weights for all the subqueries.
	 */
	OP_NEAR = 6,

	/** Match only documents where all subqueries match near and in order.
	 *
	 *  The subqueries must match at term positions within the specified
	 *  window size, in the same term position order as subquery order.
	 *
	 *  Currently subqueries must be terms or terms composed with OP_OR.
	 *
	 *  When used in a weighted context, the weight is the sum of the
	 *  weights for all the subqueries.
	 */
	OP_PHRASE = 7,

	/** Match only documents where a value slot is within a given range.
	 *
	 *  This operator never contributes weight.
	 */
	OP_VALUE_RANGE = 8,

	/** Scale the weight contributed by a subquery.
	 *
	 *  The weight is the weight of the subquery multiplied by the
	 *  specified non-negative scale factor (so if the scale factor is
	 *  zero then the subquery contributes no weight).
	 */
	OP_SCALE_WEIGHT = 9,

	/** Pick the best N subqueries and combine with OP_OR.
	 *
	 *  If you want to implement a feature which finds documents similar to
	 *  a piece of text, an obvious approach is to build an "OR" query from
	 *  all the terms in the text, and run this query against a database
	 *  containing the documents.  However such a query can contain a lots
	 *  of terms and be quite slow to perform, yet many of these terms
	 *  don't contribute usefully to the results.
	 *
	 *  The OP_ELITE_SET operator can be used instead of OP_OR in this
	 *  situation.  OP_ELITE_SET selects the most important ''N'' terms and
	 *  then acts as an OP_OR query with just these, ignoring any other
	 *  terms.  This will usually return results just as good as the full
	 *  OP_OR query, but much faster.
	 *
	 *  In general, the OP_ELITE_SET operator can be used when you have a
	 *  large OR query, but it doesn't matter if the search completely
	 *  ignores some of the less important terms in the query.
	 *
	 *  The subqueries don't have to be terms.  If they aren't then
	 *  OP_ELITE_SET could potentially pick a subset which doesn't
	 *  actually match any documents even if the full OR would match some
	 *  (because OP_ELITE_SET currently selects those subqueries which can
	 *  return the highest weights).  This is probably rare in practice
	 *  though.
	 *
	 *  You can specify a parameter to the query constructor which controls
	 *  the number of subqueries which OP_ELITE_SET will pick.  If not
	 *  specified, this defaults to 10 (Xapian used to default to
	 *  <code>ceil(sqrt(number_of_subqueries))</code> if there are more
	 *  than 100 subqueries, but this rather arbitrary special case was
	 *  dropped in 1.3.0).  For example, this will pick the best 7 terms:
	 *
	 *  <pre>
	 *  Xapian::Query query(Xapian::Query::OP_ELITE_SET, subqs.begin(), subqs.end(), 7);
	 *  </pre>
	 *
	 *  If the number of subqueries is less than this threshold,
	 *  OP_ELITE_SET behaves identically to OP_OR.
	 *
	 *  When used with a sharded database, OP_ELITE_SET currently picks
	 *  the subqueries to use separately for each shard based on the
	 *  maximum weight they can return in that shard.  This means it
	 *  probably won't select exactly the same terms, and so the results
	 *  of the search may not be exactly the same as for a single database
	 *  with equivalent contents.
	 */
	OP_ELITE_SET = 10,

	/** Match only documents where a value slot is >= a given value.
	 *
	 *  Similar to @a OP_VALUE_RANGE, but open-ended.
	 *
	 *  This operator never contributes weight.
	 */
	OP_VALUE_GE = 11,

	/** Match only documents where a value slot is <= a given value.
	 *
	 *  Similar to @a OP_VALUE_RANGE, but open-ended.
	 *
	 *  This operator never contributes weight.
	 */
	OP_VALUE_LE = 12,

	/** Match like OP_OR but weighting as if a single term.
	 *
	 *  The weight is calculated combining the statistics for the
	 *  subqueries to approximate the weight of a single term occurring
	 *  with those statistics.
	 */
	OP_SYNONYM = 13,

	/** Pick the maximum weight of any subquery.
	 *
	 *  Matches the same documents as @a OP_OR, but the weight contributed
	 *  is the maximum weight from any matching subquery (for OP_OR, it's
	 *  the sum of the weights from the matching subqueries).
	 *
	 *  Added in Xapian 1.3.2.
	 */
	OP_MAX = 14,

	/** Wildcard expansion.
	 *
	 *  Added in Xapian 1.3.3.
	 */
	OP_WILDCARD = 15,

	/** Construct an invalid query.
	 *
	 *  This can be useful as a placeholder - for example @a RangeProcessor
	 *  uses it as a return value to indicate that a range hasn't been
	 *  recognised.
	 */
	OP_INVALID = 99,

	/** Value returned by get_type() for a term. */
	LEAF_TERM = 100,

	/** Value returned by get_type() for a PostingSource. */
	LEAF_POSTING_SOURCE,

	/** Value returned by get_type() for MatchAll or equivalent.
	 *
	 *  This is returned for any <code>Xapian::Query(std::string())</code>
	 *  object.
	 */
	LEAF_MATCH_ALL,

	/** Value returned by get_type() for MatchNothing or equivalent.
	 *
	 *  This is returned for any <code>Xapian::Query()</code> object.
	 */
	LEAF_MATCH_NOTHING
    };

    enum {
	/** Throw an error if OP_WILDCARD exceeds its expansion limit.
	 *
	 *  Xapian::WildcardError will be thrown when the query is actually
	 *  run.
	 */
	WILDCARD_LIMIT_ERROR,
	/** Stop expanding when OP_WILDCARD reaches its expansion limit.
	 *
	 *  This makes the wildcard expand to only the first N terms (sorted
	 *  by byte order).
	 */
	WILDCARD_LIMIT_FIRST,
	/** Limit OP_WILDCARD expansion to the most frequent terms.
	 *
	 *  If OP_WILDCARD would expand to more than its expansion limit, the
	 *  most frequent terms are taken.  This approach works well for cases
	 *  such as expanding a partial term at the end of a query string which
	 *  the user hasn't finished typing yet - as well as being less expense
	 *  to evaluate than the full expansion, using only the most frequent
	 *  terms tends to give better results too.
	 */
	WILDCARD_LIMIT_MOST_FREQUENT
    };

    /** Construct a query matching no documents.
     *
     *  @a MatchNothing is a static instance of this.
     *
     *  When combined with other Query objects using the various supported
     *  operators, <code>Query()</code> works like @c false in boolean logic,
     *  so <code>Query() & q</code> is @c Query(), while
     *  <code>Query() | q</code> is @c q.
     */
    XAPIAN_NOTHROW(Query()) { }

    /// Destructor.
    ~Query() { }

    /** Copying is allowed.
     *
     *  The internals are reference counted, so copying is cheap.
     */
    Query(const Query & o) : internal(o.internal) { }

    /** Copying is allowed.
     *
     *  The internals are reference counted, so assignment is cheap.
     */
    Query & operator=(const Query & o) { internal = o.internal; return *this; }

#ifdef XAPIAN_MOVE_SEMANTICS
    /// Move constructor.
    Query(Query &&) = default;

    /// Move assignment operator.
    Query & operator=(Query &&) = default;
#endif

    /** Construct a Query object for a term.
     *
     *  @param term The term.  An empty string constructs a query matching
     *		    all documents (@a MatchAll is a static instance of this).
     *  @param wqf  The within-query frequency. (default: 1)
     *  @param pos  The query position.  Currently this is mainly used to
     *		    determine the order of terms obtained via
     *		    get_terms_begin(). (default: 0)
     */
    Query(const std::string & term,
	  Xapian::termcount wqf = 1,
	  Xapian::termpos pos = 0);

    /** Construct a Query object for a PostingSource. */
    explicit Query(Xapian::PostingSource * source);

    /** Scale using OP_SCALE_WEIGHT.
     *
     *  @param factor Non-negative real number to multiply weights by.
     *  @param subquery Query object to scale weights from.
     */
    Query(double factor, const Xapian::Query & subquery);

    /** Scale using OP_SCALE_WEIGHT.
     *
     *  In this form, the op_ parameter is totally redundant - use
     *  Query(factor, subquery) in preference.
     *
     *  @param op_	Must be OP_SCALE_WEIGHT.
     *  @param factor	Non-negative real number to multiply weights by.
     *  @param subquery	Query object to scale weights from.
     */
    Query(op op_, const Xapian::Query & subquery, double factor);

    /** Construct a Query object by combining two others.
     *
     *  @param op_	The operator to combine the queries with.
     *  @param a	First subquery.
     *  @param b	Second subquery.
     */
    Query(op op_, const Xapian::Query & a, const Xapian::Query & b)
    {
	init(op_, 2);
	bool positional = (op_ == OP_NEAR || op_ == OP_PHRASE);
	add_subquery(positional, a);
	add_subquery(positional, b);
	done();
    }

    /** Construct a Query object by combining two terms.
     *
     *  @param op_	The operator to combine the terms with.
     *  @param a	First term.
     *  @param b	Second term.
     */
    Query(op op_, const std::string & a, const std::string & b)
    {
	init(op_, 2);
	add_subquery(false, a);
	add_subquery(false, b);
	done();
    }

    /** Construct a Query object for a single-ended value range.
     *
     *  @param op_		Must be OP_VALUE_LE or OP_VALUE_GE currently.
     *  @param slot		The value slot to work over.
     *  @param range_limit	The limit of the range.
     */
    Query(op op_, Xapian::valueno slot, const std::string & range_limit);

    /** Construct a Query object for a value range.
     *
     *  @param op_		Must be OP_VALUE_RANGE currently.
     *  @param slot		The value slot to work over.
     *  @param range_lower	Lower end of the range.
     *  @param range_upper	Upper end of the range.
     */
    Query(op op_, Xapian::valueno slot,
	  const std::string & range_lower, const std::string & range_upper);

    /** Query constructor for OP_WILDCARD queries.
     *
     *  @param op_	Must be OP_WILDCARD
     *  @param pattern	The wildcard pattern - currently this is just a string
     *			and the wildcard expands to terms which start with
     *			exactly this string.
     *	@param max_expansion	The maximum number of terms to expand to
     *				(default: 0, which means no limit)
     *	@param max_type	How to enforce max_expansion - one of
     *			@a WILDCARD_LIMIT_ERROR (the default),
     *			@a WILDCARD_LIMIT_FIRST or
     *			@a WILDCARD_LIMIT_MOST_FREQUENT.
     *			When searching multiple databases, the expansion limit
     *			is currently applied independently for each database,
     *			so the total number of terms may be higher than the
     *			limit.  This is arguably a bug, and may change in
     *			future versions.
     *	@param combiner The @a Query::op to combine the terms with - one of
     *			@a OP_SYNONYM (the default), @a OP_OR or @a OP_MAX.
     */
    Query(op op_,
	  const std::string & pattern,
	  Xapian::termcount max_expansion = 0,
	  int max_type = WILDCARD_LIMIT_ERROR,
	  op combiner = OP_SYNONYM);

    /** Construct a Query object from a begin/end iterator pair.
     *
     *  Dereferencing the iterator should return a Xapian::Query, a non-NULL
     *  Xapian::Query*, a std::string or a type which converts to one of
     *  these (e.g. const char*).
     *
     *  If begin == end then there are no subqueries and the resulting Query
     *  won't match anything.
     *
     *  @param op_	The operator to combine the queries with.
     *  @param begin	Begin iterator.
     *  @param end	End iterator.
     *  @param window	Window size for OP_NEAR and OP_PHRASE, or 0 to use the
     *			number of subqueries as the window size (default: 0).
     */
    template<typename I>
    Query(op op_, I begin, I end, Xapian::termcount window = 0)
    {
	if (begin != end) {
	    typedef typename std::iterator_traits<I>::iterator_category iterator_category;
	    init(op_, window, begin, end, iterator_category());
	    bool positional = (op_ == OP_NEAR || op_ == OP_PHRASE);
	    for (I i = begin; i != end; ++i) {
		add_subquery(positional, *i);
	    }
	    done();
	}
    }

#ifdef SWIG
    // SWIG's %template doesn't seem to handle a templated ctor so we
    // provide this fake specialised form of the above prototype.
    Query(op op_, XapianSWIGQueryItor qbegin, XapianSWIGQueryItor qend,
	  Xapian::termcount parameter = 0);

# ifdef SWIGJAVA
    Query(op op_, XapianSWIGStrItor qbegin, XapianSWIGStrItor qend,
	  Xapian::termcount parameter = 0);
# endif
#endif

    /** Begin iterator for terms in the query object.
     *
     *  The iterator returns terms in ascending query position order, and
     *  will return the same term in each unique position it occurs in.
     *  If you want the terms in sorted order and without duplicates, see
     *  get_unique_terms_begin().
     */
    const TermIterator get_terms_begin() const;

    /// End iterator for terms in the query object.
    const TermIterator XAPIAN_NOTHROW(get_terms_end() const) {
	return TermIterator();
    }

    /** Begin iterator for unique terms in the query object.
     *
     *  Terms are sorted and terms with the same name removed from the list.
     *
     *  If you want the terms in ascending query position order, see
     *  get_terms_begin().
     */
    const TermIterator get_unique_terms_begin() const;

    /// End iterator for unique terms in the query object.
    const TermIterator XAPIAN_NOTHROW(get_unique_terms_end() const) {
	return TermIterator();
    }

    /** Return the length of this query object. */
    Xapian::termcount XAPIAN_NOTHROW(get_length() const) XAPIAN_PURE_FUNCTION;

    /** Check if this query is Xapian::Query::MatchNothing. */
    bool XAPIAN_NOTHROW(empty() const) {
	return internal.get() == 0;
    }

    /** Serialise this object into a string. */
    std::string serialise() const;

    /** Unserialise a string and return a Query object.
     *
     *  @param serialised	the string to unserialise.
     *  @param reg		Xapian::Registry object to use to unserialise
     *				user-subclasses of Xapian::PostingSource
     *				(default: standard registry).
     */
    static const Query unserialise(const std::string & serialised,
				   const Registry & reg = Registry());

    /** Get the type of the top level of the query. */
    op XAPIAN_NOTHROW(get_type() const) XAPIAN_PURE_FUNCTION;

    /** Get the number of subqueries of the top level query. */
    size_t XAPIAN_NOTHROW(get_num_subqueries() const) XAPIAN_PURE_FUNCTION;

    /** Read a top level subquery.
      *
      * @param n  Return the n-th subquery (starting from 0) - only valid when
      *		  0 <= n < get_num_subqueries().
      */
    const Query get_subquery(size_t n) const;

    /// Return a string describing this object.
    std::string get_description() const;

    /** Combine with another Xapian::Query object using OP_AND.
     *
     *  @since Since Xapian 1.4.10, when called on a Query object which is
     *  OP_AND and has a reference count of 1, then @a o is appended as a new
     *  subquery (provided @a o is a different Query object and
     *  <code>!o.empty()</code>).
     */
    const Query operator&=(const Query & o);

    /** Combine with another Xapian::Query object using OP_OR.
     *
     *  @since Since Xapian 1.4.10, when called on a Query object which is
     *  OP_OR and has a reference count of 1, then @a o is appended as a new
     *  subquery (provided @a o is a different Query object and
     *  <code>!o.empty()</code>).
     */
    const Query operator|=(const Query & o);

    /** Combine with another Xapian::Query object using OP_XOR.
     *
     *  @since Since Xapian 1.4.10, when called on a Query object which is
     *  OP_XOR and has a reference count of 1, then @a o is appended as a new
     *  subquery (provided @a o is a different Query object and
     *  <code>!o.empty()</code>).
     */
    const Query operator^=(const Query & o);

    /** Scale using OP_SCALE_WEIGHT.
     *
     *  @param factor Non-negative real number to multiply weights by.
     */
    const Query operator*=(double factor) {
	return (*this = Query(factor, *this));
    }

    /** Inverse scale using OP_SCALE_WEIGHT.
     *
     *  @param factor Positive real number to divide weights by.
     */
    const Query operator/=(double factor) {
	return (*this = Query(1.0 / factor, *this));
    }

    /** @private @internal */
    explicit Query(Internal * internal_) : internal(internal_) { }

    /** Construct with just an operator.
     *
     *  @param op_ The operator to use - currently only OP_INVALID is useful.
     */
    explicit Query(Query::op op_) {
	init(op_, 0);
	if (op_ != Query::OP_INVALID) done();
    }

  private:
    void init(Query::op op_, size_t n_subqueries, Xapian::termcount window = 0);

    template<typename I>
    void init(Query::op op_, Xapian::termcount window,
	      const I & begin, const I & end, std::random_access_iterator_tag)
    {
	init(op_, end - begin, window);
    }

    template<typename I>
    void init(Query::op op_, Xapian::termcount window,
	      const I &, const I &, std::input_iterator_tag)
    {
	init(op_, 0, window);
    }

    void add_subquery(bool positional, const Xapian::Query & subquery);

    void add_subquery(bool, const std::string & subquery) {
	add_subquery(false, Xapian::Query(subquery));
    }

    void add_subquery(bool positional, const Xapian::Query * subquery) {
	// FIXME: subquery NULL?
	add_subquery(positional, *subquery);
    }

    void done();
};

/** Combine two Xapian::Query objects using OP_AND. */
inline const Query
operator&(const Query & a, const Query & b)
{
    return Query(Query::OP_AND, a, b);
}

/** Combine two Xapian::Query objects using OP_OR. */
inline const Query
operator|(const Query & a, const Query & b)
{
    return Query(Query::OP_OR, a, b);
}

/** Combine two Xapian::Query objects using OP_XOR. */
inline const Query
operator^(const Query & a, const Query & b)
{
    return Query(Query::OP_XOR, a, b);
}

/** Scale a Xapian::Query object using OP_SCALE_WEIGHT.
 *
 *  @param factor Non-negative real number to multiply weights by.
 *  @param q Xapian::Query object.
 */
inline const Query
operator*(double factor, const Query & q)
{
    return Query(factor, q);
}

/** Scale a Xapian::Query object using OP_SCALE_WEIGHT.
 *
 *  @param q Xapian::Query object.
 *  @param factor Non-negative real number to multiply weights by.
 */
inline const Query
operator*(const Query & q, double factor)
{
    return Query(factor, q);
}

/** Inverse-scale a Xapian::Query object using OP_SCALE_WEIGHT.
 *
 *  @param factor Positive real number to divide weights by.
 *  @param q Xapian::Query object.
 */
inline const Query
operator/(const Query & q, double factor)
{
    return Query(1.0 / factor, q);
}

/** @private @internal */
class InvertedQuery_ {
    const Query & query;

    void operator=(const InvertedQuery_ &);

    explicit InvertedQuery_(const Query & query_) : query(query_) { }

  public:
    // GCC 4.2 seems to needs a copy ctor.
    InvertedQuery_(const InvertedQuery_ & o) : query(o.query) { }

    operator Query() const {
	return Query(Query::OP_AND_NOT, Query(std::string()), query);
    }

    friend const InvertedQuery_ operator~(const Query &q);

    friend const Query operator&(const Query & a, const InvertedQuery_ & b);

    friend const Query operator&=(Query & a, const InvertedQuery_ & b);
};

/** Combine two Xapian::Query objects using OP_AND_NOT.
 *
 *  E.g. Xapian::Query q = q1 &~ q2;
 */
inline const Query
operator&(const Query & a, const InvertedQuery_ & b)
{
    return Query(Query::OP_AND_NOT, a, b.query);
}

/** Combine two Xapian::Query objects using OP_AND_NOT with result in the first.
 *
 *  E.g. q1 &=~ q2;
 */
inline const Query
operator&=(Query & a, const InvertedQuery_ & b)
{
    return (a = Query(Query::OP_AND_NOT, a, b.query));
}

#ifndef DOXYGEN /* @internal doesn't seem to avoid a warning here. */
/** @internal Helper to allow q1 &~ q2 to work. */
inline const InvertedQuery_
operator~(const Query &q)
{
    return InvertedQuery_(q);
}
#endif

namespace Internal {
class AndContext;
class OrContext;
class XorContext;
}

/** @private @internal */
class Query::Internal : public Xapian::Internal::intrusive_base {
  public:
    XAPIAN_NOTHROW(Internal()) { }

    virtual ~Internal();

    virtual PostingIterator::Internal * postlist(QueryOptimiser * qopt, double factor) const = 0;

    virtual void postlist_sub_and_like(Xapian::Internal::AndContext& ctx,
				       QueryOptimiser * qopt,
				       double factor) const;

    virtual void postlist_sub_or_like(Xapian::Internal::OrContext& ctx,
				      QueryOptimiser * qopt,
				      double factor) const;

    virtual void postlist_sub_xor(Xapian::Internal::XorContext& ctx,
				  QueryOptimiser * qopt,
				  double factor) const;

    virtual termcount XAPIAN_NOTHROW(get_length() const) XAPIAN_PURE_FUNCTION;

    virtual void serialise(std::string & result) const = 0;

    static Query::Internal * unserialise(const char ** p, const char * end, const Registry & reg);

    virtual Query::op XAPIAN_NOTHROW(get_type() const) XAPIAN_PURE_FUNCTION = 0;
    virtual size_t XAPIAN_NOTHROW(get_num_subqueries() const) XAPIAN_PURE_FUNCTION;
    virtual const Query get_subquery(size_t n) const;

    virtual std::string get_description() const = 0;

    // Pass argument as void* to avoid need to include <vector>.
    virtual void gather_terms(void * void_terms) const;
};

inline const Query
Query::operator&=(const Query & o)
{
    if (o.empty()) {
	// q &= empty_query sets q to empty_query.
	*this = o;
    } else if (this != &o &&
	       internal.get() &&
	       internal->_refs == 1 &&
	       get_type() == OP_AND) {
	// Appending a subquery to an existing AND.
	add_subquery(false, o);
    } else {
	*this = Query(OP_AND, *this, o);
    }
    return *this;
}

inline const Query
Query::operator|=(const Query & o)
{
    if (o.empty()) {
	// q |= empty_query is a no-op.
    } else if (this != &o &&
	       internal.get() &&
	       internal->_refs == 1 &&
	       get_type() == OP_OR) {
	// Appending a subquery to an existing OR.
	add_subquery(false, o);
    } else {
	*this = Query(OP_OR, *this, o);
    }
    return *this;
}

inline const Query
Query::operator^=(const Query & o)
{
    if (o.empty()) {
	// q ^= empty_query is a no-op.
    } else if (internal.get() == o.internal.get()) {
	// q ^= q gives MatchNothing.
	internal = NULL;
    } else if (internal.get() &&
	       internal->_refs == 1 &&
	       get_type() == OP_XOR) {
	// Appending a subquery to an existing XOR.
	add_subquery(false, o);
    } else {
	*this = Query(OP_XOR, *this, o);
    }
    return *this;
}

}

#endif // XAPIAN_INCLUDED_QUERY_H
