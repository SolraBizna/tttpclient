// -*- c -*-

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

uniform vec2 scale;
uniform vec2 offset;
uniform vec2 font_glyph_size;

attribute vec2 font_coord_in;
attribute vec2 palette_coord_in;

varying vec4 data;

void main(void) {
  gl_Position = vec4(gl_Vertex.xy * scale * offset, 0, 1);
  data = vec4(font_coord_in * font_glyph_size, palette_coord_in);
}
