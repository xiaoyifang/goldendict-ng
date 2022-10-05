/** @file
 *  @brief Functions to assist creating language-idiomatic iterator wrappers.
 */
/* Copyright (C) 2014,2016,2017,2019 Olly Betts
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

#ifndef XAPIAN_INCLUDED_ITERATOR_H
#define XAPIAN_INCLUDED_ITERATOR_H

/* These functions are not intended to be used from user C++ code - they are
 * provided to make implementing language-idiomatic wrappers around Xapian's
 * iterator classes easier in bindings for other languages.
 *
 * If you make use of this API, please let us know on xapian-devel so we can
 * coordinate any incompatible changes which might be required in the future.
 *
 * Currently known users:
 *
 *  * xapian-bindings:
 *    + Java
 *    + PHP
 *  * xapian-core:
 *    + examples/xapian-pos.cc
 *  * xapian-glib
 */

#include <xapian.h>

namespace Xapian {

/** @internal Determine if iterator is valid to dereference. */
inline bool iterator_valid(const Xapian::ESetIterator& it) {
    return it.off_from_end != 0;
}

/** @internal Determine if iterator is valid to dereference. */
inline bool iterator_valid(const Xapian::MSetIterator& it) {
    return it.off_from_end != 0;
}

/** @internal Rewind iterator. */
inline void iterator_rewind(Xapian::ESetIterator& it) {
    it.off_from_end = it.eset.size();
}

/** @internal Rewind iterator. */
inline void iterator_rewind(Xapian::MSetIterator& it) {
    it.off_from_end = it.mset.size();
}

/** @internal Is the iterator at the start? */
inline bool iterator_rewound(Xapian::ESetIterator& it) {
    return it.off_from_end == it.eset.size();
}

/** @internal Is the iterator at the start? */
inline bool iterator_rewound(Xapian::MSetIterator& it) {
    return it.off_from_end == it.mset.size();
}

/** @internal Determine if iterator is valid to dereference. */
inline bool iterator_valid(const Xapian::PositionIterator& it) {
    return it.internal != NULL;
}

/** @internal Determine if iterator is valid to dereference. */
inline bool iterator_valid(const Xapian::PostingIterator& it) {
    return it.internal != NULL;
}

/** @internal Determine if iterator is valid to dereference. */
inline bool iterator_valid(const Xapian::TermIterator& it) {
    return it.internal != NULL;
}

/** @internal Determine if iterator is valid to dereference. */
inline bool iterator_valid(const Xapian::ValueIterator& it) {
    return it.internal != NULL;
}

/** @internal Determine if iterator is valid to dereference. */
inline bool iterator_valid(const Xapian::Utf8Iterator& it) {
    return it != Xapian::Utf8Iterator();
}

}

#endif // XAPIAN_INCLUDED_ITERATOR_H
