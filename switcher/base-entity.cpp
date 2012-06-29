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
 * The BaseEntity class
 */

#include "switcher/base-entity.h"

namespace switcher
{

  BaseEntity::BaseEntity ()
  {
    g_print ("call: BaseEntity constructor\n");
  }
  
  BaseEntity::~BaseEntity () { 
    if (base_entity_manager_ != NULL)
      {
	base_entity_manager_->unref_entity (name_);
      }

    g_print ("call: BaseEntity destructor for %s\n",get_name().c_str());
    //TODO remove properties
  }

  void
  BaseEntity::set_manager (BaseEntityManager *manager)
  {
    base_entity_manager_ = manager;
  }

  std::string
  BaseEntity::get_name()
  {
    return name_;
  }

  
  bool
  BaseEntity::register_property (GObject *object, std::string object_property, std::string prefix)
  {
    GParamSpec *pspec = g_object_class_find_property (G_OBJECT_GET_CLASS(object), object_property.c_str());
    if (pspec == NULL)
      {
	g_printerr ("property not found %s\n",object_property.c_str());
	return false;
      }

    Property::ptr prop (new Property (object, pspec));
    //gchar * prop_name = g_strconcat (name_.c_str(), "_", pspec->name, NULL);
    //properties_ [prop_name] = prop;
    //g_free (prop_name);
    //    g_print ("%s\n",G_OBJECT_TYPE_NAME(object));
    properties_[prefix + "/" + object_property] = prop; 
    
    return true;
  }


  void
  BaseEntity::print_properties ()
  {
    for( std::map<std::string, Property::ptr>::iterator ii=properties_.begin(); ii!=properties_.end(); ++ii)
      {
     	g_print ("\n....\n%s\n",(*ii).first.c_str());
     	(*ii).second->print ();
      }
  }


  std::vector<std::string>
  BaseEntity::get_property_names ()
  {
    std::vector<std::string> list_of_properties;
    for(std::map<std::string, Property::ptr>::iterator it = properties_.begin(); it != properties_.end(); ++it) 
      {
	list_of_properties.push_back(it->first);
      }
    return list_of_properties;
  }

  bool 
  BaseEntity::set_property (std::string name, std::string value)
  {
    if (properties_.find( name ) == properties_.end())
      return false;

    Property::ptr prop = properties_[name];
    prop->set (value);
    return true;
  }

  std::string 
  BaseEntity::get_property (std::string name)
  {
    if (properties_.find( name ) == properties_.end())
      return "property not found";

    Property::ptr prop = properties_[name];
    return prop->get ();
  }

}
