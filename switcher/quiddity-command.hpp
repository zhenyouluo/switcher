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

/**
 * The Quiddity command
 */

#ifndef __SWITCHER_QUIDDITY_COMMAND_H__
#define __SWITCHER_QUIDDITY_COMMAND_H__

#include <glib.h>  // gint64
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "./information-tree.hpp"

namespace switcher {
class QuiddityCommand {
 public:
  using ptr = std::shared_ptr<QuiddityCommand>;
  enum command {
    invalid_command = -1,
    auto_invoke = 0,
    create,
    create_nick_named,
    get_class_doc,
    get_classes,
    get_classes_doc,
    get_method_description,
    get_method_description_by_class,
    get_methods_description,
    get_methods_description_by_class,
    get_quiddities_description,
    get_quiddity_description,
    get_signal_description,
    get_signal_description_by_class,
    get_signals_description,
    get_signals_description_by_class,
    has_method,
    invoke,
    list_signal_subscribers,
    list_subscribed_signals,
    list_subscribed_signals_json,
    make_signal_subscriber,
    quit,
    remove,
    remove_signal_subscriber,
    set_property,
    subscribe_property,
    subscribe_signal,
    unsubscribe_property,
    unsubscribe_signal
  };

  command id_{invalid_command};
  std::vector<std::string> args_{};
  std::vector<std::string> vector_arg_{};
  std::vector<std::string> result_{};
  std::vector<std::string> expected_result_{};
  gint64 time_{-1};  // monotonic time, in microseconds
  bool success_{false};
  void clear();
  void set_id(command id);
  void add_arg(std::string arg);
  void set_vector_arg(std::vector<std::string> vector_arg);
  static command get_id_from_string(const char* com);
  static const char* get_string_from_id(QuiddityCommand::command id);
  static QuiddityCommand::ptr make_command_from_tree(InfoTree::ptr tree);
  static const std::map<int, const char*> command_names_;
  InfoTree::ptr get_info_tree() const;
};

}  // namespace switcher
#endif
