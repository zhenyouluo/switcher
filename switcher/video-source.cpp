/*
 * Copyright (C) 2012-2013 Nicolas Bouillot (http://www.nicolasbouillot.net)
 *
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

#include "video-source.h"
#include "gst-utils.h"

namespace switcher
{
  VideoSource::VideoSource () 
  {
    video_tee_ = NULL;
    videocaps_ = gst_caps_new_simple ("video/x-raw-yuv",
				      // "format", GST_TYPE_FOURCC,
				      // GST_MAKE_FOURCC ('U', 'Y', 'V', 'Y'),
				      // "format", GST_TYPE_FOURCC,
				      // GST_MAKE_FOURCC ('I', '4', '2', '0'),
				      //"framerate", GST_TYPE_FRACTION, 30, 1,
				      // "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1, 
				      //  "width", G_TYPE_INT, 640, 
				      //  "height", G_TYPE_INT, 480,
				      NULL);
    
  }

  VideoSource::~VideoSource () 
  {
    clean_video_source_elements ();
    gst_caps_unref (videocaps_);
  }

  void 
  VideoSource::make_elements () //FIXME rename that
  {
    if (!GstUtils::make_element ("tee",&video_tee_))
      g_warning ("VideoSource: tee element is mandatory\n");

     gst_bin_add_many (GST_BIN (bin_),
      		      video_tee_,
      		      NULL);

  }

  void 
  VideoSource::clean_video_source_elements () 
  {
    unregister_shmdata_writer (make_file_name ("video"));
  }

  void
  VideoSource::set_raw_video_element (GstElement *element)
  {
    reset_bin ();
    clean_video_source_elements ();
    make_elements ();

    rawvideo_ = element;
    
    gst_bin_add (GST_BIN (bin_), rawvideo_);
    gst_element_link (rawvideo_, video_tee_);
    
    //creating a connector for the raw video
    ShmdataWriter::ptr rawvideo_connector;
    rawvideo_connector.reset (new ShmdataWriter ());
    std::string rawconnector_name = make_file_name ("video"); 
    rawvideo_connector->set_path (rawconnector_name.c_str());
    rawvideo_connector->plug (bin_, video_tee_, videocaps_);
    register_shmdata_writer (rawvideo_connector);
    
    GstUtils::wait_state_changed (bin_);
    GstUtils::sync_state_with_parent (rawvideo_);
    GstUtils::sync_state_with_parent (video_tee_);

  }

}
