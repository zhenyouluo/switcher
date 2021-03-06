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

#include "./gst-shmdata-subscriber.hpp"
#include <gst/gst.h>
#include <string>

namespace switcher {

GstShmdataSubscriber::GstShmdataSubscriber(GstElement* element,
                                           on_caps_cb_t on_caps_cb,
                                           on_stat_monitor_t on_stat_monitor_cb,
                                           on_delete_t on_delete_cb)
    : element_(element),
      on_caps_cb_(on_caps_cb),
      on_stat_monitor_cb_(on_stat_monitor_cb),
      on_delete_cb_(on_delete_cb),
      ptask_([this]() { this->stat_monitor(); }, std::chrono::milliseconds(1000)) {
  if (!GST_IS_ELEMENT(element_)) {
    g_warning("cannot monitor gstshmdata metadata, not a GstElement");
    return;
  }
  gst_object_ref(static_cast<gpointer>(element));
  signal_handler_id_ = g_signal_connect(
      G_OBJECT(element_), "notify::caps", G_CALLBACK(GstShmdataSubscriber::on_caps_cb), this);
  notify_caps();
}

GstShmdataSubscriber::~GstShmdataSubscriber() {
  if (nullptr != on_delete_cb_) on_delete_cb_();
  if (GST_IS_ELEMENT(element_)) {
    if (0 != signal_handler_id_) g_signal_handler_disconnect(element_, signal_handler_id_);
    gst_object_unref(static_cast<gpointer>(element_));
  }
}

void GstShmdataSubscriber::on_caps_cb(GObject* /*gobject*/,
                                      GParamSpec* /*pspec*/,
                                      gpointer user_data) {
  GstShmdataSubscriber* context = static_cast<GstShmdataSubscriber*>(user_data);
  if (!context->on_caps_cb_) {
    g_debug("got caps, but no caps callback in GstShmdataSubscriber");
    return;
  }
  context->notify_caps();
}

void GstShmdataSubscriber::notify_caps() {
  GValue val = G_VALUE_INIT;
  g_value_init(&val, G_TYPE_STRING);
  g_object_get_property(G_OBJECT(element_), "caps", &val);
  if (nullptr != g_value_get_string(&val)) on_caps_cb_(std::string(g_value_get_string(&val)));
  g_value_unset(&val);
}

void GstShmdataSubscriber::stat_monitor() {
  if (!on_stat_monitor_cb_) {
    g_debug("no stat monitor callback, not monitoring");
    return;
  }
  ShmdataStat stat;
  {
    GValue val = G_VALUE_INIT;
    g_value_init(&val, G_TYPE_UINT64);
    g_object_get_property(G_OBJECT(element_), "bytes", &val);
    stat.bytes_ = g_value_get_uint64(&val);
    g_value_unset(&val);
  }
  {
    GValue val = G_VALUE_INIT;
    g_value_init(&val, G_TYPE_UINT64);
    g_object_get_property(G_OBJECT(element_), "buffers", &val);
    stat.accesses_ = g_value_get_uint64(&val);
    g_value_unset(&val);
  }
  on_stat_monitor_cb_(stat);
}

}  // namespace switcher
