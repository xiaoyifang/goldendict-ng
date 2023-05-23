/*
 * Copyright (C) 2021 Veloman Yunkan
 * Copyright (C) 2020 Matthieu Gautier <mgautier@kymeria.fr>
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

#ifndef ZIM_ITEM_H
#define ZIM_ITEM_H

#include "zim.h"
#include "blob.h"
#include <string>

namespace zim
{
  class Dirent;
  class FileImpl;

  /**
   * An `Item` in an `Archive`
   *
   * All `Item`'s methods are threadsafe.
   */
  class LIBZIM_API Item
  {
    public: // types
      typedef std::pair<std::string, offset_type> DirectAccessInfo;

    public: // functions
      explicit Item(std::shared_ptr<FileImpl> file_, entry_index_type idx_);

      std::string getTitle() const;
      std::string getPath() const;
      std::string getMimetype() const;

      /** Get the data associated to the item
       *
       * Get the data of the item, starting at offset.
       *
       * @param offset The number of byte to skip at begining of the data.
       * @return A blob corresponding to the data.
       */
      Blob getData(offset_type offset=0) const;

      /** Get the data associated to the item
       *
       * Get the `size` bytes of data of the item, starting at offset.
       *
       * @param offset The number of byte to skip at begining of the data.
       * @param size The number of byte to read.
       * @return A blob corresponding to the data.
       */
      Blob getData(offset_type offset, size_type size) const;

      /** The size of the item.
       *
       * @return The size (in byte) of the item.
       */
      size_type getSize() const;

      /** Direct access information.
       *
       * Some item are stored raw in the zim file.
       * If possible, this function give information about which file
       * and at which to read to get the data.
       *
       * It can be usefull as an optimisation when interacting with other system
       * by reopeing the file and reading the content bypassing the libzim.
       *
       * @return A pair of filename/offset specifying where read the content.
       *         If it is not possible to have direct access for this item,
       *         return a pair of `{"", 0}`
       */
      DirectAccessInfo getDirectAccessInformation() const;

      entry_index_type getIndex() const   { return m_idx; }

#ifdef ZIM_PRIVATE
      cluster_index_type getClusterIndex() const;
#endif

    private: // data
      std::shared_ptr<FileImpl> m_file;
      entry_index_type m_idx;
      std::shared_ptr<const Dirent> m_dirent;
  };

}

#endif // ZIM_ITEM_H

