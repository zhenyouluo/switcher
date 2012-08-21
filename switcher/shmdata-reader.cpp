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

#include "switcher/shmdata-reader.h"

namespace switcher
{
  ShmdataReader::ShmdataReader()
  {}

  ShmdataReader::~ShmdataReader()
  {
    if (reader_ != NULL)
      shmdata_base_reader_close (reader_);
  }

  void 
  ShmdataReader::plug (const char *absolute_path, GstElement *bin, GstElement *sink_element)
  {
    sink_element_ = sink_element;
    reader_ = shmdata_base_reader_init (absolute_path, 
					bin, 
					ShmdataReader::on_first_data,
					this);
  }
  

  void 
  ShmdataReader::on_first_data (shmdata_base_reader_t * context, void *user_data)
  {
    ShmdataReader *reader = static_cast<ShmdataReader *>(user_data);
    shmdata_base_reader_set_sink (context, reader->sink_element_);
  }

}