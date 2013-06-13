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

#include "quiddity-documentation.h"
#include "quiddity-manager-impl.h"
#include "quiddity.h" 

//removing shmdata 
#include <gio/gio.h>

//the quiddities to manage (line sorted)
#include "aac.h"
#include "aravis-genicam.h"
#include "audio-test-source.h"
#include "create-remove-spy.h"
#include "decodebin2.h"
#include "deinterleave.h"
#include "fake-shmdata-writer.h"
#include "fakesink.h"
#include "file-sdp.h"
#include "gst-parse-to-bin-src.h"
#include "gst-video-parse-to-bin-src.h"
#include "h264.h"
#include "http-sdp.h"
#include "http-sdp-dec.h"
#include "logger.h"
#include "osc-ctrl-server.h"
#include "pulse-sink.h"
#include "rtp-session.h"
#include "runtime.h"
#include "shmdata-to-file.h"
#include "shmdata-from-gdp-file.h"
#include "soap-ctrl-client.h"
#include "soap-ctrl-server.h"
#include "udpsink.h"
#include "uridecodebin.h"
#include "uris.h"
#include "video-rate.h"
#include "video-test-source.h"
#include "vorbis.h"
#include "xvimagesink.h"

namespace switcher
{

  QuiddityManager_Impl::ptr 
  QuiddityManager_Impl::make_manager ()
  {
    QuiddityManager_Impl::ptr manager(new QuiddityManager_Impl);
    return manager;
  }
  
  QuiddityManager_Impl::ptr 
  QuiddityManager_Impl::make_manager (std::string name)
  {
    QuiddityManager_Impl::ptr manager(new QuiddityManager_Impl(name));
    return manager;
  }

  QuiddityManager_Impl::QuiddityManager_Impl() 
  {
    init ("default");
  }
  
  QuiddityManager_Impl::QuiddityManager_Impl(std::string name) 
  {
    init (name);
  }

  void
  QuiddityManager_Impl::init (std::string name)
  {
    name_ = name;
    init_gmainloop ();
    creation_hook_ = NULL;
    removal_hook_ = NULL;
    creation_hook_user_data_ = NULL;
    removal_hook_user_data_ = NULL;
    remove_shmdata_sockets ();
    register_classes ();
    classes_doc_.reset (new JSONBuilder ());
    make_classes_doc ();
  }

  QuiddityManager_Impl::~QuiddityManager_Impl()
  {
    g_main_loop_quit (mainloop_);
    g_main_context_unref (main_context_);
    
  }

  void
  QuiddityManager_Impl::remove_shmdata_sockets ()
  {
   
    GFile *shmdata_dir = g_file_new_for_commandline_arg (Quiddity::get_socket_dir().c_str ());

    gchar *shmdata_prefix = g_strconcat (Quiddity::get_socket_name_prefix ().c_str (), 
					 name_.c_str (), 
					 "_",
					 NULL);
    
    gboolean res;
    GError *error;
    GFileEnumerator *enumerator;
    GFileInfo *info;
    GFile *descend;
    char *relative_path;
    
    error = NULL;
    enumerator =
      g_file_enumerate_children (shmdata_dir, "*",
				 G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL,
				 &error);
    if (! enumerator)
      return;
    error = NULL;
    info = g_file_enumerator_next_file (enumerator, NULL, &error);
    while ((info) && (!error))
      {
	descend = g_file_get_child (shmdata_dir, g_file_info_get_name (info));
	//g_assert (descend != NULL);
	relative_path = g_file_get_relative_path (shmdata_dir, descend);
	
	error = NULL;
	
	if (g_str_has_prefix (relative_path, shmdata_prefix))
	  {
	    g_warning ("deleting previous shmdata socket (%s)", g_file_get_path (descend));
	    res = g_file_delete (descend, NULL, &error);
	    if(res != TRUE)
	      g_warning ("socket cannot be deleted");
	  }
	
	g_object_unref (descend);
	error = NULL;
	info = g_file_enumerator_next_file (enumerator, NULL, &error);
      }
    if (error != NULL)
      g_debug ("error not NULL");
    
    error = NULL;
    res = g_file_enumerator_close (enumerator, NULL, &error);
    if (res != TRUE)
      g_debug ("QuiddityManager_Impl: file enumerator not properly closed");
    if (error != NULL)
      g_debug ("error not NULL");
    
    g_object_unref (shmdata_dir);
    g_free (shmdata_prefix);
  }
  
  std::string
  QuiddityManager_Impl::get_name ()
  {
    return name_; 
  }
  
