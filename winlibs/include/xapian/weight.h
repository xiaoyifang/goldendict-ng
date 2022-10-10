/** @file
 * @brief Weighting scheme API.
 */
/* Copyright (C) 2004,2007,2008,2009,2010,2011,2012,2015,2016,2019 Olly Betts
 * Copyright (C) 2009 Lemur Consulting Ltd
 * Copyright (C) 2013,2014 Aarsh Shah
 * Copyright (C) 2016 Vivek Pal
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

#ifndef XAPIAN_INCLUDED_WEIGHT_H
#define XAPIAN_INCLUDED_WEIGHT_H

#include <string>

#include <xapian/types.h>
#include <xapian/visibility.h>

namespace Xapian {

/** Abstract base class for weighting schemes. */
class XAPIAN_VISIBILITY_DEFAULT Weight {
  protected:
    /// Stats which the weighting scheme can use (see @a need_stat()).
    typedef enum {
	/// Number of documents in the collection.
	COLLECTION_SIZE = 1,
	/// Number of documents in the RSet.
	RSET_SIZE = 2,
	/// Average length of documents in the collection.
	AVERAGE_LENGTH = 4,
	/// How many documents the current term is in.
	TERMFREQ = 8,
	/// How many documents in the RSet the current term is in.
	RELTERMFREQ = 16,
	/// Sum of wqf for terms in the query.
	QUERY_LENGTH = 32,
	/// Within-query-frequency of the current term.
	WQF = 64,
	/// Within-document-frequency of the current term in the current document.
	WDF = 128,
	/// Length of the current document (sum wdf).
	DOC_LENGTH = 256,
	/// Lower bound on (non-zero) document lengths.
	DOC_LENGTH_MIN = 512,
	/// Upper bound on document lengths.
	DOC_LENGTH_MAX = 1024,
	/// Upper bound on wdf.
	WDF_MAX = 2048,
	/// Sum of wdf over the whole collection for the current term.
	COLLECTION_FREQ = 4096,
	/// Number of unique terms in the current document.
	UNIQUE_TERMS = 8192,
	/** Sum of lengths of all documents in the collection.
	 *
	 *  This gives the total number of term occurrences.
	 */
	TOTAL_LENGTH = COLLECTION_SIZE | AVERAGE_LENGTH
    } stat_flags;

    /** Tell Xapian that your subclass will want a particular statistic.
     *
     *  Some of the statistics can be costly to fetch or calculate, so
     *  Xapian needs to know which are actually going to be used.  You
     *  should call need_stat() from your constructor for each such
     *  statistic.
     *
     * @param flag  The stat_flags value for a required statistic.
     */
    void need_stat(stat_flags flag) {
	stats_needed = stat_flags(stats_needed | flag);
    }

    /** Allow the subclass to perform any initialisation it needs to.
     *
     *  @param factor	  Any scaling factor (e.g. from OP_SCALE_WEIGHT).
     *			  If the Weight object is for the term-independent
     *			  weight supplied by get_sumextra()/get_maxextra(),
     *			  then init(0.0) is called (starting from Xapian
     *			  1.2.11 and 1.3.1 - earlier versions failed to
     *			  call init() for such Weight objects).
     */
    virtual void init(double factor) = 0;

  private:
    /// Don't allow assignment.
    void operator=(const Weight &);

    /// A bitmask of the statistics this weighting scheme needs.
    stat_flags stats_needed;

    /// The number of documents in the collection.
    Xapian::doccount collection_size_;

    /// The number of documents marked as relevant.
    Xapian::doccount rset_size_;

    /// The average length of a document in the collection.
    Xapian::doclength average_length_;

    /// The number of documents which this term indexes.
    Xapian::doccount termfreq_;

    // The collection frequency of the term.
    Xapian::termcount collectionfreq_;

    /// The number of relevant documents which this term indexes.
    Xapian::doccount reltermfreq_;

    /// The length of the query.
    Xapian::termcount query_length_;

    /// The within-query-frequency of this term.
    Xapian::termcount wqf_;

    /// A lower bound on the minimum length of any document in the database.
    Xapian::termcount doclength_lower_bound_;

    /// An upper bound on the maximum length of any document in the database.
    Xapian::termcount doclength_upper_bound_;

    /// An upper bound on the wdf of this term.
    Xapian::termcount wdf_upper_bound_;

  public:

    /// Default constructor, needed by subclass constructors.
    Weight() : stats_needed() { }

    /** Type of smoothing to use with the Language Model Weighting scheme.
     *
     *  Default is TWO_STAGE_SMOOTHING.
     */
    typedef enum {
	TWO_STAGE_SMOOTHING = 1,
	DIRICHLET_SMOOTHING = 2,
	ABSOLUTE_DISCOUNT_SMOOTHING = 3,
	JELINEK_MERCER_SMOOTHING = 4,
	DIRICHLET_PLUS_SMOOTHING = 5
    } type_smoothing;

    class Internal;

    /** Virtual destructor, because we have virtual methods. */
    virtual ~Weight();

    /** Clone this object.
     *
     *  This method allocates and returns a copy of the object it is called on.
     *
     *  If your subclass is called FooWeight and has parameters a and b, then
     *  you would implement FooWeight::clone() like so:
     *
     *  FooWeight * FooWeight::clone() const { return new FooWeight(a, b); }
     *
     *  Note that the returned object will be deallocated by Xapian after use
     *  with "delete".  If you want to handle the deletion in a special way
     *  (for example when wrapping the Xapian API for use from another
     *  language) then you can define a static <code>operator delete</code>
     *  method in your subclass as shown here:
     *  https://trac.xapian.org/ticket/554#comment:1
     */
    virtual Weight * clone() const = 0;

    /** Return the name of this weighting scheme.
     *
     *  This name is used by the remote backend.  It is passed along with the
     *  serialised parameters to the remote server so that it knows which class
     *  to create.
     *
     *  Return the full namespace-qualified name of your class here - if
     *  your class is called FooWeight, return "FooWeight" from this method
     *  (Xapian::BM25Weight returns "Xapian::BM25Weight" here).
     *
     *  If you don't want to support the remote backend, you can use the
     *  default implementation which simply returns an empty string.
     */
    virtual std::string name() const;

    /** Return this object's parameters serialised as a single string.
     *
     *  If you don't want to support the remote backend, you can use the
     *  default implementation which simply throws Xapian::UnimplementedError.
     */
    virtual std::string serialise() const;

