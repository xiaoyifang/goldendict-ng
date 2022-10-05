/** @file
 * @brief API for working with Xapian databases
 */
/* Copyright 1999,2000,2001 BrightStation PLC
 * Copyright 2002 Ananova Ltd
 * Copyright 2002,2003,2004,2005,2006,2007,2008,2009,2011,2012,2013,2014,2015,2016,2017,2019 Olly Betts
 * Copyright 2006,2008 Lemur Consulting Ltd
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

#ifndef XAPIAN_INCLUDED_DATABASE_H
#define XAPIAN_INCLUDED_DATABASE_H

#if !defined XAPIAN_IN_XAPIAN_H && !defined XAPIAN_LIB_BUILD
# error Never use <xapian/database.h> directly; include <xapian.h> instead.
#endif

#include <iosfwd>
#include <string>
#ifdef XAPIAN_MOVE_SEMANTICS
# include <utility>
#endif
#include <vector>

#include <xapian/attributes.h>
#include <xapian/deprecated.h>
#include <xapian/intrusive_ptr.h>
#include <xapian/types.h>
#include <xapian/positioniterator.h>
#include <xapian/postingiterator.h>
#include <xapian/termiterator.h>
#include <xapian/valueiterator.h>
#include <xapian/visibility.h>

namespace Xapian {

class Compactor;
class Document;

/** This class is used to access a database, or a group of databases.
 *
 *  For searching, this class is used in conjunction with an Enquire object.
 *
 *  @exception InvalidArgumentError will be thrown if an invalid
 *  argument is supplied, for example, an unknown database type.
 *
 *  @exception DatabaseOpeningError may be thrown if the database cannot
 *  be opened (for example, a required file cannot be found).
 *
 *  @exception DatabaseVersionError may be thrown if the database is in an
 *  unsupported format (for example, created by a newer version of Xapian
 *  which uses an incompatible format).
 */
class XAPIAN_VISIBILITY_DEFAULT Database {
	/// @internal Implementation behind check() static methods.
	static size_t check_(const std::string * path_ptr, int fd, int opts,
			     std::ostream *out);

	/// Internal helper behind public compact() methods.
	void compact_(const std::string * output_ptr,
		      int fd,
		      unsigned flags,
		      int block_size,
		      Xapian::Compactor * compactor) const;

    public:
	class Internal;
	/// @private @internal Reference counted internals.
	std::vector<Xapian::Internal::intrusive_ptr<Internal> > internal;

	/** Add an existing database (or group of databases) to those
	 *  accessed by this object.
	 *
	 *  @param database the database(s) to add.
	 */
	void add_database(const Database & database);

	/** Return number of shards in this Database object. */
	size_t size() const {
	    return internal.size();
	}

	/** Create a Database with no databases in.
	 */
	Database();

	/** Open a Database, automatically determining the database
	 *  backend to use.
	 *
	 * @param path directory that the database is stored in.
	 * @param flags  Bitwise-or of Xapian::DB_* constants.
	 */
	explicit Database(const std::string &path, int flags = 0);

	/** Open a single-file Database.
	 *
	 *  This method opens a single-file Database given a file descriptor
	 *  open on it.  Xapian looks starting at the current file offset,
	 *  allowing a single file database to be easily embedded within
	 *  another file.
	 *
	 * @param fd  file descriptor for the file.  Xapian takes ownership of
	 *            this and will close it when the database is closed.
	 * @param flags  Bitwise-or of Xapian::DB_* constants.
	 */
	explicit Database(int fd, int flags = 0);

	/** @private @internal Create a Database from its internals.
	 */
	explicit Database(Internal *internal);

	/** Destroy this handle on the database.
	 *
	 *  If there are no copies of this object remaining, the database(s)
	 *  will be closed.
	 */
	virtual ~Database();

	/** Copying is allowed.  The internals are reference counted, so
	 *  copying is cheap.
	 *
	 *  @param other	The object to copy.
	 */
	Database(const Database &other);

	/** Assignment is allowed.  The internals are reference counted,
	 *  so assignment is cheap.
	 *
	 *  @param other	The object to copy.
	 */
	void operator=(const Database &other);

#ifdef XAPIAN_MOVE_SEMANTICS
	/// Move constructor.
	Database(Database&& o);

	/// Move assignment operator.
	Database& operator=(Database&& o);
#endif

	/** Re-open the database.
	 *
	 *  This re-opens the database(s) to the latest available version(s).
	 *  It can be used either to make sure the latest results are returned,
	 *  or to recover from a Xapian::DatabaseModifiedError.
	 *
	 *  Calling reopen() on a database which has been closed (with @a
	 *  close()) will always raise a Xapian::DatabaseError.
	 *
	 *  @return	true if the database might have been reopened (if false
	 *		is returned, the database definitely hasn't been
	 *		reopened, which applications may find useful when
	 *		caching results, etc).  In Xapian < 1.3.0, this method
	 *		did not return a value.
	 */
	bool reopen();

	/** Close the database.
	 *
	 *  This closes the database and closes all its file handles.
	 *
	 *  For a WritableDatabase, if a transaction is active it will be
	 *  aborted, while if no transaction is active commit() will be
	 *  implicitly called.  Also the write lock is released.
	 *
	 *  Closing a database cannot be undone - in particular, calling
	 *  reopen() after close() will not reopen it, but will instead throw a
	 *  Xapian::DatabaseError exception.
	 *
	 *  Calling close() again on a database which has already been closed
	 *  has no effect (and doesn't raise an exception).
	 *
	 *  After close() has been called, calls to other methods of the
	 *  database, and to methods of other objects associated with the
	 *  database, will either:
	 *
	 *   - behave exactly as they would have done if the database had not
	 *     been closed (this can only happen if all the required data is
	 *     cached)
	 *
	 *   - raise a Xapian::DatabaseError exception indicating that the
	 *     database is closed.
	 *
	 *  The reason for this behaviour is that otherwise we'd have to check
	 *  that the database is still open on every method call on every
	 *  object associated with a Database, when in many cases they are
	 *  working on data which has already been loaded and so they are able
	 *  to just behave correctly.
	 *
	 *  This method was added in Xapian 1.1.0.
	 */
	virtual void close();

