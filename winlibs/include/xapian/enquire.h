/** @file
 * @brief API for running queries
 */
/* Copyright 1999,2000,2001 BrightStation PLC
 * Copyright 2001,2002 Ananova Ltd
 * Copyright 2002,2003,2004,2005,2006,2007,2008,2009,2011,2012,2013,2014,2015,2016 Olly Betts
 * Copyright 2009 Lemur Consulting Ltd
 * Copyright 2011 Action Without Borders
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

#ifndef XAPIAN_INCLUDED_ENQUIRE_H
#define XAPIAN_INCLUDED_ENQUIRE_H

#if !defined XAPIAN_IN_XAPIAN_H && !defined XAPIAN_LIB_BUILD
# error Never use <xapian/enquire.h> directly; include <xapian.h> instead.
#endif

#include "xapian/deprecated.h"
#include <string>

#include <xapian/attributes.h>
#include <xapian/eset.h>
#include <xapian/intrusive_ptr.h>
#include <xapian/mset.h>
#include <xapian/stem.h>
#include <xapian/types.h>
#include <xapian/termiterator.h>
#include <xapian/visibility.h>

namespace Xapian {

class Database;
class Document;
class ErrorHandler;
class ExpandDecider;
class KeyMaker;
class MatchSpy;
class Query;
class Weight;

/** A relevance set (R-Set).
 *  This is the set of documents which are marked as relevant, for use
 *  in modifying the term weights, and in performing query expansion.
 */
class XAPIAN_VISIBILITY_DEFAULT RSet {
    public:
	/// Class holding details of RSet
	class Internal;

	/// @private @internal Reference counted internals.
	Xapian::Internal::intrusive_ptr<Internal> internal;

	/// Copy constructor
	RSet(const RSet &rset);

	/// Assignment operator
	void operator=(const RSet &rset);

#ifdef XAPIAN_MOVE_SEMANTICS
	/// Move constructor.
	RSet(RSet && o);

	/// Move assignment operator.
	RSet & operator=(RSet && o);
#endif

	/// Default constructor
	RSet();

	/// Destructor
	~RSet();

	/** The number of documents in this R-Set */
	Xapian::doccount size() const;

	/** Test if this R-Set is empty */
	bool empty() const;

	/// Add a document to the relevance set.
	void add_document(Xapian::docid did);

	/// Add a document to the relevance set.
	void add_document(const Xapian::MSetIterator & i) { add_document(*i); }

	/// Remove a document from the relevance set.
	void remove_document(Xapian::docid did);

	/// Remove a document from the relevance set.
	void remove_document(const Xapian::MSetIterator & i) { remove_document(*i); }

	/// Test if a given document in the relevance set.
	bool contains(Xapian::docid did) const;

	/// Test if a given document in the relevance set.
	bool contains(const Xapian::MSetIterator & i) const { return contains(*i); }

	/// Return a string describing this object.
	std::string get_description() const;
};

/** Base class for matcher decision functor.
 */
class XAPIAN_VISIBILITY_DEFAULT MatchDecider {
	/// Don't allow assignment.
	void operator=(const MatchDecider &);

	/// Don't allow copying.
	MatchDecider(const MatchDecider &);

    public:
	/// Default constructor
	MatchDecider() { }

	/** Decide whether we want this document to be in the MSet.
	 *
	 *  @param doc	The document to test.
	 *
	 *  @return true if the document is acceptable, or false if the document
	 *  should be excluded from the MSet.
	 */
	virtual bool operator()(const Xapian::Document &doc) const = 0;

	/// Destructor.
	virtual ~MatchDecider();
};

/** This class provides an interface to the information retrieval
 *  system for the purpose of searching.
 *
 *  Databases are usually opened lazily, so exceptions may not be
 *  thrown where you would expect them to be.  You should catch
 *  Xapian::Error exceptions when calling any method in Xapian::Enquire.
 *
 *  @exception Xapian::InvalidArgumentError will be thrown if an invalid
 *  argument is supplied, for example, an unknown database type.
 */
