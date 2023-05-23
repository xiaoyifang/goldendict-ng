/*
 * Copyright (C) 2020-2021 Veloman Yunkan
 * Copyright (C) 2018-2020 Matthieu Gautier <mgautier@kymeria.fr>
 * Copyright (C) 2006 Tommi Maekitalo
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * is provided AS IS, WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, and
 * NON-INFRINGEMENT.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#ifndef ZIM_ZIM_H
#define ZIM_ZIM_H

#include <cstdint>

#ifdef __GNUC__
#define DEPRECATED __attribute__((deprecated))
#elif defined(_MSC_VER)
#define DEPRECATED __declspec(deprecated)
#else
#praga message("WARNING: You need to implement DEPRECATED for this compiler")
#define DEPRECATED
#endif

#include <zim/zim_config.h>

#if defined(LIBZIM_DLL) && defined(LIBZIM_BUILDING_LIBRARY)
    #define LIBZIM_API __declspec(dllexport)
#elif defined(LIBZIM_DLL)
    #define LIBZIM_API __declspec(dllimport)
#else
    #define LIBZIM_API
#endif

namespace zim
{
  // An index of an entry (in a zim file)
  typedef uint32_t entry_index_type;

  // An index of an cluster (in a zim file)
  typedef uint32_t cluster_index_type;

  // An index of a blog (in a cluster)
  typedef uint32_t blob_index_type;

  // The size of something (entry, zim, cluster, blob, ...)
  typedef uint64_t size_type;

  // An offset.
  typedef uint64_t offset_type;

  enum class Compression
  {
    None = 1,

    // intermediate values correspond to compression
    // methods that are no longer supported

    Zstd = 5
  };

  static const char MimeHtmlTemplate[] = "text/x-zim-htmltemplate";

  /**
   * Various types of integrity checks performed by `zim::validate()`.
   */
  enum class IntegrityCheck
  {
    /**
     * Validates the checksum of the ZIM file.
     */
    CHECKSUM,

    /**
     * Checks that offsets in UrlPtrList are valid.
     */
    DIRENT_PTRS,

    /**
     * Checks that dirents are properly sorted.
     */
    DIRENT_ORDER,

    /**
     * Checks that entries in the title index are valid and properly sorted.
     */
    TITLE_INDEX,

    /**
     * Checks that offsets in ClusterPtrList are valid.
     */
    CLUSTER_PTRS,

    /**
     * Checks that mime-type values in dirents are valid.
     */
    DIRENT_MIMETYPES,

    ////////////////////////////////////////////////////////////////////////////
    // End of integrity check types.
    // COUNT must be the last one and denotes the count of all checks
    ////////////////////////////////////////////////////////////////////////////

    /**
     * `COUNT` is not a valid integrity check type. It exists to tell the
     * number of all supported integrity checks.
     */
    COUNT
  };
}

#endif // ZIM_ZIM_H