	/// Return a string describing this object.
	virtual std::string get_description() const;

	/** An iterator pointing to the start of the postlist
	 *  for a given term.
	 *
	 *  @param tname	The termname to iterate postings for.  If the
	 *			term name is the empty string, the iterator
	 *			returned will list all the documents in the
	 *			database.  Such an iterator will always return
	 *			a WDF value of 1, since there is no obvious
	 *			meaning for this quantity in this case.
	 */
	PostingIterator postlist_begin(const std::string &tname) const;

	/** Corresponding end iterator to postlist_begin().
	 */
	PostingIterator XAPIAN_NOTHROW(postlist_end(const std::string &) const) {
	    return PostingIterator();
	}

	/** An iterator pointing to the start of the termlist
	 *  for a given document.
	 *
	 *  @param did	The document id of the document to iterate terms for.
	 */
	TermIterator termlist_begin(Xapian::docid did) const;

	/** Corresponding end iterator to termlist_begin().
	 */
	TermIterator XAPIAN_NOTHROW(termlist_end(Xapian::docid) const) {
	    return TermIterator();
	}

	/** Does this database have any positional information? */
	bool has_positions() const;

	/** An iterator pointing to the start of the position list
	 *  for a given term in a given document.
	 */
	PositionIterator positionlist_begin(Xapian::docid did, const std::string &tname) const;

	/** Corresponding end iterator to positionlist_begin().
	 */
	PositionIterator XAPIAN_NOTHROW(positionlist_end(Xapian::docid, const std::string &) const) {
	    return PositionIterator();
	}

	/** An iterator which runs across all terms with a given prefix.
	 *
	 *  @param prefix The prefix to restrict the returned terms to (default:
	 *		  iterate all terms)
	 */
	TermIterator allterms_begin(const std::string & prefix = std::string()) const;

	/** Corresponding end iterator to allterms_begin(prefix).
	 */
	TermIterator XAPIAN_NOTHROW(allterms_end(const std::string & = std::string()) const) {
	    return TermIterator();
	}

	/// Get the number of documents in the database.
	Xapian::doccount get_doccount() const;

	/// Get the highest document id which has been used in the database.
	Xapian::docid get_lastdocid() const;

	/// Get the average length of the documents in the database.
	Xapian::doclength get_avlength() const;

	/** New name for get_avlength().
	 *
	 *  Added for forward compatibility with the next release series.
	 *
	 *  @since 1.4.17.
	 */
	double get_average_length() const { return get_avlength(); }

	/** Get the total length of all the documents in the database.
	 *
	 *  Added in Xapian 1.4.5.
	 */
	Xapian::totallength get_total_length() const;

	/// Get the number of documents in the database indexed by a given term.
	Xapian::doccount get_termfreq(const std::string & tname) const;

	/** Check if a given term exists in the database.
	 *
	 *  @param tname	The term to test the existence of.
	 *
	 *  @return	true if and only if the term exists in the database.
	 *		This is the same as (get_termfreq(tname) != 0), but
	 *		will often be more efficient.
	 */
	bool term_exists(const std::string & tname) const;

	/** Return the total number of occurrences of the given term.
	 *
	 *  This is the sum of the number of occurrences of the term in each
	 *  document it indexes: i.e., the sum of the within document
	 *  frequencies of the term.
	 *
	 *  @param tname  The term whose collection frequency is being
	 *  requested.
	 */
	Xapian::termcount get_collection_freq(const std::string & tname) const;

	/** Return the frequency of a given value slot.
	 *
	 *  This is the number of documents which have a (non-empty) value
	 *  stored in the slot.
	 *
	 *  @param slot The value slot to examine.
	 */
	Xapian::doccount get_value_freq(Xapian::valueno slot) const;

	/** Get a lower bound on the values stored in the given value slot.
	 *
	 *  If there are no values stored in the given value slot, this will
	 *  return an empty string.
	 *
	 *  @param slot The value slot to examine.
	 */
	std::string get_value_lower_bound(Xapian::valueno slot) const;

	/** Get an upper bound on the values stored in the given value slot.
	 *
	 *  If there are no values stored in the given value slot, this will
	 *  return an empty string.
	 *
	 *  @param slot The value slot to examine.
	 */
	std::string get_value_upper_bound(Xapian::valueno slot) const;

	/** Get a lower bound on the length of a document in this DB.
	 *
	 *  This bound does not include any zero-length documents.
	 */
	Xapian::termcount get_doclength_lower_bound() const;

	/// Get an upper bound on the length of a document in this DB.
	Xapian::termcount get_doclength_upper_bound() const;

	/// Get an upper bound on the wdf of term @a term.
	Xapian::termcount get_wdf_upper_bound(const std::string & term) const;

	/// Return an iterator over the value in slot @a slot for each document.
	ValueIterator valuestream_begin(Xapian::valueno slot) const;

	/// Return end iterator corresponding to valuestream_begin().
	ValueIterator XAPIAN_NOTHROW(valuestream_end(Xapian::valueno) const) {
	    return ValueIterator();
	}

	/// Get the length of a document.
	Xapian::termcount get_doclength(Xapian::docid did) const;

	/// Get the number of unique terms in document.
	Xapian::termcount get_unique_terms(Xapian::docid did) const;

	/** Send a "keep-alive" to remote databases to stop them timing out.
	 *
	 *  Has no effect on non-remote databases.
	 */
	void keep_alive();

