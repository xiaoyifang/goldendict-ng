/** @file
 * @brief Define XAPIAN_VISIBILITY_* macros.
 */
// Copyright (C) 2007,2010,2014,2019 Olly Betts
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

#ifndef XAPIAN_INCLUDED_VISIBILITY_H
#define XAPIAN_INCLUDED_VISIBILITY_H

// See https://gcc.gnu.org/wiki/Visibility for more information about GCC's
// symbol visibility support.

#include "xapian/version.h"
#ifdef XAPIAN_ENABLE_VISIBILITY
# define XAPIAN_VISIBILITY_DEFAULT __attribute__((visibility("default")))
#else
# define XAPIAN_VISIBILITY_DEFAULT
#endif

#endif
