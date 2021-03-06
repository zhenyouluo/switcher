/*
 * This file is part of switcher-v4l2.
 *
 * switcher-v4l2 is free software: you can redistribute it and/or modify
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

#include "./v4l2src.hpp"
#include <fcntl.h>
#include <gst/video/video.h>
#include <linux/videodev2.h>
#include <string.h>
#include <sys/ioctl.h>
#include <cstdlib>  // For srand() and rand()
#include <ctime>    // For time()
#include "switcher/gst-utils.hpp"
#include "switcher/quiddity-manager-impl.hpp"
#include "switcher/scope-exit.hpp"

namespace switcher {
SWITCHER_MAKE_QUIDDITY_DOCUMENTATION(V4L2Src,
                                     "v4l2src",
                                     "v4l2 Video Capture",
                                     "video",
                                     "writer/device",
                                     "Discover and use v4l2 supported capture cards and cameras",
                                     "GPL",
                                     "Nicolas Bouillot");

V4L2Src::V4L2Src(const std::string&)
    : gst_pipeline_(
          std::make_unique<GstPipeliner>(nullptr, nullptr, [this](GstObject* gstobj, GError * err) {
            on_gst_error(gstobj, err);
          })) {
  init_startable(this);
}

void V4L2Src::set_shm_suffix() {
  if (is_current_pixel_format_raw_video())
    shmpath_ = make_file_name(raw_suffix_);
  else
    shmpath_ = make_file_name(enc_suffix_);
  g_object_set(G_OBJECT(shmsink_.get_raw()),
               "socket-path",
               shmpath_.c_str(),
               "initial-size",
               kDefaultInitialShmsize,
               nullptr);
}

bool V4L2Src::init() {
  if (!v4l2src_ || !capsfilter_ || !shmsink_) return false;
  // device inspector
  check_folder_for_v4l2_devices("/dev");
  update_capture_device();

  if (capture_devices_.empty()) {
    g_message("ERROR:No video4linux device detected.");
    return false;
  }
  devices_id_ =
      pmanage<MPtr(&PContainer::make_selection<>)>("device",
                                                   [this](const size_t& val) {
                                                     devices_enum_.select(val);
                                                     update_device_specific_properties();
                                                     return true;
                                                   },
                                                   [this]() { return devices_enum_.get(); },
                                                   "Capture Device",
                                                   "Enumeration of v4l2 capture devices",
                                                   devices_enum_);
  group_id_ = pmanage<MPtr(&PContainer::make_group)>(
      "config", "Capture device configuration", "device specific parameters");
  update_device_specific_properties();
  codecs_ =
      std::make_unique<GstVideoCodec>(static_cast<Quiddity*>(this), make_file_name(raw_suffix_));
  set_shm_suffix();
  return true;
}

bool V4L2Src::fetch_available_resolutions() {
  CaptureDescription& description = capture_devices_[devices_enum_.get()];
  const char* file_path = description.absolute_path_.c_str();
  int fd = open(file_path, O_RDONLY);
  if (fd < 0) {
    g_debug("V4L2Src: Could not open device %s", file_path);
    return false;
  }

  v4l2_frmsizeenum frmsize;
  memset(&frmsize, 0, sizeof(frmsize));
  frmsize.pixel_format = std::get<0>(description.pixel_formats_[pixel_format_enum_.get()]);
  frmsize.index = 0;
  unsigned default_width = 0;
  unsigned default_height = 0;

  description.frame_size_discrete_.clear();
  while (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) >= 0 &&
         frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
    if (frmsize.index == 0) {
      default_width = frmsize.discrete.width;
      default_height = frmsize.discrete.height;
    }

    description.frame_size_discrete_.push_back(std::make_pair(
        std::to_string(frmsize.discrete.width), std::to_string(frmsize.discrete.height)));
    frmsize.index++;
  }

  if (frmsize.type != V4L2_FRMSIZE_TYPE_DISCRETE) {
    description.frame_size_stepwise_max_width_ = frmsize.stepwise.max_width;
    description.frame_size_stepwise_min_width_ = frmsize.stepwise.min_width;
    description.frame_size_stepwise_step_width_ = frmsize.stepwise.step_width;
    description.frame_size_stepwise_max_height_ = frmsize.stepwise.max_height;
    description.frame_size_stepwise_min_height_ = frmsize.stepwise.min_height;
    description.frame_size_stepwise_step_height_ = frmsize.stepwise.step_height;
    default_width = frmsize.stepwise.max_width;
    default_height = frmsize.stepwise.max_height;
    width_ = default_width;
    height_ = default_height;
  } else {
    description.frame_size_stepwise_max_width_ = -1;
    description.frame_size_stepwise_min_width_ = -1;
    description.frame_size_stepwise_step_width_ = -1;
    description.frame_size_stepwise_max_height_ = -1;
    description.frame_size_stepwise_min_height_ = -1;
    description.frame_size_stepwise_step_height_ = -1;
  }

  close(fd);

  return true;
}

bool V4L2Src::fetch_available_frame_intervals() {
  CaptureDescription& description = capture_devices_[devices_enum_.get()];
  const char* file_path = description.absolute_path_.c_str();
  int fd = open(file_path, O_RDONLY);
  if (fd < 0) {
    g_debug("V4L2Src: Could not open device %s", file_path);
    return false;
  }

  v4l2_frmivalenum frmival;
  memset(&frmival, 0, sizeof(frmival));
  frmival.pixel_format = std::get<0>(description.pixel_formats_[pixel_format_enum_.get()]);

  // Only true with continuous resolution devices.
  if (width_ != -1) {
    frmival.width = width_;
  } else {
    frmival.width = atoi(description.frame_size_discrete_[resolutions_enum_.get()].first.c_str());
  }
  if (height_ != -1) {
    frmival.height = width_;
  } else {
    frmival.height = atoi(description.frame_size_discrete_[resolutions_enum_.get()].second.c_str());
  }

  frmival.index = 0;

  description.frame_interval_discrete_.clear();
  while (ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival) >= 0 &&
         frmival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
    if (frmival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
      description.frame_interval_discrete_.push_back(
          std::make_pair(std::to_string(frmival.discrete.numerator),
                         std::to_string(frmival.discrete.denominator)));
    }
    frmival.index++;
  }

  if (frmival.type != V4L2_FRMIVAL_TYPE_DISCRETE) {
    // g_print ("frametime (s) for rate min %u/%u\nrate max %u/%u\n time step
    // %u/%u\n",
    //  frmival.stepwise.min.numerator,
    //  frmival.stepwise.min.denominator,
    //  frmival.stepwise.max.numerator,
    //  frmival.stepwise.max.denominator,
    //  frmival.stepwise.step.numerator,
    //  frmival.stepwise.step.denominator);
    description.frame_interval_stepwise_max_numerator_ = frmival.stepwise.max.numerator;
    description.frame_interval_stepwise_max_denominator_ = frmival.stepwise.max.denominator;
    description.frame_interval_stepwise_min_numerator_ = frmival.stepwise.max.numerator;
    description.frame_interval_stepwise_min_denominator_ = frmival.stepwise.max.denominator;
    description.frame_interval_stepwise_step_numerator_ = frmival.stepwise.step.numerator;
    description.frame_interval_stepwise_step_denominator_ = frmival.stepwise.step.denominator;
  } else {
    description.frame_interval_stepwise_max_numerator_ = -1;
    description.frame_interval_stepwise_max_denominator_ = -1;
    description.frame_interval_stepwise_min_numerator_ = -1;
    description.frame_interval_stepwise_min_denominator_ = -1;
    description.frame_interval_stepwise_step_numerator_ = -1;
    description.frame_interval_stepwise_step_denominator_ = -1;
  }

  close(fd);

  return true;
}

void V4L2Src::update_capture_device() {
  gint i = 0;
  std::vector<std::string> names;
  std::vector<std::string> nicks;
  for (auto& it : capture_devices_) {
    names.push_back(it.card_);
    nicks.push_back(it.file_device_);
    i++;
  }
  devices_enum_ = Selection<>(std::make_pair(names, nicks), 0);
}

void V4L2Src::update_device_specific_properties() {
  if (capture_devices_.empty()) return;

  update_pixel_format();
  update_discrete_resolution();
  update_width_height();
  update_tv_standard();
  update_discrete_framerate();
  update_framerate_numerator_denominator();
}

void V4L2Src::update_discrete_resolution() {
  CaptureDescription& cap_descr = capture_devices_[devices_enum_.get()];
  pmanage<MPtr(&PContainer::remove)>(resolutions_id_);
  resolutions_id_ = 0;

  if (!cap_descr.frame_size_discrete_.empty()) {
    width_ = -1;
    height_ = -1;
    std::vector<std::string> names;

    for (auto& it : cap_descr.frame_size_discrete_)
      names.push_back(std::string(it.first) + "x" + std::string(it.second));

    resolutions_enum_ = Selection<>(std::move(names), 0);
    resolutions_id_ = pmanage<MPtr(&PContainer::make_parented_selection<>)>(
        "resolution",
        "config",
        [this](const size_t& val) {
          resolutions_enum_.select(val);
          fetch_available_frame_intervals();
          update_discrete_framerate();
          return true;
        },
        [this]() { return resolutions_enum_.get(); },
        "Resolution",
        "Resolution of selected capture devices",
        resolutions_enum_);

    pmanage<MPtr(&PContainer::set<size_t>)>(resolutions_id_, 0);
  }
}

void V4L2Src::update_discrete_framerate() {
  CaptureDescription& cap_descr = capture_devices_[devices_enum_.get()];
  pmanage<MPtr(&PContainer::remove)>(framerates_enum_id_);
  framerates_enum_id_ = 0;
  if (cap_descr.frame_interval_discrete_.empty()) return;
  std::vector<std::string> names;
  for (auto& it : cap_descr.frame_interval_discrete_) {
    // inversing enumerator and denominator because gst wants
    // framerate while v4l2 gives frame interval
    names.push_back(it.second + "/" + it.first);
  }

  framerates_enum_ = Selection<>(std::move(names), 0);
  framerates_enum_id_ = pmanage<MPtr(&PContainer::make_parented_selection<>)>(
      "framerate",
      "config",
      [this](const size_t& val) {
        framerates_enum_.select(val);
        return true;
      },
      [this]() { return framerates_enum_.get(); },
      "Framerate",
      "Framerate of selected capture devices",
      framerates_enum_);
}

bool V4L2Src::is_current_pixel_format_raw_video() const {
  std::string video_raw("video/x-raw");
  if (std::string(pixel_format_enum_.get_attached(), 0, video_raw.size()) != video_raw)
    return false;
  return true;
}

void V4L2Src::update_pixel_format() {
  CaptureDescription& cap_descr = capture_devices_[devices_enum_.get()];
  pmanage<MPtr(&PContainer::remove)>(pixel_format_id_);
  pixel_format_id_ = 0;
  if (cap_descr.pixel_formats_.empty()) return;
  std::vector<std::string> nicks;
  std::vector<std::string> names;
  for (auto& it : cap_descr.pixel_formats_) {
    nicks.push_back(std::get<1>(it));
    names.push_back(std::get<2>(it));
  }
  pixel_format_enum_ = Selection<>(std::make_pair(std::move(names), std::move(nicks)), 0);
  pixel_format_id_ = pmanage<MPtr(&PContainer::make_parented_selection<>)>(
      "pixel_format",
      "config",
      [this](const size_t& val) {
        pixel_format_enum_.select(val);
        set_shm_suffix();
        if (!is_current_pixel_format_raw_video()) codecs_->set_none();
        g_debug("pix selected");
        fetch_available_resolutions();
        update_discrete_resolution();
        update_width_height();
        return true;
      },
      [this]() { return pixel_format_enum_.get(); },
      "Pixel format",
      "Pixel format of selected capture devices",
      pixel_format_enum_);

  pmanage<MPtr(&PContainer::set<size_t>)>(pixel_format_id_, 0);
}

void V4L2Src::update_width_height() {
  CaptureDescription& cap_descr = capture_devices_[devices_enum_.get()];
  pmanage<MPtr(&PContainer::remove)>(width_id_);
  width_id_ = 0;
  pmanage<MPtr(&PContainer::remove)>(height_id_);
  height_id_ = 0;

  if (cap_descr.frame_size_stepwise_max_width_ < 1) return;

  width_ = cap_descr.frame_size_stepwise_max_width_ / 2;
  width_id_ =
      pmanage<MPtr(&PContainer::make_parented_int)>("width",
                                                    "config",
                                                    [this](const int& val) {
                                                      width_ = val;
                                                      fetch_available_frame_intervals();
                                                      update_framerate_numerator_denominator();
                                                      return true;
                                                    },
                                                    [this]() { return width_; },
                                                    "Width",
                                                    "Capture width",
                                                    width_,
                                                    cap_descr.frame_size_stepwise_min_width_,
                                                    cap_descr.frame_size_stepwise_max_width_);
  height_ = cap_descr.frame_size_stepwise_max_height_ / 2;
  height_id_ =
      pmanage<MPtr(&PContainer::make_parented_int)>("height",
                                                    "config",
                                                    [this](const int& val) {
                                                      height_ = val;
                                                      fetch_available_frame_intervals();
                                                      update_framerate_numerator_denominator();
                                                      return true;
                                                    },
                                                    [this]() { return height_; },
                                                    "Height",
                                                    "Capture height",
                                                    height_,
                                                    cap_descr.frame_size_stepwise_min_height_,
                                                    cap_descr.frame_size_stepwise_max_height_);

  fetch_available_frame_intervals();
  pmanage<MPtr(&PContainer::set<int>)>(width_id_, width_);
  pmanage<MPtr(&PContainer::set<int>)>(height_id_, height_);
}

void V4L2Src::update_framerate_numerator_denominator() {
  CaptureDescription& cap_descr = capture_devices_[devices_enum_.get()];
  pmanage<MPtr(&PContainer::remove)>(framerate_id_);
  framerate_id_ = 0;
  if (cap_descr.frame_interval_stepwise_max_numerator_ < 1) return;
  framerate_id_ =
      pmanage<MPtr(&PContainer::make_parented_fraction)>("framerate",
                                                         "config",
                                                         [this](const Fraction& val) {
                                                           framerate_ = val;
                                                           return true;
                                                         },
                                                         [this]() { return framerate_; },
                                                         "Framerate",
                                                         "Capture framerate",
                                                         framerate_,
                                                         1,
                                                         1,
                                                         120,
                                                         120);
}

void V4L2Src::update_tv_standard() {
  CaptureDescription& cap_descr = capture_devices_[devices_enum_.get()];
  pmanage<MPtr(&PContainer::remove)>(tv_standards_id_);
  tv_standards_id_ = 0;
  if (cap_descr.tv_standards_.empty()) return;
  std::vector<std::string> names;
  for (auto& it : cap_descr.tv_standards_) names.push_back(it);
  tv_standards_enum_ = Selection<>(std::move(names), 0);
  tv_standards_id_ = pmanage<MPtr(&PContainer::make_parented_selection<>)>(
      "tv_standard",
      "config",
      [this](const size_t& val) {
        tv_standards_enum_.select(val);
        return true;
      },
      [this]() { return tv_standards_enum_.get(); },
      "TV standard",
      "TV standard of selected capture devices",
      tv_standards_enum_);
}

bool V4L2Src::remake_elements() {
  if (capture_devices_.empty()) {
    g_debug("V4L2Src: no capture device available for starting capture");
    return false;
  }
  if (!UGstElem::renew(v4l2src_, {"device", "norm"}) || !UGstElem::renew(capsfilter_, {"caps"}) ||
      !UGstElem::renew(shmsink_, {"socket-path", "initial-size"})) {
    g_warning("V4L2Src: issue when with elements for video capture");
    return false;
  }
  return true;
}

std::string V4L2Src::pixel_format_to_string(unsigned pf_id) {
  std::string pixfmt;
  pixfmt += (char)(pf_id & 0xff);
  pixfmt += (char)((pf_id >> 8) & 0xff);
  pixfmt += (char)((pf_id >> 16) & 0xff);
  pixfmt += (char)((pf_id >> 24) & 0xff);
  return pixfmt;
}

bool V4L2Src::inspect_file_device(const char* file_path) {
  int fd = open(file_path, O_RDONLY);
  if (fd < 0) {
    g_debug("V4L2Src: inspecting file gets negative file descriptor");
    return false;
  }

  On_scope_exit { close(fd); };

  CaptureDescription description;
  description.absolute_path_ = file_path;
  struct v4l2_capability vcap;
  ioctl(fd, VIDIOC_QUERYCAP, &vcap);
  description.file_device_ = file_path;
  description.card_ = (char*)vcap.card;
  description.bus_info_ = (char*)vcap.bus_info;
  description.driver_ = (char*)vcap.driver;
  // g_print ("-------------------------- card %s bus %s driver %s\n",
  //        (char *)vcap.card,
  //        (char *)vcap.bus_info,
  //        (char *)vcap.driver);
  // pixel format

  v4l2_fmtdesc fmt;
  unsigned default_pixel_format = 0;
  memset(&fmt, 0, sizeof(fmt));
  fmt.index = 0;
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  while (ioctl(fd, VIDIOC_ENUM_FMT, &fmt) >= 0) {
    if (fmt.pixelformat != 0) {
      if (default_pixel_format == 0) default_pixel_format = fmt.pixelformat;
      // g_print ("******** pixel format  %s \n %s",
      //          pixel_format_to_string(fmt.pixelformat).c_str (),
      //          (const char *)fmt.description);
      GstStructure* structure = gst_v4l2_object_v4l2fourcc_to_structure(fmt.pixelformat);
      if (nullptr != structure) {
        GstCaps* caps = gst_caps_new_full(structure, nullptr);
        On_scope_exit { gst_caps_unref(caps); };
        gchar* tmp = gst_caps_to_string(caps);
        On_scope_exit { g_free(tmp); };
        description.pixel_formats_.push_back(
            std::make_tuple(fmt.pixelformat, tmp, reinterpret_cast<const char*>(fmt.description)));
      } else {
        g_warning("v4l2: pixel format %s not suported",
                  pixel_format_to_string(fmt.pixelformat).c_str());
      }
    }
    fmt.index++;
  }

  if (default_pixel_format == 0) {
    g_debug("no default pixel format found for %s, returning", file_path);
    return false;
  }

  v4l2_standard std;
  memset(&std, 0, sizeof(std));
  std.index = 0;
  description.tv_standards_.push_back("none");
  while (ioctl(fd, VIDIOC_ENUMSTD, &std) >= 0) {
    description.tv_standards_.push_back((char*)std.name);
    // g_print ("TV standard %s\n", (char *)std.name);
    std.index++;
  }

  capture_devices_.push_back(description);

  return true;
}

bool V4L2Src::check_folder_for_v4l2_devices(const char* dir_path) {
  GFile* inspected_dir = g_file_new_for_commandline_arg(dir_path);
  gboolean res;
  GError* error;
  GFileEnumerator* enumerator;
  GFileInfo* info;
  error = nullptr;
  enumerator = g_file_enumerate_children(
      inspected_dir, "*", G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, nullptr, &error);
  if (!enumerator) return false;
  error = nullptr;
  info = g_file_enumerator_next_file(enumerator, nullptr, &error);
  while ((info) && (!error)) {
    GFile* descend = g_file_get_child(inspected_dir, g_file_info_get_name(info));
    char* absolute_path = g_file_get_path(descend);
    On_scope_exit {
      if (nullptr != absolute_path) g_free(absolute_path);
    };
    if (g_str_has_prefix(absolute_path, "/dev/video")
        /*|| g_str_has_prefix (absolute_path, "/dev/radio")
          || g_str_has_prefix (absolute_path, "/dev/vbi")
          || g_str_has_prefix (absolute_path, "/dev/vtx") */
        ) {
      inspect_file_device(absolute_path);
    }
    g_object_unref(descend);
    error = nullptr;
    info = g_file_enumerator_next_file(enumerator, nullptr, &error);
  }
  if (error != nullptr) g_debug("error not nullptr");
  error = nullptr;
  res = g_file_enumerator_close(enumerator, nullptr, &error);
  if (res != TRUE) g_debug("V4L2Src: file enumerator not properly closed");
  if (error != nullptr) g_debug("V4L2Src: error not nullptr");
  g_object_unref(inspected_dir);
  return true;
}