	/** Get a document from the database, given its document id.
	 *
	 *  This method returns a Xapian::Document object which provides the
	 *  information about a document.
	 *
	 *  @param did   The document id of the document to retrieve.
	 *
	 *  @return      A Xapian::Document object containing the document data
	 *
	 *  @exception Xapian::DocNotFoundError      The document specified
	 *		could not be found in the database.
	 *
	 *  @exception Xapian::InvalidArgumentError  did was 0, which is not
	 *		a valid document id.
	 */
	Xapian::Document get_document(Xapian::docid did) const;

	/** Get a document from the database, given its document id.
	 *
	 *  This method returns a Xapian::Document object which provides the
	 *  information about a document.
	 *
	 *  @param did   The document id of the document to retrieve.
	 *  @param flags Zero or more flags bitwise-or-ed together (currently
	 *		 only Xapian::DOC_ASSUME_VALID is supported).
	 *
	 *  @return      A Xapian::Document object containing the document data
	 *
	 *  @exception Xapian::DocNotFoundError      The document specified
	 *		could not be found in the database.
	 *
	 *  @exception Xapian::InvalidArgumentError  did was 0, which is not
	 *		a valid document id.
	 */
	Xapian::Document get_document(Xapian::docid did, unsigned flags) const;

	/** Suggest a spelling correction.
	 *
	 *  @param word			The potentially misspelled word.
	 *  @param max_edit_distance	Only consider words which are at most
	 *	@a max_edit_distance edits from @a word.  An edit is a
	 *	character insertion, deletion, or the transposition of two
	 *	adjacent characters (default is 2).
	 */
	std::string get_spelling_suggestion(const std::string &word,
					    unsigned max_edit_distance = 2) const;

	/** An iterator which returns all the spelling correction targets.
	 *
	 *  This returns all the words which are considered as targets for the
	 *  spelling correction algorithm.  The frequency of each word is
	 *  available as the term frequency of each entry in the returned
	 *  iterator.
	 */
	Xapian::TermIterator spellings_begin() const;

	/// Corresponding end iterator to spellings_begin().
	Xapian::TermIterator XAPIAN_NOTHROW(spellings_end() const) {
	    return Xapian::TermIterator();
	}

	/** An iterator which returns all the synonyms for a given term.
	 *
	 *  @param term	    The term to return synonyms for.
	 */
	Xapian::TermIterator synonyms_begin(const std::string &term) const;

	/// Corresponding end iterator to synonyms_begin(term).
	Xapian::TermIterator XAPIAN_NOTHROW(synonyms_end(const std::string &) const) {
	    return Xapian::TermIterator();
	}

	/** An iterator which returns all terms which have synonyms.
	 *
	 *  @param prefix   If non-empty, only terms with this prefix are
	 *		    returned.
	 */
	Xapian::TermIterator synonym_keys_begin(const std::string &prefix = std::string()) const;

	/// Corresponding end iterator to synonym_keys_begin(prefix).
	Xapian::TermIterator XAPIAN_NOTHROW(synonym_keys_end(const std::string & = std::string()) const) {
	    return Xapian::TermIterator();
	}

	/** Get the user-specified metadata associated with a given key.
	 *
	 *  User-specified metadata allows you to store arbitrary information
	 *  in the form of (key, value) pairs.  See @a
	 *  WritableDatabase::set_metadata() for more information.
	 *
	 *  When invoked on a Xapian::Database object representing multiple
	 *  databases, currently only the metadata for the first is considered
	 *  but this behaviour may change in the future.
	 *
	 *  If there is no piece of metadata associated with the specified
	 *  key, an empty string is returned (this applies even for backends
	 *  which don't support metadata).
	 *
	 *  Empty keys are not valid, and specifying one will cause an
	 *  exception.
	 *
	 *  @param key The key of the metadata item to access.
	 *
	 *  @return    The retrieved metadata item's value.
	 *
	 *  @exception Xapian::InvalidArgumentError will be thrown if the
	 *	       key supplied is empty.
	 */
	std::string get_metadata(const std::string & key) const;

	/** An iterator which returns all user-specified metadata keys.
	 *
	 *  When invoked on a Xapian::Database object representing multiple
	 *  databases, currently only the metadata for the first is considered
	 *  but this behaviour may change in the future.
	 *
	 *  If the backend doesn't support metadata, then this method returns
	 *  an iterator which compares equal to that returned by
	 *  metadata_keys_end().
	 *
	 *  @param prefix   If non-empty, only keys with this prefix are
	 *		    returned.
	 *
	 *  @exception Xapian::UnimplementedError will be thrown if the
	 *			backend implements user-specified metadata, but
	 *			doesn't implement iterating its keys (currently
	 *			this happens for the InMemory backend).
	 */
	Xapian::TermIterator metadata_keys_begin(const std::string &prefix = std::string()) const;

	/// Corresponding end iterator to metadata_keys_begin().
	Xapian::TermIterator XAPIAN_NOTHROW(metadata_keys_end(const std::string & = std::string()) const) {
	    return Xapian::TermIterator();
	}

	/** Get a UUID for the database.
	 *
	 *  The UUID will persist for the lifetime of the database.
	 *
	 *  Replicas (eg, made with the replication protocol, or by copying all
	 *  the database files) will have the same UUID.  However, copies (made
	 *  with copydatabase, or xapian-compact) will have different UUIDs.
	 *
	 *  If the backend does not support UUIDs or this database has no
	 *  subdatabases, the UUID will be empty.
	 *
	 *  If this database has multiple sub-databases, the UUID string will
	 *  contain the UUIDs of all the sub-databases.
	 */
	std::string get_uuid() const;

