/*
  Copyright (C) 2015-2016 Solra Bizna

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

#include "font.hh"
#include "io.hh"
#include <png.h>

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <iostream>

static void png_error_handler(png_structp libpng, const char* what) {
  (void)libpng;
  throw std::string("libpng error while loading the font: ")+what;
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

Font::Font(const char* fontpath)
  : buffer(NULL) {
  FILE* f = IO::OpenRawPathForRead(fontpath);
  if(!f) throw std::string("Opening the font: ")+strerror(errno);
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
  if((width & 15) || (height & 15))
    throw std::string("Selected font image does not contain a 16 x 16 grid of glyphs");
  this->width = width; this->height = height;
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
  buffer = (uint8_t*)safe_malloc(total_size);
  if(!buffer) throw std::string("memory allocation failure");
  uint8_t* p = buffer + height * sizeof(uint8_t*);
  uint8_t** q = (uint8_t**)buffer;
  for(uint32_t y = 0; y < height; ++y) {
    *q++ = p;
    p += width * 4;
  }
  png_read_image(libpng, (uint8_t**)buffer);
  png_read_end(libpng, info);
  bool has_alpha = false, has_color = false, has_grays = false;
  for(uint32_t y = 0; y < height; ++y) {
    uint8_t* p = ((uint8_t**)buffer)[y];
    for(uint32_t x = 0; x < width; ++x) {
      uint8_t r = *p++; uint8_t g = *p++; uint8_t b = *p++; uint8_t a = *p++;
      if(r == 255 && g == 0 && b == 255 && a == 255) {
        /* pure magenta = transparent */
        p[-4] = r = 0;
        p[-3] = g = 0;
        p[-2] = b = 0;
        p[-1] = a = 0;
      }
      if(a == 0) has_alpha = true;
      else {
        if(a != 255) has_alpha = true;
        if(r != g || g != b) has_color = true;
        if(r != 255) has_grays = true;
      }
    }
    if(has_alpha && has_color) break;
  }
  if(!has_color && !has_grays) {
    if(!has_alpha)
      std::cerr << "Warning: The chosen font appears to be a featureless white void" << std::endl;
#if 0
    // deleted; alpha and gray have different curves
    else {
      std::cerr << "Warning: The chosen font contains no information outside its alpha channels. Consider converting it to grayscale." << std::endl;
      /* no information was in the color channels, convert font from an alpha
         image to a grayscale one */
      has_alpha = false;
      for(uint32_t y = 0; y < height; ++y) {
        uint8_t* p = row_buffer->GetRows()[y];
        for(uint32_t x = 0; x < width; ++x) {
          p[2] = p[1] = p[0] = p[3]; p[3] = 255;
        }
      }
    }
#endif
  }
  // and then we clean up, through the magic of RAII
}

Font::~Font() {
  if(buffer) { safe_free(buffer); }
}