    /** Unserialise parameters.
     *
     *  This method unserialises parameters serialised by the @a serialise()
     *  method and allocates and returns a new object initialised with them.
     *
     *  If you don't want to support the remote backend, you can use the
     *  default implementation which simply throws Xapian::UnimplementedError.
     *
     *  Note that the returned object will be deallocated by Xapian after use
     *  with "delete".  If you want to handle the deletion in a special way
     *  (for example when wrapping the Xapian API for use from another
     *  language) then you can define a static <code>operator delete</code>
     *  method in your subclass as shown here:
     *  https://trac.xapian.org/ticket/554#comment:1
     *
     *  @param serialised	A string containing the serialised parameters.
     */
    virtual Weight * unserialise(const std::string & serialised) const;

    /** Calculate the weight contribution for this object's term to a document.
     *
     *  The parameters give information about the document which may be used
     *  in the calculations:
     *
     *  @param wdf    The within document frequency of the term in the document.
     *  @param doclen The document's length (unnormalised).
     *  @param uniqterms	Number of unique terms in the document (used
     *				for absolute smoothing).
     */
    virtual double get_sumpart(Xapian::termcount wdf,
			       Xapian::termcount doclen,
			       Xapian::termcount uniqterms) const = 0;

    /** Return an upper bound on what get_sumpart() can return for any document.
     *
     *  This information is used by the matcher to perform various
     *  optimisations, so strive to make the bound as tight as possible.
     */
    virtual double get_maxpart() const = 0;

    /** Calculate the term-independent weight component for a document.
     *
     *  The parameter gives information about the document which may be used
     *  in the calculations:
     *
     *  @param doclen The document's length (unnormalised).
     *  @param uniqterms The number of unique terms in the document.
     */
    virtual double get_sumextra(Xapian::termcount doclen,
				Xapian::termcount uniqterms) const = 0;

    /** Return an upper bound on what get_sumextra() can return for any
     *  document.
     *
     *  This information is used by the matcher to perform various
     *  optimisations, so strive to make the bound as tight as possible.
     */
    virtual double get_maxextra() const = 0;

    /** @private @internal Initialise this object to calculate weights for term
     *  @a term.
     *
     *  Old version of method, as used by 1.4.18 and earlier.  This
     *  should only be referenced from inside the library and 1.4.19 and
     *  later will call the new version instead.  We continue to provide it
     *  mainly to avoid triggering ABI checking tools.
     *
     *  @param stats	  Source of statistics.
     *  @param query_len_ Query length.
     *  @param term	  The term for the new object.
     *  @param wqf_	  The within-query-frequency of @a term.
     *  @param factor	  Any scaling factor (e.g. from OP_SCALE_WEIGHT).
     */
    void init_(const Internal & stats, Xapian::termcount query_len_,
	       const std::string & term, Xapian::termcount wqf_,
	       double factor);

    /** @private @internal Initialise this object to calculate weights for term
     *  @a term.
     *
     *  @param stats	  Source of statistics.
     *  @param query_len_ Query length.
     *  @param term	  The term for the new object.
     *  @param wqf_	  The within-query-frequency of @a term.
     *  @param factor	  Any scaling factor (e.g. from OP_SCALE_WEIGHT).
     *  @param postlist   Pointer to a LeafPostList for the term (cast to void*
     *			  to avoid needing to forward declare class
     *			  LeafPostList in public API headers) which can be used
     *			  to get wdf upper bound
     */
    void init_(const Internal & stats, Xapian::termcount query_len_,
	       const std::string & term, Xapian::termcount wqf_,
	       double factor, void* postlist);

    /** @private @internal Initialise this object to calculate weights for a
     *  synonym.
     *
     *  @param stats	   Source of statistics.
     *  @param query_len_  Query length.
     *  @param factor	   Any scaling factor (e.g. from OP_SCALE_WEIGHT).
     *  @param termfreq    The termfreq to use.
     *  @param reltermfreq The reltermfreq to use.
     *  @param collection_freq The collection frequency to use.
     */
    void init_(const Internal & stats, Xapian::termcount query_len_,
	       double factor, Xapian::doccount termfreq,
	       Xapian::doccount reltermfreq, Xapian::termcount collection_freq);

    /** @private @internal Initialise this object to calculate the extra weight
     *  component.
     *
     *  @param stats	  Source of statistics.
     *  @param query_len_ Query length.
     */
    void init_(const Internal & stats, Xapian::termcount query_len_);

    /** @private @internal Return true if the document length is needed.
     *
     *  If this method returns true, then the document length will be fetched
     *  and passed to @a get_sumpart().  Otherwise 0 may be passed for the
     *  document length.
     */
    bool get_sumpart_needs_doclength_() const {
	return stats_needed & DOC_LENGTH;
    }

    /** @private @internal Return true if the WDF is needed.
     *
     *  If this method returns true, then the WDF will be fetched and passed to
     *  @a get_sumpart().  Otherwise 0 may be passed for the wdf.
     */
    bool get_sumpart_needs_wdf_() const {
	return stats_needed & WDF;
    }

    /** @private @internal Return true if the number of unique terms is needed.
     *
     *  If this method returns true, then the number of unique terms will be
     *  fetched and passed to @a get_sumpart().  Otherwise 0 may be passed for
     *  the number of unique terms.
     */
    bool get_sumpart_needs_uniqueterms_() const {
	return stats_needed & UNIQUE_TERMS;
    }

    /// @private @internal Test if this is a BoolWeight object.
    bool is_bool_weight_() const {
	// Checking the name isn't ideal, but (get_maxpart() == 0.0) isn't
	// required to work without init() having been called.  We can at
	// least avoid the virtual method call in most non-BoolWeight cases
	// as most other classes will need at least some stats.
	return stats_needed == 0 && name() == "Xapian::BoolWeight";
    }

  protected:
    /** Don't allow copying.
     *
     *  This would ideally be private, but that causes a compilation error
     *  with GCC 4.1 (which appears to be a bug).
     */
    Weight(const Weight &);

    /// The number of documents in the collection.
    Xapian::doccount get_collection_size() const { return collection_size_; }

    /// The number of documents marked as relevant.
    Xapian::doccount get_rset_size() const { return rset_size_; }

    /// The average length of a document in the collection.
    Xapian::doclength get_average_length() const { return average_length_; }

    /// The number of documents which this term indexes.
    Xapian::doccount get_termfreq() const { return termfreq_; }

    /// The number of relevant documents which this term indexes.
    Xapian::doccount get_reltermfreq() const { return reltermfreq_; }

    /// The collection frequency of the term.
    Xapian::termcount get_collection_freq() const { return collectionfreq_; }

    /// The length of the query.
    Xapian::termcount get_query_length() const { return query_length_; }

    /// The within-query-frequency of this term.
    Xapian::termcount get_wqf() const { return wqf_; }

    /** An upper bound on the maximum length of any document in the database.
     *
     *  This should only be used by get_maxpart() and get_maxextra().
     */
    Xapian::termcount get_doclength_upper_bound() const {
	return doclength_upper_bound_;
    }