class XAPIAN_VISIBILITY_DEFAULT Enquire {
    public:
	/// Copying is allowed (and is cheap).
	Enquire(const Enquire & other);

	/// Assignment is allowed (and is cheap).
	void operator=(const Enquire & other);

#ifdef XAPIAN_MOVE_SEMANTICS
	/// Move constructor.
	Enquire(Enquire&& o);

	/// Move assignment operator.
	Enquire& operator=(Enquire&& o);
#endif

	class Internal;
	/// @private @internal Reference counted internals.
	Xapian::Internal::intrusive_ptr<Internal> internal;

	/** Create a Xapian::Enquire object.
	 *
	 *  This specification cannot be changed once the Xapian::Enquire is
	 *  opened: you must create a new Xapian::Enquire object to access a
	 *  different database, or set of databases.
	 *
	 *  The database supplied must have been initialised (ie, must not be
	 *  the result of calling the Database::Database() constructor).  If
	 *  you need to handle a situation where you have no databases
	 *  gracefully, a database created with DB_BACKEND_INMEMORY can be
	 *  passed here to provide a completely empty database.
	 *
	 *  @param database Specification of the database or databases to
	 *	   use.
	 *
	 *  @exception Xapian::InvalidArgumentError will be thrown if an
	 *  empty Database object is supplied.
	 */
	explicit Enquire(const Database &database);

	/** Create a Xapian::Enquire object.
	 *
	 *  This specification cannot be changed once the Xapian::Enquire is
	 *  opened: you must create a new Xapian::Enquire object to access a
	 *  different database, or set of databases.
	 *
	 *  The database supplied must have been initialised (ie, must not be
	 *  the result of calling the Database::Database() constructor).  If
	 *  you need to handle a situation where you have no databases
	 *  gracefully, a database created with DB_BACKEND_INMEMORY can be
	 *  passed here to provide a completely empty database.
	 *
	 *  @param database Specification of the database or databases to
	 *	   use.
	 *  @param errorhandler_  This parameter is deprecated (since Xapian
	 *	   1.3.1), and as of 1.3.5 it's ignored completely.
	 *
	 *  @exception Xapian::InvalidArgumentError will be thrown if an
	 *  empty Database object is supplied.
	 */
	XAPIAN_DEPRECATED_EX(Enquire(const Database &database, ErrorHandler * errorhandler_));

	/** Close the Xapian::Enquire object.
	 */
	~Enquire();

	/** Set the query to run.
	 *
	 *  @param query  the new query to run.
	 *  @param qlen   the query length to use in weight calculations -
	 *	by default the sum of the wqf of all terms is used.
	 */
	void set_query(const Xapian::Query & query, Xapian::termcount qlen = 0);

	/** Get the current query.
	 *
	 *  If called before set_query(), this will return a default
	 *  initialised Query object.
	 */
	const Xapian::Query & get_query() const;

	/** Add a matchspy.
	 *
	 *  This matchspy will be called with some of the documents which match
	 *  the query, during the match process.  Exactly which of the matching
	 *  documents are passed to it depends on exactly when certain
	 *  optimisations occur during the match process, but it can be
	 *  controlled to some extent by setting the @a checkatleast parameter
	 *  to @a get_mset().
	 *
	 *  In particular, if there are enough matching documents, at least the
	 *  number specified by @a checkatleast will be passed to the matchspy.
	 *  This means that you can force the matchspy to be shown all matching
	 *  documents by setting @a checkatleast to the number of documents in
	 *  the database.
	 *
	 *  @param spy       The MatchSpy subclass to add.  The caller must
	 *                   ensure that this remains valid while the Enquire
	 *                   object remains active, or until @a
	 *                   clear_matchspies() is called.
	 */
	void add_matchspy(MatchSpy * spy);

	/** Remove all the matchspies.
	 */
	void clear_matchspies();

	/** Set the weighting scheme to use for queries.
	 *
	 *  @param weight_  the new weighting scheme.  If no weighting scheme
	 *		    is specified, the default is BM25 with the
	 *		    default parameters.
	 */
	void set_weighting_scheme(const Weight &weight_);

