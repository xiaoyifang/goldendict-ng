/** @file
 *  @brief Class for wrapping type returned by an input_iterator.
 */
/* Copyright (C) 2004,2008,2009,2013,2014 Olly Betts
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

#ifndef XAPIAN_INCLUDED_DEREFWRAPPER_H
#define XAPIAN_INCLUDED_DEREFWRAPPER_H

#if !defined XAPIAN_IN_XAPIAN_H && !defined XAPIAN_LIB_BUILD
# error Never use <xapian/derefwraper.h> directly; include <xapian.h> instead.
#endif

namespace Xapian {

/** @private @internal Class which returns a value when dereferenced with
 *  operator*.
 *
 *  We need this wrapper class to implement input_iterator semantics for the
 *  postfix operator++ methods of some of our iterator classes.
 */
template<typename T>
class DerefWrapper_ {
    /// Don't allow assignment.
#if __cplusplus >= 201103L
    // Avoid warnings with newer clang.
    void operator=(const DerefWrapper_&) = delete;
#else
    void operator=(const DerefWrapper_&);
#endif

    /// The value.
    T res;

  public:
#if __cplusplus >= 201103L
    /// Default copy constructor.
    DerefWrapper_(const DerefWrapper_&) = default;
#endif

    explicit DerefWrapper_(const T &res_) : res(res_) { }
    const T & operator*() const { return res; }
};

}

#endif // XAPIAN_INCLUDED_DEREFWRAPPER_H
