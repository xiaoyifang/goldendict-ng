/** @file
 * @brief parsing a user query string to build a Xapian::Query object
 */
/* Copyright (C) 2005,2006,2007,2008,2009,2010,2011,2012,2013,2014,2015,2016,2017,2018,2021 Olly Betts
 * Copyright (C) 2010 Adam Sjøgren
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

#ifndef XAPIAN_INCLUDED_QUERYPARSER_H
#define XAPIAN_INCLUDED_QUERYPARSER_H

#if !defined XAPIAN_IN_XAPIAN_H && !defined XAPIAN_LIB_BUILD
# error Never use <xapian/queryparser.h> directly; include <xapian.h> instead.
#endif

#include <xapian/attributes.h>
#include <xapian/deprecated.h>
#include <xapian/intrusive_ptr.h>
#include <xapian/query.h>
#include <xapian/termiterator.h>
#include <xapian/visibility.h>

#include <set>
#include <string>

namespace Xapian {

class Database;
class Stem;

/** Abstract base class for stop-word decision functor.
 *
 *  If you just want to use an existing stopword list, see
 *  Xapian::SimpleStopper.
 */
class XAPIAN_VISIBILITY_DEFAULT Stopper
    : public Xapian::Internal::opt_intrusive_base {
    /// Don't allow assignment.
    void operator=(const Stopper &);

    /// Don't allow copying.
    Stopper(const Stopper &);

  public:
    /// Default constructor.
    Stopper() { }

    /** Is term a stop-word?
     *
     *  @param term	The term to test.
     */
    virtual bool operator()(const std::string & term) const = 0;

    /// Class has virtual methods, so provide a virtual destructor.
    virtual ~Stopper() { }

    /// Return a string describing this object.
    virtual std::string get_description() const;

    /** Start reference counting this object.
     *
     *  You can hand ownership of a dynamically allocated Stopper
     *  object to Xapian by calling release() and then passing the object to a
     *  Xapian method.  Xapian will arrange to delete the object once it is no
     *  longer required.
     */
    Stopper * release() {
	opt_intrusive_base::release();
	return this;
    }

    /** Start reference counting this object.
     *
     *  You can hand ownership of a dynamically allocated Stopper
     *  object to Xapian by calling release() and then passing the object to a
     *  Xapian method.  Xapian will arrange to delete the object once it is no
     *  longer required.
     */
    const Stopper * release() const {
	opt_intrusive_base::release();
	return this;
    }
};

/// Simple implementation of Stopper class - this will suit most users.
class XAPIAN_VISIBILITY_DEFAULT SimpleStopper : public Stopper {
    std::set<std::string> stop_words;

  public:
    /// Default constructor.
    SimpleStopper() { }

    /** Initialise from a pair of iterators.
     *
     *  Xapian includes stopword list files for many languages.  You can
     *  initialise from a file like so:
     *  @code
     *  std::ifstream words("stopwords/english/stop.txt");
     *  Xapian::SimplerStopper stopper(std::istream_iterator<std::string>(words), std::istream_iterator<std::string>());
     *  @endcode
     *
     *  In bindings for other languages it isn't possible to pass a C++
     *  iterator pair, so instead this constructor is wrapped to allow
     *  passing a filename.
     */
    template<class Iterator>
    SimpleStopper(Iterator begin, Iterator end) : stop_words(begin, end) { }

    /// Add a single stop word.
    void add(const std::string & word) { stop_words.insert(word); }

    virtual bool operator()(const std::string & term) const {
	return stop_words.find(term) != stop_words.end();
    }

    virtual std::string get_description() const;
};

enum {
    RP_SUFFIX = 1,
    RP_REPEATED = 2,
    RP_DATE_PREFER_MDY = 4
};

/// Base class for range processors.
class XAPIAN_VISIBILITY_DEFAULT RangeProcessor
    : public Xapian::Internal::opt_intrusive_base {
    /// Don't allow assignment.
    void operator=(const RangeProcessor &);

    /// Don't allow copying.
    RangeProcessor(const RangeProcessor &);

  protected:
    /** The value slot to process.
     *
     *  If this range processor isn't value-based, it can ignore this member.
     */
    Xapian::valueno slot;

    /** The prefix (or suffix with RP_SUFFIX) string to look for. */
    std::string str;

    /** Flags.
     *
     *  Bitwise-or (| in C++) of zero or more of the following:
     *  * Xapian::RP_SUFFIX - require @a str as a suffix
     *    instead of a prefix.
     *  * Xapian::RP_REPEATED - optionally allow @a str
     *    on both ends of the range - e.g. $1..$10 or
     *    5m..50m.  By default a prefix is only checked for on
     *    the start (e.g. date:1/1/1980..31/12/1989), and a
     *    suffix only on the end (e.g. 2..12kg).
     */
    unsigned flags;

  public:
    /** Default constructor. */
    RangeProcessor() : slot(Xapian::BAD_VALUENO), flags(0) { }

    /** Constructor.
     *
     *  @param slot_	Which value slot to generate ranges over.
     *  @param str_	A string to look for to recognise values as belonging
     *			to this range (as a prefix by default, or as a suffix
     *			if flags Xapian::RP_SUFFIX is specified).
     *  @param flags_	Zero or more of the following flags, combined with
     *			bitwise-or (| in C++):
     *			 * Xapian::RP_SUFFIX - require @a str_ as a suffix
     *			   instead of a prefix.
     *			 * Xapian::RP_REPEATED - optionally allow @a str_
     *			   on both ends of the range - e.g. $1..$10 or
     *			   5m..50m.  By default a prefix is only checked for on
     *			   the start (e.g. date:1/1/1980..31/12/1989), and a
     *			   suffix only on the end (e.g. 2..12kg).
     */
    explicit RangeProcessor(Xapian::valueno slot_,
			    const std::string& str_ = std::string(),
			    unsigned flags_ = 0)
	: slot(slot_), str(str_), flags(flags_) { }

    /// Destructor.
    virtual ~RangeProcessor();

    /** Check prefix/suffix on range.
     *
     *  If they match, remove the prefix/suffix and then call operator()()
     *  to try to handle the range.
     */
    Xapian::Query check_range(const std::string& b, const std::string& e);

    /** Check for a valid range of this type.
     *
     *  Override this method to implement your own range handling.
     *
     *  @param begin	The start of the range as specified in the query string
     *			by the user.
     *  @param end	The end of the range as specified in the query string
     *			by the user (empty string for no upper limit).
     *
     *  @return		An OP_VALUE_RANGE Query object (or if end.empty(), an
     *			OP_VALUE_GE Query object).  Or if the range isn't one
     *			which this object can handle then
     *			Xapian::Query(Xapian::Query::OP_INVALID) will be
     *			returned.
     */
    virtual Xapian::Query
	operator()(const std::string &begin, const std::string &end);

    /** Start reference counting this object.
     *
     *  You can hand ownership of a dynamically allocated RangeProcessor
     *  object to Xapian by calling release() and then passing the object to a
     *  Xapian method.  Xapian will arrange to delete the object once it is no
     *  longer required.
     */
    RangeProcessor * release() {
	opt_intrusive_base::release();
	return this;
    }

    /** Start reference counting this object.
     *
     *  You can hand ownership of a dynamically allocated RangeProcessor
     *  object to Xapian by calling release() and then passing the object to a
     *  Xapian method.  Xapian will arrange to delete the object once it is no
     *  longer required.
     */
    const RangeProcessor * release() const {
	opt_intrusive_base::release();
	return this;
    }
};

