/*
 * Copyright (C) 2012-2013 Nicolas Bouillot (http://www.nicolasbouillot.net)
 *
 * This file is part of switcher-gtk.
 *
 * switcher-gtk is free software; you can redistribute it and/or
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

#include "gtkvideo.h"
#include "switcher/gst-utils.h"
#include "switcher/quiddity-command.h"
#include <gst/gst.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkcursor.h>
#include <unistd.h>  //sleep

namespace switcher
{
  SWITCHER_MAKE_QUIDDITY_DOCUMENTATION(GTKVideo, 
				       "Video Display",
				       "video sink", 
				       "Video window with fullscreen",
				       "GPL",
				       "gtkvideosink",
				       "Nicolas Bouillot");
  
  guint GTKVideo::instances_counter_ = 0;

  bool
  GTKVideo::init ()
  {

    if (!GstUtils::make_element ("xvimagesink", &xvimagesink_))
      return false;
    
    if (!gtk_init_check (NULL, NULL))
      {
	g_debug ("GTKVideo::init, cannot init gtk");
	return false;
      }
    
    wait_window_mutex_ = g_mutex_new ();
    wait_window_cond_ = g_cond_new ();

    window_destruction_mutex_ = g_mutex_new ();
    window_destruction_cond_ = g_cond_new ();

    //set the name before registering properties
    set_name (gst_element_get_name (xvimagesink_));
    g_object_set (G_OBJECT (xvimagesink_), "sync", FALSE, NULL);

    on_error_command_ = new QuiddityCommand ();
    on_error_command_->id_ = QuiddityCommand::remove;
    on_error_command_->add_arg (get_nick_name ());

    g_object_set_data (G_OBJECT (xvimagesink_), 
     		       "on-error-command",
     		       (gpointer)on_error_command_);
    
    set_sink_element (xvimagesink_);

    main_window_ = NULL;  
    video_window_ = NULL; 
    is_fullscreen_ = FALSE;
    if (instances_counter_ == 0)
      g_thread_new ("GTKMainLoopThread", GThreadFunc(gtk_main_loop_thread), this);
    instances_counter_++;
        
    custom_props_.reset (new CustomPropertyHelper ());
    fullscreen_prop_spec_ = 
      custom_props_->make_boolean_property ("fullscreen", 
					    "Enable/Disable Fullscreen",
					    (gboolean)FALSE,
					    (GParamFlags) G_PARAM_READWRITE,
					    GTKVideo::set_fullscreen,
					    GTKVideo::get_fullscreen,
					    this);
    register_property_by_pspec (custom_props_->get_gobject (), 
				fullscreen_prop_spec_, 
				"fullscreen",
				"Fullscreen");

    g_object_set (G_OBJECT (xvimagesink_),
		  "force-aspect-ratio", TRUE,
		  "draw-borders", TRUE,
		  NULL);

    blank_cursor_ = NULL;
    main_window_ = NULL;

    gtk_idle_add ((GtkFunction)create_ui,
		  this);
    return true;
  }
  
  gpointer
  GTKVideo::gtk_main_loop_thread (gpointer user_data)
  {
    g_debug ("GTKVideo::gtk_main_loop_thread starting");
    gtk_main ();
    return NULL;
  }


  gboolean 
  GTKVideo::key_pressed_cb(GtkWidget *widget, GdkEventKey *event, gpointer data)
  {
    GTKVideo *context = static_cast<GTKVideo *>(data);
    QuiddityManager_Impl::ptr manager;

    switch (event->keyval)
      {
      case GDK_f:
	context->toggle_fullscreen();
	break;
      case GDK_F:
	context->toggle_fullscreen();
	break;
      case GDK_Escape:
	context->toggle_fullscreen();
	break;
      case GDK_q:
	manager = context->manager_impl_.lock ();
	if ((bool) manager)
	  manager->remove (context->get_nick_name ());
	else
	  g_debug ("GTKVideo::key_pressed_cb q pressed, closing window");
	break;
      default:
	break;
      }

    return TRUE;
  }

  GTKVideo::GTKVideo ()
  {
  }


  
   void  
   GTKVideo::window_destroyed (gpointer user_data)
   {
     GTKVideo *context = static_cast <GTKVideo *> (user_data);
     g_mutex_lock (context->window_destruction_mutex_);
     g_cond_signal (context->window_destruction_cond_);
     g_mutex_unlock (context->window_destruction_mutex_);
   }

   gboolean  
   GTKVideo::destroy_window (gpointer user_data)
   {
     GTKVideo *context = static_cast <GTKVideo *> (user_data);
     gtk_widget_destroy (GTK_WIDGET(context->main_window_));
     return  G_SOURCE_REMOVE;
   }



  GTKVideo::~GTKVideo ()
  {
    reset_bin ();
        
    //destroy child widgets too
     if (main_window_ != NULL && GTK_IS_WIDGET (main_window_))
       {
	  g_mutex_lock (window_destruction_mutex_);
	 
	  g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
	  		  destroy_window, 
	  		  this,
	  		  window_destroyed);
	  g_cond_wait (window_destruction_cond_, window_destruction_mutex_);
	  g_mutex_unlock (window_destruction_mutex_);
       }


    if (blank_cursor_ != NULL)
      gdk_cursor_destroy (blank_cursor_);

    //FIXME
     // instances_counter_ --;
     // if (instances_counter_ == 0)
     //   {
     // 	 g_debug ("GTKVideo::~GTKVideo invoking gtk_main_quit");
     // 	 gtk_main_quit ();
     //   }
    
     if (on_error_command_ != NULL)
       delete on_error_command_;

    g_cond_free (wait_window_cond_);
    g_mutex_free (wait_window_mutex_);
    g_cond_free (window_destruction_cond_);
    g_mutex_free (window_destruction_mutex_);
  }

  
  void 
  GTKVideo::realize_cb (GtkWidget *widget, void *user_data) 
  {
    GTKVideo *context = static_cast <GTKVideo *> (user_data);
    g_print ("realized debut \n");


    GdkWindow *window = gtk_widget_get_window (widget);
    
    if (!gdk_window_ensure_native (window))
      g_debug ("Couldn't create native window needed for GstXOverlay!");

     gdk_threads_enter ();
     GdkDisplay *display =  gdk_display_get_default ();
     gdk_display_sync (display);
    
     /* Retrieve window handler from GDK */
 #if defined (GDK_WINDOWING_WIN32)
     context->window_handle_ = (guintptr)GDK_WINDOW_HWND (window);
 #elif defined (GDK_WINDOWING_QUARTZ)
     context->window_handle_ = gdk_quartz_window_get_nsview (window);
 #elif defined (GDK_WINDOWING_X11)
     context->window_handle_ = GDK_WINDOW_XID (window);
 #endif


     g_object_set_data (G_OBJECT (context->xvimagesink_), 
       		       "window-handle",
       		       (gpointer)&context->window_handle_);

     gdk_threads_leave ();

    g_mutex_lock (context->wait_window_mutex_);
    g_cond_signal (context->wait_window_cond_);
    g_mutex_unlock (context->wait_window_mutex_);
  }
  
  /* This function is called when the main window is closed */
  void 
  GTKVideo::delete_event_cb (GtkWidget *widget, GdkEvent *event, void *user_data) 
  {
    GTKVideo *context = static_cast <GTKVideo *> (user_data);
    QuiddityManager_Impl::ptr manager = context->manager_impl_.lock ();
    if ((bool) manager)
      manager->remove (context->get_nick_name ());
    else
      g_debug ("GTKVideo::delete_event_cb cannot remove quiddity");
  }
  
  gboolean 
  GTKVideo::expose_cb (GtkWidget *widget, GdkEventExpose *event, void *user_data) 
  {
    return FALSE;
  }

  void 
  GTKVideo::create_ui (void *user_data) 
  {
    GTKVideo *context = static_cast <GTKVideo *> (user_data);
   
    //g_print ("create_ui debut \n");
   
    context->main_window_ = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    g_signal_connect (G_OBJECT (context->main_window_), 
     		      "delete-event", G_CALLBACK (delete_event_cb), context);
    
    context->video_window_ = gtk_drawing_area_new ();
    gtk_widget_set_double_buffered (context->video_window_, FALSE);
    g_signal_connect (context->video_window_, "realize", G_CALLBACK (realize_cb), context);
    // g_signal_connect (context->video_window_, 
    // 		      "expose_event", 
    // 		      G_CALLBACK (expose_cb), 
    // 		      context);
    gtk_container_add (GTK_CONTAINER (context->main_window_), context->video_window_);
  }

  void 
  GTKVideo::toggle_fullscreen()
  {
    if (is_fullscreen_)
      set_fullscreen (FALSE, this);
    else
      set_fullscreen (TRUE, this);
  }
  
  gboolean 
  GTKVideo::get_fullscreen (void *user_data)
  {
    GTKVideo *context = static_cast<GTKVideo *> (user_data);
    return context->is_fullscreen_;
  }

  void 
  GTKVideo::set_fullscreen (gboolean fullscreen, void *user_data)
  {
    GTKVideo *context = static_cast<GTKVideo *> (user_data);
    
     if (fullscreen)
       {
     	if (context->main_window_ != NULL)
     	  {
     	    gdk_window_set_cursor(GDK_WINDOW(context->video_window_->window), context->blank_cursor_);
     	    gtk_window_fullscreen(GTK_WINDOW(context->main_window_));
     	  }
     	context->is_fullscreen_ = TRUE;
       }
     else
       {
     	if (context->main_window_ != NULL)
     	  {
     	    gdk_window_set_cursor(GDK_WINDOW(context->video_window_->window), NULL);
     	    gtk_window_unfullscreen(GTK_WINDOW(context->main_window_));
     	  }
     	context->is_fullscreen_ = FALSE;
       }
    context->custom_props_->notify_property_changed (context->fullscreen_prop_spec_);
  }

  gboolean 
  GTKVideo::on_window_state_event (GtkWidget *widget,
				   GdkEvent  *event,
				   gpointer   user_data)
  {
    g_print ("on_window_state_event\n");
    return FALSE;
  }

  gboolean 
  GTKVideo::on_destroy_event (GtkWidget *widget,
			      GdkEvent  *event,
			      gpointer   user_data)
  {
    GTKVideo *context = static_cast <GTKVideo *> (user_data);
    //g_print ("window destroyed \n");
    return FALSE;
  }

  void 
  GTKVideo::on_shmdata_connect (std::string shmdata_sochet_path) 
  {
    g_mutex_lock (wait_window_mutex_);

    gtk_window_set_default_size (GTK_WINDOW (main_window_), 640, 480);
    
    blank_cursor_ = gdk_cursor_new(GDK_BLANK_CURSOR);

    gtk_widget_set_events (main_window_, GDK_KEY_PRESS_MASK );
    g_signal_connect(G_OBJECT(main_window_), 
       		     "key-press-event",
		     G_CALLBACK(GTKVideo::key_pressed_cb), 
       		     this);

    // g_signal_connect (main_window_, 
    // 		      "window-state-event", 
    // 		      G_CALLBACK (on_window_state_event), 
    // 		      this);
    
    gtk_idle_add ((GtkFunction)gtk_widget_show_all,
		  main_window_);
    
    g_cond_wait (wait_window_cond_, wait_window_mutex_);
    g_mutex_unlock (wait_window_mutex_);

    set_fullscreen (is_fullscreen_, this);
  }
  
}