    /** A lower bound on the minimum length of any document in the database.
     *
     *  This bound does not include any zero-length documents.
     *
     *  This should only be used by get_maxpart() and get_maxextra().
     */
    Xapian::termcount get_doclength_lower_bound() const {
	return doclength_lower_bound_;
    }

    /** An upper bound on the wdf of this term.
     *
     *  This should only be used by get_maxpart() and get_maxextra().
     */
    Xapian::termcount get_wdf_upper_bound() const {
	return wdf_upper_bound_;
    }

    /// Total length of all documents in the collection.
    Xapian::totallength get_total_length() const {
	return Xapian::totallength(average_length_ * collection_size_ + 0.5);
    }
};

/** Class implementing a "boolean" weighting scheme.
 *
 *  This weighting scheme gives all documents zero weight.
 */
class XAPIAN_VISIBILITY_DEFAULT BoolWeight : public Weight {
    BoolWeight * clone() const;

    void init(double factor);

  public:
    /** Construct a BoolWeight. */
    BoolWeight() { }

    std::string name() const;

    std::string serialise() const;
    BoolWeight * unserialise(const std::string & serialised) const;

    double get_sumpart(Xapian::termcount wdf,
		       Xapian::termcount doclen,
		       Xapian::termcount uniqterms) const;
    double get_maxpart() const;

    double get_sumextra(Xapian::termcount doclen,
			Xapian::termcount uniqterms) const;
    double get_maxextra() const;
};

/// Xapian::Weight subclass implementing the tf-idf weighting scheme.
class XAPIAN_VISIBILITY_DEFAULT TfIdfWeight : public Weight {
    /* Three character string indicating the normalizations for tf(wdf), idf and
       tfidf weight. */
    std::string normalizations;

    /// The factor to multiply with the weight.
    double factor;

    TfIdfWeight * clone() const;

    void init(double factor);

    /* When additional normalizations are implemented in the future, the additional statistics for them
       should be accessed by these functions. */
    double get_wdfn(Xapian::termcount wdf, char c) const;
    double get_idfn(Xapian::doccount termfreq, char c) const;
    double get_wtn(double wt, char c) const;

  public:
    /** Construct a TfIdfWeight
     *
     *  @param normalizations	A three character string indicating the
     *				normalizations to be used for the tf(wdf), idf
     *				and document weight.  (default: "ntn")
     *
     * The @a normalizations string works like so:
     *
     * @li The first character specifies the normalization for the wdf.  The
     *     following normalizations are currently supported:
     *
     *     @li 'n': None.      wdfn=wdf
     *     @li 'b': Boolean    wdfn=1 if term in document else wdfn=0
     *     @li 's': Square     wdfn=wdf*wdf
     *     @li 'l': Logarithmic wdfn=1+log<sub>e</sub>(wdf)
     *     @li 'L': Log average wdfn=(1+log(wdf))/(1+log(doclen/unique_terms))
     *
     *     The Max-wdf and Augmented Max wdf normalizations haven't yet been
     *     implemented.
     *
     * @li The second character indicates the normalization for the idf.  The
     *     following normalizations are currently supported:
     *
     *     @li 'n': None    idfn=1
     *     @li 't': TfIdf   idfn=log(N/Termfreq) where N is the number of
     *         documents in collection and Termfreq is the number of documents
     *         which are indexed by the term t.
     *     @li 'p': Prob    idfn=log((N-Termfreq)/Termfreq)
     *     @li 'f': Freq    idfn=1/Termfreq
     *     @li 's': Squared idfn=(log(N/Termfreq))²
     *
     * @li The third and the final character indicates the normalization for
     *     the document weight.  The following normalizations are currently
     *     supported:
     *
     *     @li 'n': None wtn=tfn*idfn
     *
     * Implementing support for more normalizations of each type would require
     * extending the backend to track more statistics.
     */
    explicit TfIdfWeight(const std::string &normalizations);

    /** Construct a TfIdfWeight using the default normalizations ("ntn"). */
    TfIdfWeight()
    : normalizations("ntn")
    {
	need_stat(TERMFREQ);
	need_stat(WDF);
	need_stat(WDF_MAX);
	need_stat(COLLECTION_SIZE);
    }

    std::string name() const;

    std::string serialise() const;
    TfIdfWeight * unserialise(const std::string & serialised) const;

    double get_sumpart(Xapian::termcount wdf,
		       Xapian::termcount doclen,
		       Xapian::termcount uniqterm) const;
    double get_maxpart() const;

    double get_sumextra(Xapian::termcount doclen,
			Xapian::termcount uniqterms) const;
    double get_maxextra() const;
};


/// Xapian::Weight subclass implementing the BM25 probabilistic formula.
class XAPIAN_VISIBILITY_DEFAULT BM25Weight : public Weight {
    /// Factor to multiply the document length by.
    mutable Xapian::doclength len_factor;

    /// Factor combining all the document independent factors.
    mutable double termweight;

    /// The BM25 parameters.
    double param_k1, param_k2, param_k3, param_b;

    /// The minimum normalised document length value.
    Xapian::doclength param_min_normlen;

    BM25Weight * clone() const;

    void init(double factor);

  public:
    /** Construct a BM25Weight.
     *
     *  @param k1  A non-negative parameter controlling how influential
     *		   within-document-frequency (wdf) is.  k1=0 means that
     *		   wdf doesn't affect the weights.  The larger k1 is, the more
     *		   wdf influences the weights.  (default 1)
     *
     *  @param k2  A non-negative parameter which controls the strength of a
     *		   correction factor which depends upon query length and
     *		   normalised document length.  k2=0 disable this factor; larger
     *		   k2 makes it stronger.  (default 0)
     *
     *  @param k3  A non-negative parameter controlling how influential
     *		   within-query-frequency (wqf) is.  k3=0 means that wqf
     *		   doesn't affect the weights.  The larger k3 is, the more
     *		   wqf influences the weights.  (default 1)
     *
     *  @param b   A parameter between 0 and 1, controlling how strong the
     *		   document length normalisation of wdf is.  0 means no
     *		   normalisation; 1 means full normalisation.  (default 0.5)
     *
     *  @param min_normlen  A parameter specifying a minimum value for
     *		   normalised document length.  Normalised document length
     *		   values less than this will be clamped to this value, helping
     *		   to prevent very short documents getting large weights.
     *		   (default 0.5)
     */
    BM25Weight(double k1, double k2, double k3, double b, double min_normlen)
	: param_k1(k1), param_k2(k2), param_k3(k3), param_b(b),
	  param_min_normlen(min_normlen)
    {
	if (param_k1 < 0) param_k1 = 0;
	if (param_k2 < 0) param_k2 = 0;
	if (param_k3 < 0) param_k3 = 0;
	if (param_b < 0) {
	    param_b = 0;
	} else if (param_b > 1) {
	    param_b = 1;
	}
	need_stat(COLLECTION_SIZE);
	need_stat(RSET_SIZE);
	need_stat(TERMFREQ);
	need_stat(RELTERMFREQ);
	need_stat(WDF);
	need_stat(WDF_MAX);
	if (param_k2 != 0 || (param_k1 != 0 && param_b != 0)) {
	    need_stat(DOC_LENGTH_MIN);
	    need_stat(AVERAGE_LENGTH);
	}
	if (param_k1 != 0 && param_b != 0) need_stat(DOC_LENGTH);
	if (param_k2 != 0) need_stat(QUERY_LENGTH);
	if (param_k3 != 0) need_stat(WQF);
    }

