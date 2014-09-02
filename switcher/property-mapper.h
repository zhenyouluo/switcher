/*
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

#ifndef __SWITCHER_PROPERTY_MAPPER_H__
#define __SWITCHER_PROPERTY_MAPPER_H__

#include "quiddity.h"
#include "custom-property-helper.h"

namespace switcher {

  class PropertyMapper:public Quiddity {
  public:
    SWITCHER_DECLARE_QUIDDITY_PUBLIC_MEMBERS(PropertyMapper);
    PropertyMapper();
    ~PropertyMapper();
    PropertyMapper(const PropertyMapper &);
      PropertyMapper & operator=(const PropertyMapper &);
    bool init();

  private:
      std::weak_ptr < Quiddity > source_quiddity_;
      std::string source_property_name_;

      std::weak_ptr < Quiddity > sink_quiddity_;
    GParamSpec *sink_quiddity_pspec_;
      std::string sink_property_name_;

    //clip values (and scale accordingly)
      CustomPropertyHelper::ptr custom_props_;
    GParamSpec *sink_min_spec_;
    GParamSpec *sink_max_spec_;
    GParamSpec *source_min_spec_;
    GParamSpec *source_max_spec_;

    double sink_min_;
    double sink_max_;
    double source_min_;
    double source_max_;

    void make_numerical_source_properties();
    void make_numerical_sink_properties();
    static gboolean set_source_property_method(gchar * quiddity_name,
                                               gchar * property_name,
                                               void *user_data);
    static void property_cb(GObject * gobject,
                            GParamSpec * pspec, gpointer user_data);
    static gboolean set_sink_property_method(gchar * quiddity_name,
                                             gchar * property_name,
                                             void *user_data);
    void unsubscribe_source_property();
    static void set_double_value(gdouble value, void *user_data);
    static gdouble get_double_value(void *user_data);
  };
}                               // end of namespace

#endif                          // ifndef
