/*
 * Copyright (C) 2018 Matthieu Gautier <mgautier@kymeria.fr>
 * Copyright (C) 2009 Tommi Maekitalo
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

#ifndef ZIM_BLOB_H
#define ZIM_BLOB_H

#include "zim.h"

#include <iostream>
#include <string>
#include <algorithm>
#include <memory>

namespace zim
{
  /**
   * A blob is a pointer to data, potentially stored in an `Archive`.
   *
   * All `Blob`'s methods are threadsafe.
   */
  class LIBZIM_API Blob
  {
    public: // types
      using DataPtr = std::shared_ptr<const char>;

    public: // functions
      /**
       * Constuct a empty `Blob`
       */
      Blob();

      /**
       * Constuct `Blob` pointing to `data`.
       *
       * The created blob only point to the data and doesn't own it.
       * User must care that data is not freed before using the blob.
       */
      Blob(const char* data, size_type size);

      /**
       * Constuct `Blob` pointing to `data`.
       *
       * The created blob shares the ownership on data.
       */
      Blob(const DataPtr& buffer, size_type size);

      operator std::string() const { return std::string(_data.get(), _size); }
      const char* data() const  { return _data.get(); }
      const char* end() const   { return _data.get() + _size; }
      size_type size() const     { return _size; }

   private:
     DataPtr _data;
     size_type _size;
  };

  inline std::ostream& operator<< (std::ostream& out, const Blob& blob)
  {
    if (blob.data())
      out.write(blob.data(), blob.size());
    return out;
  }

  inline bool operator== (const Blob& b1, const Blob& b2)
  {
    return b1.size() == b2.size()
        && std::equal(b1.data(), b1.data() + b1.size(), b2.data());
  }
}

#endif // ZIM_BLOB_H