  void
  QuiddityManager_Impl::register_classes ()
  {
    //registering quiddities
    abstract_factory_.register_class<AAC> (AAC::doc_.get_class_name (), 
					   AAC::doc_.get_json_root_node ());
    abstract_factory_.register_class<AudioTestSource> (AudioTestSource::doc_.get_class_name (), 
      						       AudioTestSource::doc_.get_json_root_node ());
    abstract_factory_.register_class<AravisGenicam> (AravisGenicam::doc_.get_class_name (), 
      						     AravisGenicam::doc_.get_json_root_node ());
    abstract_factory_.register_class<CreateRemoveSpy> (CreateRemoveSpy::doc_.get_class_name (), 
						       CreateRemoveSpy::doc_.get_json_root_node ());
    abstract_factory_.register_class<Decodebin2> (Decodebin2::doc_.get_class_name (), 
						  Decodebin2::doc_.get_json_root_node ());
    abstract_factory_.register_class<Deinterleave> (Deinterleave::doc_.get_class_name (), 
						    Deinterleave::doc_.get_json_root_node ());
    abstract_factory_.register_class<FakeShmdataWriter> (FakeShmdataWriter::doc_.get_class_name (), 
							 FakeShmdataWriter::doc_.get_json_root_node ());
    abstract_factory_.register_class<FakeSink> (FakeSink::doc_.get_class_name (), 
						FakeSink::doc_.get_json_root_node ());
    abstract_factory_.register_class<FileSDP> (FileSDP::doc_.get_class_name (), 
					       FileSDP::doc_.get_json_root_node ());
    abstract_factory_.register_class<GstParseToBinSrc> (GstParseToBinSrc::doc_.get_class_name (),
      							GstParseToBinSrc::doc_.get_json_root_node ());
    abstract_factory_.register_class<GstVideoParseToBinSrc> (GstVideoParseToBinSrc::doc_.get_class_name (),
							     GstVideoParseToBinSrc::doc_.get_json_root_node ());
    abstract_factory_.register_class<H264> (H264::doc_.get_class_name (), 
      					    H264::doc_.get_json_root_node ());
    abstract_factory_.register_class<HTTPSDP> (HTTPSDP::doc_.get_class_name (), 
      					       HTTPSDP::doc_.get_json_root_node ());
    abstract_factory_.register_class<HTTPSDPDec> (HTTPSDPDec::doc_.get_class_name (), 
      					       HTTPSDPDec::doc_.get_json_root_node ());
    abstract_factory_.register_class<Logger> (Logger::doc_.get_class_name (), 
      					       Logger::doc_.get_json_root_node ());
    abstract_factory_.register_class<OscCtrlServer> (OscCtrlServer::doc_.get_class_name (), 
						     OscCtrlServer::doc_.get_json_root_node ());
    abstract_factory_.register_class<PulseSink> (PulseSink::doc_.get_class_name (), 
      						 PulseSink::doc_.get_json_root_node ());
    abstract_factory_.register_class<RtpSession> (RtpSession::doc_.get_class_name (), 
      						  RtpSession::doc_.get_json_root_node ());
    abstract_factory_.register_class<Runtime> (Runtime::doc_.get_class_name (), 
       					       Runtime::doc_.get_json_root_node ());
    abstract_factory_.register_class<ShmdataFromGDPFile> (ShmdataFromGDPFile::doc_.get_class_name (), 
      							  ShmdataFromGDPFile::doc_.get_json_root_node ());
    abstract_factory_.register_class<ShmdataToFile> (ShmdataToFile::doc_.get_class_name (), 
      						     ShmdataToFile::doc_.get_json_root_node ());
    abstract_factory_.register_class<SoapCtrlClient> (SoapCtrlClient::doc_.get_class_name (), 
      						      SoapCtrlClient::doc_.get_json_root_node ());
    abstract_factory_.register_class<SoapCtrlServer> (SoapCtrlServer::doc_.get_class_name (), 
      						      SoapCtrlServer::doc_.get_json_root_node ());
    abstract_factory_.register_class<UDPSink> (UDPSink::doc_.get_class_name (), 
       					       UDPSink::doc_.get_json_root_node ());
    abstract_factory_.register_class<Uridecodebin> (Uridecodebin::doc_.get_class_name (), 
       						    Uridecodebin::doc_.get_json_root_node ());
    abstract_factory_.register_class<Uris> (Uris::doc_.get_class_name (), 
     					    Uris::doc_.get_json_root_node ());
    abstract_factory_.register_class<VideoRate> (VideoRate::doc_.get_class_name (),
     						 VideoRate::doc_.get_json_root_node ());
    abstract_factory_.register_class<VideoTestSource> (VideoTestSource::doc_.get_class_name (),
						       VideoTestSource::doc_.get_json_root_node ());
    abstract_factory_.register_class<Vorbis> (Vorbis::doc_.get_class_name (),
     					      Vorbis::doc_.get_json_root_node ());
    abstract_factory_.register_class<Xvimagesink> (Xvimagesink::doc_.get_class_name (),
       						   Xvimagesink::doc_.get_json_root_node ());
  }

