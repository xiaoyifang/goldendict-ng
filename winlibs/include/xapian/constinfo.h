/** @file
 *  @brief Mechanism for accessing a struct of constant information
 */
// Copyright (C) 2003,2004,2005,2007,2008,2009,2010,2012,2013,2015 Olly Betts
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

#ifndef XAPIAN_INCLUDED_XAPIAN_CONSTINFO_H
#define XAPIAN_INCLUDED_XAPIAN_CONSTINFO_H

#include <xapian/attributes.h>
#include <xapian/visibility.h>

namespace Xapian {
namespace Internal {

/** @private @internal */
struct constinfo {
    unsigned char C_tab[256];
    int major, minor, revision;
    char str[8];
    unsigned stemmer_name_len;
    // FIXME: We don't want to fix the size of this in the API headers.
    char stemmer_data[256];
};

/** @private @internal
 *
 *  Rather than having a separate function to access each piece of information,
 *  we put it all into a structure and have a single function which returns a
 *  pointer to this (and we mark that function with attribute const, so the
 *  compiler should be able to CSE calls to it.  This means that when Xapian is
 *  loaded as a shared library we save N-1 relocations (where N is the
 *  number of pieces of information), which reduces the library load time.
 */
XAPIAN_VISIBILITY_DEFAULT
const struct constinfo * XAPIAN_NOTHROW(get_constinfo_()) XAPIAN_CONST_FUNCTION;

}
}

#endif /* XAPIAN_INCLUDED_XAPIAN_CONSTINFO_H */