	/** Set the weighting scheme to use for expansion.
	 *
	 *  If you don't call this method, the default is as if you'd used:
	 *
	 *  get_expansion_scheme("trad");
	 *
	 *  @param eweightname_  A string in lowercase specifying the name of
	 *                       the scheme to be used. The following schemes
	 *                       are currently available:
	 *                       "bo1" : The Bo1 scheme for query expansion.
	 *                       "trad" : The TradWeight scheme for query expansion.
	 *  @param expand_k_ The parameter required for TradWeight query expansion.
	 *                   A default value of 1.0 is used if none is specified.
	 */
	void set_expansion_scheme(const std::string &eweightname_,
				  double expand_k_ = 1.0) const;

	/** Set the collapse key to use for queries.
	 *
	 *  @param collapse_key  value number to collapse on - at most one MSet
	 *	entry with each particular value will be returned
	 *	(default is Xapian::BAD_VALUENO which means no collapsing).
	 *
	 *  @param collapse_max	 Max number of items with the same key to leave
	 *			 after collapsing (default 1).
	 *
	 *	The MSet returned by get_mset() will have only the "best"
	 *	(at most) @a collapse_max entries with each particular
	 *	value of @a collapse_key ("best" being highest ranked - i.e.
	 *	highest weight or highest sorting key).
	 *
	 *	An example use might be to create a value for each document
	 *	containing an MD5 hash of the document contents.  Then
	 *	duplicate documents from different sources can be eliminated at
	 *	search time by collapsing with @a collapse_max = 1 (it's better
	 *	to eliminate duplicates at index time, but this may not be
	 *	always be possible - for example the search may be over more
	 *	than one Xapian database).
	 *
	 *	Another use is to group matches in a particular category (e.g.
	 *	you might collapse a mailing list search on the Subject: so
	 *	that there's only one result per discussion thread).  In this
	 *	case you can use get_collapse_count() to give the user some
	 *	idea how many other results there are.  And if you index the
	 *	Subject: as a boolean term as well as putting it in a value,
	 *	you can offer a link to a non-collapsed search restricted to
	 *	that thread using a boolean filter.
	 */
	void set_collapse_key(Xapian::valueno collapse_key,
			      Xapian::doccount collapse_max = 1);

	/** Ordering of docids.
	 *
	 *  Parameter to Enquire::set_docid_order().
	 */
	typedef enum {
	    /** docids sort in ascending order (default) */
	    ASCENDING = 1,
	    /** docids sort in descending order. */
	    DESCENDING = 0,
	    /** docids sort in whatever order is most efficient for the
	     *  backend. */
	    DONT_CARE = 2
	} docid_order;

	/** Set sort order for document IDs.
	 *
	 *  This order only has an effect on documents which would otherwise
	 *  have equal rank.  When ordering by relevance without a sort key,
	 *  this means documents with equal weight.  For a boolean match
	 *  with no sort key, this means all documents.  And if a sort key
	 *  is used, this means documents with the same sort key (and also equal
	 *  weight if ordering on relevance before or after the sort key).
	 *
	 * @param order  This can be:
	 * - Xapian::Enquire::ASCENDING
	 *	docids sort in ascending order (default)
	 * - Xapian::Enquire::DESCENDING
	 *	docids sort in descending order
	 * - Xapian::Enquire::DONT_CARE
	 *      docids sort in whatever order is most efficient for the backend
	 *
	 *  Note: If you add documents in strict date order, then a boolean
	 *  search - i.e. set_weighting_scheme(Xapian::BoolWeight()) - with
	 *  set_docid_order(Xapian::Enquire::DESCENDING) is an efficient
	 *  way to perform "sort by date, newest first", and with
	 *  set_docid_order(Xapian::Enquire::ASCENDING) a very efficient way
	 *  to perform "sort by date, oldest first".
	 */
	void set_docid_order(docid_order order);

