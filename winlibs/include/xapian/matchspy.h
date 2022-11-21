/** @file
 * @brief MatchSpy implementation.
 */
/* Copyright (C) 2007,2008,2009,2010,2011,2012,2013,2014,2015 Olly Betts
 * Copyright (C) 2007,2009 Lemur Consulting Ltd
 * Copyright (C) 2010 Richard Boulton
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

#ifndef XAPIAN_INCLUDED_MATCHSPY_H
#define XAPIAN_INCLUDED_MATCHSPY_H

#if !defined XAPIAN_IN_XAPIAN_H && !defined XAPIAN_LIB_BUILD
# error Never use <xapian/matchspy.h> directly; include <xapian.h> instead.
#endif

#include <xapian/attributes.h>
#include <xapian/intrusive_ptr.h>
#include <xapian/termiterator.h>
#include <xapian/visibility.h>

#include <string>
#include <map>

namespace Xapian {

class Document;
class Registry;

/** Abstract base class for match spies.
 *
 *  The subclasses will generally accumulate information seen during the match,
 *  to calculate aggregate functions, or other profiles of the matching
 *  documents.
 */
class XAPIAN_VISIBILITY_DEFAULT MatchSpy
    : public Xapian::Internal::opt_intrusive_base {
  private:
    /// Don't allow assignment.
    void operator=(const MatchSpy &);

    /// Don't allow copying.
    MatchSpy(const MatchSpy &);

  public:
    /// Default constructor, needed by subclass constructors.
    XAPIAN_NOTHROW(MatchSpy()) {}

    /** Virtual destructor, because we have virtual methods. */
    virtual ~MatchSpy();

    /** Register a document with the match spy.
     *
     *  This is called by the matcher once with each document seen by the
     *  matcher during the match process.  Note that the matcher will often not
     *  see all the documents which match the query, due to optimisations which
     *  allow low-weighted documents to be skipped, and allow the match process
     *  to be terminated early.
     *
     *  @param doc The document seen by the match spy.
     *  @param wt The weight of the document.
     */
    virtual void operator()(const Xapian::Document &doc,
			    double wt) = 0;

    /** Clone the match spy.
     *
     *  The clone should inherit the configuration of the parent, but need not
     *  inherit the state.  ie, the clone does not need to be passed
     *  information about the results seen by the parent.
     *
     *  If you don't want to support the remote backend in your match spy, you
     *  can use the default implementation which simply throws
     *  Xapian::UnimplementedError.
     *
     *  Note that the returned object will be deallocated by Xapian after use
     *  with "delete".  If you want to handle the deletion in a special way
     *  (for example when wrapping the Xapian API for use from another
     *  language) then you can define a static <code>operator delete</code>
     *  method in your subclass as shown here:
     *  https://trac.xapian.org/ticket/554#comment:1
     */
    virtual MatchSpy * clone() const;

    /** Return the name of this match spy.
     *
     *  This name is used by the remote backend.  It is passed with the
     *  serialised parameters to the remote server so that it knows which class
     *  to create.
     *
     *  Return the full namespace-qualified name of your class here - if your
     *  class is called MyApp::FooMatchSpy, return "MyApp::FooMatchSpy" from
     *  this method.
     *
     *  If you don't want to support the remote backend in your match spy, you
     *  can use the default implementation which simply throws
     *  Xapian::UnimplementedError.
     */
    virtual std::string name() const;

    /** Return this object's parameters serialised as a single string.
     *
     *  If you don't want to support the remote backend in your match spy, you
     *  can use the default implementation which simply throws
     *  Xapian::UnimplementedError.
     */
    virtual std::string serialise() const;

    /** Unserialise parameters.
     *
     *  This method unserialises parameters serialised by the @a serialise()
     *  method and allocates and returns a new object initialised with them.
     *
     *  If you don't want to support the remote backend in your match spy, you
     *  can use the default implementation which simply throws
     *  Xapian::UnimplementedError.
     *
     *  Note that the returned object will be deallocated by Xapian after use
     *  with "delete".  If you want to handle the deletion in a special way
     *  (for example when wrapping the Xapian API for use from another
     *  language) then you can define a static <code>operator delete</code>
     *  method in your subclass as shown here:
     *  https://trac.xapian.org/ticket/554#comment:1
     *
     *  @param serialised	A string containing the serialised results.
     *  @param context	Registry object to use for unserialisation to permit
     *			MatchSpy subclasses with sub-MatchSpy objects to be
     *			implemented.
     */
    virtual MatchSpy * unserialise(const std::string & serialised,
				   const Registry & context) const;

    /** Serialise the results of this match spy.
     *
     *  If you don't want to support the remote backend in your match spy, you
     *  can use the default implementation which simply throws
     *  Xapian::UnimplementedError.
     */
    virtual std::string serialise_results() const;

    /** Unserialise some results, and merge them into this matchspy.
     *
     *  The order in which results are merged should not be significant, since
     *  this order is not specified (and will vary depending on the speed of
     *  the search in each sub-database).
     *
     *  If you don't want to support the remote backend in your match spy, you
     *  can use the default implementation which simply throws
     *  Xapian::UnimplementedError.
     *
     *  @param serialised	A string containing the serialised results.
     */
    virtual void merge_results(const std::string & serialised);

    /** Return a string describing this object.
     *
     *  This default implementation returns a generic answer, to avoid forcing
     *  those deriving their own MatchSpy subclasses from having to implement
     *  this (they may not care what get_description() gives for their
     *  subclass).
     */
    virtual std::string get_description() const;

    /** Start reference counting this object.
     *
     *  You can hand ownership of a dynamically allocated MatchSpy
     *  object to Xapian by calling release() and then passing the object to a
     *  Xapian method.  Xapian will arrange to delete the object once it is no
     *  longer required.
     */
    MatchSpy * release() {
	opt_intrusive_base::release();
	return this;
    }

    /** Start reference counting this object.
     *
     *  You can hand ownership of a dynamically allocated MatchSpy
     *  object to Xapian by calling release() and then passing the object to a
     *  Xapian method.  Xapian will arrange to delete the object once it is no
     *  longer required.
     */
    const MatchSpy * release() const {
	opt_intrusive_base::release();
	return this;
    }
};