	/** Test if this database is currently locked for writing.
	 *
	 *  If the underlying object is actually a WritableDatabase, always
	 *  returns true.
	 *
	 *  Otherwise tests if there's a writer holding the lock (or if
	 *  we can't test for a lock without taking it on the current platform,
	 *  throw Xapian::UnimplementedError).  If there's an error while
	 *  trying to test the lock, throws Xapian::DatabaseLockError.
	 *
	 *  For multi-databases, this tests each sub-database and returns
	 *  true if any of them are locked.
	 */
	bool locked() const;

	/** Get the revision of the database.
	 *
	 *  The revision is an unsigned integer which increases with each
	 *  commit.
	 *
	 *  The database must have exactly one sub-database, which must be of
	 *  type chert or glass.  Otherwise an exception will be thrown.
	 *
	 *  Experimental - see
	 *  https://xapian.org/docs/deprecation#experimental-features
	 */
	Xapian::rev get_revision() const;

	/** Check the integrity of a database or database table.
	 *
	 *  @param path	Path to database or table
	 *  @param opts	Options to use for check
	 *  @param out	std::ostream to write output to (NULL for no output)
	 */
	static size_t check(const std::string & path, int opts = 0,
			    std::ostream *out = NULL) {
	    return check_(&path, 0, opts, out);
	}

	/** Check the integrity of a single file database.
	 *
	 *  @param fd   file descriptor for the database.  The current file
	 *              offset is used, allowing checking a single file
	 *              database which is embedded within another file.  Xapian
	 *              takes ownership of the file descriptor and will close
	 *              it before returning.
	 *  @param opts	Options to use for check
	 *  @param out	std::ostream to write output to (NULL for no output)
	 */
	static size_t check(int fd, int opts = 0, std::ostream *out = NULL) {
	    return check_(NULL, fd, opts, out);
	}

	/** Produce a compact version of this database.
	 *
	 *  New 1.3.4.  Various methods of the Compactor class were deprecated
	 *  in 1.3.4.
	 *
	 *  @param output Path to write the compact version to.
	 *		  This can be the same as an input if that input is a
	 *		  stub database (in which case the database(s) listed
	 *		  in the stub will be compacted to a new database and
	 *		  then the stub will be atomically updated to point to
	 *		  this new database).
	 *
	 *  @param flags Any of the following combined using bitwise-or (| in
	 *		 C++):
	 *   - Xapian::DBCOMPACT_NO_RENUMBER By default the document ids will
	 *		be renumbered the output - currently by applying the
	 *		same offset to all the document ids in a particular
	 *		source database.  If this flag is specified, then this
	 *		renumbering doesn't happen, but all the document ids
	 *		must be unique over all source databases.  Currently
	 *		the ranges of document ids in each source must not
	 *		overlap either, though this restriction may be removed
	 *		in the future.
	 *   - Xapian::DBCOMPACT_MULTIPASS
	 *		If merging more than 3 databases, merge the postlists
	 *		in multiple passes, which is generally faster but
	 *		requires more disk space for temporary files.
	 *   - Xapian::DBCOMPACT_SINGLE_FILE
	 *		Produce a single-file database (only supported for
	 *		glass currently).
	 *   - At most one of:
	 *     - Xapian::Compactor::STANDARD - Don't split items unnecessarily.
	 *     - Xapian::Compactor::FULL     - Split items whenever it saves
	 *       space (the default).
	 *     - Xapian::Compactor::FULLER   - Allow oversize items to save
	 *       more space (not recommended if you ever plan to update the
	 *       compacted database).
	 *
	 *  @param block_size This specifies the block size (in bytes) for
	 *		to use for the output.  For glass, the block size must
	 *		be a power of 2 between 2048 and 65536 (inclusive), and
	 *		the default (also used if an invalid value is passed)
	 *		is 8192 bytes.
	 */
	void compact(const std::string & output,
		     unsigned flags = 0,
		     int block_size = 0) {
	    compact_(&output, 0, flags, block_size, NULL);
	}

	/** Produce a compact version of this database.
	 *
	 *  New 1.3.4.  Various methods of the Compactor class were deprecated
	 *  in 1.3.4.
	 *
	 *  This variant writes a single-file database to the specified file
	 *  descriptor.  Only the glass backend supports such databases, so
	 *  this form is only supported for this backend.
	 *
	 *  @param fd   File descriptor to write the compact version to.  The
	 *		descriptor needs to be readable and writable (open with
	 *		O_RDWR) and seekable.  The current file offset is used,
	 *		allowing compacting to a single file database embedded
	 *		within another file.  Xapian takes ownership of the
	 *		file descriptor and will close it before returning.
	 *
	 *  @param flags Any of the following combined using bitwise-or (| in
	 *		C++):
	 *   - Xapian::DBCOMPACT_NO_RENUMBER By default the document ids will
	 *		be renumbered the output - currently by applying the
	 *		same offset to all the document ids in a particular
	 *		source database.  If this flag is specified, then this
	 *		renumbering doesn't happen, but all the document ids
	 *		must be unique over all source databases.  Currently
	 *		the ranges of document ids in each source must not
	 *		overlap either, though this restriction may be removed
	 *		in the future.
	 *   - Xapian::DBCOMPACT_MULTIPASS
	 *		If merging more than 3 databases, merge the postlists
	 *		in multiple passes, which is generally faster but
	 *		requires more disk space for temporary files.
	 *   - Xapian::DBCOMPACT_SINGLE_FILE
	 *		Produce a single-file database (only supported for
	 *		glass currently) - this flag is implied in this form
	 *		and need not be specified explicitly.
	 *
	 *  @param block_size This specifies the block size (in bytes) for
	 *		to use for the output.  For glass, the block size must
	 *		be a power of 2 between 2048 and 65536 (inclusive), and
	 *		the default (also used if an invalid value is passed)
	 *		is 8192 bytes.
	 */
	void compact(int fd,
		     unsigned flags = 0,
		     int block_size = 0) {
	    compact_(NULL, fd, flags, block_size, NULL);
	}