	/** Set the percentage and/or weight cutoffs.
	 *
	 * @param percent_cutoff Minimum percentage score for returned
	 *	documents. If a document has a lower percentage score than this,
	 *	it will not appear in the MSet.  If your intention is to return
	 *	only matches which contain all the terms in the query, then
	 *	it's more efficient to use Xapian::Query::OP_AND instead of
	 *	Xapian::Query::OP_OR in the query than to use set_cutoff(100).
	 *	(default 0 => no percentage cut-off).
	 * @param weight_cutoff Minimum weight for a document to be returned.
	 *	If a document has a lower score that this, it will not appear
	 *	in the MSet.  It is usually only possible to choose an
	 *	appropriate weight for cutoff based on the results of a
	 *	previous run of the same query; this is thus mainly useful for
	 *	alerting operations.  The other potential use is with a user
	 *	specified weighting scheme.
	 *	(default 0 => no weight cut-off).
	 */
	void set_cutoff(int percent_cutoff, double weight_cutoff = 0);

	/** Set the sorting to be by relevance only.
	 *
	 *  This is the default.
	 */
	void set_sort_by_relevance();

	/** Set the sorting to be by value only.
	 *
	 *  Note that sorting by values uses a string comparison, so to use
	 *  this to sort by a numeric value you'll need to store the numeric
	 *  values in a manner which sorts appropriately.  For example, you
	 *  could use Xapian::sortable_serialise() (which works for floating
	 *  point numbers as well as integers), or store numbers padded with
	 *  leading zeros or spaces, or with the number of digits prepended.
	 *
	 * @param sort_key  value number to sort on.
	 *
	 * @param reverse   If true, reverses the sort order.
	 */
	void set_sort_by_value(Xapian::valueno sort_key, bool reverse);

	/** Set the sorting to be by key generated from values only.
	 *
	 * @param sorter    The functor to use for generating keys.
	 *
	 * @param reverse   If true, reverses the sort order.
	 */
	void set_sort_by_key(Xapian::KeyMaker * sorter, bool reverse);

	/** Set the sorting to be by value, then by relevance for documents
	 *  with the same value.
	 *
	 *  Note that sorting by values uses a string comparison, so to use
	 *  this to sort by a numeric value you'll need to store the numeric
	 *  values in a manner which sorts appropriately.  For example, you
	 *  could use Xapian::sortable_serialise() (which works for floating
	 *  point numbers as well as integers), or store numbers padded with
	 *  leading zeros or spaces, or with the number of digits prepended.
	 *
	 * @param sort_key  value number to sort on.
	 *
	 * @param reverse   If true, reverses the sort order.
	 */
	void set_sort_by_value_then_relevance(Xapian::valueno sort_key,
					      bool reverse);

	/** Set the sorting to be by keys generated from values, then by
	 *  relevance for documents with identical keys.
	 *
	 * @param sorter    The functor to use for generating keys.
	 *
	 * @param reverse   If true, reverses the sort order.
	 */
	void set_sort_by_key_then_relevance(Xapian::KeyMaker * sorter,
					    bool reverse);

	/** Set the sorting to be by relevance then value.
	 *
	 *  Note that sorting by values uses a string comparison, so to use
	 *  this to sort by a numeric value you'll need to store the numeric
	 *  values in a manner which sorts appropriately.  For example, you
	 *  could use Xapian::sortable_serialise() (which works for floating
	 *  point numbers as well as integers), or store numbers padded with
	 *  leading zeros or spaces, or with the number of digits prepended.
	 *
	 *  Note that with the default BM25 weighting scheme parameters,
	 *  non-identical documents will rarely have the same weight, so
	 *  this setting will give very similar results to
	 *  set_sort_by_relevance().  It becomes more useful with particular
	 *  BM25 parameter settings (e.g. BM25Weight(1,0,1,0,0)) or custom
	 *  weighting schemes.
	 *
	 * @param sort_key  value number to sort on.
	 *
	 * @param reverse   If true, reverses the sort order of sort_key.
	 *		    Beware that in 1.2.16 and earlier, the sense
	 *		    of this parameter was incorrectly inverted
	 *		    and inconsistent with the other set_sort_by_...
	 *		    methods.  This was fixed in 1.2.17, so make that
	 *		    version a minimum requirement if this detail
	 *		    matters to your application.
	 */
	void set_sort_by_relevance_then_value(Xapian::valueno sort_key,
					      bool reverse);

