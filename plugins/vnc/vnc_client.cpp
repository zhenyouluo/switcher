/*
 *
 * posture is free software; you can redistribute it and/or
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

#include "./vnc_client.hpp"
#include "switcher/std2.hpp"

#include <functional>
#include <iostream>

using namespace std;

namespace switcher {

SWITCHER_MAKE_QUIDDITY_DOCUMENTATION(
    VncClientSrc,
    "vncclientsrc",
    "VNC client",
    "video",
    "writer",
    "Connects to a VNC server and outputs the video to a shmdata",
    "LGPL",
    "Emmanuel Durand");

VncClientSrc::VncClientSrc(const std::string &):
    custom_props_(std::make_shared<CustomPropertyHelper> ()) {
}

VncClientSrc::~VncClientSrc() {
  stop();
}

bool
VncClientSrc::start() {
  if (capture_truecolor_)
    rfb_client_ = rfbGetClient(8, 3, 4);
  else
    rfb_client_ = rfbGetClient(5, 3, 2);

  if (!rfb_client_)
    return false;

  rfbClientSetClientData(rfb_client_, (void*)(&VncClientSrc::resize_vnc) /*pointer as a tag, never called*/, this);
  rfb_client_->MallocFrameBuffer = VncClientSrc::resize_vnc;
  rfb_client_->canHandleNewFBSize = TRUE;
  rfb_client_->GotFrameBufferUpdate = VncClientSrc::update_vnc;

  int argc = 2;
  char* server_address = const_cast<char*>(vnc_server_address_.c_str());
  char* argv[] = {(char*)"switcher", server_address};
  if (!rfbInitClient(rfb_client_, &argc, argv))
    return false;

  vnc_continue_update_ = true;
  vnc_update_thread_ = thread([&]() {
    while (vnc_continue_update_) {
      int i = WaitForMessage(rfb_client_, 500);
      if (i < 0)
        return;
      if (i > 0 && !HandleRFBServerMessage(rfb_client_))
        return;
    }
  });
  
  return true;
}

bool
VncClientSrc::stop() {
  vnc_continue_update_ = false;
  if (vnc_update_thread_.joinable())
    vnc_update_thread_.join();

  return true;
}

bool
VncClientSrc::init() {
  init_startable(this);

  vnc_server_address_prop_ = custom_props_->make_string_property("vnc_server_address",
                                            "Address of the VNC server",
                                            vnc_server_address_.c_str(),
                                            (GParamFlags) G_PARAM_READWRITE,
                                            VncClientSrc::set_vnc_server_address,
                                            VncClientSrc::get_vnc_server_address,
                                            this);
  install_property_by_pspec(custom_props_->get_gobject(),
                            vnc_server_address_prop_, "vnc_server_address",
                            "Address of the VNC server");

  capture_truecolor_prop_ = custom_props_->make_boolean_property("capture_truecolor",
                                            "Capture in 32bits if true, 16bits otherwise",
                                            capture_truecolor_,
                                            (GParamFlags) G_PARAM_READWRITE,
                                            VncClientSrc::set_capture_truecolor,
                                            VncClientSrc::get_capture_truecolor,
                                            this);
  install_property_by_pspec(custom_props_->get_gobject(),
                            capture_truecolor_prop_, "capture_truecolor",
                            "Capture in 32bits if true, 16bits otherwise");

  return true;
}

int
VncClientSrc::get_capture_truecolor(void *user_data) {
  auto ctx = (VncClientSrc *) user_data;
  return ctx->capture_truecolor_;
}

void
VncClientSrc::set_capture_truecolor(const int truecolor, void *user_data) {
  auto ctx = (VncClientSrc *) user_data;
  ctx->capture_truecolor_ = truecolor;
}

const gchar *
VncClientSrc::get_vnc_server_address(void *user_data) {
  auto ctx = (VncClientSrc *) user_data;
  return ctx->vnc_server_address_.c_str();
}

void
VncClientSrc::set_vnc_server_address(const gchar *address, void *user_data) {
  auto ctx = (VncClientSrc *) user_data;
  if (address != nullptr)
    ctx->vnc_server_address_ = address;
}

rfbBool
VncClientSrc::resize_vnc(rfbClient *client) {
  auto width = client->width;
  auto height = client->height;
  auto depth = client->format.bitsPerPixel;

  auto that = static_cast<VncClientSrc*>(rfbClientGetClientData(client, (void*)(&VncClientSrc::resize_vnc)));
  if (!that)
    return FALSE;
  that->framebuffer_.resize(width * height * depth / 8);

  client->updateRect.x = 0;
  client->updateRect.y = 0;
  client->updateRect.w = width;
  client->updateRect.h = height;

  if (that->capture_truecolor_) {
    client->format.redShift = 0;
    client->format.redMax = 255;
    client->format.greenShift = 8;
    client->format.greenMax = 255;
    client->format.blueShift = 16;
    client->format.blueMax = 255;
  }
  else {
    client->format.redShift = 11;
    client->format.redMax = 31;
    client->format.greenShift = 5;
    client->format.greenMax = 63;
    client->format.blueShift = 0;
    client->format.blueMax = 31;
  }

  client->frameBuffer = that->framebuffer_.data();
  SetFormatAndEncodings(client);

  return TRUE;
}

void
VncClientSrc::update_vnc(rfbClient *client, int x, int y, int w, int h) {
  auto that = static_cast<VncClientSrc*>(rfbClientGetClientData(client, (void*)(&VncClientSrc::resize_vnc)));

  auto width = client->width;
  auto height = client->height;
  auto depth = client->format.bitsPerPixel;

  size_t framebufferSize = width * height * depth / 8;
  if (!that->vnc_writer_ ||
      framebufferSize > that->vnc_writer_->writer(&shmdata::Writer::alloc_size) ||
      that->previous_truecolor_state_ != that->capture_truecolor_)
  {
    auto data_type = string();
    if (that->capture_truecolor_)
      data_type = "video/x-raw,format=(string)RGBA,width=(int)" + to_string(width) + ",height=(int)" + to_string(height) + ",framerate=30/1";
    else
      data_type = "video/x-raw,format=(string)RGB16,width=(int)" + to_string(width) + ",height=(int)" + to_string(height) + ",framerate=30/1";

    that->previous_truecolor_state_ = that->capture_truecolor_;

    that->vnc_writer_.reset();
    that->vnc_writer_ = std2::make_unique<ShmdataWriter>(that,
                                                         that->make_file_name("vnc"),
                                                         framebufferSize,
                                                         data_type);
    if (!that->vnc_writer_) {
      g_warning("Unable to create VNC writer");
      return;
    }
  }

  that->vnc_writer_->writer(&shmdata::Writer::copy_to_shm, client->frameBuffer, framebufferSize);
  that->vnc_writer_->bytes_written(framebufferSize);
}

}