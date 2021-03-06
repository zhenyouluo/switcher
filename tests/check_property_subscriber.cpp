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

#include <gst/gst.h>
#include <string>
#include <vector>
#include "switcher/quiddity-manager.hpp"

static bool success;
static const char* user_string = "hello world";
static switcher::QuiddityManager::ptr manager;

void mon_property_cb(const std::string& /*subscriber_name */,
                     const std::string& quiddity_name,
                     const std::string& property_name,
                     const std::string& value,
                     void* user_data) {
  if (0 != quiddity_name.compare("vid")) {
    g_warning("quiddity name does not match, got %s instead of \"vid\"", quiddity_name.c_str());
    return;
  }

  if (0 != property_name.compare("pattern")) {
    g_warning("property name does not match, got %s instead of \"pattern\"", property_name.c_str());
    return;
  }

  if (0 != value.compare("Random (television snow)")) {
    g_warning("value does not match, got %s instead of Random (television snow)", value.c_str());
    return;
  }

  if (0 != g_strcmp0((char*)user_data, "hello world")) {
    g_warning("user_data name does not match, got %s instead of \"hello world\"", (char*)user_data);
    return;
  }

  success = true;
}

int main() {
  success = false;

  {
    manager = switcher::QuiddityManager::make_manager("test_manager");

    manager->make_property_subscriber("sub", mon_property_cb, (void*)user_string);
    manager->create("videotestsrc", "vid");
    manager->subscribe_property("sub", "vid", "pattern");

    std::vector<std::string> subscribers = manager->list_property_subscribers();
    if (subscribers.size() != 1 || g_strcmp0(subscribers.at(0).c_str(), "sub") != 0) {
      g_warning("pb with list_property_subscribers");
      return 1;
    }

    std::vector<std::pair<std::string, std::string>> properties =
        manager->list_subscribed_properties("sub");
    if (properties.size() != 1 || g_strcmp0(properties.at(0).first.c_str(), "vid") ||
        g_strcmp0(properties.at(0).second.c_str(), "pattern")) {
      g_warning("pb with list_subscribed_properties");
      return 1;
    }

    manager->set_property("vid", "pattern", "1");

    manager->unsubscribe_property("sub", "vid", "pattern");
    manager->remove("vid");

    properties = manager->list_subscribed_properties("sub");
    if (properties.size() != 0) {
      g_warning("pb with automatic unsubscribe at quiddity removal");
      return 1;
    }

    manager->remove_property_subscriber("sub");
  }

  // cleanning manager
  {
    switcher::QuiddityManager::ptr empty;
    manager.swap(empty);
  }

  gst_deinit();
  if (success)
    return 0;
  else
    return 1;
}
