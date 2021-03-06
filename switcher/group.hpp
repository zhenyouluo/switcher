/*
 * This file is part of libswitcher.
 *
 * libswitcher is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __SWITCHER_GROUP_H__
#define __SWITCHER_GROUP_H__

// This class provide a property type
// it is used for being parent of a group of one or several properties

#include <iostream>
#include <utility>

namespace switcher {
class Group {
 public:
  static std::pair<bool, Group> from_string(const std::string&) {
    return std::make_pair(false, Group());
  }
  std::string to_string() const { return std::string(); }
  friend std::ostream& operator<<(std::ostream& out, const Group& grp) {
    out << grp.to_string();
    return out;
  }
};

}  // namespace switcher
#endif
