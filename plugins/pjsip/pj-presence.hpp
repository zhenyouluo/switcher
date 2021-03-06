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

#ifndef __SWITCHER_PJ_PRESENCE_H__
#define __SWITCHER_PJ_PRESENCE_H__

#include <glib.h>  // gboolean
#include <pjsua-lib/pjsua.h>

#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include "switcher/selection.hpp"

namespace switcher {
class SIPPlugin;
class PJCall;
class PJSIP;

class PJPresence {
  friend PJSIP;
  friend SIPPlugin;
  friend PJCall;  // for account local uri

 public:
  PJPresence();
  ~PJPresence();
  PJPresence(const PJPresence&) = delete;
  PJPresence& operator=(const PJPresence&) = delete;

  enum { AVAILABLE, BUSY, AWAY, OFFLINE, OPT_MAX };

 private:
  pjsua_acc_id account_id_{-1};
  pjsua_acc_config cfg_;
  std::mutex registration_mutex_{};
  std::condition_variable registration_cond_{};
  // online status
  Selection<> status_{{"Available", "Busy", "Away", "Offline"}, 0};
  std::string custom_status_{""};
  // sip registration status (read only)
  bool registered_{false};
  // account info
  std::string sip_local_user_{};  // full uri like 'sip:me@mydomain.ca:5060'
  pj_pool_t* acc_info_pool_{nullptr};
  std::map<std::string, pjsua_buddy_id> buddy_id_{};
  // registration
  static void on_registration_state(pjsua_acc_id acc_id, pjsua_reg_info* info);
  void register_account(const std::string& sip_user, const std::string& sip_password);
  static gboolean register_account_wrapped(gchar* user, gchar* password, void* user_data);
  void unregister_account(bool notify_tree = true);
  static gboolean unregister_account_wrapped(gpointer /*unused */, void* user_data);

  // buddies
  void add_buddy(const std::string& sip_user);
  void del_buddy(const std::string& sip_user);
  static gboolean add_buddy_wrapped(gchar* buddy_uri, void* user_data);
  static gboolean del_buddy_wrapped(gchar* buddy_uri, void* user_data);
  static void on_buddy_state(pjsua_buddy_id buddy_id);
  // online status
  void change_online_status(gint status);
  // name buddy method
  static gboolean name_buddy_wrapped(gchar* name, gchar* uri, void* user_data);
  void name_buddy(std::string name, std::string sip_user);
  pjsua_buddy_id get_id_from_buddy_name(const std::string& name);
  // pjsip functions
  static void on_reg_state(pjsua_acc_id acc_id);
  static void on_incoming_subscribe(pjsua_acc_id acc_id,
                                    pjsua_srv_pres* srv_pres,
                                    pjsua_buddy_id buddy_id,
                                    const pj_str_t* from,
                                    pjsip_rx_data* rdata,
                                    pjsip_status_code* code,
                                    pj_str_t* reason,
                                    pjsua_msg_data* msg_data);
  static void on_buddy_evsub_state(pjsua_buddy_id buddy_id, pjsip_evsub* sub, pjsip_event* event);
};

}  // namespace switcher
#endif