	/** Set the sorting to be by relevance, then by keys generated from
	 *  values.
	 *
	 *  Note that with the default BM25 weighting scheme parameters,
	 *  non-identical documents will rarely have the same weight, so
	 *  this setting will give very similar results to
	 *  set_sort_by_relevance().  It becomes more useful with particular
	 *  BM25 parameter settings (e.g. BM25Weight(1,0,1,0,0)) or custom
	 *  weighting schemes.
	 *
	 * @param sorter    The functor to use for generating keys.
	 *
	 * @param reverse   If true, reverses the sort order of the generated
	 *		    keys.  Beware that in 1.2.16 and earlier, the sense
	 *		    of this parameter was incorrectly inverted
	 *		    and inconsistent with the other set_sort_by_...
	 *		    methods.  This was fixed in 1.2.17, so make that
	 *		    version a minimum requirement if this detail
	 *		    matters to your application.
	 */
	void set_sort_by_relevance_then_key(Xapian::KeyMaker * sorter,
					    bool reverse);

	/** Set a time limit for the match.
	 *
	 *  Matches with check_at_least set high can take a long time in some
	 *  cases.  You can set a time limit on this, after which check_at_least
	 *  will be turned off.
	 *
	 *  @param time_limit  time in seconds after which to disable
	 *		       check_at_least (default: 0.0 which means no
	 *		       time limit)
	 *
	 *  Limitations:
	 *
	 *  This feature is currently supported on platforms which support POSIX
	 *  interval timers.  Interaction with the remote backend when using
	 *  multiple databases may have bugs.  There's not currently a way to
	 *  force the match to end after a certain time.
	 */
	void set_time_limit(double time_limit);

	/** Get (a portion of) the match set for the current query.
	 *
	 *  @param first     the first item in the result set to return.
	 *		     A value of zero corresponds to the first item
	 *		     returned being that with the highest score.
	 *		     A value of 10 corresponds to the first 10 items
	 *		     being ignored, and the returned items starting
	 *		     at the eleventh.
	 *  @param maxitems  the maximum number of items to return.  If you
	 *		     want all matches, then you can pass the result
	 *		     of calling get_doccount() on the Database object
	 *		     (though if you are doing this so you can filter
	 *		     results, you are likely to get much better
	 *		     performance by using Xapian's match-time filtering
	 *		     features instead).  You can pass 0 for maxitems
	 *		     which will give you an empty MSet with valid
	 *		     statistics (such as get_matches_estimated())
	 *		     calculated without looking at any postings, which
	 *		     is very quick, but means the estimates may be
	 *		     more approximate and the bounds may be much
	 *		     looser.
	 *  @param checkatleast  the minimum number of items to check.  Because
	 *		     the matcher optimises, it won't consider every
	 *		     document which might match, so the total number
	 *		     of matches is estimated.  Setting checkatleast
	 *		     forces it to consider at least this many matches
	 *		     and so allows for reliable paging links.
	 *  @param omrset    the relevance set to use when performing the query.
	 *  @param mdecider  a decision functor to use to decide whether a
	 *		     given document should be put in the MSet.
	 *
	 *  @return	     A Xapian::MSet object containing the results of the
	 *		     query.
	 *
	 *  @exception Xapian::InvalidArgumentError  See class documentation.
	 */
	MSet get_mset(Xapian::doccount first, Xapian::doccount maxitems,
		      Xapian::doccount checkatleast = 0,
		      const RSet * omrset = 0,
		      const MatchDecider * mdecider = 0) const;