	/** Produce a compact version of this database.
	 *
	 *  New 1.3.4.  Various methods of the Compactor class were deprecated
	 *  in 1.3.4.
	 *
	 *  The @a compactor functor allows handling progress output and
	 *  specifying how user metadata is merged.
	 *
	 *  @param output Path to write the compact version to.
	 *		  This can be the same as an input if that input is a
	 *		  stub database (in which case the database(s) listed
	 *		  in the stub will be compacted to a new database and
	 *		  then the stub will be atomically updated to point to
	 *		  this new database).
	 *
	 *  @param flags Any of the following combined using bitwise-or (| in
	 *		 C++):
	 *   - Xapian::DBCOMPACT_NO_RENUMBER By default the document ids will
	 *		be renumbered the output - currently by applying the
	 *		same offset to all the document ids in a particular
	 *		source database.  If this flag is specified, then this
	 *		renumbering doesn't happen, but all the document ids
	 *		must be unique over all source databases.  Currently
	 *		the ranges of document ids in each source must not
	 *		overlap either, though this restriction may be removed
	 *		in the future.
	 *   - Xapian::DBCOMPACT_MULTIPASS
	 *		If merging more than 3 databases, merge the postlists
	 *		in multiple passes, which is generally faster but
	 *		requires more disk space for temporary files.
	 *   - Xapian::DBCOMPACT_SINGLE_FILE
	 *		Produce a single-file database (only supported for
	 *		glass currently).
	 *
	 *  @param block_size This specifies the block size (in bytes) for
	 *		to use for the output.  For glass, the block size must
	 *		be a power of 2 between 2048 and 65536 (inclusive), and
	 *		the default (also used if an invalid value is passed)
	 *		is 8192 bytes.
	 *
	 *  @param compactor Functor
	 */
	void compact(const std::string & output,
		     unsigned flags,
		     int block_size,
		     Xapian::Compactor & compactor)
	{
	    compact_(&output, 0, flags, block_size, &compactor);
	}

	/** Produce a compact version of this database.
	 *
	 *  New 1.3.4.  Various methods of the Compactor class were deprecated
	 *  in 1.3.4.
	 *
	 *  The @a compactor functor allows handling progress output and
	 *  specifying how user metadata is merged.
	 *
	 *  This variant writes a single-file database to the specified file
	 *  descriptor.  Only the glass backend supports such databases, so
	 *  this form is only supported for this backend.
	 *
	 *  @param fd   File descriptor to write the compact version to.  The
	 *		descriptor needs to be readable and writable (open with
	 *		O_RDWR) and seekable.  The current file offset is used,
	 *		allowing compacting to a single file database embedded
	 *		within another file.  Xapian takes ownership of the
	 *		file descriptor and will close it before returning.
	 *
	 *  @param flags Any of the following combined using bitwise-or (| in
	 *		 C++):
	 *   - Xapian::DBCOMPACT_NO_RENUMBER By default the document ids will
	 *		be renumbered the output - currently by applying the
	 *		same offset to all the document ids in a particular
	 *		source database.  If this flag is specified, then this
	 *		renumbering doesn't happen, but all the document ids
	 *		must be unique over all source databases.  Currently
	 *		the ranges of document ids in each source must not
	 *		overlap either, though this restriction may be removed
	 *		in the future.
	 *   - Xapian::DBCOMPACT_MULTIPASS
	 *		If merging more than 3 databases, merge the postlists
	 *		in multiple passes, which is generally faster but
	 *		requires more disk space for temporary files.
	 *   - Xapian::DBCOMPACT_SINGLE_FILE
	 *		Produce a single-file database (only supported for
	 *		glass currently) - this flag is implied in this form
	 *		and need not be specified explicitly.
	 *
	 *  @param block_size This specifies the block size (in bytes) for
	 *		to use for the output.  For glass, the block size must
	 *		be a power of 2 between 2048 and 65536 (inclusive), and
	 *		the default (also used if an invalid value is passed)
	 *		is 8192 bytes.
	 *
	 *  @param compactor Functor
	 */
	void compact(int fd,
		     unsigned flags,
		     int block_size,
		     Xapian::Compactor & compactor)
	{
	    compact_(NULL, fd, flags, block_size, &compactor);
	}
};

/** This class provides read/write access to a database.
 */
class XAPIAN_VISIBILITY_DEFAULT WritableDatabase : public Database {
    public:
	/** Destroy this handle on the database.
	 *
	 *  If no other handles to this database remain, the database will be
	 *  closed.
	 *
	 *  If a transaction is active cancel_transaction() will be implicitly
	 *  called; if no transaction is active commit() will be implicitly
	 *  called, but any exception will be swallowed (because throwing
	 *  exceptions in C++ destructors is problematic).  If you aren't using
	 *  transactions and want to know about any failure to commit changes,
	 *  call commit() explicitly before the destructor gets called.
	 */
	virtual ~WritableDatabase();

	/** Create a WritableDatabase with no subdatabases.
	 *
	 *  The created object isn't very useful in this state - it's intended
	 *  as a placeholder value.
	 */
	WritableDatabase();

