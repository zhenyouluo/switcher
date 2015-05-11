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

#include "./audio-test-source.hpp"
#include <gst/gst.h>
#include "./gst-utils.hpp"
#include "./std2.hpp"

namespace switcher {
SWITCHER_MAKE_QUIDDITY_DOCUMENTATION(AudioTestSource,
                                     "Sine",
                                     "audio",
                                     "Creates audio test signals",
                                     "LGPL",
                                     "audiotestsrc",
                                     "Nicolas Bouillot");

AudioTestSource::AudioTestSource(const std::string &):
    gst_pipeline_(std2::make_unique<GstPipeliner>()) {
}

bool AudioTestSource::init() {
  if (!audiotestsrc_) {
    g_warning("audiotestsrc creation failed");
    return false;
  }
  if (!shmdatasink_) {
    g_warning("shmdatasink creation failed");
    return false;
  }

  init_startable(this);

  g_object_set(G_OBJECT(audiotestsrc_.get_raw()), "is-live", TRUE, nullptr);
  g_object_set(G_OBJECT(audiotestsrc_.get_raw()), "samplesperbuffer", 512, nullptr);
  g_object_set(G_OBJECT(shmdatasink_.get_raw()),
               "socket-path", make_file_name("audio").c_str(),
               nullptr);
  // registering
  install_property(G_OBJECT(audiotestsrc_.get_raw()), "volume", "volume", "Volume");
  install_property(G_OBJECT(audiotestsrc_.get_raw()), "freq", "freq", "Frequency");
  install_property(G_OBJECT(audiotestsrc_.get_raw()), "wave", "wave", "Signal Form");
  gst_bin_add_many(GST_BIN(gst_pipeline_->get_pipeline()),
                   audiotestsrc_.get_raw(),
                   shmdatasink_.get_raw(),
                   nullptr);
  gst_element_link(audiotestsrc_.get_raw(), shmdatasink_.get_raw());
  return true;
}

AudioTestSource::~AudioTestSource() {
}

bool AudioTestSource::start() {
  gst_pipeline_->play(true);
  return true;
}

bool AudioTestSource::stop() {
  gst_pipeline_->play(false);
  return true;
}
}