/** Handle a date range.
 *
 *  Begin and end must be dates in a recognised format.
 */
class XAPIAN_VISIBILITY_DEFAULT DateRangeProcessor : public RangeProcessor {
    int epoch_year;

  public:
    /** Constructor.
     *
     *  @param slot_	The value number to return from operator().
     *
     *  @param flags_	Zero or more of the following flags, combined with
     *			bitwise-or:
     *			 * Xapian::RP_DATE_PREFER_MDY - interpret ambiguous
     *			   dates as month/day/year rather than day/month/year.
     *
     *  @param epoch_year_  Year to use as the epoch for dates with 2 digit
     *			    years (default: 1970, so 1/1/69 is 2069 while
     *			    1/1/70 is 1970).
     */
    explicit DateRangeProcessor(Xapian::valueno slot_,
				unsigned flags_ = 0,
				int epoch_year_ = 1970)
	: RangeProcessor(slot_, std::string(), flags_),
	  epoch_year(epoch_year_) { }

    /** Constructor.
     *
     *  @param slot_	The value slot number to query.
     *
     *  @param str_	A string to look for to recognise values as belonging
     *			to this date range.
     *
     *  @param flags_	Zero or more of the following flags, combined with
     *			bitwise-or:
     *			 * Xapian::RP_SUFFIX - require @a str_ as a suffix
     *			   instead of a prefix.
     *			 * Xapian::RP_REPEATED - optionally allow @a str_
     *			   on both ends of the range - e.g. $1..$10 or
     *			   5m..50m.  By default a prefix is only checked for on
     *			   the start (e.g. date:1/1/1980..31/12/1989), and a
     *			   suffix only on the end (e.g. 2..12kg).
     *			 * Xapian::RP_DATE_PREFER_MDY - interpret ambiguous
     *			   dates as month/day/year rather than day/month/year.
     *
     *  @param epoch_year_  Year to use as the epoch for dates with 2 digit
     *			    years (default: 1970, so 1/1/69 is 2069 while
     *			    1/1/70 is 1970).
     *
     *  The string supplied in str_ is used by @a operator() to decide whether
     *  the pair of strings supplied to it constitute a valid range.  If
     *  prefix_ is true, the first value in a range must begin with str_ (and
     *  the second value may optionally begin with str_);
     *  if prefix_ is false, the second value in a range must end with str_
     *  (and the first value may optionally end with str_).
     *
     *  If str_ is empty, the Xapian::RP_SUFFIX and Xapian::RP_REPEATED are
     *  irrelevant, and no special strings are required at the start or end of
     *  the strings defining the range.
     *
     *  The remainder of both strings defining the endpoints must be valid
     *  dates.
     *
     *  For example, if str_ is "created:", Xapian::RP_SUFFIX is not specified,
     *  and the range processor has been added to the queryparser, the
     *  queryparser will accept "created:1/1/2000..31/12/2001".
     */
    DateRangeProcessor(Xapian::valueno slot_, const std::string &str_,
		       unsigned flags_ = 0, int epoch_year_ = 1970)
	: RangeProcessor(slot_, str_, flags_),
	  epoch_year(epoch_year_) { }

    /** Check for a valid date range.
     *
     *  If any specified prefix is present, and the range looks like a
     *  date range, the dates are converted to the format YYYYMMDD and
     *  combined into a value range query.
     *
     *  @param begin	The start of the range as specified in the query string
     *			by the user.
     *  @param end	The end of the range as specified in the query string
     *			by the user.
     */
    Xapian::Query operator()(const std::string& begin, const std::string& end);
};

/** Handle a number range.
 *
 *  This class must be used on values which have been encoded using
 *  Xapian::sortable_serialise() which turns numbers into strings which
 *  will sort in the same order as the numbers (the same values can be
 *  used to implement a numeric sort).
 */
class XAPIAN_VISIBILITY_DEFAULT NumberRangeProcessor : public RangeProcessor {
  public:
    /** Constructor.
     *
     *  @param slot_    The value slot number to query.
     *
     *  @param str_     A string to look for to recognise values as belonging
     *                  to this numeric range.
     *
     *  @param flags_	Zero or more of the following flags, combined with
     *			bitwise-or:
     *			 * Xapian::RP_SUFFIX - require @a str_ as a suffix
     *			   instead of a prefix.
     *			 * Xapian::RP_REPEATED - optionally allow @a str_
     *			   on both ends of the range - e.g. $1..$10 or
     *			   5m..50m.  By default a prefix is only checked for on
     *			   the start (e.g. date:1/1/1980..31/12/1989), and a
     *			   suffix only on the end (e.g. 2..12kg).
     *
     *  The string supplied in str_ is used by @a operator() to decide whether
     *  the pair of strings supplied to it constitute a valid range.  If
     *  prefix_ is true, the first value in a range must begin with str_ (and
     *  the second value may optionally begin with str_);
     *  if prefix_ is false, the second value in a range must end with str_
     *  (and the first value may optionally end with str_).
     *
     *  If str_ is empty, the setting of prefix_ is irrelevant, and no special
     *  strings are required at the start or end of the strings defining the
     *  range.
     *
     *  The remainder of both strings defining the endpoints must be valid
     *  floating point numbers. (FIXME: define format recognised).
     *
     *  For example, if str_ is "$" and prefix_ is true, and the range
     *  processor has been added to the queryparser, the queryparser will
     *  accept "$10..50" or "$10..$50", but not "10..50" or "10..$50" as valid
     *  ranges.  If str_ is "kg" and prefix_ is false, the queryparser will
     *  accept "10..50kg" or "10kg..50kg", but not "10..50" or "10kg..50" as
     *  valid ranges.
     */
    NumberRangeProcessor(Xapian::valueno slot_,
			 const std::string &str_ = std::string(),
			 unsigned flags_ = 0)
	: RangeProcessor(slot_, str_, flags_) { }