  std::vector<std::string> 
  QuiddityManager_Impl::get_classes ()
  {
    //return abstract_factory_.get_classes_documentation ();
    return abstract_factory_.get_keys ();
  }

  void
  QuiddityManager_Impl::make_classes_doc ()
  {
    std::vector<JSONBuilder::Node> docs = abstract_factory_.get_classes_documentation ();
    classes_doc_->reset ();
    classes_doc_->begin_object ();
    classes_doc_->set_member_name ("classes");
    classes_doc_->begin_array ();
    for(std::vector<JSONBuilder::Node>::iterator it = docs.begin(); it != docs.end(); ++it) 
      classes_doc_->add_node_value (*it);
    classes_doc_->end_array ();
    classes_doc_->end_object ();
  }

  std::string 
  QuiddityManager_Impl::get_classes_doc ()
  {
    return classes_doc_->get_string (true);
  }
  
  std::string 
  QuiddityManager_Impl::get_class_doc (std::string class_name)
  {
    if (abstract_factory_.key_exists (class_name))
      {
	JSONBuilder::Node doc = abstract_factory_.get_class_documentation (class_name);
	return JSONBuilder::get_string (doc, true);
      }
    else
      return "{ \"error\":\"class not found\" }";
  }

  bool 
  QuiddityManager_Impl::class_exists (std::string class_name)
  {
    return abstract_factory_.key_exists (class_name);
  }


  bool 
  QuiddityManager_Impl::init_quiddity (Quiddity::ptr quiddity)
  {
    quiddity->set_manager_impl (shared_from_this());
    if (!quiddity->init ())
      return false;
    // g_critical ("QuiddityManager_Impl: intialization of %s (%s) return false",
    // 	       quiddity->get_name ().c_str (),
    // 	       quiddity->get_documentation ().get_class_name ().c_str ());
    quiddities_.insert (quiddity->get_name(),quiddity);
    quiddities_nick_names_.insert (quiddity->get_nick_name (),quiddity->get_name());
    
    if (creation_hook_ != NULL)
      (*creation_hook_) (quiddity->get_nick_name (), creation_hook_user_data_);

    return true;
  }

  //for use of the "get description by class" methods 
  std::string 
  QuiddityManager_Impl::create_without_hook (std::string quiddity_class)
  {
    if(!class_exists (quiddity_class))
      return "";
    
    Quiddity::ptr quiddity = abstract_factory_.create (quiddity_class);
    if (quiddity.get() != NULL)
      {
	quiddity->set_manager_impl (shared_from_this());
	if (!quiddity->init ())
	  "{\"error\":\"cannot init quiddity class\"}";
	quiddities_.insert (quiddity->get_name(),quiddity);
	quiddities_nick_names_.insert (quiddity->get_nick_name (),quiddity->get_name());
      }
    return quiddity->get_nick_name ();
  }

  std::string 
  QuiddityManager_Impl::create (std::string quiddity_class)
  {
     if(!class_exists (quiddity_class))
      return "";
    
     Quiddity::ptr quiddity = abstract_factory_.create (quiddity_class);
     if (quiddity.get() != NULL)
       if (!init_quiddity (quiddity))
	 {
	   g_debug ("initialization of %s failled",quiddity_class.c_str ());
	   return "";
	 }
     return quiddity->get_nick_name ();
  }

  std::string 
  QuiddityManager_Impl::create (std::string quiddity_class, std::string nick_name)
  {
    if(!class_exists (quiddity_class))
      return "";

    Quiddity::ptr quiddity = abstract_factory_.create (quiddity_class);

    if (quiddity.get() != NULL)
      {
	if (!nick_name.empty () && !quiddities_nick_names_.contains (nick_name))
	  quiddity->set_nick_name (nick_name);
	else
	  g_debug ("QuiddityManager_Impl::create: nick name %s not valid, using %s",
		   nick_name.c_str (),
		   quiddity->get_name().c_str ());

	if (!init_quiddity (quiddity))
	  {
	    g_debug ("initialization of %s with name %s failled\n",
		     quiddity_class.c_str (), nick_name.c_str ());
	    
	    return "";
	  }
      }
    return quiddity->get_nick_name ();
  }

