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

#ifndef PAINT_FRAMEBUFFER_HH
#define PAINT_FRAMEBUFFER_HH

class Framebuffer {
  uint8_t* framebuffer, *glyphbuffer; // glyphbuffer points inside framebuffer
  int32_t width, height;
  int32_t dirty_left, dirty_top, dirty_right, dirty_bot;
public:
  Framebuffer() : framebuffer(nullptr), glyphbuffer(nullptr),
                  width(0), height(0), dirty_left(0), dirty_top(0),
                  dirty_right(0), dirty_bot(0) {}
  Framebuffer(int32_t width, int32_t height)
    : width(width), height(height),
      dirty_left(0), dirty_top(0), dirty_right(width-1), dirty_bot(height-1) {
    framebuffer = reinterpret_cast<uint8_t*>(safe_calloc(width*2, height));
    glyphbuffer = framebuffer + width * height;
  }
  Framebuffer(const Framebuffer& other) {
    Resize(other.width, other.height, true);
    if(other.framebuffer) {
      memcpy(framebuffer, other.framebuffer, width*height*2);
      dirty_left = 0;
      dirty_top = 0;
      dirty_right = width-1;
      dirty_bot = height-1;
    }
  }
  Framebuffer(Framebuffer&& other) {
    if(framebuffer) safe_free(framebuffer);
    framebuffer = other.framebuffer;
    glyphbuffer = other.glyphbuffer;
    width = other.width;
    height = other.height;
    dirty_left = other.dirty_left;
    dirty_top = other.dirty_top;
    dirty_right = other.dirty_right;
    dirty_bot = other.dirty_bot;
    other.framebuffer = nullptr;
    other.glyphbuffer = nullptr;
    other.width = 0;
    other.height = 0;
    other.dirty_left = 0;
    other.dirty_top = 0;
    other.dirty_right = 0;
    other.dirty_bot = 0;
  }
  Framebuffer& operator=(const Framebuffer& other) {
    if(width == other.width && height == other.height) {
      Copy(other, 0, 0);
      dirty_left = other.dirty_left;
      dirty_top = other.dirty_top;
      dirty_right = other.dirty_right;
      dirty_bot = other.dirty_bot;
    }
    else {
      Resize(other.width, other.height, true);
      if(other.framebuffer) {
        memcpy(framebuffer, other.framebuffer, width*height*2);
        dirty_left = 0;
        dirty_top = 0;
        dirty_right = width-1;
        dirty_bot = height-1;
      }
    }
    return *this;
  }
  Framebuffer& operator=(Framebuffer&& other) {
    if(framebuffer) safe_free(framebuffer);
    framebuffer = other.framebuffer;
    glyphbuffer = other.glyphbuffer;
    width = other.width;
    height = other.height;
    dirty_left = other.dirty_left;
    dirty_top = other.dirty_top;
    dirty_right = other.dirty_right;
    dirty_bot = other.dirty_bot;
    other.framebuffer = nullptr;
    other.glyphbuffer = nullptr;
    other.width = 0;
    other.height = 0;
    other.dirty_left = 0;
    other.dirty_top = 0;
    other.dirty_right = 0;
    other.dirty_bot = 0;
    return *this;
  }
  ~Framebuffer() { if(framebuffer != nullptr) safe_free(framebuffer); }
  inline uint8_t* GetColorPointer(int x = 0, int y = 0) const {
    return framebuffer + width*y + x;
  }
  inline uint8_t* GetGlyphPointer(int x = 0, int y = 0) const {
    return glyphbuffer + width*y + x;
  }
  inline int32_t GetWidth() const { return width; }
  inline int32_t GetHeight() const { return height; }
  inline int32_t GetPitch() const { return width; }
  inline uint8_t* GetBuffer() const { return framebuffer; }
  void PrintStr(int x, int y, uint8_t color, const std::string& str) {
    uint8_t* cp = GetColorPointer(x, y);
    uint8_t* gp = GetGlyphPointer(x, y);
    for(auto c : str) { *cp++ = color; *gp++ = c; }
  }
  inline void DirtyPoint(int x, int y) {
    DirtyRegion(x, y, x, y);
  }
  inline void DirtyRect(int x, int y, int w, int h) {
    DirtyRegion(x, y, x+w-1, y+h-1);
  }
  inline void DirtyRegion(int l, int t, int r, int b) {
    if(l < dirty_left) dirty_left = l;
    if(t < dirty_top) dirty_top = t;
    if(r > dirty_right) dirty_right = r;
    if(b > dirty_bot) dirty_bot = b;
  }
  inline void DirtyWhole() {
    DirtyRect(0, 0, width, height);
  }
  void Update(Display& display) {
    if(dirty_left <= dirty_right && dirty_top <= dirty_bot) {
      display.Update(width, height,
                     dirty_left, dirty_top,
                     dirty_right-dirty_left+1,
                     dirty_bot-dirty_top+1,
                     framebuffer);
    }
    dirty_left = width; dirty_top = height;
    dirty_right = 0; dirty_bot = 0;
  }
  void Update(Framebuffer& dst, int dst_x = 0, int dst_y = 0) {
    if(dirty_left <= dirty_right && dirty_top <= dirty_bot) {
      int src_x = dirty_left;
      int src_y = dirty_top;
      int src_w = dirty_right - dirty_left + 1;
      int src_h = dirty_bot - dirty_top + 1;
      dst_x += src_x;
      dst_y += src_y;
      if(dst.width == width && dst_x == 0 && src_x == 0 && src_w == width) {
        memcpy(dst.GetColorPointer(0,dst_y), GetColorPointer(0,src_y),
               width*src_h);
        memcpy(dst.GetGlyphPointer(0,dst_y), GetGlyphPointer(0,src_y),
               width*src_h);
      }
      else {
        for(int y = 0; y < src_h; ++y) {
          memcpy(dst.GetColorPointer(dst_x,dst_y+y), GetColorPointer(src_x,
                                                                     src_y+y),
                 src_w);
          memcpy(dst.GetGlyphPointer(dst_x,dst_y+y), GetGlyphPointer(src_x,
                                                                     src_y+y),
                 src_w);
        }
      }
      dst.DirtyRect(dst_x, dst_y, src_w, src_h);
    }
    dirty_left = width; dirty_top = height;
    dirty_right = 0; dirty_bot = 0;
  }
  void Copy(const Framebuffer& src, int dst_x, int dst_y) {
    DirtyRegion(dst_x, dst_y, dst_x + src.width, dst_y + src.height);
    if(src.width == width && dst_x == 0) {
      memcpy(GetColorPointer(0,dst_y), src.GetColorPointer(0,0),
             src.width*src.height);
      memcpy(GetGlyphPointer(0,dst_y), src.GetGlyphPointer(0,0),
             src.width*src.height);
    }
    else {
      for(int y = 0; y < src.height; ++y) {
        memcpy(GetColorPointer(dst_x,dst_y+y), src.GetColorPointer(0,y),
               src.width);
        memcpy(GetGlyphPointer(dst_x,dst_y+y), src.GetGlyphPointer(0,y),
               src.width);
      }
    }
  }
  void Resize(int32_t new_width, int32_t new_height, bool uninitialized=false){
    if(framebuffer) safe_free(framebuffer);
    width = new_width;
    height = new_height;
    if(uninitialized)
      framebuffer = reinterpret_cast<uint8_t*>(safe_malloc(width*2*height));
    else
      framebuffer = reinterpret_cast<uint8_t*>(safe_calloc(width*2,height));
    glyphbuffer = framebuffer + width * height;
    if(!uninitialized) {
      dirty_left = 0; dirty_top = 0; dirty_right = width-1; dirty_bot=height-1;
    }
  }
};

#endif