    /** Check for a valid numeric range.
     *
     *  If BEGIN..END is a valid numeric range with the specified prefix/suffix
     *  (if one was specified), the prefix/suffix is removed, the string
     *  converted to a number, and encoded with Xapian::sortable_serialise(),
     *  and a value range query is built.
     *
     *  @param begin	The start of the range as specified in the query string
     *			by the user.
     *  @param end	The end of the range as specified in the query string
     *			by the user.
     */
    Xapian::Query operator()(const std::string& begin, const std::string& end);
};

/// Base class for value range processors.
class XAPIAN_VISIBILITY_DEFAULT ValueRangeProcessor
    : public Xapian::Internal::opt_intrusive_base {
    /// Don't allow assignment.
    void operator=(const ValueRangeProcessor &);

    /// Don't allow copying.
    ValueRangeProcessor(const ValueRangeProcessor &);

  public:
    /// Default constructor.
    ValueRangeProcessor() { }

    /// Destructor.
    virtual ~ValueRangeProcessor();

    /** Check for a valid range of this type.
     *
     *  @param[in,out] begin	The start of the range as specified in the query
     *				string by the user.  This parameter is a
     *				non-const reference so the ValueRangeProcessor
     *				can modify it to return the value to start the
     *				range with.
     *  @param[in,out] end	The end of the range.  This is also a non-const
     *				reference so it can be modified.
     *
     *  @return	If this ValueRangeProcessor recognises the range BEGIN..END it
     *		returns the value slot number to range filter on.  Otherwise it
     *		returns Xapian::BAD_VALUENO.
     */
    virtual Xapian::valueno operator()(std::string &begin, std::string &end) = 0;

    /** Start reference counting this object.
     *
     *  You can hand ownership of a dynamically allocated ValueRangeProcessor
     *  object to Xapian by calling release() and then passing the object to a
     *  Xapian method.  Xapian will arrange to delete the object once it is no
     *  longer required.
     */
    ValueRangeProcessor * release() {
	opt_intrusive_base::release();
	return this;
    }

    /** Start reference counting this object.
     *
     *  You can hand ownership of a dynamically allocated ValueRangeProcessor
     *  object to Xapian by calling release() and then passing the object to a
     *  Xapian method.  Xapian will arrange to delete the object once it is no
     *  longer required.
     */
    const ValueRangeProcessor * release() const {
	opt_intrusive_base::release();
	return this;
    }
};

/** Handle a string range.
 *
 *  The end points can be any strings.
 *
 *  @deprecated Use Xapian::RangeProcessor instead (added in 1.3.6).
 */
class XAPIAN_DEPRECATED_CLASS_EX XAPIAN_VISIBILITY_DEFAULT StringValueRangeProcessor : public ValueRangeProcessor {
  protected:
    /** The value slot to process. */
    Xapian::valueno valno;

    /** Whether to look for @a str as a prefix or suffix. */
    bool prefix;

    /** The prefix (or suffix if prefix==false) string to look for. */
    std::string str;

  public:
    /** Constructor.
     *
     *  @param slot_	The value number to return from operator().
     */
    explicit StringValueRangeProcessor(Xapian::valueno slot_)
	: valno(slot_), str() { }

    /** Constructor.
     *
     *  @param slot_	The value number to return from operator().
     *  @param str_     A string to look for to recognise values as belonging
     *                  to this range.
     *  @param prefix_	Flag specifying whether to check for str_ as a prefix
     *			or a suffix.
     */
    StringValueRangeProcessor(Xapian::valueno slot_, const std::string &str_,
			      bool prefix_ = true)
	: valno(slot_), prefix(prefix_), str(str_) { }

    /** Check for a valid string range.
     *
     *  @param[in,out] begin	The start of the range as specified in the
     *				query string by the user.  This parameter is a
     *				non-const reference so the ValueRangeProcessor
     *				can modify it to return the value to start the
     *				range with.
     *  @param[in,out] end	The end of the range.  This is also a non-const
     *				reference so it can be modified.
     *
     *  @return	A StringValueRangeProcessor always accepts a range it is
     *		offered, and returns the value of slot_ passed at construction
     *		time.  It doesn't modify @a begin or @a end.
     */
    Xapian::valueno operator()(std::string &begin, std::string &end);
};

/** Handle a date range.
 *
 *  Begin and end must be dates in a recognised format.
 *
 *  @deprecated Use Xapian::DateRangeProcessor instead (added in 1.3.6).
 */
class XAPIAN_DEPRECATED_CLASS_EX XAPIAN_VISIBILITY_DEFAULT DateValueRangeProcessor : public StringValueRangeProcessor {
    bool prefer_mdy;
    int epoch_year;

  public:
    /** Constructor.
     *
     *  @param slot_	    The value number to return from operator().
     *  @param prefer_mdy_  Should ambiguous dates be interpreted as
     *			    month/day/year rather than day/month/year?
     *			    (default: false)
     *  @param epoch_year_  Year to use as the epoch for dates with 2 digit
     *			    years (default: 1970, so 1/1/69 is 2069 while
     *			    1/1/70 is 1970).
     */
    DateValueRangeProcessor(Xapian::valueno slot_, bool prefer_mdy_ = false,
			    int epoch_year_ = 1970)
	: StringValueRangeProcessor(slot_),
	  prefer_mdy(prefer_mdy_), epoch_year(epoch_year_) { }

