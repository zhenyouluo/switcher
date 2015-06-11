/*
 * This file is part of switcher-jack.
 *
 * switcher-myplugin is free software; you can redistribute it and/or
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

#include "switcher/gst-utils.hpp"
#include "switcher/scope-exit.hpp"
#include "switcher/std2.hpp"
#include "switcher/shmdata-utils.hpp"
#include "./shmdata-to-jack.hpp"
#include "./audio-resampler.hpp"

namespace switcher {
SWITCHER_MAKE_QUIDDITY_DOCUMENTATION(
    ShmdataToJack,
    "Audio Display (Jack)",
    "audio",
    "Audio display",
    "LGPL",
    "jacksink",
    "Nicolas Bouillot");

ShmdataToJack::ShmdataToJack(const std::string &name):
    shmcntr_(static_cast<Quiddity *>(this)),
    gst_pipeline_(std2::make_unique<GstPipeliner>(nullptr, nullptr)),
    custom_props_(std::make_shared<CustomPropertyHelper>()),
    jack_client_(name.c_str()) {
  jack_client_.set_jack_process_callback(&ShmdataToJack::jack_process, this);
  jack_client_.set_on_xrun_callback([this](uint n){on_xrun(n);});
}

bool ShmdataToJack::init() {
  if (!jack_client_) {
    g_warning("JackClient cannot be instancied");
    return false;
  }
  if (!make_elements())
    return false;
  shmcntr_.install_connect_method(
      [this](const std::string &shmpath){return this->on_shmdata_connect(shmpath);},
      [this](const std::string &){return this->on_shmdata_disconnect();},
      [this](){return this->on_shmdata_disconnect();},
      [this](const std::string &caps){return this->can_sink_caps(caps);},
      1);
  install_property(G_OBJECT(volume_), "volume", "volume", "Volume");
  install_property(G_OBJECT(volume_), "mute", "mute", "Mute");
  return true;
}

int ShmdataToJack::jack_process (jack_nframes_t nframes, void *arg){
  auto context = static_cast<ShmdataToJack *>(arg);
  { std::unique_lock<std::mutex> lock(context->output_ports_mutex_, std::defer_lock);
    if (lock.try_lock()) {
      auto write_zero = false;
      auto num_channels = context->output_ports_.size();
      if (num_channels > 0 &&  context->ring_buffers_[0].get_usage() < nframes){
        //g_print("jack missed a buffer\n");
        write_zero = true;
      }
      for (unsigned int i = 0; i < context->output_ports_.size(); ++i) {
        jack_sample_t *buf =
            (jack_sample_t *)jack_port_get_buffer(context->output_ports_[i].get_raw(), nframes);
        if (!write_zero) { 
          context->ring_buffers_[i].pop_samples((std::size_t)nframes, buf);
        } else {
          for (unsigned int j = 0; j < nframes; ++j)
            buf[j] = 0;
        }
      }
    }  // locked
  }  // releasing lock
  return 0;
}

void ShmdataToJack::on_xrun(uint num_of_missed_samples) {
  g_warning ("jack xrun (delay of %u samples)", num_of_missed_samples);
  jack_nframes_t jack_buffer_size = jack_client_.get_buffer_size();
  for (auto &it: ring_buffers_) {
    // this is safe since on_xrun is called right before jack_process,
    // on the same thread
    it.shrink_to((std::size_t)jack_buffer_size * 1.5);
  }
}

void ShmdataToJack::on_handoff_cb(GstElement */*object*/,
                                  GstBuffer *buf,
                                  GstPad *pad,
                                  gpointer user_data) {
  ShmdataToJack *context = static_cast<ShmdataToJack *>(user_data);
  auto current_time = jack_frame_time(context->jack_client_.get_raw());
  GstCaps *caps = gst_pad_get_current_caps(pad);
  if (nullptr == caps)
    return;
  On_scope_exit {gst_caps_unref(caps);};
  // gchar *string_caps = gst_caps_to_string(caps);
  // On_scope_exit {if (nullptr != string_caps) g_free(string_caps);};
  // g_print("on handoff, negotiated caps is %s\n", string_caps);
  const GValue *val =
      gst_structure_get_value(gst_caps_get_structure(caps, 0),
                              "channels");
  const int channels = g_value_get_int(val);
  context->check_output_ports(channels);
  //getting buffer infomation:
  GstMapInfo map;
  if (!gst_buffer_map (buf, &map, GST_MAP_READ)) {
    g_warning("gst_buffer_map failled: canceling audio buffer access");
    return;
  }
  On_scope_exit{gst_buffer_unmap (buf, &map);};
  jack_nframes_t duration = map.size / (4 * channels);
  // setting the smoothing value affecting (20 sec)
  context->drift_observer_.set_smoothing_factor(
      (double)duration / (20.0 * (double)context->jack_client_.get_sample_rate()));
  std::size_t new_size = 
      (std::size_t)context->drift_observer_.set_current_time_info(current_time, duration);
  --context->debug_buffer_usage_;
  if (0 == context->debug_buffer_usage_){
    g_debug("buffer load is %lu, ratio is %f\n",
            context->ring_buffers_[0].get_usage(),
            context->drift_observer_.get_ratio());
    context->debug_buffer_usage_ = 1000;
  }
  jack_sample_t *tmp_buf = (jack_sample_t *)map.data;
  for (int i = 0; i < channels; ++i) {
    AudioResampler<jack_sample_t> resample(duration, new_size, tmp_buf, i, channels);
    auto emplaced =
        context->ring_buffers_[i].put_samples(
        new_size,
        [&resample]() {
          // return resample.zero_pole_get_next_sample();
          return resample.linear_get_next_sample();
        });
    if (emplaced != new_size)
      g_warning("overflow of %lu samples", new_size - emplaced);
  }
}

