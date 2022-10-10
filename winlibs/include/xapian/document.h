/** @file
 * @brief API for working with documents
 */
/* Copyright 1999,2000,2001 BrightStation PLC
 * Copyright 2002 Ananova Ltd
 * Copyright 2002,2003,2004,2006,2007,2009,2010,2011,2012,2013,2014,2018 Olly Betts
 * Copyright 2009 Lemur Consulting Ltd
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

#ifndef XAPIAN_INCLUDED_DOCUMENT_H
#define XAPIAN_INCLUDED_DOCUMENT_H

#if !defined XAPIAN_IN_XAPIAN_H && !defined XAPIAN_LIB_BUILD
# error Never use <xapian/document.h> directly; include <xapian.h> instead.
#endif

#include <string>

#include <xapian/attributes.h>
#include <xapian/intrusive_ptr.h>
#include <xapian/types.h>
#include <xapian/termiterator.h>
#include <xapian/valueiterator.h>
#include <xapian/visibility.h>

namespace Xapian {

/** A handle representing a document in a Xapian database.
 *
 *  The Document class fetches information from the database lazily.  Usually
 *  this behaviour isn't visible to users (except for the speed benefits), but
 *  if the document in the database is modified or deleted, then preexisting
 *  Document objects may return the old or new versions of data (or throw
 *  Xapian::DocNotFoundError in the case of deletion).
 *
 *  Since Database objects work on a snapshot of the database's state, the
 *  situation above can only happen with a WritableDatabase object, or if
 *  you call Database::reopen() on a Database object.
 *
 *  We recommend you avoid designs where this behaviour is an issue, but if
 *  you need a way to make a non-lazy version of a Document object, you can do
 *  this like so:
 *
 *      doc = Xapian::Document::unserialise(doc.serialise());
 */
class XAPIAN_VISIBILITY_DEFAULT Document {
    public:
	class Internal;
	/// @private @internal Reference counted internals.
	Xapian::Internal::intrusive_ptr<Internal> internal;

	/** @private @internal Constructor is only used by internal classes.
	 *
	 *  @param internal_ pointer to internal opaque class
	 */
	explicit Document(Internal *internal_);

	/** Copying is allowed.  The internals are reference counted, so
	 *  copying is cheap.
	 *
	 *  @param other	The object to copy.
	 */
	Document(const Document &other);

	/** Assignment is allowed.  The internals are reference counted,
	 *  so assignment is cheap.
	 *
	 *  @param other	The object to copy.
	 */
	void operator=(const Document &other);

#ifdef XAPIAN_MOVE_SEMANTICS
	/// Move constructor.
	Document(Document&& o);

	/// Move assignment operator.
	Document& operator=(Document&& o);
#endif

	/// Make a new empty Document
	Document();

	/// Destructor
	~Document();

	/** Get value by number.
	 *
	 *  Returns an empty string if no value with the given number is present
	 *  in the document.
	 *
	 *  @param slot The number of the value.
	 */
	std::string get_value(Xapian::valueno slot) const;

	/** Add a new value.
	 *
	 *  The new value will replace any existing value with the same number
	 *  (or if the new value is empty, it will remove any existing value
	 *  with the same number).
	 *
	 *  @param slot		The value slot to add the value in.
	 *  @param value	The value to set.
	 */
	void add_value(Xapian::valueno slot, const std::string &value);

	/// Remove any value with the given number.
	void remove_value(Xapian::valueno slot);

	/// Remove all values associated with the document.
	void clear_values();

	/** Get data stored in the document.
	 *
	 *  This is potentially a relatively expensive operation, and shouldn't
	 *  normally be used during the match (e.g. in a PostingSource or match
	 *  decider functor.  Put data for use by match deciders in a value
	 *  instead.
	 */
	std::string get_data() const;

	/** Set data stored in the document.
	 *
	 *  Xapian treats the data as an opaque blob.  It may try to compress
	 *  it, but other than that it will just store it and return it when
	 *  requested.
	 *
	 *  @param data	The data to store.
	 */
	void set_data(const std::string &data);

	/** Add an occurrence of a term at a particular position.
	 *
	 *  Multiple occurrences of the term at the same position are
	 *  represented only once in the positional information, but do
	 *  increase the wdf.
	 *
	 *  If the term is not already in the document, it will be added to
	 *  it.
	 *
	 *  @param tname     The name of the term.
	 *  @param tpos      The position of the term.
	 *  @param wdfinc    The increment that will be applied to the wdf
	 *                   for this term.
	 */
	void add_posting(const std::string & tname,
			 Xapian::termpos tpos,
			 Xapian::termcount wdfinc = 1);