    BM25Weight()
	: param_k1(1), param_k2(0), param_k3(1), param_b(0.5),
	  param_min_normlen(0.5)
    {
	need_stat(COLLECTION_SIZE);
	need_stat(RSET_SIZE);
	need_stat(TERMFREQ);
	need_stat(RELTERMFREQ);
	need_stat(WDF);
	need_stat(WDF_MAX);
	need_stat(DOC_LENGTH_MIN);
	need_stat(AVERAGE_LENGTH);
	need_stat(DOC_LENGTH);
	need_stat(WQF);
    }

    std::string name() const;

    std::string serialise() const;
    BM25Weight * unserialise(const std::string & serialised) const;

    double get_sumpart(Xapian::termcount wdf,
		       Xapian::termcount doclen,
		       Xapian::termcount uniqterm) const;
    double get_maxpart() const;

    double get_sumextra(Xapian::termcount doclen,
			Xapian::termcount uniqterms) const;
    double get_maxextra() const;
};

/// Xapian::Weight subclass implementing the BM25+ probabilistic formula.
class XAPIAN_VISIBILITY_DEFAULT BM25PlusWeight : public Weight {
    /// Factor to multiply the document length by.
    mutable Xapian::doclength len_factor;

    /// Factor combining all the document independent factors.
    mutable double termweight;

    /// The BM25+ parameters.
    double param_k1, param_k2, param_k3, param_b;

    /// The minimum normalised document length value.
    Xapian::doclength param_min_normlen;

    /// Additional parameter delta in the BM25+ formula.
    double param_delta;

    BM25PlusWeight * clone() const;

    void init(double factor);

  public:
    /** Construct a BM25PlusWeight.
     *
     *  @param k1  A non-negative parameter controlling how influential
     *		   within-document-frequency (wdf) is.  k1=0 means that
     *		   wdf doesn't affect the weights.  The larger k1 is, the more
     *		   wdf influences the weights.  (default 1)
     *
     *  @param k2  A non-negative parameter which controls the strength of a
     *		   correction factor which depends upon query length and
     *		   normalised document length.  k2=0 disable this factor; larger
     *		   k2 makes it stronger.  The paper which describes BM25+
     *		   ignores BM25's document-independent component (so implicitly
     *		   k2=0), but we support non-zero k2 too.  (default 0)
     *
     *  @param k3  A non-negative parameter controlling how influential
     *		   within-query-frequency (wqf) is.  k3=0 means that wqf
     *		   doesn't affect the weights.  The larger k3 is, the more
     *		   wqf influences the weights.  (default 1)
     *
     *  @param b   A parameter between 0 and 1, controlling how strong the
     *		   document length normalisation of wdf is.  0 means no
     *		   normalisation; 1 means full normalisation.  (default 0.5)
     *
     *  @param min_normlen  A parameter specifying a minimum value for
     *		   normalised document length.  Normalised document length
     *		   values less than this will be clamped to this value, helping
     *		   to prevent very short documents getting large weights.
     *		   (default 0.5)
     *
     *  @param delta  A parameter for pseudo tf value to control the scale
     *		      of the tf lower bound. Delta(δ) can be tuned for example
     *		      from 0.0 to 1.5 but BM25+ can still work effectively
     *		      across collections with a fixed δ = 1.0. (default 1.0)
     */
    BM25PlusWeight(double k1, double k2, double k3, double b,
		   double min_normlen, double delta)
	: param_k1(k1), param_k2(k2), param_k3(k3), param_b(b),
	  param_min_normlen(min_normlen), param_delta(delta)
    {
	if (param_k1 < 0) param_k1 = 0;
	if (param_k2 < 0) param_k2 = 0;
	if (param_k3 < 0) param_k3 = 0;
	if (param_delta < 0) param_delta = 0;
	if (param_b < 0) {
	    param_b = 0;
	} else if (param_b > 1) {
	    param_b = 1;
	}
	need_stat(COLLECTION_SIZE);
	need_stat(RSET_SIZE);
	need_stat(TERMFREQ);
	need_stat(RELTERMFREQ);
	need_stat(WDF);
	need_stat(WDF_MAX);
	if (param_k2 != 0 || (param_k1 != 0 && param_b != 0)) {
	    need_stat(DOC_LENGTH_MIN);
	    need_stat(AVERAGE_LENGTH);
	}
	if (param_k1 != 0 && param_b != 0) need_stat(DOC_LENGTH);
	if (param_k2 != 0) need_stat(QUERY_LENGTH);
	if (param_k3 != 0) need_stat(WQF);
	if (param_delta != 0) {
	    need_stat(AVERAGE_LENGTH);
	    need_stat(DOC_LENGTH);
	    need_stat(WQF);
	}
    }

    BM25PlusWeight()
	: param_k1(1), param_k2(0), param_k3(1), param_b(0.5),
	  param_min_normlen(0.5), param_delta(1)
    {
	need_stat(COLLECTION_SIZE);
	need_stat(RSET_SIZE);
	need_stat(TERMFREQ);
	need_stat(RELTERMFREQ);
	need_stat(WDF);
	need_stat(WDF_MAX);
	need_stat(DOC_LENGTH_MIN);
	need_stat(AVERAGE_LENGTH);
	need_stat(DOC_LENGTH);
	need_stat(WQF);
    }

    std::string name() const;

    std::string serialise() const;
    BM25PlusWeight * unserialise(const std::string & serialised) const;

    double get_sumpart(Xapian::termcount wdf,
		       Xapian::termcount doclen,
		       Xapian::termcount uniqterm) const;
    double get_maxpart() const;

    double get_sumextra(Xapian::termcount doclen,
			Xapian::termcount uniqterms) const;
    double get_maxextra() const;
};