    /** Constructor.
     *
     *  @param slot_	    The value number to return from operator().
     *
     *  @param str_     A string to look for to recognise values as belonging
     *                  to this date range.
     *
     *  @param prefix_  Whether to look for the string at the start or end of
     *                  the values.  If true, the string is a prefix; if
     *                  false, the string is a suffix (default: true).
     *
     *  @param prefer_mdy_  Should ambiguous dates be interpreted as
     *			    month/day/year rather than day/month/year?
     *			    (default: false)
     *
     *  @param epoch_year_  Year to use as the epoch for dates with 2 digit
     *			    years (default: 1970, so 1/1/69 is 2069 while
     *			    1/1/70 is 1970).
     *
     *  The string supplied in str_ is used by @a operator() to decide whether
     *  the pair of strings supplied to it constitute a valid range.  If
     *  prefix_ is true, the first value in a range must begin with str_ (and
     *  the second value may optionally begin with str_);
     *  if prefix_ is false, the second value in a range must end with str_
     *  (and the first value may optionally end with str_).
     *
     *  If str_ is empty, the setting of prefix_ is irrelevant, and no special
     *  strings are required at the start or end of the strings defining the
     *  range.
     *
     *  The remainder of both strings defining the endpoints must be valid
     *  dates.
     *
     *  For example, if str_ is "created:" and prefix_ is true, and the range
     *  processor has been added to the queryparser, the queryparser will
     *  accept "created:1/1/2000..31/12/2001".
     */
    DateValueRangeProcessor(Xapian::valueno slot_, const std::string &str_,
			    bool prefix_ = true,
			    bool prefer_mdy_ = false, int epoch_year_ = 1970)
	: StringValueRangeProcessor(slot_, str_, prefix_),
	  prefer_mdy(prefer_mdy_), epoch_year(epoch_year_) { }

#ifndef SWIG
    /** Constructor.
     *
     *  This is like the previous version, but with const char * instead of
     *  std::string - we need this overload as otherwise
     *  DateValueRangeProcessor(1, "date:") quietly interprets the second
     *  argument as a boolean in preference to std::string.  If you want to
     *  be compatible with 1.2.12 and earlier, then explicitly convert to
     *  std::string, i.e.: DateValueRangeProcessor(1, std::string("date:"))
     *
     *  @param slot_	    The value number to return from operator().
     *
     *  @param str_     A string to look for to recognise values as belonging
     *                  to this date range.
     *
     *  @param prefix_  Whether to look for the string at the start or end of
     *                  the values.  If true, the string is a prefix; if
     *                  false, the string is a suffix (default: true).
     *
     *  @param prefer_mdy_  Should ambiguous dates be interpreted as
     *			    month/day/year rather than day/month/year?
     *			    (default: false)
     *
     *  @param epoch_year_  Year to use as the epoch for dates with 2 digit
     *			    years (default: 1970, so 1/1/69 is 2069 while
     *			    1/1/70 is 1970).
     *
     *  The string supplied in str_ is used by @a operator() to decide whether
     *  the pair of strings supplied to it constitute a valid range.  If
     *  prefix_ is true, the first value in a range must begin with str_ (and
     *  the second value may optionally begin with str_);
     *  if prefix_ is false, the second value in a range must end with str_
     *  (and the first value may optionally end with str_).
     *
     *  If str_ is empty, the setting of prefix_ is irrelevant, and no special
     *  strings are required at the start or end of the strings defining the
     *  range.
     *
     *  The remainder of both strings defining the endpoints must be valid
     *  dates.
     *
     *  For example, if str_ is "created:" and prefix_ is true, and the range
     *  processor has been added to the queryparser, the queryparser will
     *  accept "created:1/1/2000..31/12/2001".
     */
    DateValueRangeProcessor(Xapian::valueno slot_, const char * str_,
			    bool prefix_ = true,
			    bool prefer_mdy_ = false, int epoch_year_ = 1970)
	: StringValueRangeProcessor(slot_, str_, prefix_),
	  prefer_mdy(prefer_mdy_), epoch_year(epoch_year_) { }
#endif

    /** Check for a valid date range.
     *
     *  @param[in,out] begin	The start of the range as specified in the
     *				query string by the user.  This parameter is a
     *				non-const reference so the ValueRangeProcessor
     *				can modify it to return the value to start the
     *				range with.
     *  @param[in,out] end	The end of the range.  This is also a non-const
     *				reference so it can be modified.
     *
     *  @return	If BEGIN..END is a sensible date range, this method modifies
     *		them into the format YYYYMMDD and returns the value of slot_
     *		passed at construction time.  Otherwise it returns
     *		Xapian::BAD_VALUENO.
     */
    Xapian::valueno operator()(std::string &begin, std::string &end);
};

/** Handle a number range.
 *
 *  This class must be used on values which have been encoded using
 *  Xapian::sortable_serialise() which turns numbers into strings which
 *  will sort in the same order as the numbers (the same values can be
 *  used to implement a numeric sort).
 *
 *  @deprecated Use Xapian::NumberRangeProcessor instead (added in 1.3.6).
 */
class XAPIAN_DEPRECATED_CLASS_EX XAPIAN_VISIBILITY_DEFAULT NumberValueRangeProcessor : public StringValueRangeProcessor {
  public:
    /** Constructor.
     *
     *  @param slot_   The value number to return from operator().
     */
    explicit NumberValueRangeProcessor(Xapian::valueno slot_)
	: StringValueRangeProcessor(slot_) { }

    /** Constructor.
     *
     *  @param slot_    The value number to return from operator().
     *
     *  @param str_     A string to look for to recognise values as belonging
     *                  to this numeric range.
     *
     *  @param prefix_  Whether to look for the string at the start or end of
     *                  the values.  If true, the string is a prefix; if
     *                  false, the string is a suffix (default: true).
     *
     *  The string supplied in str_ is used by @a operator() to decide whether
     *  the pair of strings supplied to it constitute a valid range.  If
     *  prefix_ is true, the first value in a range must begin with str_ (and
     *  the second value may optionally begin with str_);
     *  if prefix_ is false, the second value in a range must end with str_
     *  (and the first value may optionally end with str_).
     *
     *  If str_ is empty, the setting of prefix_ is irrelevant, and no special
     *  strings are required at the start or end of the strings defining the
     *  range.
     *
     *  The remainder of both strings defining the endpoints must be valid
     *  floating point numbers. (FIXME: define format recognised).
     *
     *  For example, if str_ is "$" and prefix_ is true, and the range
     *  processor has been added to the queryparser, the queryparser will
     *  accept "$10..50" or "$10..$50", but not "10..50" or "10..$50" as valid
     *  ranges.  If str_ is "kg" and prefix_ is false, the queryparser will
     *  accept "10..50kg" or "10kg..50kg", but not "10..50" or "10kg..50" as
     *  valid ranges.
     */
    NumberValueRangeProcessor(Xapian::valueno slot_, const std::string &str_,
			      bool prefix_ = true)
	: StringValueRangeProcessor(slot_, str_, prefix_) { }

