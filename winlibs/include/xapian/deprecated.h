/** @file
 * @brief Define XAPIAN_DEPRECATED() and related macros.
 */
// Copyright (C) 2006,2007,2009,2011,2012,2013,2014 Olly Betts
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

#ifndef XAPIAN_INCLUDED_DEPRECATED_H
#define XAPIAN_INCLUDED_DEPRECATED_H

#if !defined XAPIAN_IN_XAPIAN_H && !defined XAPIAN_LIB_BUILD
# error Never use <xapian/deprecated.h> directly; include <xapian.h> instead.
#endif

// How to make use of XAPIAN_DEPRECATED, etc is documented in HACKING - see
// the section "Marking Features as Deprecated".  Don't forget to update the
// documentation of deprecated methods for end users in docs/deprecation.rst
// too!

// Don't give deprecation warnings for features marked as externally deprecated
// when building the library.
#ifdef XAPIAN_IN_XAPIAN_H
# define XAPIAN_DEPRECATED_EX(D) XAPIAN_DEPRECATED(D)
# define XAPIAN_DEPRECATED_CLASS_EX XAPIAN_DEPRECATED_CLASS
#else
# define XAPIAN_DEPRECATED_EX(D) D
# define XAPIAN_DEPRECATED_CLASS_EX
#endif

// xapian-bindings needs to wrap deprecated functions without warnings,
// so check if XAPIAN_DEPRECATED is defined so xapian-bindings can override
// it.
#ifndef XAPIAN_DEPRECATED
# ifdef __GNUC__
// __attribute__((__deprecated__)) is supported by GCC 3.1 and later, which
// is now our minimum requirement, so there's no need to check the GCC version
// in use.
#  define XAPIAN_DEPRECATED(D) D __attribute__((__deprecated__))
#  define XAPIAN_DEPRECATED_CLASS __attribute__((__deprecated__))
# elif defined _MSC_VER && _MSC_VER >= 1300
// __declspec(deprecated) is supported by MSVC 7.0 and later.
#  define XAPIAN_DEPRECATED(D) __declspec(deprecated) D
#  define XAPIAN_DEPRECATED_CLASS __declspec(deprecated)
# else
#  define XAPIAN_DEPRECATED(D) D
# endif
#endif

#ifndef XAPIAN_DEPRECATED_CLASS
# define XAPIAN_DEPRECATED_CLASS
#endif

#endif