/** Xapian::Weight subclass implementing the traditional probabilistic formula.
 *
 * This class implements the "traditional" Probabilistic Weighting scheme, as
 * described by the early papers on Probabilistic Retrieval.  BM25 generally
 * gives better results.
 *
 * TradWeight(k) is equivalent to BM25Weight(k, 0, 0, 1, 0), except that
 * the latter returns weights (k+1) times larger.
 */
class XAPIAN_VISIBILITY_DEFAULT TradWeight : public Weight {
    /// Factor to multiply the document length by.
    mutable Xapian::doclength len_factor;

    /// Factor combining all the document independent factors.
    mutable double termweight;

    /// The parameter in the formula.
    double param_k;

    TradWeight * clone() const;

    void init(double factor);

  public:
    /** Construct a TradWeight.
     *
     *  @param k  A non-negative parameter controlling how influential
     *		  within-document-frequency (wdf) and document length are.
     *		  k=0 means that wdf and document length don't affect the
     *		  weights.  The larger k is, the more they do.  (default 1)
     */
    explicit TradWeight(double k = 1.0) : param_k(k) {
	if (param_k < 0) param_k = 0;
	if (param_k != 0.0) {
	    need_stat(AVERAGE_LENGTH);
	    need_stat(DOC_LENGTH);
	}
	need_stat(COLLECTION_SIZE);
	need_stat(RSET_SIZE);
	need_stat(TERMFREQ);
	need_stat(RELTERMFREQ);
	need_stat(DOC_LENGTH_MIN);
	need_stat(WDF);
	need_stat(WDF_MAX);
    }

    std::string name() const;

    std::string serialise() const;
    TradWeight * unserialise(const std::string & serialised) const;

    double get_sumpart(Xapian::termcount wdf,
		       Xapian::termcount doclen,
		       Xapian::termcount uniqueterms) const;
    double get_maxpart() const;

    double get_sumextra(Xapian::termcount doclen,
			Xapian::termcount uniqterms) const;
    double get_maxextra() const;
};

/** This class implements the InL2 weighting scheme.
 *
 *  InL2 is a representative scheme of the Divergence from Randomness Framework
 *  by Gianni Amati.
 *
 *  This weighting scheme is useful for tasks that require early precision.
 *
 *  It uses the Inverse document frequency model (In), the Laplace method to
 *  find the aftereffect of sampling (L) and the second wdf normalization
 *  proposed by Amati to normalize the wdf in the document to the length of the
 *  document (H2).
 *
 *  For more information about the DFR Framework and the InL2 scheme, please
 *  refer to: Gianni Amati and Cornelis Joost Van Rijsbergen Probabilistic
 *  models of information retrieval based on measuring the divergence from
 *  randomness ACM Transactions on Information Systems (TOIS) 20, (4), 2002,
 *  pp. 357-389.
 */
class XAPIAN_VISIBILITY_DEFAULT InL2Weight : public Weight {
    /// The wdf normalization parameter in the formula.
    double param_c;

    /// The upper bound on the weight a term can give to a document.
    double upper_bound;

    /// The constant values which are used on every call to get_sumpart().
    double wqf_product_idf;
    double c_product_avlen;

    InL2Weight * clone() const;

    void init(double factor);

  public:
    /** Construct an InL2Weight.
     *
     *  @param c  A strictly positive parameter controlling the extent
     *		  of the normalization of the wdf to the document length. The
     *		  default value of 1 is suitable for longer queries but it may
     *		  need to be changed for shorter queries. For more information,
     *		  please refer to Gianni Amati's PHD thesis.
     */
    explicit InL2Weight(double c);

    InL2Weight()
    : param_c(1.0)
    {
	need_stat(AVERAGE_LENGTH);
	need_stat(DOC_LENGTH);
	need_stat(DOC_LENGTH_MIN);
	need_stat(DOC_LENGTH_MAX);
	need_stat(COLLECTION_SIZE);
	need_stat(WDF);
	need_stat(WDF_MAX);
	need_stat(WQF);
	need_stat(TERMFREQ);
    }

    std::string name() const;

    std::string serialise() const;
    InL2Weight * unserialise(const std::string & serialised) const;

    double get_sumpart(Xapian::termcount wdf,
		       Xapian::termcount doclen,
		       Xapian::termcount uniqterms) const;
    double get_maxpart() const;

    double get_sumextra(Xapian::termcount doclen,
			Xapian::termcount uniqterms) const;
    double get_maxextra() const;
};

/** This class implements the IfB2 weighting scheme.
 *
 *  IfB2 is a representative scheme of the Divergence from Randomness Framework
 *  by Gianni Amati.
 *
 *  It uses the Inverse term frequency model (If), the Bernoulli method to find
 *  the aftereffect of sampling (B) and the second wdf normalization proposed
 *  by Amati to normalize the wdf in the document to the length of the document
 *  (H2).
 *
 *  For more information about the DFR Framework and the IfB2 scheme, please
 *  refer to: Gianni Amati and Cornelis Joost Van Rijsbergen Probabilistic
 *  models of information retrieval based on measuring the divergence from
 *  randomness ACM Transactions on Information Systems (TOIS) 20, (4), 2002,
 *  pp. 357-389.
 */
class XAPIAN_VISIBILITY_DEFAULT IfB2Weight : public Weight {
    /// The wdf normalization parameter in the formula.
    double param_c;

    /// The upper bound on the weight.
    double upper_bound;

    /// The constant values which are used for calculations in get_sumpart().
    double wqf_product_idf;
    double c_product_avlen;
    double B_constant;

    IfB2Weight * clone() const;

    void init(double factor);

  public:
    /** Construct an IfB2Weight.
     *
     *  @param c  A strictly positive parameter controlling the extent
     *		  of the normalization of the wdf to the document length. The
     *		  default value of 1 is suitable for longer queries but it may
     *		  need to be changed for shorter queries. For more information,
     *		  please refer to Gianni Amati's PHD thesis titled
     *		  Probabilistic Models for Information Retrieval based on
     *		  Divergence from Randomness.
     */
    explicit IfB2Weight(double c);

    IfB2Weight() : param_c(1.0) {
	need_stat(AVERAGE_LENGTH);
	need_stat(DOC_LENGTH);
	need_stat(DOC_LENGTH_MIN);
	need_stat(DOC_LENGTH_MAX);
	need_stat(COLLECTION_SIZE);
	need_stat(COLLECTION_FREQ);
	need_stat(WDF);
	need_stat(WDF_MAX);
	need_stat(WQF);
	need_stat(TERMFREQ);
    }

    std::string name() const;

    std::string serialise() const;
    IfB2Weight * unserialise(const std::string & serialised) const;

    double get_sumpart(Xapian::termcount wdf,
		       Xapian::termcount doclen,
		       Xapian::termcount uniqterm) const;
    double get_maxpart() const;

    double get_sumextra(Xapian::termcount doclen,
			Xapian::termcount uniqterms) const;
    double get_maxextra() const;
};

