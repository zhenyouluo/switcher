/*
 * Copyright (C) 2012 Nicolas Bouillot (http://www.nicolasbouillot.net)
 *
 * This file is part of switcher.
 *
 * switcher is free software: you can redistribute it and/or modify
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

#include "switcher/audio-source.h"
#include <memory>

namespace switcher
{
  AudioSource::AudioSource() 
  { 
    audio_tee_ = gst_element_factory_make ("tee",NULL);
    audioconvert_ = gst_element_factory_make ("audioconvert",NULL);
    pitch_ = gst_element_factory_make ("pitch",NULL);
    resample_ = gst_element_factory_make ("audioresample",NULL);
    
    gst_bin_add_many (GST_BIN (bin_),
		      audio_tee_,
		      //audio_queue_,
		      audioconvert_,
		      pitch_,
		      resample_,
		      NULL);
    gst_element_link_many (audio_tee_,
			   //audio_queue_,
			   audioconvert_,
			   pitch_,
			   resample_,
			   NULL);
    
    register_property (G_OBJECT (pitch_),"output-rate","pitch");
    register_property (G_OBJECT (pitch_),"rate","pitch");
    register_property (G_OBJECT (pitch_),"tempo","pitch");
    register_property (G_OBJECT (pitch_),"pitch","pitch");

  }

  void 
  AudioSource::set_raw_audio_element (GstElement *elt)
  {
    rawaudio_ = elt;
    
    gst_bin_add (GST_BIN (bin_), rawaudio_);
    gst_element_link (rawaudio_, audio_tee_);

    GstCaps *audiocaps = gst_caps_new_simple ("audio/x-raw-float",
     					      NULL);

    //creating a connector for raw audio
    ShmdataWriter::ptr rawaudio_connector;
    rawaudio_connector.reset (new ShmdataWriter ());
    std::string rawconnector_name = make_file_name ("rawaudio");
    rawaudio_connector->set_path (rawconnector_name.c_str());
    rawaudio_connector->plug (bin_, audio_tee_, audiocaps);
    shmdata_writers_.insert (rawconnector_name, rawaudio_connector);
    
    //creating a connector for transformed audio
    ShmdataWriter::ptr audio_connector;
    audio_connector.reset (new ShmdataWriter ());
    std::string connector_name = make_file_name ("audio");
    audio_connector->set_path (connector_name.c_str());
    audio_connector->plug (bin_, audio_tee_, audiocaps);
    shmdata_writers_.insert (connector_name, audio_connector);
    
    //gst_object_unref (audiocaps);
    gst_caps_unref(audiocaps);
  }

}