	/** Open a database for update, automatically determining the database
	 *  backend to use.
	 *
	 *  If the database is to be created, Xapian will try
	 *  to create the directory indicated by path if it doesn't already
	 *  exist (but only the leaf directory, not recursively).
	 *
	 * @param path directory that the database is stored in.
	 * @param flags one of:
	 *  - Xapian::DB_CREATE_OR_OPEN open for read/write; create if no db
	 *    exists (the default if flags isn't specified)
	 *  - Xapian::DB_CREATE create new database; fail if db exists
	 *  - Xapian::DB_CREATE_OR_OVERWRITE overwrite existing db; create if
	 *    none exists
	 *  - Xapian::DB_OPEN open for read/write; fail if no db exists
	 *
	 *  Additionally, the following flags can be combined with action
	 *  using bitwise-or (| in C++):
	 *
	 *   - Xapian::DB_NO_SYNC don't call fsync() or similar
	 *   - Xapian::DB_FULL_SYNC try harder to ensure data is safe
	 *   - Xapian::DB_DANGEROUS don't be crash-safe, no concurrent readers
	 *   - Xapian::DB_NO_TERMLIST don't use a termlist table
	 *   - Xapian::DB_RETRY_LOCK to wait to get a write lock
	 *
	 *  @param block_size If a new database is created, this specifies
	 *		      the block size (in bytes) for backends which
	 *		      have such a concept.  For chert and glass, the
	 *		      block size must be a power of 2 between 2048 and
	 *		      65536 (inclusive), and the default (also used if
	 *		      an invalid value is passed) is 8192 bytes.
	 *
	 *  @exception Xapian::DatabaseCorruptError will be thrown if the
	 *             database is in a corrupt state.
	 *
	 *  @exception Xapian::DatabaseLockError will be thrown if a lock
	 *	       couldn't be acquired on the database.
	 */
	explicit WritableDatabase(const std::string &path,
				  int flags = 0,
				  int block_size = 0);

	/** @private @internal Create an WritableDatabase given its internals.
	 */
	explicit WritableDatabase(Database::Internal *internal);

	/** Copying is allowed.  The internals are reference counted, so
	 *  copying is cheap.
	 *
	 *  @param other	The object to copy.
	 */
	WritableDatabase(const WritableDatabase &other);

	/** Assignment is allowed.  The internals are reference counted,
	 *  so assignment is cheap.
	 *
	 *  Note that only an WritableDatabase may be assigned to an
	 *  WritableDatabase: an attempt to assign a Database is caught
	 *  at compile-time.
	 *
	 *  @param other	The object to copy.
	 */
	void operator=(const WritableDatabase &other);

#ifdef XAPIAN_MOVE_SEMANTICS
	/// Move constructor.
	WritableDatabase(WritableDatabase&& o) : Database(std::move(o)) {}

	/// Move assignment operator.
	WritableDatabase& operator=(WritableDatabase&& o) {
	    Database::operator=(std::move(o));
	    return *this;
	}
#endif

	/** Add shards from another WritableDatabase.
	 *
	 *  Any shards in @a other are added to the list of shards in this
	 *  object.  The shards are reference counted and also remain in
	 *  @a other.
	 *
	 *  @param other Another WritableDatabase object to add shards from
	 */
	void add_database(const WritableDatabase& other) {
	    // This method is provided mainly so that adding a Database to a
	    // WritableDatabase is a compile-time error - prior to 1.4.19, it
	    // would essentially act as a "black-hole" shard which discarded
	    // any changes made to it.
	    Database::add_database(other);
	}

	/** Commit any pending modifications made to the database.
	 *
	 *  For efficiency reasons, when performing multiple updates to a
	 *  database it is best (indeed, almost essential) to make as many
	 *  modifications as memory will permit in a single pass through
	 *  the database.  To ensure this, Xapian batches up modifications.
	 *
	 *  This method may be called at any time to commit any pending
	 *  modifications to the database.
	 *
	 *  If any of the modifications fail, an exception will be thrown and
	 *  the database will be left in a state in which each separate
	 *  addition, replacement or deletion operation has either been fully
	 *  performed or not performed at all: it is then up to the
	 *  application to work out which operations need to be repeated.
	 *
	 *  It's not valid to call commit() within a transaction.
	 *
	 *  Beware of calling commit() too frequently: this will make indexing
	 *  take much longer.
	 *
	 *  Note that commit() need not be called explicitly: it will be called
	 *  automatically when the database is closed, or when a sufficient
	 *  number of modifications have been made.  By default, this is every
	 *  10000 documents added, deleted, or modified.  This value is rather
	 *  conservative, and if you have a machine with plenty of memory,
	 *  you can improve indexing throughput dramatically by setting
	 *  XAPIAN_FLUSH_THRESHOLD in the environment to a larger value.
	 *
	 *  This method was new in Xapian 1.1.0 - in earlier versions it was
	 *  called flush().
	 *
	 *  @exception Xapian::DatabaseError will be thrown if a problem occurs
	 *             while modifying the database.
	 *
	 *  @exception Xapian::DatabaseCorruptError will be thrown if the
	 *             database is in a corrupt state.
	 */
	void commit();

	/** Pre-1.1.0 name for commit().
	 *
	 *  Use commit() instead.
	 */
	XAPIAN_DEPRECATED(void flush()) { commit(); }

