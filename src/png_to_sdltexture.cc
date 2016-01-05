/*
  Copyright (C) 2015 Solra Bizna

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "tttpclient.hh"
#include "io.hh"
#include <png.h>

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <iostream>
#include <memory>

static void png_error_handler(png_structp libpng, const char* what) {
  (void)libpng;
  throw std::string("libpng error: ")+what;
}

class RAIIFileClose {
  FILE*& f;
public:
  RAIIFileClose(FILE*& f) : f(f) {}
  ~RAIIFileClose() { fclose(f); }
};

class RAIIPngEnder {
  png_structp& libpng;
  png_infop& info;
public:
  RAIIPngEnder(png_structp&libpng, png_infop&info):libpng(libpng),info(info){}
  ~RAIIPngEnder() { png_destroy_read_struct(&libpng, &info, NULL); }
};

struct Wrapped_SDL_DestroyTexture {
  void operator()(SDL_Texture* tex) { SDL_DestroyTexture(tex); }
};

struct Wrapped_safe_free {
  void operator()(void* ptr) { safe_free(ptr); }
};

SDL_Texture* png_to_sdl_texture(SDL_Renderer* renderer, const char* path,
                                int& out_width, int& out_height) {
  FILE* f = IO::OpenRawPathForRead(path);
  if(!f) throw std::string("Opening ") + path + ": " + strerror(errno);
  RAIIFileClose this_variable_has_a_stupid_name_and_so_does_its_type(f);
  // I just keep stealing SubCritical's PNG loading code, I can't stop myself!
  // It does exactly what I want because I wrote it! >_<
  png_structp libpng = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                              NULL, png_error_handler, NULL);
  if(!libpng) throw std::string("error initializing libpng");
  png_infop info = png_create_info_struct(libpng);
  if(!info) {
    png_destroy_read_struct(&libpng, NULL, NULL);
    throw std::string("error initialiing libpng");
  }
  RAIIPngEnder another_stupidly_named_variable(libpng, info);
  png_init_io(libpng, f);
  png_read_info(libpng, info);
  png_uint_32 width, height;
  int depth, color_type, ilace_method, compression_method, filter_method;
  png_get_IHDR(libpng, info, &width, &height, &depth, &color_type, &ilace_method, &compression_method, &filter_method);
  out_width = width; out_height = height;
  // For a 16-bpc image, strip off 8 bits.
  if(depth >= 16) png_set_strip_16(libpng);
  // For a grayscale image, expand G to GGG.
  if(!(color_type & PNG_COLOR_MASK_COLOR)) png_set_gray_to_rgb(libpng);
  // For a palettized image, expand P to RGB.
  if((color_type & PNG_COLOR_TYPE_PALETTE)) png_set_palette_to_rgb(libpng);
  // If there is no alpha channel, fake one.
  png_set_filler(libpng, ~0, PNG_FILLER_AFTER);
  // If there is tRNS, make alpha from it.
  if(png_get_valid(libpng, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(libpng);
  // (We assume that the incoming PNG data is "close enough" to sRGB.)
  // At this point, we should now have 8-bit RGBA data.
  // Load interlaced PNGs properly.
  (void)png_set_interlace_handling(libpng);
  // Inform libpng of our changes and proceed.
  png_read_update_info(libpng, info);
  if(height * sizeof(uint8_t*) / sizeof(uint8_t*) != height)
    throw std::string("integer overflow");
  uint32_t total_size = width*height*4;
  if(total_size / 4 / height != width) throw std::string("integer overflow");
  if(total_size + height * sizeof(uint8_t*) < total_size)
    throw std::string("integer overflow");
  total_size += height * sizeof(uint8_t*);
  std::unique_ptr<SDL_Texture, Wrapped_SDL_DestroyTexture> tex
    (SDL_CreateTexture(renderer,
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
                       SDL_PIXELFORMAT_RGBA8888,
#else
                       SDL_PIXELFORMAT_ABGR8888,
#endif
                       SDL_TEXTUREACCESS_STATIC, width, height));
  if(!tex) throw std::string("Creating texture: ")+SDL_GetError();
  std::unique_ptr<uint8_t, Wrapped_safe_free> buffer
    (reinterpret_cast<uint8_t*>(safe_malloc(total_size)));
  if(!buffer) throw std::string("memory allocation failure");
  uint8_t* p = buffer.get() + height * sizeof(uint8_t*);
  uint8_t** q = reinterpret_cast<uint8_t**>(buffer.get());
  for(uint32_t y = 0; y < height; ++y) {
    *q++ = p;
    p += width * 4;
  }
  png_read_image(libpng, reinterpret_cast<uint8_t**>(buffer.get()));
  png_read_end(libpng, info);
  SDL_UpdateTexture(tex.get(), NULL,
                    reinterpret_cast<uint8_t**>(buffer.get())[0], width*4);
  return tex.release();
  // and then we clean up, through the magic of RAII
}

