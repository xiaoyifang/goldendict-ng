/** @file
 * @brief Allow rejection of terms during ESet generation.
 */
/* Copyright (C) 2007,2011,2013,2014,2015,2016 Olly Betts
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

#ifndef XAPIAN_INCLUDED_EXPANDDECIDER_H
#define XAPIAN_INCLUDED_EXPANDDECIDER_H

#if !defined XAPIAN_IN_XAPIAN_H && !defined XAPIAN_LIB_BUILD
# error Never use <xapian/expanddecider.h> directly; include <xapian.h> instead.
#endif

#include <set>
#include <string>

#include <xapian/intrusive_ptr.h>
#include <xapian/visibility.h>

namespace Xapian {

/** Virtual base class for expand decider functor. */
class XAPIAN_VISIBILITY_DEFAULT ExpandDecider
    : public Xapian::Internal::opt_intrusive_base {
    /// Don't allow assignment.
    void operator=(const ExpandDecider &);

    /// Don't allow copying.
    ExpandDecider(const ExpandDecider &);

  public:
    /// Default constructor.
    ExpandDecider() { }

    /** Do we want this term in the ESet?
     *
     *  @param term	The term to test.
     */
    virtual bool operator()(const std::string &term) const = 0;

    /** Virtual destructor, because we have virtual methods. */
    virtual ~ExpandDecider();

    /** Start reference counting this object.
     *
     *  You can hand ownership of a dynamically allocated ExpandDecider
     *  object to Xapian by calling release() and then passing the object to a
     *  Xapian method.  Xapian will arrange to delete the object once it is no
     *  longer required.
     */
    ExpandDecider * release() {
	opt_intrusive_base::release();
	return this;
    }

    /** Start reference counting this object.
     *
     *  You can hand ownership of a dynamically allocated ExpandDecider
     *  object to Xapian by calling release() and then passing the object to a
     *  Xapian method.  Xapian will arrange to delete the object once it is no
     *  longer required.
     */
    const ExpandDecider * release() const {
	opt_intrusive_base::release();
	return this;
    }
};

/** ExpandDecider subclass which rejects terms using two ExpandDeciders.
 *
 *  Terms are only accepted if they are accepted by both of the specified
 *  ExpandDecider objects.
 */
class XAPIAN_VISIBILITY_DEFAULT ExpandDeciderAnd : public ExpandDecider {
    Internal::opt_intrusive_ptr<const ExpandDecider> first, second;

  public:
    /** Terms will be checked with @a first, and if accepted, then checked
     *  with @a second.
     *
     *  @param first_	First ExpandDecider object to test with.
     *  @param second_	ExpandDecider object to test with if first_ accepts.
     */
    ExpandDeciderAnd(const ExpandDecider &first_,
		     const ExpandDecider &second_)
	: first(&first_), second(&second_) { }

    /** Compatibility method.
     *
     *  @param first_	First ExpandDecider object to test with.
     *  @param second_	ExpandDecider object to test with if first_ accepts.
     */
    ExpandDeciderAnd(const ExpandDecider *first_,
		     const ExpandDecider *second_)
	: first(first_), second(second_) { }

    virtual bool operator()(const std::string &term) const;
};

/** ExpandDecider subclass which rejects terms in a specified list.
 *
 *  ExpandDeciderFilterTerms provides an easy way to filter out terms from
 *  a fixed list when generating an ESet.
 */
class XAPIAN_VISIBILITY_DEFAULT ExpandDeciderFilterTerms : public ExpandDecider {
    std::set<std::string> rejects;

  public:
    /** The two iterators specify a list of terms to be rejected.
     *
     *  @param reject_begin	Begin iterator for the list of terms to
     *				reject.  It can be any input_iterator type
     *				which returns std::string or char * (e.g.
     *				TermIterator or char **).
     *  @param reject_end	End iterator for the list of terms to reject.
     */
    template<class Iterator>
    ExpandDeciderFilterTerms(Iterator reject_begin, Iterator reject_end)
	: rejects(reject_begin, reject_end) { }

    virtual bool operator()(const std::string &term) const;
};

/** ExpandDecider subclass which restrict terms to a particular prefix
 *
 *  ExpandDeciderFilterPrefix provides an easy way to choose terms with a
 *  particular prefix when generating an ESet.
 */
class XAPIAN_VISIBILITY_DEFAULT ExpandDeciderFilterPrefix : public ExpandDecider {
    std::string prefix;

  public:
    /** The parameter specify the prefix of terms to be retained
     *  @param prefix_   restrict terms to the particular prefix_
     */
    explicit ExpandDeciderFilterPrefix(const std::string &prefix_)
	: prefix(prefix_) { }

    virtual bool operator() (const std::string &term) const;
};

}

#endif // XAPIAN_INCLUDED_EXPANDDECIDER_H