  std::vector<std::string> 
  QuiddityManager_Impl::get_instances ()
  {
    return quiddities_nick_names_.get_keys();
  }

  std::string 
  QuiddityManager_Impl::get_quiddities_description ()
  {
    JSONBuilder::ptr descr (new JSONBuilder ());
    descr->reset ();
    descr->begin_object();
    descr->set_member_name ("quiddities");
    descr->begin_array();
    std::vector<std::string> quids = get_instances (); 
    for(std::vector<std::string>::iterator it = quids.begin(); it != quids.end(); ++it) 
      {
	descr->begin_object();
	std::shared_ptr<Quiddity> quid = get_quiddity (*it);
	descr->add_string_member ("name", quid->get_nick_name().c_str ());
	descr->add_string_member ("class", quid->get_documentation().get_class_name().c_str ());
	descr->end_object();

      }

    descr->end_array();
    descr->end_object ();

    return descr->get_string(true);
  }

  std::string 
  QuiddityManager_Impl::get_quiddity_description (std::string nick_name)
  {
    if (!quiddities_nick_names_.contains (nick_name))
      return "{ \"error\":\"quiddity not found\"}";

    JSONBuilder::ptr descr (new JSONBuilder ());
    descr->reset ();
    descr->begin_object();
    descr->add_string_member ("name", nick_name.c_str ());
    descr->add_string_member ("class", quiddities_.lookup(quiddities_nick_names_.lookup (nick_name))->get_documentation().get_class_name().c_str ());
    descr->end_object ();

    return descr->get_string(true);
  }

  Quiddity::ptr 
  QuiddityManager_Impl::get_quiddity (std::string quiddity_name)
  {
     if (!exists (quiddity_name))
      {
	g_critical ("quiddity %s not found, cannot provide ptr",quiddity_name.c_str());
	Quiddity::ptr empty_quiddity_ptr;
	return empty_quiddity_ptr;
      }
    return quiddities_.lookup (quiddities_nick_names_.lookup (quiddity_name));
  }

  bool 
  QuiddityManager_Impl::exists (std::string quiddity_name)
  {
    return quiddities_nick_names_.contains (quiddity_name);
  }

  //for use of "get description by class" methods only
  bool 
  QuiddityManager_Impl::remove_without_hook (std::string quiddity_name)
  {
    if (!exists (quiddity_name))
      {
	g_warning ("(%s) quiddity %s not found for removing",name_.c_str(), quiddity_name.c_str());
	return false; 
      }
    for (auto &it : property_subscribers_.get_map ())
      it.second->unsubscribe (get_quiddity (quiddity_name));
    for (auto &it : signal_subscribers_.get_map ())
      it.second->unsubscribe (get_quiddity (quiddity_name));
    quiddities_.remove (quiddities_nick_names_.lookup (quiddity_name));
    quiddities_nick_names_.remove (quiddity_name);
    return true;
  }

  bool 
  QuiddityManager_Impl::remove (std::string quiddity_name)
  {
    if (!exists (quiddity_name))
      {
	g_warning ("(%s) quiddity %s not found for removing",name_.c_str(), quiddity_name.c_str());
	return false; 
      }
    for (auto &it : property_subscribers_.get_map ())
      it.second->unsubscribe (get_quiddity (quiddity_name));
    for (auto &it : signal_subscribers_.get_map ())
      it.second->unsubscribe (get_quiddity (quiddity_name));
    quiddities_.remove (quiddities_nick_names_.lookup (quiddity_name));
    quiddities_nick_names_.remove (quiddity_name);
    if (removal_hook_ != NULL)
      (*removal_hook_) (quiddity_name.c_str (), removal_hook_user_data_);
    return true;
  }

  std::string 
  QuiddityManager_Impl::get_properties_description (std::string quiddity_name)
  {

    if (!exists (quiddity_name))
      {
	g_warning ("quiddity %s not found, cannot get description of properties",quiddity_name.c_str());
	return "";
      }
    return (get_quiddity (quiddity_name))->get_properties_description ();
  }