/** This class implements the IneB2 weighting scheme.
 *
 *  IneB2 is a representative scheme of the Divergence from Randomness
 *  Framework by Gianni Amati.
 *
 *  It uses the Inverse expected document frequency model (Ine), the Bernoulli
 *  method to find the aftereffect of sampling (B) and the second wdf
 *  normalization proposed by Amati to normalize the wdf in the document to the
 *  length of the document (H2).
 *
 *  For more information about the DFR Framework and the IneB2 scheme, please
 *  refer to: Gianni Amati and Cornelis Joost Van Rijsbergen Probabilistic
 *  models of information retrieval based on measuring the divergence from
 *  randomness ACM Transactions on Information Systems (TOIS) 20, (4), 2002,
 *  pp. 357-389.
 */
class XAPIAN_VISIBILITY_DEFAULT IneB2Weight : public Weight {
    /// The wdf normalization parameter in the formula.
    double param_c;

    /// The upper bound of the weight.
    double upper_bound;

    /// Constant values used in get_sumpart().
    double wqf_product_idf;
    double c_product_avlen;
    double B_constant;

    IneB2Weight * clone() const;

    void init(double factor);

  public:
    /** Construct an IneB2Weight.
     *
     *  @param c  A strictly positive parameter controlling the extent
     *		  of the normalization of the wdf to the document length. The
     *		  default value of 1 is suitable for longer queries but it may
     *		  need to be changed for shorter queries. For more information,
     *		  please refer to Gianni Amati's PHD thesis.
     */
    explicit IneB2Weight(double c);

    IneB2Weight() : param_c(1.0) {
	need_stat(AVERAGE_LENGTH);
	need_stat(DOC_LENGTH);
	need_stat(DOC_LENGTH_MIN);
	need_stat(DOC_LENGTH_MAX);
	need_stat(COLLECTION_SIZE);
	need_stat(WDF);
	need_stat(WDF_MAX);
	need_stat(WQF);
	need_stat(COLLECTION_FREQ);
	need_stat(TERMFREQ);
    }

    std::string name() const;

    std::string serialise() const;
    IneB2Weight * unserialise(const std::string & serialised) const;

    double get_sumpart(Xapian::termcount wdf,
		       Xapian::termcount doclen,
		       Xapian::termcount uniqterms) const;
    double get_maxpart() const;

    double get_sumextra(Xapian::termcount doclen,
			Xapian::termcount uniqterms) const;
    double get_maxextra() const;
};

/** This class implements the BB2 weighting scheme.
 *
 *  BB2 is a representative scheme of the Divergence from Randomness Framework
 *  by Gianni Amati.
 *
 *  It uses the Bose-Einstein probabilistic distribution (B) along with
 *  Stirling's power approximation, the Bernoulli method to find the
 *  aftereffect of sampling (B) and the second wdf normalization proposed by
 *  Amati to normalize the wdf in the document to the length of the document
 *  (H2).
 *
 *  For more information about the DFR Framework and the BB2 scheme, please
 *  refer to : Gianni Amati and Cornelis Joost Van Rijsbergen Probabilistic
 *  models of information retrieval based on measuring the divergence from
 *  randomness ACM Transactions on Information Systems (TOIS) 20, (4), 2002,
 *  pp. 357-389.
 */
class XAPIAN_VISIBILITY_DEFAULT BB2Weight : public Weight {
    /// The wdf normalization parameter in the formula.
    double param_c;

    /// The upper bound on the weight.
    double upper_bound;

    /// The constant values to be used in get_sumpart().
    double c_product_avlen;
    double B_constant;
    double wt;
    double stirling_constant_1;
    double stirling_constant_2;

    BB2Weight * clone() const;

    void init(double factor);

  public:
    /** Construct a BB2Weight.
     *
     *  @param c  A strictly positive parameter controlling the extent
     *		  of the normalization of the wdf to the document length. A
     *		  default value of 1 is suitable for longer queries but it may
     *		  need to be changed for shorter queries. For more information,
     *		  please refer to Gianni Amati's PHD thesis titled
     *		  Probabilistic Models for Information Retrieval based on
     *		  Divergence from Randomness.
     */
    explicit BB2Weight(double c);

    BB2Weight() : param_c(1.0) {
	need_stat(AVERAGE_LENGTH);
	need_stat(DOC_LENGTH);
	need_stat(DOC_LENGTH_MIN);
	need_stat(DOC_LENGTH_MAX);
	need_stat(COLLECTION_SIZE);
	need_stat(COLLECTION_FREQ);
	need_stat(WDF);
	need_stat(WDF_MAX);
	need_stat(WQF);
	need_stat(TERMFREQ);
    }

    std::string name() const;

    std::string serialise() const;
    BB2Weight * unserialise(const std::string & serialised) const;

    double get_sumpart(Xapian::termcount wdf,
		       Xapian::termcount doclen,
		       Xapian::termcount uniqterms) const;
    double get_maxpart() const;

    double get_sumextra(Xapian::termcount doclen,
			Xapian::termcount uniqterms) const;
    double get_maxextra() const;
};

/** This class implements the DLH weighting scheme, which is a representative
 *  scheme of the Divergence from Randomness Framework by Gianni Amati.
 *
 *  This is a parameter free weighting scheme and it should be used with query
 *  expansion to obtain better results. It uses the HyperGeometric Probabilistic
 *  model and Laplace's normalization to calculate the risk gain.
 *
 *  For more information about the DFR Framework and the DLH scheme, please
 *  refer to :
 *  a.) Gianni Amati and Cornelis Joost Van Rijsbergen Probabilistic
 *  models of information retrieval based on measuring the divergence from
 *  randomness ACM Transactions on Information Systems (TOIS) 20, (4), 2002, pp.
 *  357-389.
 *  b.) FUB, IASI-CNR and University of Tor Vergata at TREC 2007 Blog Track.
 *  G. Amati and E. Ambrosi and M. Bianchi and C. Gaibisso and G. Gambosi.
 *  Proceedings of the 16th Text REtrieval Conference (TREC-2007), 2008.
 */
class XAPIAN_VISIBILITY_DEFAULT DLHWeight : public Weight {
    /// Now unused but left in place in 1.4.x for ABI compatibility.
    double lower_bound;

    /// The upper bound on the weight.
    double upper_bound;

    /// The constant value to be used in get_sumpart().
    double log_constant;
    double wqf_product_factor;

    DLHWeight * clone() const;

    void init(double factor);

  public:
    DLHWeight() {
	need_stat(DOC_LENGTH);
	need_stat(COLLECTION_FREQ);
	need_stat(WDF);
	need_stat(WQF);
	need_stat(WDF_MAX);
	need_stat(DOC_LENGTH_MIN);
	need_stat(DOC_LENGTH_MAX);
	need_stat(TOTAL_LENGTH);
    }