	/** Get (a portion of) the match set for the current query.
	 *
	 *  @param first     the first item in the result set to return.
	 *		     A value of zero corresponds to the first item
	 *		     returned being that with the highest score.
	 *		     A value of 10 corresponds to the first 10 items
	 *		     being ignored, and the returned items starting
	 *		     at the eleventh.
	 *  @param maxitems  the maximum number of items to return.  If you
	 *		     want all matches, then you can pass the result
	 *		     of calling get_doccount() on the Database object
	 *		     (though if you are doing this so you can filter
	 *		     results, you are likely to get much better
	 *		     performance by using Xapian's match-time filtering
	 *		     features instead).  You can pass 0 for maxitems
	 *		     which will give you an empty MSet with valid
	 *		     statistics (such as get_matches_estimated())
	 *		     calculated without looking at any postings, which
	 *		     is very quick, but means the estimates may be
	 *		     more approximate and the bounds may be much
	 *		     looser.
	 *  @param omrset    the relevance set to use when performing the query.
	 *  @param mdecider  a decision functor to use to decide whether a
	 *		     given document should be put in the MSet.
	 *
	 *  @return	     A Xapian::MSet object containing the results of the
	 *		     query.
	 *
	 *  @exception Xapian::InvalidArgumentError  See class documentation.
	 */
	MSet get_mset(Xapian::doccount first, Xapian::doccount maxitems,
		      const RSet * omrset,
		      const MatchDecider * mdecider = 0) const {
	    return get_mset(first, maxitems, 0, omrset, mdecider);
	}

	/** Terms in the query may be returned by get_eset().
	 *
	 *  The original intended use for Enquire::get_eset() is for query
	 *  expansion - suggesting terms to add to the query, generally with
	 *  the aim of improving recall (i.e. finding more of the relevant
	 *  documents), so by default terms already in the query won't be
	 *  returned in the ESet.  For some uses you might want to consider
	 *  all terms, and this flag allows you to specify that.
	 */
	static const int INCLUDE_QUERY_TERMS = 1;

	/** Calculate exact term frequencies in get_eset().
	 *
	 *  By default, when working over multiple databases,
	 *  Enquire::get_eset() uses an approximation to the termfreq to
	 *  improve efficiency.  This should still return good results, but
	 *  if you want to calculate the exact combined termfreq then you
	 *  can use this flag.
	 */
	static const int USE_EXACT_TERMFREQ = 2;

	/** Get the expand set for the given rset.
	 *
	 *  @param maxitems  the maximum number of items to return.
	 *  @param omrset    the relevance set to use when performing
	 *		     the expand operation.
	 *  @param flags     zero or more of these values |-ed together:
	 *		      - Xapian::Enquire::INCLUDE_QUERY_TERMS query
	 *			terms may be returned from expand
	 *		      - Xapian::Enquire::USE_EXACT_TERMFREQ for multi
	 *			dbs, calculate the exact termfreq; otherwise an
	 *			approximation is used which can greatly improve
	 *			efficiency, but still returns good results.
	 *  @param edecider  a decision functor to use to decide whether a
	 *		     given term should be put in the ESet
	 *  @param min_wt    the minimum weight for included terms
	 *
	 *  @return	     An ESet object containing the results of the
	 *		     expand.
	 *
	 *  @exception Xapian::InvalidArgumentError  See class documentation.
	 */
	ESet get_eset(Xapian::termcount maxitems,
		      const RSet & omrset,
		      int flags = 0,
		      const Xapian::ExpandDecider * edecider = 0,
		      double min_wt = 0.0) const;

	/** Get the expand set for the given rset.
	 *
	 *  @param maxitems  the maximum number of items to return.
	 *  @param omrset    the relevance set to use when performing
	 *		     the expand operation.
	 *  @param edecider  a decision functor to use to decide whether a
	 *		     given term should be put in the ESet
	 *
	 *  @return	     An ESet object containing the results of the
	 *		     expand.
	 *
	 *  @exception Xapian::InvalidArgumentError  See class documentation.
	 */
	ESet get_eset(Xapian::termcount maxitems, const RSet & omrset,
			     const Xapian::ExpandDecider * edecider) const {
	    return get_eset(maxitems, omrset, 0, edecider);
	}