  std::string 
  QuiddityManager_Impl::get_property_description (std::string quiddity_name, std::string property_name)
  {

    if (!exists (quiddity_name))
      {
	g_warning ("quiddity %s not found, cannot get description of properties",quiddity_name.c_str());
	return "";
      }
    return (get_quiddity (quiddity_name))->get_property_description (property_name);
  }

  std::string 
  QuiddityManager_Impl::get_properties_description_by_class (std::string class_name)
  {
    if (!class_exists (class_name))
      return "{\"error\":\"class not found\"}";
    std::string quid_name = create_without_hook (class_name);
    if (g_strcmp0 (quid_name.c_str (), "") == 0)
      return "{\"error\":\"cannot get property because the class cannot be instanciated\"}";
    std::string descr = get_properties_description (quid_name);
    remove_without_hook (quid_name);
    return descr;
  }
  
  std::string
  QuiddityManager_Impl::get_property_description_by_class (std::string class_name, 
							  std::string property_name)
  {
    if (!class_exists (class_name))
      return "{\"error\":\"class not found\"}";
    std::string quid_name = create_without_hook (class_name);
    if (g_strcmp0 (quid_name.c_str (), "") == 0)
      return "{\"error\":\"cannot get property because the class cannot be instanciated\"}";
    std::string descr = get_property_description (quid_name, property_name);
    remove_without_hook (quid_name);
    return descr;
  }


  bool
  QuiddityManager_Impl::set_property (std::string quiddity_name,
				      std::string property_name,
				      std::string property_value)
  {
    if (!exists (quiddity_name))
      {
	g_warning ("quiddity %s not found, cannot set property",quiddity_name.c_str());
	return false;
      }
    return (get_quiddity (quiddity_name))->set_property(property_name.c_str(),property_value.c_str());
  }

 //higher level subscriber
  bool 
  QuiddityManager_Impl::make_property_subscriber (std::string subscriber_name,
					QuiddityPropertySubscriber::Callback cb,
					void *user_data)
  {
    if (property_subscribers_.contains (subscriber_name))
      {
	g_warning ("QuiddityManager_Impl, a subscriber named %s already exists\n",
		   subscriber_name.c_str ());
	return false;
      }
    
    QuiddityPropertySubscriber::ptr subscriber;
    subscriber.reset (new QuiddityPropertySubscriber());
    subscriber->set_manager_impl (shared_from_this());
    subscriber->set_name (subscriber_name.c_str ());
    subscriber->set_user_data (user_data);
    subscriber->set_callback (cb);
    property_subscribers_.insert (subscriber_name, subscriber);
    return true; 
  }

  bool
  QuiddityManager_Impl::remove_property_subscriber (std::string subscriber_name)
  {
    if (!property_subscribers_.contains (subscriber_name))
      {
	g_warning ("QuiddityManager_Impl, a subscriber named %s does not exists\n",
		   subscriber_name.c_str ());
	return false;
      }
    return property_subscribers_.remove (subscriber_name);
  }
  
  bool 
  QuiddityManager_Impl::subscribe_property (std::string subscriber_name,
					   std::string quiddity_name,
					   std::string property_name)
  {
    if (!property_subscribers_.contains (subscriber_name))
      {
	g_warning ("QuiddityManager_Impl, a subscriber named %s does not exists\n",
		   subscriber_name.c_str ());
	return false;
      }
    if (!exists (quiddity_name))
      {
	g_warning ("quiddity %s not found, cannot subscribe to property",quiddity_name.c_str());
	return false;
      }
    return property_subscribers_.lookup(subscriber_name)->subscribe (get_quiddity (quiddity_name), property_name);
  }
  
  bool 
  QuiddityManager_Impl::unsubscribe_property (std::string subscriber_name,
					      std::string quiddity_name,
					      std::string property_name)
  {
    if (!property_subscribers_.contains (subscriber_name))
      {
	g_warning ("QuiddityManager_Impl, a subscriber named %s does not exists\n",
		   subscriber_name.c_str ());
	return false;
      }
    if (!exists (quiddity_name))
      {
	g_warning ("quiddity %s not found, cannot subscribe to property",quiddity_name.c_str());
	return false;
      }
    return property_subscribers_.lookup(subscriber_name)->unsubscribe (get_quiddity (quiddity_name), property_name);
  }
  
  std::vector<std::string> 
  QuiddityManager_Impl::list_property_subscribers ()
  {
    return property_subscribers_.get_keys ();
  }

