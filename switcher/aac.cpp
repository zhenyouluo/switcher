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

#include "switcher/aac.h"

namespace switcher
{
  QuiddityDocumentation AAC::doc_ ("audio encoder", "voaacenc",
				      "AAC encoder");

  AAC::AAC ()
  {
    make_aac ();
  }

  AAC::AAC (QuiddityLifeManager::ptr life_manager)
  {
    life_manager_ = life_manager;
    make_aac ();
  }

  void 
  AAC::make_aac ()
  {
    aacbin_ = gst_element_factory_make ("bin",NULL);
    aacenc_ = gst_element_factory_make ("voaacenc",NULL);
    //set the name before registering properties
    name_ = gst_element_get_name (aacenc_);
    set_sink_element (aacbin_);
    set_on_first_data_hook (AAC::make_shmdata_writer,this);
  }
  
  QuiddityDocumentation 
  AAC::get_documentation ()
  {
    return doc_;
  }
  
  void 
  AAC::make_shmdata_writer(ShmdataReader *caller, void *aacbin_instance)
  {
    AAC *context = static_cast<AAC *>(aacbin_instance);

    caller->set_sink_element (context->aacbin_);
    gst_bin_add (GST_BIN (context->bin_), context->aacbin_);
    gst_element_sync_state_with_parent (context->bin_);
    gst_element_sync_state_with_parent (context->aacbin_);
    
    GstElement *audioconvert = gst_element_factory_make ("audioconvert",NULL);

    gst_bin_add_many (GST_BIN (context->aacbin_),
		      context->aacenc_,
		      audioconvert,
		      NULL);
    gst_element_link_many (audioconvert,
			   context->aacenc_,
			   NULL);
    
    gst_element_sync_state_with_parent (audioconvert);
    gst_element_sync_state_with_parent (context->aacenc_);

    GstPad *sink_pad = gst_element_get_static_pad (audioconvert, "sink");
    GstPad *ghost_sinkpad = gst_ghost_pad_new (NULL, sink_pad);
    gst_pad_set_active(ghost_sinkpad,TRUE);
    gst_element_add_pad (context->aacbin_, ghost_sinkpad); 
    gst_object_unref (sink_pad);
    
     GstCaps *aaccaps = gst_caps_new_simple ("audio/mpeg", NULL);
     ShmdataWriter::ptr aacframes_writer;
     aacframes_writer.reset (new ShmdataWriter ());
     std::string writer_name = context->make_shmdata_writer_name ("aacframes"); 
     aacframes_writer->set_absolute_name (writer_name.c_str());
     aacframes_writer->plug (context->bin_, context->aacenc_, aaccaps);
     context->shmdata_writers_.insert (writer_name, aacframes_writer);
    
  }

}