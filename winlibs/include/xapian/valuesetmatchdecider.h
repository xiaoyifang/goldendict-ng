/** @file
 * @brief MatchDecider subclass for filtering results by value.
 */
/* Copyright 2008 Lemur Consulting Ltd
 * Copyright 2008,2009,2011,2013,2014 Olly Betts
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

#ifndef XAPIAN_INCLUDED_VALUESETMATCHDECIDER_H
#define XAPIAN_INCLUDED_VALUESETMATCHDECIDER_H

#if !defined XAPIAN_IN_XAPIAN_H && !defined XAPIAN_LIB_BUILD
# error Never use <xapian/valuesetmatchdecider.h> directly; include <xapian.h> instead.
#endif

#include <xapian/enquire.h>
#include <xapian/types.h>
#include <xapian/visibility.h>

#include <string>
#include <set>

namespace Xapian {

class Document;

/** MatchDecider filtering results based on whether document values are in a
 *  user-defined set.
 */
class XAPIAN_VISIBILITY_DEFAULT ValueSetMatchDecider : public MatchDecider {
    /** Set of values to test for. */
    std::set<std::string> testset;

    /** The value slot to look in. */
    valueno valuenum;

    /** Whether to include or exclude documents with the specified values.
     *
     *  If true, documents with a value in the set are returned.
     *  If false, documents with a value not in the set are returned.
     */
    bool inclusive;

  public:
    /** Construct a ValueSetMatchDecider.
     *
     *  @param slot The value slot number to look in.
     *
     *  @param inclusive_ If true, match decider accepts documents which have a
     *  value in the specified slot which is a member of the test set; if
     *  false, match decider accepts documents which do not have a value in the
     *  specified slot.
     */
    ValueSetMatchDecider(Xapian::valueno slot, bool inclusive_)
	: valuenum(slot), inclusive(inclusive_) { }

    /** Add a value to the test set.
     *
     *  @param value The value to add to the test set.
     */
    void add_value(const std::string& value)
    {
	testset.insert(value);
    }

    /** Remove a value from the test set.
     *
     *  @param value The value to remove from the test set.
     */
    void remove_value(const std::string& value)
    {
	testset.erase(value);
    }

    /** Decide whether we want a particular document to be in the MSet.
     *
     *  @param doc	The document to test.
     *  @return		true if the document is acceptable, or false if the
     *			document should be excluded from the MSet.
     */
    bool operator()(const Xapian::Document& doc) const;
};

}

#endif /* XAPIAN_INCLUDED_VALUESETMATCHDECIDER_H */
