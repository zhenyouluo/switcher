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

#ifndef __SWITCHER_STRING_UTILS_H__
#define __SWITCHER_STRING_UTILS_H__

#include <string>
#include <vector>

namespace switcher {
namespace StringUtils {

std::string replace_char(const std::string& orig,
                         const char to_replace,
                         const std::string& replacement);
std::string replace_string(const std::string& orig,
                           const std::string& to_replace,
                           const std::string& replacement);

int tolower_char(int c);
int toupper_char(int c);
void toupper(std::string& str);
void tolower(std::string& str);
bool starts_with(const std::string& str, const std::string& suffix);
bool ends_with(const std::string& str, const std::string& suffix);

}  // namespace StringUtils
}  // namespace switcher
#endif