	/** Get the expand set for the given rset.
	 *
	 *  @param maxitems  the maximum number of items to return.
	 *  @param rset      the relevance set to use when performing
	 *		     the expand operation.
	 *  @param flags     zero or more of these values |-ed together:
	 *		      - Xapian::Enquire::INCLUDE_QUERY_TERMS query
	 *			terms may be returned from expand
	 *		      - Xapian::Enquire::USE_EXACT_TERMFREQ for multi
	 *			dbs, calculate the exact termfreq; otherwise an
	 *			approximation is used which can greatly improve
	 *			efficiency, but still returns good results.
	 *  @param k	     the parameter k in the query expansion algorithm
	 *		     (default is 1.0)
	 *  @param edecider  a decision functor to use to decide whether a
	 *		     given term should be put in the ESet
	 *
	 *  @param min_wt    the minimum weight for included terms
	 *
	 *  @return	     An ESet object containing the results of the
	 *		     expand.
	 *
	 *  @exception Xapian::InvalidArgumentError  See class documentation.
	 */
	XAPIAN_DEPRECATED(ESet get_eset(Xapian::termcount maxitems,
			  const RSet & rset,
			  int flags,
			  double k,
			  const Xapian::ExpandDecider * edecider = NULL,
			  double min_wt = 0.0) const) {
	    set_expansion_scheme("trad", k);
	    return get_eset(maxitems, rset, flags, edecider, min_wt);
	}

	/** Get terms which match a given document, by document id.
	 *
	 *  This method returns the terms in the current query which match
	 *  the given document.
	 *
	 *  It is possible for the document to have been removed from the
	 *  database between the time it is returned in an MSet, and the
	 *  time that this call is made.  If possible, you should specify
	 *  an MSetIterator instead of a Xapian::docid, since this will enable
	 *  database backends with suitable support to prevent this
	 *  occurring.
	 *
	 *  Note that a query does not need to have been run in order to
	 *  make this call.
	 *
	 *  @param did     The document id for which to retrieve the matching
	 *		   terms.
	 *
	 *  @return	   An iterator returning the terms which match the
	 *		   document.  The terms will be returned (as far as this
	 *		   makes any sense) in the same order as the terms
	 *		   in the query.  Terms will not occur more than once,
	 *		   even if they do in the query.
	 *
	 *  @exception Xapian::InvalidArgumentError  See class documentation.
	 *  @exception Xapian::DocNotFoundError      The document specified
	 *	could not be found in the database.
	 */
	TermIterator get_matching_terms_begin(Xapian::docid did) const;

	/** End iterator corresponding to get_matching_terms_begin() */
	TermIterator XAPIAN_NOTHROW(get_matching_terms_end(Xapian::docid /*did*/) const) {
	    return TermIterator();
	}

	/** Get terms which match a given document, by match set item.
	 *
	 *  This method returns the terms in the current query which match
	 *  the given document.
	 *
	 *  If the underlying database has suitable support, using this call
	 *  (rather than passing a Xapian::docid) will enable the system to
	 *  ensure that the correct data is returned, and that the document
	 *  has not been deleted or changed since the query was performed.
	 *
	 *  @param it   The iterator for which to retrieve the matching terms.
	 *
	 *  @return	An iterator returning the terms which match the
	 *		   document.  The terms will be returned (as far as this
	 *		   makes any sense) in the same order as the terms
	 *		   in the query.  Terms will not occur more than once,
	 *		   even if they do in the query.
	 *
	 *  @exception Xapian::InvalidArgumentError  See class documentation.
	 *  @exception Xapian::DocNotFoundError      The document specified
	 *	could not be found in the database.
	 */
	TermIterator get_matching_terms_begin(const MSetIterator &it) const;

	/** End iterator corresponding to get_matching_terms_begin() */
	TermIterator XAPIAN_NOTHROW(get_matching_terms_end(const MSetIterator &/*it*/) const) {
	    return TermIterator();
	}

	/// Return a string describing this object.
	std::string get_description() const;
};

}

#endif /* XAPIAN_INCLUDED_ENQUIRE_H */