    /** Check for a valid numeric range.
     *
     *  @param[in,out] begin	The start of the range as specified in the
     *				query string by the user.  This parameter is a
     *				non-const reference so the ValueRangeProcessor
     *				can modify it to return the value to start the
     *				range with.
     *  @param[in,out] end	The end of the range.  This is also a non-const
     *				reference so it can be modified.
     *
     *  @return	If BEGIN..END is a valid numeric range with the specified
     *		prefix/suffix (if one was specified), this method modifies
     *		them by removing the prefix/suffix, converting to a number,
     *		and encoding with Xapian::sortable_serialise(), and returns the
     *		value of slot_ passed at construction time.  Otherwise it
     *		returns Xapian::BAD_VALUENO.
     */
    Xapian::valueno operator()(std::string &begin, std::string &end);
};

/** Base class for field processors.
 */
class XAPIAN_VISIBILITY_DEFAULT FieldProcessor
    : public Xapian::Internal::opt_intrusive_base {
    /// Don't allow assignment.
    void operator=(const FieldProcessor &);

    /// Don't allow copying.
    FieldProcessor(const FieldProcessor &);

  public:
    /// Default constructor.
    FieldProcessor() { }

    /// Destructor.
    virtual ~FieldProcessor();

    /** Convert a field-prefixed string to a Query object.
     *
     *  @param str	The string to convert.
     *
     *  @return	Query object corresponding to @a str.
     */
    virtual Xapian::Query operator()(const std::string &str) = 0;

    /** Start reference counting this object.
     *
     *  You can hand ownership of a dynamically allocated FieldProcessor
     *  object to Xapian by calling release() and then passing the object to a
     *  Xapian method.  Xapian will arrange to delete the object once it is no
     *  longer required.
     */
    FieldProcessor * release() {
	opt_intrusive_base::release();
	return this;
    }

    /** Start reference counting this object.
     *
     *  You can hand ownership of a dynamically allocated FieldProcessor
     *  object to Xapian by calling release() and then passing the object to a
     *  Xapian method.  Xapian will arrange to delete the object once it is no
     *  longer required.
     */
    const FieldProcessor * release() const {
	opt_intrusive_base::release();
	return this;
    }
};

/// Build a Xapian::Query object from a user query string.
class XAPIAN_VISIBILITY_DEFAULT QueryParser {
  public:
    /// Class representing the queryparser internals.
    class Internal;
    /// @private @internal Reference counted internals.
    Xapian::Internal::intrusive_ptr<Internal> internal;

    /// Enum of feature flags.
    typedef enum {
	/// Support AND, OR, etc and bracketed subexpressions.
	FLAG_BOOLEAN = 1,
	/// Support quoted phrases.
	FLAG_PHRASE = 2,
	/// Support + and -.
	FLAG_LOVEHATE = 4,
	/// Support AND, OR, etc even if they aren't in ALLCAPS.
	FLAG_BOOLEAN_ANY_CASE = 8,
	/** Support wildcards.
	 *
	 *  At present only right truncation (e.g. Xap*) is supported.
	 *
	 *  Currently you can't use wildcards with boolean filter prefixes,
	 *  or in a phrase (either an explicitly quoted one, or one implicitly
	 *  generated by hyphens or other punctuation).
	 *
	 *  In Xapian 1.2.x, you needed to tell the QueryParser object which
	 *  database to expand wildcards from by calling set_database().  In
	 *  Xapian 1.3.3, OP_WILDCARD was added and wildcards are now
	 *  expanded when Enquire::get_mset() is called, with the expansion
	 *  using the database being searched.
	 */
	FLAG_WILDCARD = 16,
	/** Allow queries such as 'NOT apples'.
	 *
	 *  These require the use of a list of all documents in the database
	 *  which is potentially expensive, so this feature isn't enabled by
	 *  default.
	 */
	FLAG_PURE_NOT = 32,
	/** Enable partial matching.
	 *
	 *  Partial matching causes the parser to treat the query as a
	 *  "partially entered" search.  This will automatically treat the
	 *  final word as a wildcarded match, unless it is followed by
	 *  whitespace, to produce more stable results from interactive
	 *  searches.
	 *
	 *  Currently FLAG_PARTIAL doesn't do anything if the final word
	 *  in the query has a boolean filter prefix, or if it is in a phrase
	 *  (either an explicitly quoted one, or one implicitly generated by
	 *  hyphens or other punctuation).  It also doesn't do anything if
	 *  if the final word is part of a value range.
	 *
	 *  In Xapian 1.2.x, you needed to tell the QueryParser object which
	 *  database to expand wildcards from by calling set_database().  In
	 *  Xapian 1.3.3, OP_WILDCARD was added and wildcards are now
	 *  expanded when Enquire::get_mset() is called, with the expansion
	 *  using the database being searched.
	 */
	FLAG_PARTIAL = 64,

	/** Enable spelling correction.
	 *
	 *  For each word in the query which doesn't exist as a term in the
	 *  database, Database::get_spelling_suggestion() will be called and if
	 *  a suggestion is returned, a corrected version of the query string
	 *  will be built up which can be read using
	 *  QueryParser::get_corrected_query_string().  The query returned is
	 *  based on the uncorrected query string however - if you want a
	 *  parsed query based on the corrected query string, you must call
	 *  QueryParser::parse_query() again.
	 *
	 *  NB: You must also call set_database() for this to work.
	 */
	FLAG_SPELLING_CORRECTION = 128,

	/** Enable synonym operator '~'.
	 *
	 *  NB: You must also call set_database() for this to work.
	 */
	FLAG_SYNONYM = 256,

	/** Enable automatic use of synonyms for single terms.
	 *
	 *  NB: You must also call set_database() for this to work.
	 */
	FLAG_AUTO_SYNONYMS = 512,

	/** Enable automatic use of synonyms for single terms and groups of
	 *  terms.
	 *
	 *  NB: You must also call set_database() for this to work.
	 */
	FLAG_AUTO_MULTIWORD_SYNONYMS = 1024,

	/** Enable generation of n-grams from CJK text.
	 *
	 *  With this enabled, spans of CJK characters are split into unigrams
	 *  and bigrams, with the unigrams carrying positional information.
	 *  Non-CJK characters are split into words as normal.
	 *
	 *  The corresponding option needs to have been used at index time.
	 *
	 *  Flag added in Xapian 1.3.4 and 1.2.22.  This mode can be
	 *  enabled in 1.2.8 and later by setting environment variable
	 *  XAPIAN_CJK_NGRAM to a non-empty value (but doing so was deprecated
	 *  in 1.4.11).
	 */
	FLAG_CJK_NGRAM = 2048,

	/** Accumulate unstem and stoplist results.
	 *
	 *  By default, the unstem and stoplist data is reset by a call to
	 *  parse_query(), which makes sense if you use the same QueryParser
	 *  object to parse a series of independent queries.
	 *
	 *  If you're using the same QueryParser object to parse several
	 *  fields on the same query form, you may want to have the unstem
	 *  and stoplist data combined for all of them, in which case you
	 *  can use this flag to prevent this data from being reset.
	 *
	 *  @since Added in Xapian 1.4.18.
	 */
	FLAG_ACCUMULATE = 65536,

	/** Produce a query which doesn't use positional information.
	 *
	 *  With this flag enabled, no positional information will be used
	 *  and any query operations which would use it are replaced by
	 *  the nearest equivalent which doesn't (so phrase searches, NEAR
	 *  and ADJ will result in OP_AND).
	 *
	 *  @since Added in Xapian 1.4.19.
	 */
	FLAG_NO_POSITIONS = 0x20000,

	/** The default flags.
	 *
	 *  Used if you don't explicitly pass any to @a parse_query().
	 *  The default flags are FLAG_PHRASE|FLAG_BOOLEAN|FLAG_LOVEHATE.
	 *
	 *  Added in Xapian 1.0.11.
	 */
	FLAG_DEFAULT = FLAG_PHRASE|FLAG_BOOLEAN|FLAG_LOVEHATE
    } feature_flag;