bool V4L2Src::start() {
  configure_capture();
  g_object_set(G_OBJECT(gst_pipeline_->get_pipeline()), "async-handling", TRUE, nullptr);
  gst_bin_add_many(GST_BIN(gst_pipeline_->get_pipeline()),
                   v4l2src_.get_raw(),
                   capsfilter_.get_raw(),
                   shmsink_.get_raw(),
                   nullptr);
  gst_element_link_many(v4l2src_.get_raw(), capsfilter_.get_raw(), shmsink_.get_raw(), nullptr);
  shm_sub_ = std::make_unique<GstShmdataSubscriber>(
      shmsink_.get_raw(),
      [this](const std::string& caps) {
        this->graft_tree(
            ".shmdata.writer." + shmpath_,
            ShmdataUtils::make_tree(caps, ShmdataUtils::get_category(caps), ShmdataStat()));
      },
      ShmdataStat::make_tree_updater(this, ".shmdata.writer." + shmpath_));

  gst_pipeline_->play(true);
  if (!is_current_pixel_format_raw_video()) codecs_->set_none();
  codecs_->start();
  pmanage<MPtr(&PContainer::enable)>(devices_id_, false);
  pmanage<MPtr(&PContainer::enable)>(group_id_, false);
  pmanage<MPtr(&PContainer::enable)>(resolutions_id_, false);
  pmanage<MPtr(&PContainer::enable)>(width_id_, false);
  pmanage<MPtr(&PContainer::enable)>(height_id_, false);
  pmanage<MPtr(&PContainer::enable)>(tv_standards_id_, false);
  pmanage<MPtr(&PContainer::enable)>(framerates_enum_id_, false);
  pmanage<MPtr(&PContainer::enable)>(framerate_id_, false);
  pmanage<MPtr(&PContainer::enable)>(pixel_format_id_, false);
  return true;
}