    std::string name() const;

    std::string serialise() const;
    DLHWeight * unserialise(const std::string & serialised) const;

    double get_sumpart(Xapian::termcount wdf,
		       Xapian::termcount doclen,
		       Xapian::termcount uniqterms) const;
    double get_maxpart() const;

    double get_sumextra(Xapian::termcount doclen,
			Xapian::termcount uniqterms) const;
    double get_maxextra() const;
};

/** This class implements the PL2 weighting scheme.
 *
 *  PL2 is a representative scheme of the Divergence from Randomness Framework
 *  by Gianni Amati.
 *
 *  This weighting scheme is useful for tasks that require early precision.
 *
 *  It uses the Poisson approximation of the Binomial Probabilistic distribution
 *  (P) along with Stirling's approximation for the factorial value, the Laplace
 *  method to find the aftereffect of sampling (L) and the second wdf
 *  normalization proposed by Amati to normalize the wdf in the document to the
 *  length of the document (H2).
 *
 *  For more information about the DFR Framework and the PL2 scheme, please
 *  refer to : Gianni Amati and Cornelis Joost Van Rijsbergen Probabilistic models
 *  of information retrieval based on measuring the divergence from randomness
 *  ACM Transactions on Information Systems (TOIS) 20, (4), 2002, pp. 357-389.
 */
class XAPIAN_VISIBILITY_DEFAULT PL2Weight : public Weight {
    /// The wdf normalization parameter in the formula.
    double param_c;

    /** The factor to multiply weights by.
     *
     *  The misleading name is due to this having been used to store a lower
     *  bound in 1.4.0.  We no longer need to store that, and so this member
     *  has been repurposed in 1.4.1 and later (but the name left the same to
     *  ensure ABI compatibility with 1.4.0).
     */
    double lower_bound;

    /// The upper bound on the weight.
    double upper_bound;

    /// Constants for a given term in a given query.
    double P1, P2;

    /// Set by init() to (param_c * get_average_length())
    double cl;

    PL2Weight * clone() const;

    void init(double factor);

  public:
    /** Construct a PL2Weight.
     *
     *  @param c  A strictly positive parameter controlling the extent
     *		  of the normalization of the wdf to the document length. The
     *		  default value of 1 is suitable for longer queries but it may
     *		  need to be changed for shorter queries. For more information,
     *		  please refer to Gianni Amati's PHD thesis titled
     *		  Probabilistic Models for Information Retrieval based on
     *		  Divergence from Randomness.
     */
    explicit PL2Weight(double c);

    PL2Weight() : param_c(1.0) {
	need_stat(AVERAGE_LENGTH);
	need_stat(DOC_LENGTH);
	need_stat(DOC_LENGTH_MIN);
	need_stat(DOC_LENGTH_MAX);
	need_stat(COLLECTION_SIZE);
	need_stat(COLLECTION_FREQ);
	need_stat(WDF);
	need_stat(WDF_MAX);
	need_stat(WQF);
    }

    std::string name() const;

    std::string serialise() const;
    PL2Weight * unserialise(const std::string & serialised) const;

    double get_sumpart(Xapian::termcount wdf,
		       Xapian::termcount doclen,
		       Xapian::termcount uniqterms) const;
    double get_maxpart() const;

    double get_sumextra(Xapian::termcount doclen,
			Xapian::termcount uniqterms) const;
    double get_maxextra() const;
};

/// Xapian::Weight subclass implementing the PL2+ probabilistic formula.
class XAPIAN_VISIBILITY_DEFAULT PL2PlusWeight : public Weight {
    /// The factor to multiply weights by.
    double factor;

    /// The wdf normalization parameter in the formula.
    double param_c;

    /// Additional parameter delta in the PL2+ weighting formula.
    double param_delta;

    /// The upper bound on the weight.
    double upper_bound;

    /// Constants for a given term in a given query.
    double P1, P2;

    /// Set by init() to (param_c * get_average_length())
    double cl;

    /// Set by init() to get_collection_freq()) / get_collection_size()
    double mean;

    /// Weight contribution of delta term in the PL2+ function
    double dw;

    PL2PlusWeight * clone() const;

    void init(double factor_);

  public:
    /** Construct a PL2PlusWeight.
     *
     *  @param c  A strictly positive parameter controlling the extent
     *		  of the normalization of the wdf to the document length. The
     *		  default value of 1 is suitable for longer queries but it may
     *		  need to be changed for shorter queries. For more information,
     *		  please refer to Gianni Amati's PHD thesis titled
     *		  Probabilistic Models for Information Retrieval based on
     *		  Divergence from Randomness.
     *
     *  @param delta  A parameter for pseudo tf value to control the scale
     *                of the tf lower bound. Delta(δ) should be a positive
     *                real number. It can be tuned for example from 0.1 to 1.5
     *                in increments of 0.1 or so. Experiments have shown that
     *                PL2+ works effectively across collections with a fixed δ = 0.8
     *                (default 0.8)
     */
    PL2PlusWeight(double c, double delta);

    PL2PlusWeight()
	: param_c(1.0), param_delta(0.8) {
	need_stat(AVERAGE_LENGTH);
	need_stat(DOC_LENGTH);
	need_stat(DOC_LENGTH_MIN);
	need_stat(DOC_LENGTH_MAX);
	need_stat(COLLECTION_SIZE);
	need_stat(COLLECTION_FREQ);
	need_stat(WDF);
	need_stat(WDF_MAX);
	need_stat(WQF);
    }

    std::string name() const;

    std::string serialise() const;
    PL2PlusWeight * unserialise(const std::string & serialised) const;

    double get_sumpart(Xapian::termcount wdf,
		       Xapian::termcount doclen,
		       Xapian::termcount uniqterms) const;
    double get_maxpart() const;

    double get_sumextra(Xapian::termcount doclen,
			Xapian::termcount uniqterms) const;
    double get_maxextra() const;
};

/** This class implements the DPH weighting scheme.
 *
 *  DPH is a representative scheme of the Divergence from Randomness Framework
 *  by Gianni Amati.
 *
 *  This is a parameter free weighting scheme and it should be used with query
 *  expansion to obtain better results. It uses the HyperGeometric Probabilistic
 *  model and Popper's normalization to calculate the risk gain.
 *
 *  For more information about the DFR Framework and the DPH scheme, please
 *  refer to :
 *  a.) Gianni Amati and Cornelis Joost Van Rijsbergen
 *  Probabilistic models of information retrieval based on measuring the
 *  divergence from randomness ACM Transactions on Information Systems (TOIS) 20,
 *  (4), 2002, pp. 357-389.
 *  b.) FUB, IASI-CNR and University of Tor Vergata at TREC 2007 Blog Track.
 *  G. Amati and E. Ambrosi and M. Bianchi and C. Gaibisso and G. Gambosi.
 *  Proceedings of the 16th Text Retrieval Conference (TREC-2007), 2008.
 */
