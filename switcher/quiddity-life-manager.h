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

/**
 * The QuiddityLifeManager class wraps abstract factory for creating instace by name (birth) 
 * It wraps StringMap for managing instances (subsistence). 
 * Object destruction is managed through the use of std::shared_ptr
 */

#ifndef __SWITCHER_QUIDDITY_LIFE_MANAGER_H__
#define __SWITCHER_QUIDDITY_LIFE_MANAGER_H__


#include <memory>
#include "switcher/abstract-factory.h" 
#include "switcher/string-map.h"
#include "switcher/quiddity.h" 
#include "switcher/json-builder.h"

namespace switcher
{
  class Quiddity;

  class QuiddityLifeManager : public std::enable_shared_from_this<QuiddityLifeManager>
//FIXME rename that into QuiddityManager_Impl or better 
  {
  public:
    typedef std::shared_ptr< QuiddityLifeManager > ptr;
    
     static QuiddityLifeManager::ptr make_life_manager ();//will get name "default"
     static QuiddityLifeManager::ptr make_life_manager (std::string name);
    
    //**** info about manager
    std::string get_name ();
    std::vector<std::string> get_classes ();//vector of class names
    std::vector<std::string> get_instances ();//vector of instance names
    //doc (json formatted)
    std::string get_classes_doc ();
    std::string get_class_doc (std::string class_name);
    std::string get_quiddities_description ();
    bool class_exists (std::string class_name);
    bool exists (std::string quiddity_name);

    //**** life
    //creation
    std::string create (std::string quiddity_class);
    std::string create (std::string quiddity_class, 
			std::string nick_name);
    //subsistence
    std::shared_ptr<Quiddity> get_quiddity (std::string quiddity_name);
    
    //release base quiddity (destructed with the shared pointer)
    bool remove (std::string quiddity_name);

    //**** properties
    //doc (json formatted)
    std::string get_properties_description (std::string quiddity_name);
    std::string get_property_description (std::string quiddity_name, 
					  std::string property_name);
    //following "by_class" methods provide properties available after creation only
    std::string get_properties_description_by_class (std::string class_name); 
    std::string get_property_description_by_class (std::string class_name, 
						   std::string property_name); 
    //set & get
    bool set_property (std::string quiddity_name,
		       std::string property_name,
		       std::string property_value);
    std::string get_property (std::string quiddity_name, 
			      std::string property_name);
     
    //**** methods 
    //doc (json formatted)
    std::string get_methods_description (std::string quiddity_name); 
    std::string get_method_description (std::string quiddity_name, 
					std::string method_name);
    //following "by_class" methods provide properties available after creation only
    std::string get_methods_description_by_class (std::string class_name); 
    std::string get_method_description_by_class (std::string class_name, 
						 std::string method_name);
    //invoke
    bool invoke (std::string quiddity_name, 
		 std::string method_name,
		 std::vector<std::string> args);  


  private:
    QuiddityLifeManager();//will get name "default"
    QuiddityLifeManager(std::string);
    void make_classes_doc ();
    std::string name_;
    void register_classes ();
    AbstractFactory< Quiddity, std::string, JSONBuilder::Node> abstract_factory_;
    StringMap< std::shared_ptr<Quiddity> > quiddities_;
    StringMap< std::string> quiddities_nick_names_;
    void init_quiddity (std::shared_ptr<Quiddity> quiddity);
    void remove_shmdata_sockets ();
    JSONBuilder::ptr classes_doc_;
  };

} // end of namespace



#endif // ifndef