bool V4L2Src::stop() {
  shm_sub_.reset(nullptr);
  prune_tree(".shmdata.writer." + shmpath_);
  remake_elements();
  gst_pipeline_ = std::make_unique<GstPipeliner>(
      nullptr, nullptr, [this](GstObject* gstobj, GError* err) { on_gst_error(gstobj, err); });
  codecs_->stop();
  pmanage<MPtr(&PContainer::enable)>(devices_id_, true);
  pmanage<MPtr(&PContainer::enable)>(group_id_, true);
  pmanage<MPtr(&PContainer::enable)>(resolutions_id_, true);
  pmanage<MPtr(&PContainer::enable)>(width_id_, true);
  pmanage<MPtr(&PContainer::enable)>(height_id_, true);
  pmanage<MPtr(&PContainer::enable)>(tv_standards_id_, true);
  pmanage<MPtr(&PContainer::enable)>(framerates_enum_id_, true);
  pmanage<MPtr(&PContainer::enable)>(framerate_id_, true);
  pmanage<MPtr(&PContainer::enable)>(pixel_format_id_, true);
  return true;
}

bool V4L2Src::configure_capture() {
  if (capture_devices_.empty()) {
    g_debug("V4L2Src:: no capture device available for starting capture");
    return false;
  }
  g_object_set(
      G_OBJECT(v4l2src_.get_raw()), "device", devices_enum_.get_attached().c_str(), nullptr);
  if (tv_standards_id_ != 0 && tv_standards_enum_.get() > 0)  // 0 is none
    g_object_set(
        G_OBJECT(v4l2src_.get_raw()), "norm", tv_standards_enum_.get_current().c_str(), nullptr);
  std::string caps = pixel_format_enum_.get_attached();
  if (0 != width_id_)
    caps = caps + ", width=(int)" + std::to_string(width_) + ", height=(int)" +
           std::to_string(height_);
  else if (0 != resolutions_id_)
    caps = caps + ", width=(int)" +
           capture_devices_[devices_enum_.get()]
               .frame_size_discrete_[resolutions_enum_.get()]
               .first.c_str() +
           ", height=(int)" +
           capture_devices_[devices_enum_.get()]
               .frame_size_discrete_[resolutions_enum_.get()]
               .second.c_str();
  if (0 != framerates_enum_id_)
    caps = caps + ", framerate=(fraction)" +
           capture_devices_[devices_enum_.get()]
               .frame_interval_discrete_[framerates_enum_.get()]
               .second.c_str() +
           "/" +
           capture_devices_[devices_enum_.get()]
               .frame_interval_discrete_[framerates_enum_.get()]
               .first.c_str();
  else if (0 != framerate_id_) {
    caps = caps + ", framerate=(fraction)" + std::to_string(framerate_.numerator()) + "/" +
           std::to_string(framerate_.denominator());
  }
  g_debug("caps for v4l2src %s", caps.c_str());
  GstCaps* usercaps = gst_caps_from_string(caps.c_str());
  g_object_set(G_OBJECT(capsfilter_.get_raw()), "caps", usercaps, nullptr);
  gst_caps_unref(usercaps);
  return true;
}

