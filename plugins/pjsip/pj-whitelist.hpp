/*
 * This file is part of switcher-pjsip.
 *
 * switcher-pjsip is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * switcher is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with switcher.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PLUGINS_PJSIP_PJ_WHITELIST_H_
#define PLUGINS_PJSIP_PJ_WHITELIST_H_

#include <unordered_map>
#include "switcher/property-container.hpp"

namespace switcher {
class PJWhiteList {
 public:
  using on_authorization_updated_t = std::function<void(bool)>;
  PJWhiteList();
  ~PJWhiteList() = default;
  PJWhiteList(const PJWhiteList&) = delete;
  PJWhiteList& operator=(const PJWhiteList&) = delete;

  bool authorize_buddy(const std::string& sip_url, bool authorize);
  bool add(const std::string& sip_url, on_authorization_updated_t cb);
  bool remove(const std::string& sip_url);
  bool is_authorized(const std::string& sip_url) const;

 private:
  std::unordered_map<std::string, on_authorization_updated_t> on_auth_cbs_{};
  Selection<> mode_{{"authorized contacts", "everybody"}, 0};
  PContainer::prop_id_t mode_id_;
  Selection<>::index_t everybody_;
  const bool default_authorization_{true};
  std::unordered_map<std::string, bool> authorizations_{};
  static gboolean authorize_buddy_cb(gchar* sip_url, gboolean authorized, void* user_data);
};

}  // namespace switcher
#endif
