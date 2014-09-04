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

#include "./jack-sink.hpp"
#include "./gst-utils.hpp"
#include "./quiddity-command.hpp"
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

namespace switcher {
SWITCHER_MAKE_QUIDDITY_DOCUMENTATION(JackSink,
                                     "Audio Display (Jack)",
                                     "audio",
                                     "Audio display with minimal features",
                                     "LGPL",
                                     "jacksink",
                                     "Nicolas Bouillot");

bool JackSink::init_gpipe() {
  if (false == make_elements())
    return false;
  init_startable(this);

  client_name_ = g_strdup(get_nick_name().c_str());
  client_name_spec_ =
      custom_props_->make_string_property("jack-client-name",
                                          "the jack client name",
                                          "switcher",
                                          (GParamFlags) G_PARAM_READWRITE,
                                          JackSink::set_client_name,
                                          JackSink::get_client_name, this);
  install_property_by_pspec(custom_props_->get_gobject(),
                            client_name_spec_,
                            "client-name", "Client Name");

  return true;
}

JackSink::JackSink():jacksink_(nullptr),
                     custom_props_(new CustomPropertyHelper()),
                     client_name_spec_(nullptr), client_name_(nullptr) {
}

JackSink::~JackSink() {
  if (nullptr != client_name_)
    g_free(client_name_);
}

bool JackSink::make_elements() {
  GError *error = nullptr;

  gchar *description =
      g_strdup_printf
      ("audioconvert ! audioresample ! queue max-size-buffers=2 leaky=downstream ! jackaudiosink provide-clock=false slave-method=none client-name=%s sync=false buffer-time=10000",
       client_name_);

  jacksink_ = gst_parse_bin_from_description(description, TRUE, &error);
  g_object_set(G_OBJECT(jacksink_), "async-handling", TRUE, nullptr);
  g_free(description);

  if (error != nullptr) {
    g_warning("%s", error->message);
    g_error_free(error);
    return false;
  }
  return true;
}

bool JackSink::start() {
  if (false == make_elements())
    return false;
  set_sink_element(jacksink_);
  return true;
}

bool JackSink::stop() {
  reset_bin();
  return true;
}

void JackSink::set_client_name(const gchar * value, void *user_data) {
  JackSink *context = static_cast < JackSink * >(user_data);
  if (nullptr != context->client_name_)
    g_free(context->client_name_);
  context->client_name_ = g_strdup(value);
  context->custom_props_->
      notify_property_changed(context->client_name_spec_);
}

const gchar *JackSink::get_client_name(void *user_data) {
  JackSink *context = static_cast < JackSink * >(user_data);
  return context->client_name_;
}

void JackSink::on_shmdata_disconnect() {
  stop();
}

void JackSink::on_shmdata_connect(std::string /* shmdata_sochet_path */ ) {
  if (is_started()) {
    stop();
    make_elements();
    set_sink_element_no_connect(jacksink_);
  }
}

bool JackSink::can_sink_caps(std::string caps) {
  return GstUtils::can_sink_caps("audioconvert", caps);
}
}
