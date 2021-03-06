/*
 * This file is part of switcher-myplugin.
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

#include <atomic>
#include <cassert>
#include <chrono>
#include <future>
#include <string>
#include <vector>
#include "switcher/gst-shmdata-subscriber.hpp"
#include "switcher/property-container.hpp"
#include "switcher/quiddity-basic-test.hpp"
#include "switcher/quiddity-manager.hpp"

#ifdef HAVE_CONFIG_H
#include "../../../config.h"
#endif

static bool success = false;
static std::atomic<bool> do_continue{true};
static std::condition_variable cond_var{};
static std::mutex mut{};

using namespace switcher;

void on_tree_grafted(const std::string& /*subscriber_name */,
                     const std::string& quid_name,
                     const std::string& signal_name,
                     const std::vector<std::string>& params,
                     void* user_data) {
  auto manager = static_cast<QuiddityManager*>(user_data);
  size_t byte_rate =
      manager->use_tree<MPtr(&InfoTree::branch_get_value)>(quid_name, params[0] + ".byte_rate");
  if (0 != byte_rate) {
    std::unique_lock<std::mutex> lock(mut);
    success = true;
    do_continue.store(false);
    cond_var.notify_one();
  }
  std::printf(
      "%s: %s %s\n", signal_name.c_str(), params[0].c_str(), std::to_string(byte_rate).c_str());
}

int main() {
  {
    QuiddityManager::ptr manager = QuiddityManager::make_manager("test_manager");
#ifdef HAVE_CONFIG_H
    gchar* usr_plugin_dir = g_strdup_printf("./%s", LT_OBJDIR);
    manager->scan_directory_for_plugins(usr_plugin_dir);
    g_free(usr_plugin_dir);
#else
    return 1;
#endif

    // testing if two nvenc can be created simultaneously
    std::vector<std::string> nvencs;
    for (int i = 0; i < 2; ++i) {
      auto nvenc_quid = manager->create("nvenc");
      if (nvenc_quid.empty()) {
        g_warning("nvenc creating failed (i is %d) ", i);
        return 1;
      }
      nvencs.push_back(nvenc_quid);
    }
    for (auto& it : nvencs) manager->remove(it);
    nvencs.clear();

    // testing if nvenc can be used
    {
      auto nvenc_quid = manager->create("nvenc");
      if (nvenc_quid.empty()) {
        g_warning("nvenc encoding could not be created");
        return 1;
      }
      manager->remove(nvenc_quid);
    }

    // standard test
    assert(QuiddityBasicTest::test_full(manager, "nvenc"));
    manager->remove("nvenc");


    // testing nvenc is encoding
    auto video_quid = manager->create("videotestsrc");
    assert(!video_quid.empty());
    manager->use_prop<MPtr(&PContainer::set_str_str)>(video_quid.c_str(), "codec", "0");
    manager->use_prop<MPtr(&PContainer::set_str_str)>(video_quid.c_str(), "started", "true");
    // wait for video to be started
    usleep(100000);
    auto vid_shmdata_list =
        manager->use_tree<MPtr(&InfoTree::get_child_keys)>(video_quid.c_str(), "shmdata.writer");
    auto vid_shmpath = vid_shmdata_list.front();
    assert(!vid_shmpath.empty());

    auto nvenc_quid = manager->create("nvenc");
    assert(!nvenc_quid.empty());
    manager->invoke_va(nvenc_quid.c_str(), "connect", nullptr, vid_shmpath.c_str(), nullptr);

    // tracking nvenc shmdata writer byterate for evaluating success
    assert(manager->make_signal_subscriber("signal_subscriber", on_tree_grafted, manager.get()));
    assert(manager->subscribe_signal("signal_subscriber", nvenc_quid.c_str(), "on-tree-grafted"));

    // wait 3 seconds
    uint count = 3;
    while (do_continue.load()) {
      std::unique_lock<std::mutex> lock(mut);
      if (count == 0) {
        do_continue.store(false);
      } else {
        --count;
        cond_var.wait_for(lock, std::chrono::seconds(1), []() { return !do_continue.load(); });
      }
    }
  }  // end of scope is releasing the manager
  gst_deinit();
  if (success)
    return 0;
  else
    return 1;
}