    std::vector<std::pair<std::string, std::string> > 
    QuiddityManager_Impl::list_subscribed_properties (std::string subscriber_name)
    {
      if (!property_subscribers_.contains (subscriber_name))
	{
	  g_warning ("QuiddityManager_Impl, a subscriber named %s does not exists\n",
		     subscriber_name.c_str ());
	  std::vector<std::pair<std::string, std::string> > empty;
	  return empty;
	}
      
      return property_subscribers_.lookup(subscriber_name)->list_subscribed_properties ();
    }

    std::string 
    QuiddityManager_Impl::list_property_subscribers_json ()
    {
      return "{\"error\":\"to be implemented\"}";//FIXME (list_property_subscriber_json)
    }
  
    std::string 
    QuiddityManager_Impl::list_subscribed_properties_json (std::string subscriber_name)
    {
      std::vector<std::pair<std::string, std::string> > subscribed_props =
	list_subscribed_properties (subscriber_name);

      JSONBuilder *doc = new JSONBuilder ();
      doc->reset ();
      doc->begin_object ();
      doc->set_member_name ("subscribed properties");
      doc->begin_array ();
      for(std::vector<std::pair<std::string, std::string> >::iterator it = subscribed_props.begin(); 
	  it != subscribed_props.end(); ++it) 
	{
	  doc->begin_object ();
	  doc->add_string_member ("quiddity", it->first.c_str ());
	  doc->add_string_member ("property", it->second.c_str ());
	  doc->end_object ();
	}
      doc->end_array ();
      doc->end_object ();
      
      return doc->get_string(true);
    }

  //lower level subscriber
  bool
  QuiddityManager_Impl::subscribe_property_glib (std::string quiddity_name,
						std::string property_name,
						Property::Callback cb, 
						void *user_data)
  {
    if (!exists (quiddity_name))
      {
	g_warning ("quiddity %s not found, cannot subscribe to property",quiddity_name.c_str());
	return false;
      }
    return (get_quiddity (quiddity_name))->subscribe_property(property_name.c_str(),
							      cb,
							      user_data);
  }

  bool
  QuiddityManager_Impl::unsubscribe_property_glib (std::string quiddity_name,
						  std::string property_name,
						  Property::Callback cb, 
						  void *user_data)
  {
    if (!exists (quiddity_name))
      {
	g_warning ("quiddity %s not found, cannot unsubscribe to property",quiddity_name.c_str());
	return false;
      }
    return (get_quiddity (quiddity_name))->unsubscribe_property(property_name.c_str(),
								cb,
								user_data);
  }


  std::string
  QuiddityManager_Impl::get_property (std::string quiddity_name,
				 std::string property_name)
  {
    if (!exists (quiddity_name))
      {
	g_warning ("quiddity %s not found, cannot get property",quiddity_name.c_str());
	return "{\"error\":\"quiddity not found\"}";
      }
    return (get_quiddity (quiddity_name))->get_property(property_name.c_str());
  }

  bool
  QuiddityManager_Impl::has_method (std::string quiddity_name, 
				    std::string method_name)
  {
      if (!exists (quiddity_name))
      {
	g_debug ("quiddity %s not found",quiddity_name.c_str());
	return false;
      }
      Quiddity::ptr quiddity = get_quiddity (quiddity_name);
      
      return quiddity->has_method (method_name);
  }

  bool 
  QuiddityManager_Impl::invoke (std::string quiddity_name, 
				std::string method_name,
				std::vector<std::string> args)
  {
    //g_debug ("QuiddityManager_Impl::quiddity_invoke_method %s %s, arg size %d",quiddity_name.c_str(), function_name.c_str(), args.size ());
    
    if (!exists (quiddity_name))
      {
	g_debug ("quiddity %s not found, cannot invoke",quiddity_name.c_str());
	return false;
      }
    Quiddity::ptr quiddity = get_quiddity (quiddity_name);

    if (!quiddity->has_method (method_name)) 
      {
	g_debug ("method %s not found, cannot invoke",method_name.c_str());
	return false;
      }

    return quiddity->invoke_method (method_name, args);
  } 

  std::string
  QuiddityManager_Impl::get_methods_description (std::string quiddity_name)
  {
    if (!exists (quiddity_name))
      {
	g_warning ("quiddity %s not found, cannot get description of methods",quiddity_name.c_str());
	return "{\"error\":\"quiddity not found\"}";
      }
     
    return (get_quiddity (quiddity_name))->get_methods_description ();
  }


