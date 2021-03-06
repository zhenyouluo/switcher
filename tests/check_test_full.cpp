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

#include <iostream>
#include <string>
#include <vector>
#include "switcher/quiddity-basic-test.hpp"
#include "switcher/quiddity-manager.hpp"

int main() {
  bool success = true;
  {
    switcher::QuiddityManager::ptr manager = switcher::QuiddityManager::make_manager("test_full");
    for (auto& it : manager->get_classes()) {
      std::cout << "----- testing " << it << std::endl;
      if (!switcher::QuiddityBasicTest::test_full(manager, it)) {
        std::cout << "---------> issue with " << it << std::endl;
        success = false;
      }
    }
  }

  gst_deinit();
  if (success)
    return 0;
  else
    return 1;
}