class XAPIAN_VISIBILITY_DEFAULT DPHWeight : public Weight {
    /// The upper bound on the weight.
    double upper_bound;

    /// Now unused but left in place in 1.4.x for ABI compatibility.
    double lower_bound;

    /// The constant value used in get_sumpart() .
    double log_constant;
    double wqf_product_factor;

    DPHWeight * clone() const;

    void init(double factor);

  public:
    /** Construct a DPHWeight. */
    DPHWeight() {
	need_stat(DOC_LENGTH);
	need_stat(COLLECTION_FREQ);
	need_stat(WDF);
	need_stat(WQF);
	need_stat(WDF_MAX);
	need_stat(DOC_LENGTH_MIN);
	need_stat(DOC_LENGTH_MAX);
	need_stat(TOTAL_LENGTH);
    }

    std::string name() const;

    std::string serialise() const;
    DPHWeight * unserialise(const std::string & serialised) const;

    double get_sumpart(Xapian::termcount wdf,
		       Xapian::termcount doclen,
		       Xapian::termcount uniqterms) const;
    double get_maxpart() const;

    double get_sumextra(Xapian::termcount doclen,
			Xapian::termcount uniqterms) const;
    double get_maxextra() const;
};


/** Xapian::Weight subclass implementing the Language Model formula.
 *
 * This class implements the "Language Model" Weighting scheme, as
 * described by the early papers on LM by Bruce Croft.
 *
 * LM works by comparing the query to a Language Model of the document.
 * The language model itself is parameter-free, though LMWeight takes
 * parameters which specify the smoothing used.
 */
class XAPIAN_VISIBILITY_DEFAULT LMWeight : public Weight {
    /** The type of smoothing to use. */
    type_smoothing select_smoothing;

    // Parameters for handling negative value of log, and for smoothing.
    double param_log, param_smoothing1, param_smoothing2;

    /** The factor to multiply weights by.
     *
     *  The misleading name is due to this having been used to store some
     *  other value in 1.4.0.  However, that value only takes one
     *  multiplication and one division to calculate, so for 1.4.x we can just
     *  recalculate it each time we need it, and so this member has been
     *  repurposed in 1.4.1 and later (but the name left the same to ensure ABI
     *  compatibility with 1.4.0).
     */
    double weight_collection;

    LMWeight * clone() const;

    void init(double factor);

  public:
    /** Construct a LMWeight.
     *
     *  @param param_log_	A non-negative parameter controlling how much
     *				to clamp negative values returned by the log.
     *				The log is calculated by multiplying the
     *				actual weight by param_log.  If param_log is
     *				0.0, then the document length upper bound will
     *				be used (default: document length upper	bound)
     *
     *  @param select_smoothing_	A parameter of type enum
     *					type_smoothing.  This parameter
     *					controls which smoothing type to use.
     *					(default: TWO_STAGE_SMOOTHING)
     *
     *  @param param_smoothing1_	A non-negative parameter for smoothing
     *					whose meaning depends on
     *					select_smoothing_.  In
     *					JELINEK_MERCER_SMOOTHING, it plays the
     *					role of estimation and in
     *					DIRICHLET_SMOOTHING the role of query
     *					modelling. (default JELINEK_MERCER,
     *					ABSOLUTE, TWOSTAGE(0.7),
     *					DIRCHLET(2000))
     *
     *  @param param_smoothing2_	A non-negative parameter which is used
     *					with TWO_STAGE_SMOOTHING as parameter for Dirichlet's
     *					smoothing (default: 2000) and as parameter delta to
     *					control the scale of the tf lower bound in the
     *					DIRICHLET_PLUS_SMOOTHING (default 0.05).
     *
     */
    // Unigram LM Constructor to specifically mention all parameters for handling negative log value and smoothing.
    explicit LMWeight(double param_log_ = 0.0,
		      type_smoothing select_smoothing_ = TWO_STAGE_SMOOTHING,
		      double param_smoothing1_ = -1.0,
		      double param_smoothing2_ = -1.0)
	: select_smoothing(select_smoothing_), param_log(param_log_), param_smoothing1(param_smoothing1_),
	  param_smoothing2(param_smoothing2_)
    {
	if (param_smoothing1 < 0) param_smoothing1 = 0.7;
	if (param_smoothing2 < 0) {
	    if (select_smoothing == TWO_STAGE_SMOOTHING)
		param_smoothing2 = 2000.0;
	    else
		param_smoothing2 = 0.05;
	}
	need_stat(DOC_LENGTH);
	need_stat(RSET_SIZE);
	need_stat(TERMFREQ);
	need_stat(RELTERMFREQ);
	need_stat(DOC_LENGTH_MAX);
	need_stat(WDF);
	need_stat(WDF_MAX);
	need_stat(COLLECTION_FREQ);
	need_stat(TOTAL_LENGTH);
	if (select_smoothing == ABSOLUTE_DISCOUNT_SMOOTHING)
	    need_stat(UNIQUE_TERMS);
	if (select_smoothing == DIRICHLET_PLUS_SMOOTHING)
	    need_stat(DOC_LENGTH_MIN);
    }

    std::string name() const;

    std::string serialise() const;
    LMWeight * unserialise(const std::string & serialised) const;

    double get_sumpart(Xapian::termcount wdf,
		       Xapian::termcount doclen,
		       Xapian::termcount uniqterm) const;
    double get_maxpart() const;

    double get_sumextra(Xapian::termcount doclen, Xapian::termcount) const;
    double get_maxextra() const;
};

/** Xapian::Weight subclass implementing Coordinate Matching.
 *
 *  Each matching term score one point.  See Managing Gigabytes, Second Edition
 *  p181.
 */
class XAPIAN_VISIBILITY_DEFAULT CoordWeight : public Weight {
    /// The factor to multiply weights by.
    double factor;

  public:
    CoordWeight * clone() const;

    void init(double factor_);

    /** Construct a CoordWeight. */
    CoordWeight() { }

    std::string name() const;

    std::string serialise() const;
    CoordWeight * unserialise(const std::string & serialised) const;

    double get_sumpart(Xapian::termcount wdf,
		       Xapian::termcount doclen,
		       Xapian::termcount uniqterm) const;
    double get_maxpart() const;

    double get_sumextra(Xapian::termcount, Xapian::termcount) const;
    double get_maxextra() const;
};

}

#endif // XAPIAN_INCLUDED_WEIGHT_H
