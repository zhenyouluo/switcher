/*
 * This file is part of switcher-nvenc.
 *
 * switcher-myplugin is free software; you can redistribute it and/or
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

#include "./nvenc-plugin.hpp"
#include "./cuda-context.hpp"

namespace switcher {
SWITCHER_MAKE_QUIDDITY_DOCUMENTATION(
    NVencPlugin,
    "nvenc",
    "NVidia video encoder Plugin",
    "video",
    "",
    "CUDA-based video encoder",
    "LGPL",
    "Nicolas Bouillot");

NVencPlugin::NVencPlugin(const std::string &){ 
  auto devices = CudaContext::get_devices();
  std::vector<std::string> names;
  for(auto &it: devices){
    devices_nv_ids_.push_back(it.first);
    names.push_back(std::string("GPU #")
                    + std::to_string(it.first)
                    + " "
                    + it.second);
  }
  devices_ = Selection(std::move(names), 0);
  pmanage<MPtr(&PContainer::make_selection)>(
      "gpu",
      [this](size_t val){devices_.select(val); return true;},
      [this](){return devices_.get();},
      "encoder GPU",
      "Selection of the GPU used for encoding",
      devices_);
}

bool NVencPlugin::init() {
  return true;
}

}  // namespace switcher
