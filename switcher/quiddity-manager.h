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

/**
 * The QuiddityManager class
 */

#ifndef __SWITCHER_QUIDDITY_MANAGER_H__
#define __SWITCHER_QUIDDITY_MANAGER_H__

#include <vector>
#include <map>
#include <string>
#include "switcher/quiddity-life-manager.h"
#include "switcher/quiddity-command.h"
#include "switcher/quiddity-manager-wrapper.h"
#include <stdarg.h>

 namespace switcher 
 { 

   class QuiddityManager : public std::enable_shared_from_this<QuiddityManager>
   { 
   public: 
     typedef std::shared_ptr<QuiddityManager> ptr; 

     static QuiddityManager::ptr make_manager ();//will get name "default"
     static QuiddityManager::ptr make_manager (std::string name);
     ~QuiddityManager(); 
     std::string get_name ();
     
     //--------------------------------------------------------------------------------------------
     //life manager
     std::vector<std::string> get_classes (); //know which quiddities can be created
     std::vector<std::string> get_quiddities (); //know instances
     std::string get_classes_doc ();//all classes json doc *without* methods and properties
     std::string get_classes_doc_full ();//all classes json doc *with* methods and properties
     std::string get_class_doc (std::string class_name);//json doc *without* methods and properties
     std::string get_class_doc_full (std::string class_name);//json doc *with* methods and properties
     std::string create (std::string class_name); //returns the name
     std::string create (std::string class_name, 
			 std::string nick_name); // &?= chars are not allowed in nicknames
     bool remove (std::string quiddity_name);

     //--------------------------------------------------------------------------------------------
     //properties
     std::string get_properties_description (std::string quiddity_name); //json formated
     std::string get_property_description (std::string quiddity_name, std::string property_name); //json formated
     bool set_property (std::string quiddity_name,
			std::string property_name,
			std::string property_value);
     
     std::string get_property (std::string quiddity_name, 
			       std::string property_name);
     
     //--------------------------------------------------------------------------------------------
     //methods 
     std::string get_methods_description (std::string quiddity_name); //json formated
     std::string get_method_description (std::string quiddity_name, std::string method_name); //json formated
     bool invoke (std::string quiddity_name, 
		  std::string method_name,
		  std::vector<std::string> args);  

     // will invoke the given method after quiddity creation, if method exists (only one method)
     bool auto_invoke  (std::string method_name,
			std::vector<std::string> args);  

   private: 
     QuiddityManager();//will get name "default"
     QuiddityManager(std::string name); 
     QuiddityLifeManager::ptr life_manager_; //may be shared with others for automatic quiddity creation 
     std::string name_;

     //auto invoke and init
     void auto_init (std::string quiddity_name);
     std::string auto_invoke_method_name_;
     std::vector<std::string> auto_invoke_args_;

     //gmainloop
     static bool gmainloop_initialized_;
     GThread *thread_; //runing the main loop
     GMainLoop *mainloop_;
     void init_gmainloop ();
     static gpointer main_loop_thread (gpointer user_data);

     //running commands in the gmain_loop context
     static gpointer gmainloop_run (gpointer user_data);//thread for the loop
     void invoke_in_gmainloop();
     QuiddityCommand command_;
     GCond *exec_cond_; //sync current thread and gmainloop
     GMutex *exec_mutex_; //sync current thread and gmainloop
     void init_command_sync(); 
     void clear_command_sync(); 

     //ensure sequential invokation
     GMutex *seq_mutex_; 
     std::string seq_invoke (QuiddityCommand::command command, ...);
   }; 

 } // end of namespace 

#endif  