    /// Stemming strategies, for use with set_stemming_strategy().
    typedef enum {
	STEM_NONE, STEM_SOME, STEM_ALL, STEM_ALL_Z, STEM_SOME_FULL_POS
    } stem_strategy;

    /// Copy constructor.
    QueryParser(const QueryParser & o);

    /// Assignment.
    QueryParser & operator=(const QueryParser & o);

#ifdef XAPIAN_MOVE_SEMANTICS
    /// Move constructor.
    QueryParser(QueryParser && o);

    /// Move assignment operator.
    QueryParser & operator=(QueryParser && o);
#endif

    /// Default constructor.
    QueryParser();

    /// Destructor.
    ~QueryParser();

    /** Set the stemmer.
     *
     *  This sets the stemming algorithm which will be used by the query
     *  parser.  The stemming algorithm will be used according to the stemming
     *  strategy set by set_stemming_strategy().  As of 1.3.1, this defaults
     *  to STEM_SOME, but in earlier versions the default was STEM_NONE.  If
     *  you want to work with older versions, you should explicitly set
     *  a stemming strategy as well as setting a stemmer, otherwise your
     *  stemmer won't actually be used.
     *
     *  @param stemmer	The Xapian::Stem object to set.
     */
    void set_stemmer(const Xapian::Stem & stemmer);

    /** Set the stemming strategy.
     *
     *  This controls how the query parser will apply the stemming algorithm.
     *  Note that the stemming algorithm is only applied to words in free-text
     *  fields - boolean filter terms are never stemmed.
     *
     *  @param strategy	The strategy to use - possible values are:
     *   - STEM_NONE:	Don't perform any stemming.  (default in Xapian <=
     *			1.3.0)
     *   - STEM_SOME:	Stem all terms except for those which start with a
     *			capital letter, or are followed by certain characters
     *			(currently: <code>(/\@<>=*[{"</code> ), or are used
     *			with operators which need positional information.
     *			Stemmed terms are prefixed with 'Z'.  (default in
     *			Xapian >= 1.3.1)
     *   - STEM_SOME_FULL_POS:
     *			Like STEM_SOME but also stems terms used with operators
     *			which need positional information.  Added in Xapian
     *			1.4.8.
     *   - STEM_ALL:	Stem all terms (note: no 'Z' prefix is added).
     *   - STEM_ALL_Z:	Stem all terms (note: 'Z' prefix is added).  (new in
     *			Xapian 1.2.11 and 1.3.1)
     */
    void set_stemming_strategy(stem_strategy strategy);

    /** Set the stopper.
     *
     *  @param stop	The Stopper object to set (default NULL, which means no
     *			stopwords).
     */
    void set_stopper(const Stopper *stop = NULL);

    /** Set the default operator.
     *
     *  @param default_op	The operator to use to combine non-filter
     *				query items when no explicit operator is used.
     *
     *				So for example, 'weather forecast' is parsed as
     *				if it were 'weather OR forecast' by default.
     *
     *				The most useful values for this are OP_OR (the
     *				default) and OP_AND.  OP_NEAR, OP_PHRASE,
     *				OP_ELITE_SET, OP_SYNONYM and OP_MAX are also
     *				permitted.  Passing other values will result in
     *				InvalidArgumentError being thrown.
     */
    void set_default_op(Query::op default_op);

    /** Get the current default operator. */
    Query::op get_default_op() const;

    /** Specify the database being searched.
     *
     *  @param db	The database to use for spelling correction
     *			(FLAG_SPELLING_CORRECTION), and synonyms (FLAG_SYNONYM,
     *			FLAG_AUTO_SYNONYMS, and FLAG_AUTO_MULTIWORD_SYNONYMS).
     */
    void set_database(const Database &db);

    /** Specify the maximum expansion of a wildcard and/or partial term.
     *
     *  Note: you must also set FLAG_WILDCARD and/or FLAG_PARTIAL in the flags
     *  parameter to @a parse_query() for this setting to have anything to
     *  affect.
     *
     *  If you don't call this method, the default settings are no limit on
     *  wildcard expansion, and partial terms expanding to the most frequent
     *  100 terms - i.e. as if you'd called:
     *
     *  set_max_expansion(0);
     *  set_max_expansion(100, Xapian::Query::WILDCARD_LIMIT_MOST_FREQUENT, Xapian::QueryParser::FLAG_PARTIAL);
     *
     *  @param max_expansion  The maximum number of terms each wildcard in the
     *			query can expand to, or 0 for no limit (which is the
     *			default).
     *	@param max_type	@a Xapian::Query::WILDCARD_LIMIT_ERROR,
     *			@a Xapian::Query::WILDCARD_LIMIT_FIRST or
     *			@a Xapian::Query::WILDCARD_LIMIT_MOST_FREQUENT
     *			(default: Xapian::Query::WILDCARD_LIMIT_ERROR).
     *  @param flags	What to set the limit for (default:
     *			FLAG_WILDCARD|FLAG_PARTIAL, setting the limit for both
     *			wildcards and partial terms).
     *
     *  @since 1.3.3
     */
    void set_max_expansion(Xapian::termcount max_expansion,
			   int max_type = Xapian::Query::WILDCARD_LIMIT_ERROR,
			   unsigned flags = FLAG_WILDCARD|FLAG_PARTIAL);