  std::string
  QuiddityManager_Impl::get_method_description (std::string quiddity_name, std::string method_name)
  {
    if (!exists (quiddity_name))
      {
	g_warning ("quiddity %s not found, cannot get description of methods",quiddity_name.c_str());
	return "{\"error\":\"quiddity not found\"}";
      }
     
    return (get_quiddity (quiddity_name))->get_method_description (method_name);
  }

  std::string
  QuiddityManager_Impl::get_methods_description_by_class (std::string class_name)
  {
    if (!class_exists (class_name))
      return "{\"error\":\"class not found\"}";
    std::string quid_name = create_without_hook (class_name);
    std::string descr = get_methods_description (quid_name);
    remove_without_hook (quid_name);
    return descr;

  }

  std::string
  QuiddityManager_Impl::get_method_description_by_class (std::string class_name, 
							std::string method_name)
  {
    if (!class_exists (class_name))
      return "{\"error\":\"class not found\"}";
    std::string quid_name = create_without_hook (class_name);
    std::string descr = get_method_description (quid_name, method_name);
    remove_without_hook (quid_name);
    return descr;

  }

  //*** signals
  std::string 
  QuiddityManager_Impl::get_signals_description (std::string quiddity_name)
  {

    if (!exists (quiddity_name))
      {
	g_warning ("quiddity %s not found, cannot get signals description",quiddity_name.c_str());
	return "";
      }
    return (get_quiddity (quiddity_name))->get_signals_description ();
  }

  std::string 
  QuiddityManager_Impl::get_signal_description (std::string quiddity_name, std::string signal_name)
  {

    if (!exists (quiddity_name))
      {
	g_warning ("quiddity %s not found, cannot get signal description",quiddity_name.c_str());
	return "";
      }
    return (get_quiddity (quiddity_name))->get_signal_description (signal_name);
  }

  std::string 
  QuiddityManager_Impl::get_signals_description_by_class (std::string class_name)
  {
    if (!class_exists (class_name))
      return "{\"error\":\"class not found\"}";
    std::string quid_name = create_without_hook (class_name);
    if (g_strcmp0 (quid_name.c_str (), "") == 0)
      return "{\"error\":\"cannot get signal description because the class cannot be instanciated\"}";
    std::string descr = get_signals_description (quid_name);
    remove_without_hook (quid_name);
    return descr;
  }
  
  std::string
  QuiddityManager_Impl::get_signal_description_by_class (std::string class_name, 
							 std::string signal_name)
  {
    if (!class_exists (class_name))
      return "{\"error\":\"class not found\"}";
    std::string quid_name = create_without_hook (class_name);
    if (g_strcmp0 (quid_name.c_str (), "") == 0)
      return "{\"error\":\"cannot get signal because the class cannot be instanciated\"}";
    std::string descr = get_signal_description (quid_name, signal_name);
    remove_without_hook (quid_name);
    return descr;
  }

 //higher level subscriber
  bool 
  QuiddityManager_Impl::make_signal_subscriber (std::string subscriber_name,
					       QuidditySignalSubscriber::OnEmittedCallback cb,
					       void *user_data)
  {
    if (signal_subscribers_.contains (subscriber_name))
      {
	g_warning ("QuiddityManager_Impl, a subscriber named %s already exists\n",
		   subscriber_name.c_str ());
	return false;
      }
    
    QuidditySignalSubscriber::ptr subscriber;
    subscriber.reset (new QuidditySignalSubscriber());
    subscriber->set_manager_impl (shared_from_this());
    subscriber->set_name (subscriber_name.c_str ());
    subscriber->set_user_data (user_data);
    subscriber->set_callback (cb);
    signal_subscribers_.insert (subscriber_name, subscriber);
    return true; 
  }

  bool
  QuiddityManager_Impl::remove_signal_subscriber (std::string subscriber_name)
  {
    if (!signal_subscribers_.contains (subscriber_name))
      {
	g_warning ("QuiddityManager_Impl, a subscriber named %s does not exists\n",
		   subscriber_name.c_str ());
	return false;
      }
    return signal_subscribers_.remove (subscriber_name);
  }
  
  bool 
  QuiddityManager_Impl::subscribe_signal (std::string subscriber_name,
					 std::string quiddity_name,
					 std::string signal_name)
  {
    if (!signal_subscribers_.contains (subscriber_name))
      {
	g_warning ("QuiddityManager_Impl, a subscriber named %s does not exists\n",
		   subscriber_name.c_str ());
	return false;
      }
    if (!exists (quiddity_name))
      {
	g_warning ("quiddity %s not found, cannot subscribe to signal",quiddity_name.c_str());
	return false;
      }
    return signal_subscribers_.lookup(subscriber_name)->subscribe (get_quiddity (quiddity_name), signal_name);
  }
  