void ShmdataToJack::check_output_ports(unsigned int channels){
  if (channels == channels_)
    return;
  channels_ = channels;
  // thread safe operations on output ports
  { std::unique_lock<std::mutex> lock(output_ports_mutex_);
    // unregistering previous ports
    output_ports_.clear();
    // replacing with new ports
    for (unsigned int i = 0; i < channels; ++i)
      output_ports_.emplace_back(jack_client_, i);
    // replacing ring buffers
    std::vector<AudioRingBuffer<jack_sample_t>> tmp(channels);
    std::swap(ring_buffers_, tmp);
  }  // unlocking output_ports_
}

bool ShmdataToJack::make_elements() {
  GError *error = nullptr;
  std::string description(std::string("shmdatasrc ! audioconvert ! audioresample ! volume ! ")
                          + " audioconvert ! "
                          + " capsfilter caps=\"audio/x-raw, format=(string)F32LE, "
                          "layout=(string)interleaved, rate="
                          + std::to_string(jack_client_.get_sample_rate())
                          + "\" !" 
                          + " fakesink silent=true signal-handoffs=true sync=false");
  GstElement *jacksink = gst_parse_bin_from_description(description.c_str(), TRUE, &error);
  if (error != nullptr) {
    g_warning("%s", error->message);
    g_error_free(error);
    return false;
  }
  GstElement *shmdatasrc =
      GstUtils::get_first_element_from_factory_name(GST_BIN(jacksink),
                                                    "shmdatasrc");
  GstElement *volume =
      GstUtils::get_first_element_from_factory_name(GST_BIN(jacksink),
                                                    "volume");
  if (nullptr != volume_) {
    GstUtils::apply_property_value(G_OBJECT(volume_),
                                   G_OBJECT(volume),
                                   "volume");
    GstUtils::apply_property_value(G_OBJECT(volume_),
                                   G_OBJECT(volume),
                                   "mute");
  }
  GstElement *fakesink =
      GstUtils::get_first_element_from_factory_name(GST_BIN(jacksink),
                                                    "fakesink");
  handoff_handler_ = g_signal_connect(fakesink,
                                      "handoff",
                                      (GCallback)on_handoff_cb,
                                      this);
  if (nullptr != audiobin_) 
    GstUtils::clean_element(audiobin_);
  shmdatasrc_ = shmdatasrc;
  audiobin_ = jacksink;
  volume_ = volume;
  fakesink_ = fakesink;
  return true;
}

bool ShmdataToJack::start() {
  if (shmpath_.empty()) {
    g_warning("cannot start, no shmdata to connect with");
    return false;
  }
  g_object_set(G_OBJECT(shmdatasrc_), "socket-path", shmpath_.c_str(), nullptr);
  shm_sub_ = std2::make_unique<GstShmdataSubscriber>(
      shmdatasrc_,
      [this](std::string &&caps) {
        this->graft_tree(".shmdata.reader." + this->shmpath_,
                         ShmdataUtils::make_tree(caps,
                                                 ShmdataUtils::get_category(caps),
                                                 0));
      },
      [this](GstShmdataSubscriber::num_bytes_t byte_rate){
        this->graft_tree(".shmdata.reader." + this->shmpath_ + ".byte_rate",
                         data::Tree::make(std::to_string(byte_rate)));
      });
  gst_bin_add(GST_BIN(gst_pipeline_->get_pipeline()), audiobin_);
  gst_pipeline_->play(true);
  return true;
}

bool ShmdataToJack::stop() {
  {
    On_scope_exit{gst_pipeline_ = std2::make_unique<GstPipeliner>(nullptr, nullptr);};
    if (!make_elements())
      return false;
  }
  reinstall_property(G_OBJECT(volume_),
                     "volume", "volume", "Volume");
  reinstall_property(G_OBJECT(volume_),
                     "mute", "mute", "Mute");
  return true;
}

bool ShmdataToJack::on_shmdata_disconnect() {
  return stop();
}

bool ShmdataToJack::on_shmdata_connect(const std::string &shmpath) {
  shmpath_ = shmpath;
  stop();
  return start();
}

bool ShmdataToJack::can_sink_caps(const std::string &caps) {
  return GstUtils::can_sink_caps("audioconvert", caps);
}

}  // namespace switcher