    /** Specify the maximum expansion of a wildcard.
     *
     *  If any wildcard expands to more than @a max_expansion terms, an
     *  exception will be thrown.
     *
     *  This method is provided for API compatibility with Xapian 1.2.x and is
     *  deprecated - replace it with:
     *
     *  set_max_wildcard_expansion(max_expansion,
     *				   Xapian::Query::WILDCARD_LIMIT_ERROR,
     *				   Xapian::QueryParser::FLAG_WILDCARD);
     */
    XAPIAN_DEPRECATED(void set_max_wildcard_expansion(Xapian::termcount));

    /** Parse a query.
     *
     *  @param query_string  A free-text query as entered by a user
     *  @param flags         Zero or more QueryParser::feature_flag specifying
     *		what features the QueryParser should support.  Combine
     *		multiple values with bitwise-or (|) (default FLAG_DEFAULT).
     *	@param default_prefix  The default term prefix to use (default none).
     *		For example, you can pass "A" when parsing an "Author" field.
     *
     *  @exception If the query string can't be parsed, then
     *		   Xapian::QueryParserError is thrown.  You can get an English
     *		   error message to report to the user by catching it and
     *		   calling get_msg() on the caught exception.  The current
     *		   possible values (in case you want to translate them) are:
     *
     *		   @li Unknown range operation
     *		   @li parse error
     *		   @li Syntax: &lt;expression&gt; AND &lt;expression&gt;
     *		   @li Syntax: &lt;expression&gt; AND NOT &lt;expression&gt;
     *		   @li Syntax: &lt;expression&gt; NOT &lt;expression&gt;
     *		   @li Syntax: &lt;expression&gt; OR &lt;expression&gt;
     *		   @li Syntax: &lt;expression&gt; XOR &lt;expression&gt;
     */
    Query parse_query(const std::string &query_string,
		      unsigned flags = FLAG_DEFAULT,
		      const std::string &default_prefix = std::string());

    /** Add a free-text field term prefix.
     *
     *  For example:
     *
     *  @code
     *  qp.add_prefix("author", "A");
     *  @endcode
     *
     *  This allows the user to search for author:Orwell which will be
     *  converted to a search for the term "Aorwell".
     *
     *  Multiple fields can be mapped to the same prefix.  For example, you
     *  can make title: and subject: aliases for each other.
     *
     *  As of 1.0.4, you can call this method multiple times with the same
     *  value of field to allow a single field to be mapped to multiple
     *  prefixes.  Multiple terms being generated for such a field, and
     *  combined with @c Xapian::Query::OP_OR.
     *
     *  If any prefixes are specified for the empty field name (i.e. you
     *  call this method with an empty string as the first parameter)
     *  these prefixes will be used for terms without a field specifier.
     *  If you do this and also specify the @c default_prefix parameter to @c
     *  parse_query(), then the @c default_prefix parameter will override.
     *
     *  If the prefix parameter is empty, then "field:word" will produce the
     *  term "word" (and this can be one of several prefixes for a particular
     *  field, or for terms without a field specifier).
     *
     *  If you call @c add_prefix() and @c add_boolean_prefix() for the
     *  same value of @a field, a @c Xapian::InvalidOperationError exception
     *  will be thrown.
     *
     *  In 1.0.3 and earlier, subsequent calls to this method with the same
     *  value of @a field had no effect.
     *
     *  @param field   The user visible field name
     *  @param prefix  The term prefix to map this to
     */
    void add_prefix(const std::string& field, const std::string& prefix);

    /** Register a FieldProcessor.
     */
    void add_prefix(const std::string& field, Xapian::FieldProcessor * proc);

    /** Add a boolean term prefix allowing the user to restrict a
     *  search with a boolean filter specified in the free text query.
     *
     *  For example:
     *
     *  @code
     *  qp.add_boolean_prefix("site", "H");
     *  @endcode
     *
     *  This allows the user to restrict a search with site:xapian.org which
     *  will be converted to Hxapian.org combined with any weighted
     *  query with @c Xapian::Query::OP_FILTER.
     *
     *  If multiple boolean filters are specified in a query for the same
     *  prefix, they will be combined with the @c Xapian::Query::OP_OR
     *  operator.  Then, if there are boolean filters for different prefixes,
     *  they will be combined with the @c Xapian::Query::OP_AND operator.
     *
     *  Multiple fields can be mapped to the same prefix (so for example
     *  you can make site: and domain: aliases for each other).  Instances of
     *  fields with different aliases but the same prefix will still be
     *  combined with the OR operator.
     *
     *  For example, if "site" and "domain" map to "H", but author maps to "A",
     *  a search for "site:foo domain:bar author:Fred" will map to
     *  "(Hfoo OR Hbar) AND Afred".
     *
     *  As of 1.0.4, you can call this method multiple times with the same
     *  value of field to allow a single field to be mapped to multiple
     *  prefixes.  Multiple terms being generated for such a field, and
     *  combined with @c Xapian::Query::OP_OR.
     *
     *  Calling this method with an empty string for @a field will cause
     *  a @c Xapian::InvalidArgumentError.
     *
     *  If you call @c add_prefix() and @c add_boolean_prefix() for the
     *  same value of @a field, a @c Xapian::InvalidOperationError exception
     *  will be thrown.
     *
     *  In 1.0.3 and earlier, subsequent calls to this method with the same
     *  value of @a field had no effect.
     *
     *  @param field   The user visible field name
     *  @param prefix  The term prefix to map this to
     *  @param grouping	Controls how multiple filters are combined - filters
     *			with the same grouping value are combined with OP_OR,
     *			then the resulting queries are combined with OP_AND.
     *			If NULL, then @a field is used for grouping.  If an
     *			empty string, then a unique grouping is created for
     *			each filter (this is sometimes useful when each
     *			document can have multiple terms with this prefix).
     *			[default: NULL]
     */
    void add_boolean_prefix(const std::string &field, const std::string &prefix,
			    const std::string* grouping = NULL);

