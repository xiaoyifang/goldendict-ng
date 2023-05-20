/*
 * Copyright (C) 2021 Mannesh P M <manu.pm55@gmaile.com>
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

#ifndef ZIM_UUID_H
#define ZIM_UUID_H

#include <iosfwd>
#include <algorithm>
#include <cstring>
#include <string>

namespace zim
{
  struct Uuid
  {
    Uuid()
    {
      std::memset(data, 0, 16);
    }

    Uuid(const char uuid[16])
    {
      std::copy(uuid, uuid+16, data);
    }

    static Uuid generate(std::string value = "");

    bool operator== (const Uuid& other) const
      { return std::equal(data, data+16, other.data); }
    bool operator!= (const Uuid& other) const
      { return !(*this == other); }
    unsigned size() const  { return 16; }

    explicit operator std::string() const;

    char data[16];
  };

  std::ostream& operator<< (std::ostream& out, const Uuid& uuid);

}

#endif // ZIM_UUID_H