	/** Begin a transaction.
	 *
	 *  In Xapian a transaction is a group of modifications to the database
	 *  which are linked such that either all will be applied
	 *  simultaneously or none will be applied at all.  Even in the case of
	 *  a power failure, this characteristic should be preserved (as long
	 *  as the filesystem isn't corrupted, etc).
	 *
	 *  A transaction is started with begin_transaction() and can
	 *  either be committed by calling commit_transaction() or aborted
	 *  by calling cancel_transaction().
	 *
	 *  By default, a transaction implicitly calls commit() before and
	 *  after so that the modifications stand and fall without affecting
	 *  modifications before or after.
	 *
	 *  The downside of these implicit calls to commit() is that small
	 *  transactions can harm indexing performance in the same way that
	 *  explicitly calling commit() frequently can.
	 *
	 *  If you're applying atomic groups of changes and only wish to
	 *  ensure that each group is either applied or not applied, then
	 *  you can prevent the automatic commit() before and after the
	 *  transaction by starting the transaction with
	 *  begin_transaction(false).  However, if cancel_transaction is
	 *  called (or if commit_transaction isn't called before the
	 *  WritableDatabase object is destroyed) then any changes which
	 *  were pending before the transaction began will also be discarded.
	 *
	 *  Transactions aren't currently supported by the InMemory backend.
	 *
	 *  @param flushed	Is this a flushed transaction?  By default
	 *			transactions are "flushed", which means that
	 *			committing a transaction will ensure those
	 *			changes are permanently written to the
	 *			database.  By contrast, unflushed transactions
	 *			only ensure that changes within the transaction
	 *			are either all applied or all aren't.
	 *
	 *  @exception Xapian::UnimplementedError will be thrown if transactions
	 *             are not available for this database type.
	 *
	 *  @exception Xapian::InvalidOperationError will be thrown if this is
	 *             called at an invalid time, such as when a transaction
	 *             is already in progress.
	 */
	void begin_transaction(bool flushed = true);

	/** Complete the transaction currently in progress.
	 *
	 *  If this method completes successfully and this is a flushed
	 *  transaction, all the database modifications
	 *  made during the transaction will have been committed to the
	 *  database.
	 *
	 *  If an error occurs, an exception will be thrown, and none of
	 *  the modifications made to the database during the transaction
	 *  will have been applied to the database.
	 *
	 *  In all cases the transaction will no longer be in progress.
	 *
	 *  @exception Xapian::DatabaseError will be thrown if a problem occurs
	 *             while modifying the database.
	 *
	 *  @exception Xapian::DatabaseCorruptError will be thrown if the
	 *             database is in a corrupt state.
	 *
	 *  @exception Xapian::InvalidOperationError will be thrown if a
	 *	       transaction is not currently in progress.
	 *
	 *  @exception Xapian::UnimplementedError will be thrown if transactions
	 *             are not available for this database type.
	 */
	void commit_transaction();

	/** Abort the transaction currently in progress, discarding the
	 *  pending modifications made to the database.
	 *
	 *  If an error occurs in this method, an exception will be thrown,
	 *  but the transaction will be cancelled anyway.
	 *
	 *  @exception Xapian::DatabaseError will be thrown if a problem occurs
	 *             while modifying the database.
	 *
	 *  @exception Xapian::DatabaseCorruptError will be thrown if the
	 *             database is in a corrupt state.
	 *
	 *  @exception Xapian::InvalidOperationError will be thrown if a
	 *	       transaction is not currently in progress.
	 *
	 *  @exception Xapian::UnimplementedError will be thrown if transactions
	 *             are not available for this database type.
	 */
	void cancel_transaction();

	/** Add a new document to the database.
	 *
	 *  This method adds the specified document to the database,
	 *  returning a newly allocated document ID.  Automatically allocated
	 *  document IDs come from a per-database monotonically increasing
	 *  counter, so IDs from deleted documents won't be reused.
	 *
	 *  If you want to specify the document ID to be used, you should
	 *  call replace_document() instead.
	 *
	 *  Note that changes to the database won't be immediately committed to
	 *  disk; see commit() for more details.
	 *
	 *  As with all database modification operations, the effect is
	 *  atomic: the document will either be fully added, or the document
	 *  fails to be added and an exception is thrown (possibly at a
	 *  later time when commit() is called or the database is closed).
	 *
	 *  @param document The new document to be added.
	 *
	 *  @return         The document ID of the newly added document.
	 *
	 *  @exception Xapian::DatabaseError will be thrown if a problem occurs
	 *             while writing to the database.
	 *
	 *  @exception Xapian::DatabaseCorruptError will be thrown if the
	 *             database is in a corrupt state.
	 */
	Xapian::docid add_document(const Xapian::Document & document);

	/** Delete a document from the database.
	 *
	 *  This method removes the document with the specified document ID
	 *  from the database.
	 *
	 *  Note that changes to the database won't be immediately committed to
	 *  disk; see commit() for more details.
	 *
	 *  As with all database modification operations, the effect is
	 *  atomic: the document will either be fully removed, or the document
	 *  fails to be removed and an exception is thrown (possibly at a
	 *  later time when commit() is called or the database is closed).
	 *
	 *  @param did     The document ID of the document to be removed.
	 *
	 *  @exception Xapian::DatabaseError will be thrown if a problem occurs
	 *             while writing to the database.
	 *
	 *  @exception Xapian::DatabaseCorruptError will be thrown if the
	 *             database is in a corrupt state.
	 */
	void delete_document(Xapian::docid did);

	/** Delete any documents indexed by a term from the database.
	 *
	 *  This method removes any documents indexed by the specified term
	 *  from the database.
	 *
	 *  A major use is for convenience when UIDs from another system are
	 *  mapped to terms in Xapian, although this method has other uses
	 *  (for example, you could add a "deletion date" term to documents at
	 *  index time and use this method to delete all documents due for
	 *  deletion on a particular date).
	 *
	 *  @param unique_term     The term to remove references to.
	 *
	 *  @exception Xapian::DatabaseError will be thrown if a problem occurs
	 *             while writing to the database.
	 *
	 *  @exception Xapian::DatabaseCorruptError will be thrown if the
	 *             database is in a corrupt state.
	 */
	void delete_document(const std::string & unique_term);

