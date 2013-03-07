/*
 * Copyright (C) 2012-2013 Nicolas Bouillot (http://www.nicolasbouillot.net)
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

#include "switcher/file-sdp.h"
#include <glib/gprintf.h>
#include "switcher/gst-utils.h"

namespace switcher
{
  QuiddityDocumentation FileSDP::doc_ ("sdp decoding", "filesdp", 
				       "get raw stream from sdp file");
  
  bool
  FileSDP::init() 
  { 
    if (!GstUtils::make_element ("filesrc", &filesrc_)
	|| !GstUtils::make_element ("sdpdemux", &sdpdemux_))
      return false;

    media_counter_ = 0;
    //set the name before registering properties
    set_name (gst_element_get_name (filesrc_));
    add_element_to_cleaner (filesrc_);
    add_element_to_cleaner (sdpdemux_);
    
    g_signal_connect (G_OBJECT (sdpdemux_), 
		      "pad-added", 
		      (GCallback) FileSDP::pad_added_cb,
		      (gpointer) this);
    g_signal_connect (G_OBJECT (sdpdemux_),  
		      "no-more-pads",  
		      (GCallback) FileSDP::no_more_pads_cb ,  
		      (gpointer) this);    

    // g_signal_connect (G_OBJECT (sdpdemux_),  
    // 		      "pad-removed",  
    // 		      (GCallback) FileSDP::pad_removed_cb ,  
    // 		      (gpointer) this);      
   
    //registering add_data_stream
    register_method("to_shmdata",
		    (void *)&to_shmdata_wrapped, 
		    Method::make_arg_type_description (G_TYPE_STRING, NULL),
		    (gpointer)this);
    set_method_description ("to_shmdata", 
			    "get raw streams from an sdp description distributed over http and write them to shmdatas", 
			    Method::make_arg_description ("url", 
							  "the url to the sdp file",
							  NULL));
    //registering "latency"
    register_property (G_OBJECT (sdpdemux_),"latency","sdpdemux");
    return true;
  }
  
  QuiddityDocumentation 
  FileSDP::get_documentation ()
  {
    return doc_;
  }

  void 
  FileSDP::no_more_pads_cb (GstElement* object, gpointer user_data)   
  {   
    //g_print ("no more pad");
    //FileSDP *context = static_cast<FileSDP *>(user_data);
  }


  void 
  FileSDP::pad_added_cb (GstElement* object, GstPad* pad, gpointer user_data)   
  {   
    FileSDP *context = static_cast<FileSDP *>(user_data);
    
    const gchar *padname= gst_structure_get_name (gst_caps_get_structure(gst_pad_get_caps (pad),0));
    g_debug ("httpsdp new pad name is %s",padname);
    g_debug ("httpsdp new caps %s",gst_caps_to_string (gst_pad_get_caps (pad)));

    GstElement *identity;
    GstUtils::make_element ("identity", &identity);
    g_object_set (identity, "sync", TRUE, NULL);

    gst_bin_add (GST_BIN (context->bin_), identity);
    GstUtils::link_static_to_request (pad, identity);
    GstUtils::sync_state_with_parent (identity);
    
    //giving a name to the stream
    gchar **padname_splitted = g_strsplit_set (padname, "/",-1);
    gchar media_name[256];
    g_sprintf (media_name,"%s_%d",padname_splitted[0],context->media_counter_);
    context->media_counter_++;
    g_strfreev(padname_splitted);

    //creating a shmdata
    ShmdataWriter::ptr connector;
    connector.reset (new ShmdataWriter ());
    std::string connector_name = context->make_file_name (media_name);
    connector->set_path (connector_name.c_str());
    //connector->plug (context->bin_, pad);
    GstCaps *caps = gst_pad_get_caps_reffed (pad);
    connector->plug (context->bin_, identity, caps);
    if (G_IS_OBJECT (caps))
      gst_object_unref (caps);
    context->shmdata_writers_.insert (connector_name, connector);
    g_message ("%s created a new shmdata writer (%s)", 
	       context->get_nick_name ().c_str(), 
	       connector_name.c_str ());
  }   

  
  gboolean
  FileSDP::to_shmdata_wrapped (gpointer uri, 
			      gpointer user_data)
  {
    FileSDP *context = static_cast<FileSDP *>(user_data);
    
    if (context->to_shmdata ((char *)uri))
      return TRUE;
    else
      return FALSE;
  }

  bool
  FileSDP::to_shmdata (std::string uri)
  {
    g_debug ("FileSDP::to_shmdata set location %s", uri.c_str ());
    g_object_set (G_OBJECT (filesrc_), "location", uri.c_str (), NULL);  
    gst_bin_add_many (GST_BIN (bin_), filesrc_, sdpdemux_, NULL);
    gst_element_link (filesrc_,sdpdemux_);
    GstUtils::sync_state_with_parent (filesrc_);
    GstUtils::sync_state_with_parent (sdpdemux_);
    g_debug ("to_shmdata end");
    return true;
  }


}