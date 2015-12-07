// -*- c -*-

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
