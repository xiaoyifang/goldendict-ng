/** @file
 *  @brief typedefs for Xapian
 */
/* Copyright (C) 2007,2010,2011,2013,2014,2017,2018 Olly Betts
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

#ifndef XAPIAN_INCLUDED_TYPES_H
#define XAPIAN_INCLUDED_TYPES_H

#if !defined XAPIAN_IN_XAPIAN_H && !defined XAPIAN_LIB_BUILD
# error Never use <xapian/types.h> directly; include <xapian.h> instead.
#endif

#include <xapian/deprecated.h>
#include <xapian/version.h>

namespace Xapian {

/** A count of documents.
 *
 *  This is used to hold values such as the number of documents in a database
 *  and the frequency of a term in the database.
 */
typedef unsigned XAPIAN_DOCID_BASE_TYPE doccount;

/** A signed difference between two counts of documents.
 *
 *  This is used by the Xapian classes which are STL containers of documents
 *  for "difference_type".
 */
typedef XAPIAN_DOCID_BASE_TYPE doccount_diff;

/** A unique identifier for a document.
 *
 *  Docid 0 is invalid, providing an "out of range" value which can be
 *  used to mean "not a valid document".
 */
typedef unsigned XAPIAN_DOCID_BASE_TYPE docid;

/** A normalised document length.
 *
 *  The normalised document length is the document length divided by the
 *  average document length in the database.
 */
typedef double doclength;

/** The percentage score for a document in an MSet.
 *
 *  @deprecated This type is deprecated as of Xapian 1.3.0 - use the standard
 *  type int instead, which should work with older Xapian too.
 */
XAPIAN_DEPRECATED(typedef int percent);

/** A counts of terms.
 *
 *  This is used to hold values such as the Within Document Frequency (wdf).
 */
typedef unsigned XAPIAN_TERMCOUNT_BASE_TYPE termcount;

/** A signed difference between two counts of terms.
 *
 *  This is used by the Xapian classes which are STL containers of terms
 *  for "difference_type".
 */
typedef XAPIAN_TERMCOUNT_BASE_TYPE termcount_diff;

/** A term position within a document or query.
 */
typedef unsigned XAPIAN_TERMPOS_BASE_TYPE termpos;

/** A signed difference between two term positions.
 *
 *  This is used by the Xapian classes which are STL containers of positions
 *  for "difference_type".
 */
typedef XAPIAN_TERMPOS_BASE_TYPE termpos_diff; /* FIXME: can overflow. */

/** A timeout value in milliseconds.
 *
 *  There are 1000 milliseconds in a second, so for example, to set a
 *  timeout of 5 seconds use 5000.
 *
 *  @deprecated This type is deprecated as of Xapian 1.3.0 - use the standard
 *  POSIX type useconds_t instead, which should work with older Xapian too.
 */
XAPIAN_DEPRECATED(typedef unsigned timeout);

/** The number for a value slot in a document.
 *
 *  Value slot numbers are unsigned and (currently) a 32-bit quantity, with
 *  Xapian::BAD_VALUENO being represented by the largest possible value.
 *  Therefore value slots 0 to 0xFFFFFFFE are available for use.
 */
typedef unsigned valueno;

/** A signed difference between two value slot numbers.
 *
 *  This is used by the Xapian classes which are STL containers of values
 *  for "difference_type".
 */
typedef int valueno_diff; /* FIXME: can overflow. */

/** The weight of a document or term.
 *
 *  @deprecated This type is deprecated as of Xapian 1.3.0 - use the standard
 *  C++ type double instead, which should work with older Xapian too.
 */
XAPIAN_DEPRECATED(typedef double weight);

/** Reserved value to indicate "no valueno". */
const valueno BAD_VALUENO = 0xffffffff;

/** Revision number of a database.
 *
 *  For databases which support this, it increases with each commit.
 *
 *  Experimental - see https://xapian.org/docs/deprecation#experimental-features
 */
typedef XAPIAN_REVISION_TYPE rev;

/** The total length of all documents in a database.
 *
 *  Added in Xapian 1.4.5.
 */
typedef XAPIAN_TOTALLENGTH_TYPE totallength;

}

#endif /* XAPIAN_INCLUDED_TYPES_H */