GstStructure* V4L2Src::gst_v4l2_object_v4l2fourcc_to_structure(guint32 fourcc) {
  GstStructure* structure = NULL;

  switch (fourcc) {
    case V4L2_PIX_FMT_MJPEG: /* Motion-JPEG */
#ifdef V4L2_PIX_FMT_PJPG
    case V4L2_PIX_FMT_PJPG: /* Progressive-JPEG */
#endif
    case V4L2_PIX_FMT_JPEG: /* JFIF JPEG */
      structure = gst_structure_new_empty("image/jpeg");
      break;
    case V4L2_PIX_FMT_YYUV:  /* 16  YUV 4:2:2     */
    case V4L2_PIX_FMT_HI240: /*  8  8-bit color   */
      /* FIXME: get correct fourccs here */
      break;
#ifdef V4L2_PIX_FMT_MPEG4
    case V4L2_PIX_FMT_MPEG4:
      structure = gst_structure_new(
          "video/mpeg", "mpegversion", G_TYPE_INT, 4, "systemstream", G_TYPE_BOOLEAN, FALSE, NULL);
      break;
#endif
#ifdef V4L2_PIX_FMT_H263
    case V4L2_PIX_FMT_H263:
      structure = gst_structure_new("video/x-h263", "variant", G_TYPE_STRING, "itu", NULL);
      break;
#endif
#ifdef V4L2_PIX_FMT_H264
    case V4L2_PIX_FMT_H264: /* H.264 */
      structure = gst_structure_new_empty("video/x-h264");
      break;
#endif
    case V4L2_PIX_FMT_RGB332:
    case V4L2_PIX_FMT_RGB555X:
    case V4L2_PIX_FMT_RGB565X:
      /* FIXME: get correct fourccs here */
      break;
    case V4L2_PIX_FMT_GREY: /*  8  Greyscale     */
    case V4L2_PIX_FMT_RGB555:
    case V4L2_PIX_FMT_RGB565:
    case V4L2_PIX_FMT_RGB24:
    case V4L2_PIX_FMT_BGR24:
    case V4L2_PIX_FMT_RGB32:
    case V4L2_PIX_FMT_BGR32:
    case V4L2_PIX_FMT_NV12: /* 12  Y/CbCr 4:2:0  */
    case V4L2_PIX_FMT_NV21: /* 12  Y/CrCb 4:2:0  */
    case V4L2_PIX_FMT_YVU410:
    case V4L2_PIX_FMT_YUV410:
    case V4L2_PIX_FMT_YUV420: /* I420/IYUV */
    case V4L2_PIX_FMT_YUYV:
    case V4L2_PIX_FMT_YVU420:
    case V4L2_PIX_FMT_UYVY:
#if 0
    case V4L2_PIX_FMT_Y41P:
#endif
    case V4L2_PIX_FMT_YUV422P:
#ifdef V4L2_PIX_FMT_YVYU
    case V4L2_PIX_FMT_YVYU:
#endif
    case V4L2_PIX_FMT_YUV411P: {
      GstVideoFormat format;

      switch (fourcc) {
        case V4L2_PIX_FMT_GREY: /*  8  Greyscale     */
          format = GST_VIDEO_FORMAT_GRAY8;
          break;
        case V4L2_PIX_FMT_RGB555:
          format = GST_VIDEO_FORMAT_RGB15;
          break;
        case V4L2_PIX_FMT_RGB565:
          format = GST_VIDEO_FORMAT_RGB16;
          break;
        case V4L2_PIX_FMT_RGB24:
          format = GST_VIDEO_FORMAT_RGB;
          break;
        case V4L2_PIX_FMT_BGR24:
          format = GST_VIDEO_FORMAT_BGR;
          break;
        case V4L2_PIX_FMT_RGB32:
          format = GST_VIDEO_FORMAT_RGBx;
          break;
        case V4L2_PIX_FMT_BGR32:
          format = GST_VIDEO_FORMAT_BGRx;
          break;
        case V4L2_PIX_FMT_NV12:
          format = GST_VIDEO_FORMAT_NV12;
          break;
        case V4L2_PIX_FMT_NV21:
          format = GST_VIDEO_FORMAT_NV21;
          break;
        case V4L2_PIX_FMT_YVU410:
          format = GST_VIDEO_FORMAT_YVU9;
          break;
        case V4L2_PIX_FMT_YUV410:
          format = GST_VIDEO_FORMAT_YUV9;
          break;
        case V4L2_PIX_FMT_YUV420:
          format = GST_VIDEO_FORMAT_I420;
          break;
        case V4L2_PIX_FMT_YUYV:
          format = GST_VIDEO_FORMAT_YUY2;
          break;
        case V4L2_PIX_FMT_YVU420:
          format = GST_VIDEO_FORMAT_YV12;
          break;
        case V4L2_PIX_FMT_UYVY:
          format = GST_VIDEO_FORMAT_UYVY;
          break;
#if 0
        case V4L2_PIX_FMT_Y41P:
          format = GST_VIDEO_FORMAT_Y41P;
          break;
#endif
        case V4L2_PIX_FMT_YUV411P:
          format = GST_VIDEO_FORMAT_Y41B;
          break;
        case V4L2_PIX_FMT_YUV422P:
          format = GST_VIDEO_FORMAT_Y42B;
          break;
#ifdef V4L2_PIX_FMT_YVYU
        case V4L2_PIX_FMT_YVYU:
          format = GST_VIDEO_FORMAT_YVYU;
          break;
#endif
        default:
          format = GST_VIDEO_FORMAT_UNKNOWN;
          g_assert_not_reached();
          break;
      }
      structure = gst_structure_new(
          "video/x-raw", "format", G_TYPE_STRING, gst_video_format_to_string(format), NULL);
      break;
    }
    case V4L2_PIX_FMT_DV:
      structure = gst_structure_new("video/x-dv", "systemstream", G_TYPE_BOOLEAN, TRUE, NULL);
      break;
    case V4L2_PIX_FMT_MPEG: /* MPEG          */
      structure = gst_structure_new("video/mpegts", "systemstream", G_TYPE_BOOLEAN, TRUE, NULL);
      break;
    case V4L2_PIX_FMT_WNVA: /* Winnov hw compres */
      break;
#ifdef V4L2_PIX_FMT_SBGGR8
    case V4L2_PIX_FMT_SBGGR8:
      structure = gst_structure_new_empty("video/x-bayer");
      break;
#endif
#ifdef V4L2_PIX_FMT_SN9C10X
    case V4L2_PIX_FMT_SN9C10X:
      structure = gst_structure_new_empty("video/x-sonix");
      break;
#endif
#ifdef V4L2_PIX_FMT_PWC1
    case V4L2_PIX_FMT_PWC1:
      structure = gst_structure_new_empty("video/x-pwc1");
      break;
#endif
#ifdef V4L2_PIX_FMT_PWC2
    case V4L2_PIX_FMT_PWC2:
      structure = gst_structure_new_empty("video/x-pwc2");
      break;
#endif
    default:
      GST_DEBUG("Unknown fourcc 0x%08x %" GST_FOURCC_FORMAT, fourcc, GST_FOURCC_ARGS(fourcc));
      break;
  }
  return structure;
}

void V4L2Src::on_gst_error(GstObject*, GError* err) {
  g_message("ERROR:camera error %s", err->message);
  self_destruct();
}

}  // namespace switcher
