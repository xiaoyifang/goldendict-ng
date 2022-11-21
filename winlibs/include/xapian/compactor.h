/** @file
 * @brief Compact a database, or merge and compact several.
 */
/* Copyright (C) 2003,2004,2005,2006,2007,2008,2009,2010,2011,2013,2014,2015,2018 Olly Betts
 * Copyright (C) 2008 Lemur Consulting Ltd
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

#ifndef XAPIAN_INCLUDED_COMPACTOR_H
#define XAPIAN_INCLUDED_COMPACTOR_H

#if !defined XAPIAN_IN_XAPIAN_H && !defined XAPIAN_LIB_BUILD
# error Never use <xapian/compactor.h> directly; include <xapian.h> instead.
#endif

#include <xapian/constants.h>
#include <xapian/deprecated.h>
#include <xapian/intrusive_ptr.h>
#include <xapian/visibility.h>
#include <string>

namespace Xapian {

class Database;

/** Compact a database, or merge and compact several.
 */
class XAPIAN_VISIBILITY_DEFAULT Compactor {
  public:
    /// Class containing the implementation.
    class Internal;

    /** Compaction level. */
    typedef enum {
	/** Don't split items unnecessarily. */
	STANDARD = 0,
	/** Split items whenever it saves space (the default). */
	FULL = 1,
	/** Allow oversize items to save more space (not recommended if you
	 *  ever plan to update the compacted database). */
	FULLER = 2
    } compaction_level;

  private:
    /// @internal Reference counted internals.
    Xapian::Internal::intrusive_ptr<Internal> internal;

    void set_flags_(unsigned flags, unsigned mask = 0);

  public:
    Compactor();

    virtual ~Compactor();

    /** Set the block size to use for tables in the output database.
     *
     *  @param block_size	The block size to use.  Valid block sizes are
     *				currently powers of two between 2048 and 65536,
     *				with the default being 8192, but the valid
     *				sizes and default may change in the future.
     */
    XAPIAN_DEPRECATED(void set_block_size(size_t block_size));

    /** Set whether to preserve existing document id values.
     *
     *  @param renumber	The default is true, which means that document ids will
     *			be renumbered - currently by applying the same offset
     *			to all the document ids in a particular source
     *			database.
     *
     *			If false, then the document ids must be unique over all
     *			source databases.  Currently the ranges of document ids
     *			in each source must not overlap either, though this
     *			restriction may be removed in the future.
     */
    XAPIAN_DEPRECATED(void set_renumber(bool renumber)) {
	set_flags_(renumber ? 0 : DBCOMPACT_NO_RENUMBER,
		   ~unsigned(DBCOMPACT_NO_RENUMBER));
    }

    /** Set whether to merge postlists in multiple passes.
     *
     *  @param multipass	If true and merging more than 3 databases,
     *  merge the postlists in multiple passes, which is generally faster but
     *  requires more disk space for temporary files.  By default we don't do
     *  this.
     */
    XAPIAN_DEPRECATED(void set_multipass(bool multipass)) {
	set_flags_(multipass ? DBCOMPACT_MULTIPASS : 0,
		   ~unsigned(DBCOMPACT_MULTIPASS));
    }

    /** Set the compaction level.
     *
     *  @param compaction Available values are:
     *  - Xapian::Compactor::STANDARD - Don't split items unnecessarily.
     *  - Xapian::Compactor::FULL     - Split items whenever it saves space
     *    (the default).
     *  - Xapian::Compactor::FULLER   - Allow oversize items to save more space
     *    (not recommended if you ever plan to update the compacted database).
     */
    XAPIAN_DEPRECATED(void set_compaction_level(compaction_level compaction)) {
	set_flags_(compaction, ~unsigned(STANDARD|FULL|FULLER));
    }

    /** Set where to write the output.
     *
     *  @deprecated Use Database::compact(destdir[, compactor]) instead.
     *
     *  @param destdir	Output path.  This can be the same as an input if that
     *			input is a stub database (in which case the database(s)
     *			listed in the stub will be compacted to a new database
     *			and then the stub will be atomically updated to point
     *			to this new database).
     */
    XAPIAN_DEPRECATED(void set_destdir(const std::string & destdir));

    /** Add a source database.
     *
     *  @deprecated Use Database::compact(destdir[, compactor]) instead.
     *
     *  @param srcdir	The path to the source database to add.
     */
    XAPIAN_DEPRECATED(void add_source(const std::string & srcdir));

    /** Perform the actual compaction/merging operation.
     *
     *  @deprecated Use Database::compact(destdir[, compactor]) instead.
     */
    XAPIAN_DEPRECATED(void compact());

    /** Update progress.
     *
     *  Subclass this method if you want to get progress updates during
     *  compaction.  This is called for each table first with empty status,
     *  And then one or more times with non-empty status.
     *
     *  The default implementation does nothing.
     *
     *  @param table	The table currently being compacted.
     *  @param status	A status message.
     */
    virtual void
    set_status(const std::string & table, const std::string & status);

    /** Resolve multiple user metadata entries with the same key.
     *
     *  When merging, if the same user metadata key is set in more than one
     *  input, then this method is called to allow this to be resolving in
     *  an appropriate way.
     *
     *  The default implementation just returns tags[0].
     *
     *  For multipass this will currently get called multiple times for the
     *  same key if there are duplicates to resolve in each pass, but this
     *  may change in the future.
     *
     *  Since 1.4.6, an implementation of this method can return an empty
     *  string to indicate that the appropriate result is to not set a value
     *  for this user metadata key in the output database.  In older versions,
     *  you should not return an empty string.
     *
     *  @param key	The metadata key with duplicate entries.
     *  @param num_tags	How many tags there are.
     *  @param tags	An array of num_tags strings containing the tags to
     *			merge.
     */
    virtual std::string
    resolve_duplicate_metadata(const std::string & key,
			       size_t num_tags, const std::string tags[]);
};

}

#endif /* XAPIAN_INCLUDED_COMPACTOR_H */
