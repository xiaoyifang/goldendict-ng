/** @file
 * @brief Class for looking up user subclasses during unserialisation.
 */
/* Copyright 2009 Lemur Consulting Ltd
 * Copyright 2009,2011,2013,2014 Olly Betts
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

#ifndef XAPIAN_INCLUDED_REGISTRY_H
#define XAPIAN_INCLUDED_REGISTRY_H

#if !defined XAPIAN_IN_XAPIAN_H && !defined XAPIAN_LIB_BUILD
# error Never use <xapian/registry.h> directly; include <xapian.h> instead.
#endif

#include <xapian/intrusive_ptr.h>
#include <xapian/visibility.h>
#include <string>

namespace Xapian {

// Forward declarations.
class LatLongMetric;
class MatchSpy;
class PostingSource;
class Weight;

/** Registry for user subclasses.
 *
 *  This class provides a way for the remote server to look up user subclasses
 *  when unserialising.
 */
class XAPIAN_VISIBILITY_DEFAULT Registry {
  public:
    /// Class holding details of the registry.
    class Internal;

  private:
    /// @internal Reference counted internals.
    Xapian::Internal::intrusive_ptr<Internal> internal;

  public:
    /** Copy constructor.
     *
     *  The internals are reference counted, so copying is cheap.
     *
     *  @param other	The object to copy.
     */
    Registry(const Registry & other);

    /** Assignment operator.
     *
     *  The internals are reference counted, so assignment is cheap.
     *
     *  @param other	The object to copy.
     */
    Registry & operator=(const Registry & other);

#ifdef XAPIAN_MOVE_SEMANTICS
    /** Move constructor.
     *
     * @param other	The object to move.
     */
    Registry(Registry && other);

    /** Move assignment operator.
     *
     * @param other	The object to move.
     */
    Registry & operator=(Registry && other);
#endif

    /** Default constructor.
     *
     *  The registry will contain all standard subclasses of user-subclassable
     *  classes.
     */
    Registry();

    ~Registry();

    /** Register a weighting scheme.
     *
     *  @param wt	The weighting scheme to register.
     */
    void register_weighting_scheme(const Xapian::Weight &wt);

    /** Get the weighting scheme given a name.
     *
     *  @param name	The name of the weighting scheme to find.
     *  @return		An object with the requested name, or NULL if the
     *			weighting scheme could not be found.  The returned
     *			object is owned by the registry and so must not be
     *			deleted by the caller.
     */
    const Xapian::Weight *
	    get_weighting_scheme(const std::string & name) const;

    /** Register a user-defined posting source class.
     *
     *  @param source	The posting source to register.
     */
    void register_posting_source(const Xapian::PostingSource &source);

    /** Get a posting source given a name.
     *
     *  @param name	The name of the posting source to find.
     *  @return		An object with the requested name, or NULL if the
     *			posting source could not be found.  The returned
     *			object is owned by the registry and so must not be
     *			deleted by the caller.
     */
    const Xapian::PostingSource *
	    get_posting_source(const std::string & name) const;

    /** Register a user-defined match spy class.
     *
     *  @param spy	The match spy to register.
     */
    void register_match_spy(const Xapian::MatchSpy &spy);

    /** Get a match spy given a name.
     *
     *  @param name	The name of the match spy to find.
     *  @return		An object with the requested name, or NULL if the
     *			match spy could not be found.  The returned
     *			object is owned by the registry and so must not be
     *			deleted by the caller.
     */
    const Xapian::MatchSpy *
	    get_match_spy(const std::string & name) const;

    /// Register a user-defined lat-long metric class.
    void register_lat_long_metric(const Xapian::LatLongMetric &metric);

    /** Get a lat-long metric given a name.
     *
     *  The returned metric is owned by the registry object.
     *
     *  Returns NULL if the metric could not be found.
     */
    const Xapian::LatLongMetric *
	    get_lat_long_metric(const std::string & name) const;

};

}

#endif /* XAPIAN_INCLUDED_REGISTRY_H */