    /** Add a boolean term prefix allowing the user to restrict a
     *  search with a boolean filter specified in the free text query.
     *
     *  This is an older version of this method - use the version with
     *  the `grouping` parameter in preference to this one.
     *
     *  @param field   The user visible field name
     *  @param prefix  The term prefix to map this to
     *  @param exclusive Controls how multiple filters are combined.  If
     *			true then @a prefix is used as the `grouping` value,
     *			so terms with the same prefix are combined with OP_OR,
     *			then the resulting queries are combined with OP_AND.
     *			If false, then a unique grouping is created for
     *			each filter (this is sometimes useful when each
     *			document can have multiple terms with this prefix).
     */
    void add_boolean_prefix(const std::string &field, const std::string &prefix,
			    bool exclusive) {
	if (exclusive) {
	    add_boolean_prefix(field, prefix);
	} else {
	    std::string empty_grouping;
	    add_boolean_prefix(field, prefix, &empty_grouping);
	}
    }

    /** Register a FieldProcessor for a boolean prefix.
     */
    void add_boolean_prefix(const std::string &field, Xapian::FieldProcessor *proc,
			    const std::string* grouping = NULL);

    /** Register a FieldProcessor for a boolean prefix.
     *
     *  This is an older version of this method - use the version with
     *  the `grouping` parameter in preference to this one.
     */
    void add_boolean_prefix(const std::string &field, Xapian::FieldProcessor *proc,
			    bool exclusive) {
	if (exclusive) {
	    add_boolean_prefix(field, proc);
	} else {
	    std::string empty_grouping;
	    add_boolean_prefix(field, proc, &empty_grouping);
	}
    }

    /// Begin iterator over terms omitted from the query as stopwords.
    TermIterator stoplist_begin() const;

    /// End iterator over terms omitted from the query as stopwords.
    TermIterator XAPIAN_NOTHROW(stoplist_end() const) {
	return TermIterator();
    }

    /// Begin iterator over unstemmed forms of the given stemmed query term.
    TermIterator unstem_begin(const std::string &term) const;

    /// End iterator over unstemmed forms of the given stemmed query term.
    TermIterator XAPIAN_NOTHROW(unstem_end(const std::string &) const) {
	return TermIterator();
    }

    /// Register a RangeProcessor.
    void add_rangeprocessor(Xapian::RangeProcessor * range_proc,
			    const std::string* grouping = NULL);

    /** Register a ValueRangeProcessor.
     *
     *  This method is provided for API compatibility with Xapian 1.2.x and is
     *  deprecated - use @a add_rangeprocessor() with a RangeProcessor instead.
     */
    XAPIAN_DEPRECATED(void add_valuerangeprocessor(Xapian::ValueRangeProcessor * vrproc)) {
#ifdef __GNUC__
// Avoid deprecation warnings if compiling without optimisation.
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
	/// Compatibility shim.
	class ShimRangeProcessor : public RangeProcessor {
	    Xapian::Internal::opt_intrusive_ptr<Xapian::ValueRangeProcessor> vrp;

	  public:
	    ShimRangeProcessor(Xapian::ValueRangeProcessor * vrp_)
		: RangeProcessor(Xapian::BAD_VALUENO), vrp(vrp_) { }

	    Xapian::Query
	    operator()(const std::string &begin, const std::string &end)
	    {
		std::string b = begin, e = end;
		slot = (*vrp)(b, e);
		if (slot == Xapian::BAD_VALUENO)
		    return Xapian::Query(Xapian::Query::OP_INVALID);
		return RangeProcessor::operator()(b, e);
	    }
	};

	add_rangeprocessor((new ShimRangeProcessor(vrproc))->release());
#ifdef __GNUC__
# pragma GCC diagnostic pop
#endif
    }

    /** Get the spelling-corrected query string.
     *
     *  This will only be set if FLAG_SPELLING_CORRECTION is specified when
     *  QueryParser::parse_query() was last called.
     *
     *  If there were no corrections, an empty string is returned.
     */
    std::string get_corrected_query_string() const;

    /// Return a string describing this object.
    std::string get_description() const;
};

inline void
QueryParser::set_max_wildcard_expansion(Xapian::termcount max_expansion)
{
    set_max_expansion(max_expansion,
		      Xapian::Query::WILDCARD_LIMIT_ERROR,
		      FLAG_WILDCARD);
}

/// @private @internal Helper for sortable_serialise().
XAPIAN_VISIBILITY_DEFAULT
size_t XAPIAN_NOTHROW(sortable_serialise_(double value, char * buf));

/** Convert a floating point number to a string, preserving sort order.
 *
 *  This method converts a floating point number to a string, suitable for
 *  using as a value for numeric range restriction, or for use as a sort
 *  key.
 *
 *  The conversion is platform independent.
 *
 *  The conversion attempts to ensure that, for any pair of values supplied
 *  to the conversion algorithm, the result of comparing the original
 *  values (with a numeric comparison operator) will be the same as the
 *  result of comparing the resulting values (with a string comparison
 *  operator).  On platforms which represent doubles with the precisions
 *  specified by IEEE_754, this will be the case: if the representation of
 *  doubles is more precise, it is possible that two very close doubles
 *  will be mapped to the same string, so will compare equal.
 *
 *  Note also that both zero and -zero will be converted to the same
 *  representation: since these compare equal, this satisfies the
 *  comparison constraint, but it's worth knowing this if you wish to use
 *  the encoding in some situation where this distinction matters.
 *
 *  Handling of NaN isn't (currently) guaranteed to be sensible.
 *
 *  @param value	The number to serialise.
 */
inline std::string sortable_serialise(double value) {
    char buf[9];
    return std::string(buf, sortable_serialise_(value, buf));
}

/** Convert a string encoded using @a sortable_serialise back to a floating
 *  point number.
 *
 *  This expects the input to be a string produced by @a sortable_serialise().
 *  If the input is not such a string, the value returned is undefined (but
 *  no error will be thrown).
 *
 *  The result of the conversion will be exactly the value which was
 *  supplied to @a sortable_serialise() when making the string on platforms
 *  which represent doubles with the precisions specified by IEEE_754, but
 *  may be a different (nearby) value on other platforms.
 *
 *  @param serialised	The serialised string to decode.
 */
XAPIAN_VISIBILITY_DEFAULT
double XAPIAN_NOTHROW(sortable_unserialise(const std::string & serialised));

}

#endif // XAPIAN_INCLUDED_QUERYPARSER_H