	/** Add a term to the document, without positional information.
	 *
	 *  Any existing positional information for the term will be left
	 *  unmodified.
	 *
	 *  @param tname     The name of the term.
	 *  @param wdfinc    The increment that will be applied to the wdf
	 *                   for this term (default: 1).
	 */
	void add_term(const std::string & tname, Xapian::termcount wdfinc = 1);

	/** Add a boolean filter term to the document.
	 *
	 *  This method adds @a term to the document with wdf of 0 -
	 *  this is generally what you want for a term used for boolean
	 *  filtering as the wdf of such terms is ignored, and it doesn't
	 *  make sense for them to contribute to the document's length.
	 *
	 *  If the specified term already indexes this document, this method
	 *  has no effect.
	 *
	 *  It is exactly the same as add_term(term, 0).
	 *
	 *  This method was added in Xapian 1.0.18.
	 *
	 *  @param term		The term to add.
	 */
	void add_boolean_term(const std::string & term) { add_term(term, 0); }

	/** Remove a posting of a term from the document.
	 *
	 *  Note that the term will still index the document even if all
	 *  occurrences are removed.  To remove a term from a document
	 *  completely, use remove_term().
	 *
	 *  @param tname     The name of the term.
	 *  @param tpos      The position of the term.
	 *  @param wdfdec    The decrement that will be applied to the wdf
	 *                   when removing this posting.  The wdf will not go
	 *                   below the value of 0.
	 *
	 *  @exception Xapian::InvalidArgumentError will be thrown if the term
	 *  is not at the position specified in the position list for this term
	 *  in this document.
	 *
	 *  @exception Xapian::InvalidArgumentError will be thrown if the term
	 *  is not in the document
	 */
	void remove_posting(const std::string & tname,
			    Xapian::termpos tpos,
			    Xapian::termcount wdfdec = 1);

	/** Remove a range of postings for a term.
	 *
	 *  Any instances of the term at positions >= @a term_pos_first and
	 *  <= @a term_pos_last will be removed, and the wdf reduced by
	 *  @a wdf_dec for each instance removed (the wdf will not ever go
	 *  below zero though).
	 *
	 *  It's OK if the term doesn't occur in the range of positions
	 *  specified (unlike @a remove_posting()).  And if
	 *  term_pos_first > term_pos_last, this method does nothing.
	 *
	 *  @return The number of postings removed.
	 *
	 *  @exception Xapian::InvalidArgumentError will be thrown if the term
	 *  is not in the document
	 *
	 *  @since Added in Xapian 1.4.8.
	 */
	Xapian::termpos remove_postings(const std::string& term,
					Xapian::termpos term_pos_first,
					Xapian::termpos term_pos_last,
					Xapian::termcount wdf_dec = 1);

	/** Remove a term and all postings associated with it.
	 *
	 *  @param tname  The name of the term.
	 *
	 *  @exception Xapian::InvalidArgumentError will be thrown if the term
	 *  is not in the document
	 */
	void remove_term(const std::string & tname);

	/// Remove all terms (and postings) from the document.
	void clear_terms();

	/** The length of the termlist - i.e. the number of different terms
	 *  which index this document.
	 */
	Xapian::termcount termlist_count() const;

	/// Iterator for the terms in this document.
	TermIterator termlist_begin() const;

	/// Equivalent end iterator for termlist_begin().
	TermIterator XAPIAN_NOTHROW(termlist_end() const) {
	    return TermIterator();
	}

	/// Count the values in this document.
	Xapian::termcount values_count() const;

	/// Iterator for the values in this document.
	ValueIterator values_begin() const;

	/// Equivalent end iterator for values_begin().
	ValueIterator XAPIAN_NOTHROW(values_end() const) {
	    return ValueIterator();
	}

	/** Get the document id which is associated with this document (if any).
	 *
	 *  NB If multiple databases are being searched together, then this
	 *  will be the document id in the individual database, not the merged
	 *  database!
	 *
	 *  @return If this document came from a database, return the document
	 *	    id in that database.  Otherwise, return 0 (in Xapian
	 *	    1.0.22/1.2.4 or later; prior to this the returned value was
	 *	    uninitialised).
	 */
	docid get_docid() const;

	/** Serialise document into a string.
	 *
	 *  The document representation may change between Xapian releases:
	 *  even between minor versions.  However, it is guaranteed not to
	 *  change if the remote database protocol has not changed between
	 *  releases.
	 */
	std::string serialise() const;

	/** Unserialise a document from a string produced by serialise().
	 */
	static Document unserialise(const std::string &serialised);

	/// Return a string describing this object.
	std::string get_description() const;
};

}

#endif // XAPIAN_INCLUDED_DOCUMENT_H