  bool 
  QuiddityManager_Impl::unsubscribe_signal (std::string subscriber_name,
					     std::string quiddity_name,
					     std::string signal_name)
  {
    if (!signal_subscribers_.contains (subscriber_name))
      {
	g_warning ("QuiddityManager_Impl, a subscriber named %s does not exists\n",
		   subscriber_name.c_str ());
	return false;
      }
    if (!exists (quiddity_name))
      {
	g_warning ("quiddity %s not found, cannot subscribe to signal",quiddity_name.c_str());
	return false;
      }
    return signal_subscribers_.lookup(subscriber_name)->unsubscribe (get_quiddity (quiddity_name), signal_name);
  }

  std::vector<std::string> 
  QuiddityManager_Impl::list_signal_subscribers ()
  {
    return signal_subscribers_.get_keys ();
  }

    std::vector<std::pair<std::string, std::string> > 
    QuiddityManager_Impl::list_subscribed_signals (std::string subscriber_name)
    {
      if (!signal_subscribers_.contains (subscriber_name))
	{
	  g_warning ("QuiddityManager_Impl, a subscriber named %s does not exists\n",
		     subscriber_name.c_str ());
	  std::vector<std::pair<std::string, std::string> > empty;
	  return empty;
	}
      
      return signal_subscribers_.lookup(subscriber_name)->list_subscribed_signals ();
    }

    std::string 
    QuiddityManager_Impl::list_signal_subscribers_json ()
    {
      return "{\"error\":\"to be implemented\"}";//FIXME (list_signal_subscriber_json)
    }
  
    std::string 
    QuiddityManager_Impl::list_subscribed_signals_json (std::string subscriber_name)
    {
      std::vector<std::pair<std::string, std::string> > subscribed_sigs =
	list_subscribed_signals (subscriber_name);

      JSONBuilder *doc = new JSONBuilder ();
      doc->reset ();
      doc->begin_object ();
      doc->set_member_name ("subscribed signals");
      doc->begin_array ();
      for(std::vector<std::pair<std::string, std::string> >::iterator it = subscribed_sigs.begin(); 
	  it != subscribed_sigs.end(); ++it) 
	{
	  doc->begin_object ();
	  doc->add_string_member ("quiddity", it->first.c_str ());
	  doc->add_string_member ("signal", it->second.c_str ());
	  doc->end_object ();
	}
      doc->end_array ();
      doc->end_object ();
      
      return doc->get_string(true);
    }

  bool 
  QuiddityManager_Impl::set_created_hook (quiddity_created_hook hook, 
					  void *user_data)
  {
    if (creation_hook_ != NULL)
      return false;
    creation_hook_ = hook;
    creation_hook_user_data_ = user_data;
    return true;
  }
  
  bool 
  QuiddityManager_Impl::set_removed_hook (quiddity_removed_hook hook,
					  void *user_data)
  {
    if (removal_hook_ != NULL)
      return false;
    removal_hook_ = hook;
    removal_hook_user_data_ = user_data;
    return true;
  }
  
  void 
  QuiddityManager_Impl::reset_create_remove_hooks ()
  {
    creation_hook_ = NULL;
    removal_hook_ = NULL;
    creation_hook_user_data_ = NULL;
    removal_hook_user_data_ = NULL;
  }


  void
  QuiddityManager_Impl::init_gmainloop ()
  {
    if (! gst_is_initialized ())
      gst_init (NULL,NULL);
    
    main_context_ = g_main_context_new ();
    mainloop_ = g_main_loop_new (main_context_, FALSE);
    //mainloop_ = g_main_loop_new (NULL, FALSE);
    GstRegistry *registry;
    registry = gst_registry_get_default();
    //TODO add option for scanning a path
    gst_registry_scan_path (registry, "/usr/local/lib/gstreamer-0.10/");
    thread_ = g_thread_new ("SwitcherMainLoop", GThreadFunc(main_loop_thread), this);
  }
  
  gpointer
  QuiddityManager_Impl::main_loop_thread (gpointer user_data)
  {
    QuiddityManager_Impl *context = static_cast<QuiddityManager_Impl*>(user_data);
    g_main_loop_run (context->mainloop_);
    return NULL;
  }
  
  GMainContext *
  QuiddityManager_Impl::get_g_main_context ()
  {
    return main_context_;
  } 
  
} // end of namespace