/** Class for counting the frequencies of values in the matching documents.
 */
class XAPIAN_VISIBILITY_DEFAULT ValueCountMatchSpy : public MatchSpy {
  public:
    struct Internal;

#ifndef SWIG // SWIG doesn't need to know about the internal class
    /// @private @internal
    struct XAPIAN_VISIBILITY_DEFAULT Internal
	    : public Xapian::Internal::intrusive_base
    {
	/// The slot to count.
	Xapian::valueno slot;

	/// Total number of documents seen by the match spy.
	Xapian::doccount total;

	/// The values seen so far, together with their frequency.
	std::map<std::string, Xapian::doccount> values;

	Internal() : slot(Xapian::BAD_VALUENO), total(0) {}
	explicit Internal(Xapian::valueno slot_) : slot(slot_), total(0) {}
    };
#endif

  protected:
    /** @private @internal Reference counted internals. */
    Xapian::Internal::intrusive_ptr<Internal> internal;

  public:
    /// Construct an empty ValueCountMatchSpy.
    ValueCountMatchSpy() {}

    /// Construct a MatchSpy which counts the values in a particular slot.
    explicit ValueCountMatchSpy(Xapian::valueno slot_)
	    : internal(new Internal(slot_)) {}

    /** Return the total number of documents tallied. */
    size_t XAPIAN_NOTHROW(get_total() const) {
	return internal.get() ? internal->total : 0;
    }

    /** Get an iterator over the values seen in the slot.
     *
     *  Items will be returned in ascending alphabetical order.
     *
     *  During the iteration, the frequency of the current value can be
     *  obtained with the get_termfreq() method on the iterator.
     */
    TermIterator values_begin() const;

    /** End iterator corresponding to values_begin() */
    TermIterator XAPIAN_NOTHROW(values_end() const) {
	return TermIterator();
    }

    /** Get an iterator over the most frequent values seen in the slot.
     *
     *  Items will be returned in descending order of frequency.  Values with
     *  the same frequency will be returned in ascending alphabetical order.
     *
     *  During the iteration, the frequency of the current value can be
     *  obtained with the get_termfreq() method on the iterator.
     *
     *  @param maxvalues The maximum number of values to return.
     */
    TermIterator top_values_begin(size_t maxvalues) const;

    /** End iterator corresponding to top_values_begin() */
    TermIterator XAPIAN_NOTHROW(top_values_end(size_t) const) {
	return TermIterator();
    }

    /** Implementation of virtual operator().
     *
     *  This implementation tallies values for a matching document.
     *
     *  @param doc	The document to tally values for.
     *  @param wt	The weight of the document (ignored by this class).
     */
    void operator()(const Xapian::Document &doc, double wt);

    virtual MatchSpy * clone() const;
    virtual std::string name() const;
    virtual std::string serialise() const;
    virtual MatchSpy * unserialise(const std::string & serialised,
				   const Registry & context) const;
    virtual std::string serialise_results() const;
    virtual void merge_results(const std::string & serialised);
    virtual std::string get_description() const;
};

}

#endif // XAPIAN_INCLUDED_MATCHSPY_H
