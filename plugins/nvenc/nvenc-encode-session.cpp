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

#include <glib.h>  // log
#include "./nvenc-encode-session.hpp"
#include "switcher/scope-exit.hpp"

namespace switcher {
NVencES::NVencES(uint32_t device_id
                 //, uint32_t width, uint32_t height,  NV_ENC_BUFFER_FORMAT format
                 ):
    cu_ctx_(device_id){
  On_scope_exit{
    if (!safe_bool_idiom())
      g_warning("NV encoder session initialization failled");
  };
  if (!cu_ctx_)
    return;
  NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS params;
  params.version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
  params.apiVersion = NVENCAPI_VERSION;
  params.device = cu_ctx_.cuda_ctx_;
  params.deviceType = NV_ENC_DEVICE_TYPE_CUDA;
  if (NV_ENC_SUCCESS != NVencAPI::api.nvEncOpenEncodeSessionEx(&params, &encoder_)) {
      encoder_ = nullptr;
      return;
  }
  
  
  // // input buffers
  // for (auto &it: input_bufs_){
  //   NV_ENC_CREATE_INPUT_BUFFER buf;
  //   buf.version = NV_ENC_CREATE_INPUT_BUFFER_VER;
  //   buf.width = width;
  //   buf.height = height;
  //   buf.memoryHeap = NV_ENC_MEMORY_HEAP_SYSMEM_CACHED;
  //   buf.bufferFmt = format;
  //   if (NV_ENC_SUCCESS != NVencAPI::api.nvEncCreateInputBuffer (encoder_, &buf)){
  //     g_warning("nvenc cannot create input buffer");
  //     NVencAPI::api.nvEncDestroyEncoder (encoder_);
  //     encoder_ = nullptr;
  //     return;
  //   }
  //   it = cin_buf.inputBuffer;
  // }
  // // output buffers
  // for (auto &it: output_bufs_){
  //   NV_ENC_CREATE_BITSTREAM_BUFFER buf;
  //   cout_buf.version = NV_ENC_CREATE_BITSTREAM_BUFFER_VER;
  //   /* 2 MB should be large enough to hold most output frames.
  //    * NVENC will automatically increase this if it's not enough. */
  //   buf.size = 2 * 1024 * 1024;
  //   buf.memoryHeap = NV_ENC_MEMORY_HEAP_SYSMEM_CACHED;
  //   if (NV_ENC_SUCCESS != NVencAPI::api.nvEncCreateBitstreamBuffer (encoder_, &buf)){
  //     g_warning("nvenc cannot create output buffer");
  //     NVencAPI::api.nvEncDestroyEncoder (encoder_);
  //     encoder_ = nullptr;
  //     return;
  //   }
  //   it = buf.bitstreamBuffer;
  // }

}

NVencES::~NVencES(){
  if (safe_bool_idiom() && !(NV_ENC_SUCCESS == NVencAPI::api.nvEncDestroyEncoder(encoder_)))
    g_warning("BUG! (destroying NV encoder session)");
}

std::vector<std::pair<std::string, GUID>> NVencES::get_supported_codecs(){
  std::vector<std::pair<std::string, GUID>> res;
  // supported codecs
  uint32_t i, num = 0;
  GUID guids[16];
  if (NV_ENC_SUCCESS != NVencAPI::api.nvEncGetEncodeGUIDs (encoder_, guids, 16, &num)){
    g_warning("issue with nvEncGetEncodeGUIDs");
    return res;
  }
  for (i = 0; i < num; ++i) {
    if (is_same(guids[i], NV_ENC_CODEC_H264_GUID)){
      res.push_back(std::make_pair(std::string("H264"), guids[i]));
    } else if (is_same(guids[i], NV_ENC_CODEC_HEVC_GUID)) {
      res.push_back(std::make_pair(std::string("HEVC"), guids[i]));
    } else
      g_warning("unknown codec GUID from nvenc");
  }
  return res;
}

std::vector<std::pair<std::string, GUID>> NVencES::get_presets(GUID encodeGUID){
  std::vector<std::pair<std::string, GUID>> res;
  // supported codecs
  uint32_t i, num = 0;
  GUID guids[16];
  if (NV_ENC_SUCCESS !=
      NVencAPI::api.nvEncGetEncodePresetGUIDs(encoder_, encodeGUID,
                                              guids, 16, &num)){
    g_warning("issue with nvEncGetEncodePresetGUIDs");
    return res;
  }
  for (i = 0; i < num; ++i) {
    if (is_same(guids[i], NV_ENC_PRESET_DEFAULT_GUID)){
      res.push_back(std::make_pair(std::string("Default"), guids[i]));
    } else if (is_same(guids[i], NV_ENC_PRESET_HP_GUID)) {
      res.push_back(std::make_pair(std::string("HP"), guids[i]));
    } else if (is_same(guids[i], NV_ENC_PRESET_HQ_GUID)) {
      res.push_back(std::make_pair(std::string("HQ"), guids[i]));
    } else if (is_same(guids[i], NV_ENC_PRESET_BD_GUID)) {
      res.push_back(std::make_pair(std::string("BD"), guids[i]));
    } else if (is_same(guids[i], NV_ENC_PRESET_LOW_LATENCY_DEFAULT_GUID)) {
      res.push_back(std::make_pair(std::string("Low Latency default"), guids[i]));
    } else if (is_same(guids[i], NV_ENC_PRESET_LOW_LATENCY_HQ_GUID)) {
      res.push_back(std::make_pair(std::string("Low Latency HQ"), guids[i]));
    } else if (is_same(guids[i], NV_ENC_PRESET_LOW_LATENCY_HP_GUID)) {
      res.push_back(std::make_pair(std::string("Low Latency HP"), guids[i]));
    } else if (is_same(guids[i], NV_ENC_PRESET_LOSSLESS_DEFAULT_GUID)) {
      res.push_back(std::make_pair(std::string("Lossless default"), guids[i]));
    } else if (is_same(guids[i], NV_ENC_PRESET_LOSSLESS_HP_GUID)) {
      res.push_back(std::make_pair(std::string("Lossless HP"), guids[i]));
    } else
      g_warning("unknown preset GUID from nvenc");
  }
  return res;
}

std::vector<std::pair<std::string, GUID>> NVencES::get_profiles(GUID encodeGUID){
  std::vector<std::pair<std::string, GUID>> res;
  // supported codecs
  uint32_t i, num = 0;
  GUID guids[16];
  NVencAPI::api.nvEncGetEncodeProfileGUIDs (encoder_, encodeGUID, guids, 16, &num);
  for (i = 0; i < num; ++i) {
    if (is_same(guids[i], NV_ENC_CODEC_PROFILE_AUTOSELECT_GUID)){
      res.push_back(std::make_pair(std::string("Autoselect"), guids[i]));
    } else if (is_same(guids[i], NV_ENC_H264_PROFILE_BASELINE_GUID)) {
      res.push_back(std::make_pair(std::string("Baseline"), guids[i]));
    } else if (is_same(guids[i], NV_ENC_H264_PROFILE_MAIN_GUID)) {
      res.push_back(std::make_pair(std::string("Main (H264)"), guids[i]));
    } else if (is_same(guids[i], NV_ENC_H264_PROFILE_HIGH_GUID)) {
      res.push_back(std::make_pair(std::string("High"), guids[i]));
    } else if (is_same(guids[i], NV_ENC_H264_PROFILE_HIGH_444_GUID)) {
      res.push_back(std::make_pair(std::string("High 444"), guids[i]));
    } else if (is_same(guids[i], NV_ENC_H264_PROFILE_STEREO_GUID)) {
      res.push_back(std::make_pair(std::string("Stereo"), guids[i]));
    } else if (is_same(guids[i], NV_ENC_H264_PROFILE_SVC_TEMPORAL_SCALABILTY)) {
      res.push_back(std::make_pair(std::string("SVC temporal scalability"), guids[i]));
    } else if (is_same(guids[i], NV_ENC_H264_PROFILE_CONSTRAINED_HIGH_GUID)) {
      res.push_back(std::make_pair(std::string("Constrained high"), guids[i]));
    } else if (is_same(guids[i], NV_ENC_HEVC_PROFILE_MAIN_GUID)) {
      res.push_back(std::make_pair(std::string("Main (HEVC)"), guids[i]));
    } else
      g_warning("unknown profile GUID from nvenc");
  }
  return res;
}



bool NVencES::is_same(GUID g1, GUID g2){
  return (g1.Data1 == g2.Data1 && g1.Data2 == g2.Data2 && g1.Data3 == g2.Data3
          && g1.Data4[0] == g2.Data4[0] && g1.Data4[1] == g2.Data4[1]
          && g1.Data4[2] == g2.Data4[2] && g1.Data4[3] == g2.Data4[3]
          && g1.Data4[4] == g2.Data4[4] && g1.Data4[5] == g2.Data4[5]
          && g1.Data4[6] == g2.Data4[6] && g1.Data4[7] == g2.Data4[7]);
}

}  // namespace switcher