	/** Replace a given document in the database.
	 *
	 *  This method replaces the document with the specified document ID.
	 *  If document ID @a did isn't currently used, the document will be
	 *  added with document ID @a did.
	 *
	 *  The monotonic counter used for automatically allocating document
	 *  IDs is increased so that the next automatically allocated document
	 *  ID will be did + 1.  Be aware that if you use this method to
	 *  specify a high document ID for a new document, and also use
	 *  WritableDatabase::add_document(), Xapian may get to a state where
	 *  this counter wraps around and will be unable to automatically
	 *  allocate document IDs!
	 *
	 *  Note that changes to the database won't be immediately committed to
	 *  disk; see commit() for more details.
	 *
	 *  As with all database modification operations, the effect is
	 *  atomic: the document will either be fully replaced, or the document
	 *  fails to be replaced and an exception is thrown (possibly at a
	 *  later time when commit() is called or the database is closed).
	 *
	 *  @param did     The document ID of the document to be replaced.
	 *  @param document The new document.
	 *
	 *  @exception Xapian::DatabaseError will be thrown if a problem occurs
	 *             while writing to the database.
	 *
	 *  @exception Xapian::DatabaseCorruptError will be thrown if the
	 *             database is in a corrupt state.
	 */
	void replace_document(Xapian::docid did,
			      const Xapian::Document & document);

	/** Replace any documents matching a term.
	 *
	 *  This method replaces any documents indexed by the specified term
	 *  with the specified document.  If any documents are indexed by the
	 *  term, the lowest document ID will be used for the document,
	 *  otherwise a new document ID will be generated as for add_document.
	 *
	 *  One common use is to allow UIDs from another system to easily be
	 *  mapped to terms in Xapian.  Note that this method doesn't
	 *  automatically add unique_term as a term, so you'll need to call
	 *  document.add_term(unique_term) first when using replace_document()
	 *  in this way.
	 *
	 *  Note that changes to the database won't be immediately committed to
	 *  disk; see commit() for more details.
	 *
	 *  As with all database modification operations, the effect is
	 *  atomic: the document(s) will either be fully replaced, or the
	 *  document(s) fail to be replaced and an exception is thrown
	 *  (possibly at a
	 *  later time when commit() is called or the database is closed).
	 *
	 *  @param unique_term    The "unique" term.
	 *  @param document The new document.
	 *
	 *  @return         The document ID that document was given.
	 *
	 *  @exception Xapian::DatabaseError will be thrown if a problem occurs
	 *             while writing to the database.
	 *
	 *  @exception Xapian::DatabaseCorruptError will be thrown if the
	 *             database is in a corrupt state.
	 */
	Xapian::docid replace_document(const std::string & unique_term,
				       const Xapian::Document & document);

	/** Add a word to the spelling dictionary.
	 *
	 *  If the word is already present, its frequency is increased.
	 *
	 *  @param word	    The word to add.
	 *  @param freqinc  How much to increase its frequency by (default 1).
	 */
	void add_spelling(const std::string & word,
			  Xapian::termcount freqinc = 1) const;

	/** Remove a word from the spelling dictionary.
	 *
	 *  The word's frequency is decreased, and if would become zero or less
	 *  then the word is removed completely.
	 *
	 *  @param word	    The word to remove.
	 *  @param freqdec  How much to decrease its frequency by (default 1).
	 */
	void remove_spelling(const std::string & word,
			     Xapian::termcount freqdec = 1) const;

	/** Add a synonym for a term.
	 *
	 *  @param term		The term to add a synonym for.
	 *  @param synonym	The synonym to add.  If this is already a
	 *			synonym for @a term, then no action is taken.
	 */
	void add_synonym(const std::string & term,
			 const std::string & synonym) const;

	/** Remove a synonym for a term.
	 *
	 *  @param term		The term to remove a synonym for.
	 *  @param synonym	The synonym to remove.  If this isn't currently
	 *			a synonym for @a term, then no action is taken.
	 */
	void remove_synonym(const std::string & term,
			    const std::string & synonym) const;

	/** Remove all synonyms for a term.
	 *
	 *  @param term		The term to remove all synonyms for.  If the
	 *			term has no synonyms, no action is taken.
	 */
	void clear_synonyms(const std::string & term) const;

	/** Set the user-specified metadata associated with a given key.
	 *
	 *  This method sets the metadata value associated with a given key.
	 *  If there is already a metadata value stored in the database with
	 *  the same key, the old value is replaced.  If you want to delete an
	 *  existing item of metadata, just set its value to the empty string.
	 *
	 *  User-specified metadata allows you to store arbitrary information
	 *  in the form of (key, value) pairs.
	 *
	 *  There's no hard limit on the number of metadata items, or the size
	 *  of the metadata values.  Metadata keys have a limited length, which
	 *  depend on the backend.  We recommend limiting them to 200 bytes.
	 *  Empty keys are not valid, and specifying one will cause an
	 *  exception.
	 *
	 *  Metadata modifications are committed to disk in the same way as
	 *  modifications to the documents in the database are: i.e.,
	 *  modifications are atomic, and won't be committed to disk
	 *  immediately (see commit() for more details).  This allows metadata
	 *  to be used to link databases with versioned external resources
	 *  by storing the appropriate version number in a metadata item.
	 *
	 *  You can also use the metadata to store arbitrary extra information
	 *  associated with terms, documents, or postings by encoding the
	 *  termname and/or document id into the metadata key.
	 *
	 *  @param key       The key of the metadata item to set.
	 *
	 *  @param metadata  The value of the metadata item to set.
	 *
	 *  @exception Xapian::DatabaseError will be thrown if a problem occurs
	 *             while writing to the database.
	 *
	 *  @exception Xapian::DatabaseCorruptError will be thrown if the
	 *             database is in a corrupt state.
	 *
	 *  @exception Xapian::InvalidArgumentError will be thrown if the
	 *             key supplied is empty.
	 *
	 *  @exception Xapian::UnimplementedError will be thrown if the
	 *             database backend in use doesn't support user-specified
	 *             metadata.
	 */
	void set_metadata(const std::string & key, const std::string & metadata);

	/// Return a string describing this object.
	std::string get_description() const;
};

}

#endif /* XAPIAN_INCLUDED_DATABASE_H */
