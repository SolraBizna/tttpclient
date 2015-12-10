// -*- c -*-

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

uniform sampler2DRect font, palette;
//varying vec2 font_coord; // X,Y = character cell
//varying vec2 palette_coord; // Z = bg, W = fg
varying vec4 data; // X,Y = character cell, Z = bg, W = fg

#if SRGB_MODE && !SRGB_TEXTURES
uniform sampler1D sRGB_to_linear_sampler;
const float sRGB_to_linear_table_size = float(SRGB_TO_LINEAR_TABLE_SIZE);
const vec4 sRGB_to_linear_deshift_mul = vec4((sRGB_to_linear_table_size-1.0)/sRGB_to_linear_table_size,(sRGB_to_linear_table_size-1.0)/sRGB_to_linear_table_size,(sRGB_to_linear_table_size-1.0)/sRGB_to_linear_table_size,1);
const vec4 sRGB_to_linear_deshift_add = vec4(-1.0/sRGB_to_linear_table_size,-1.0/sRGB_to_linear_table_size,-1.0/sRGB_to_linear_table_size,0);
vec4 linear_to_sRGB(vec4 q) {
  q = q * sRGB_to_linear_deshift_mul + sRGB_to_linear_deshift_add;
  return vec4(texture1D(sRGB_to_linear_sampler, q.r).r,
              texture1D(sRGB_to_linear_sampler, q.g).r,
              texture1D(sRGB_to_linear_sampler, q.b).r,
              q.a);
}
#else
vec4 sRGB_to_linear(vec4 q) { return q; }
#endif
#if SRGB_MODE && !SRGB_FRAMEBUFFER
uniform sampler1D linear_to_sRGB_sampler;
const float linear_to_sRGB_table_size = (float)LINEAR_TO_SRGB_TABLE_SIZE;
const vec4 linear_to_sRGB_deshift_mul = vec4((linear_to_sRGB_table_size-1.0)/linear_to_sRGB_table_size,(linear_to_sRGB_table_size-1.0)/linear_to_sRGB_table_size,(linear_to_sRGB_table_size-1.0)/linear_to_sRGB_table_size,1);
const vec4 linear_to_sRGB_deshift_add = vec4(-1.0/linear_to_sRGB_table_size,-1.0/linear_to_sRGB_table_size,-1.0/linear_to_sRGB_table_size,0);
vec4 linear_to_sRGB(vec4 q) {
  q = q * linear_to_sRGB_deshift_mul + linear_to_sRGB_deshift_add;
  return vec4(texture1D(linear_to_sRGB_sampler, q.r).r,
              texture1D(linear_to_sRGB_sampler, q.g).r,
              texture1D(linear_to_sRGB_sampler, q.b).r,
              q.a);
}
#else
vec4 linear_to_sRGB(vec4 q) { return q; }
#endif

void main(void) {
  vec4 texel = sRGB_to_linear(texture2DRect(font, data.xy));
  vec4 bg = sRGB_to_linear(texture2DRect(palette, vec2(data.z,0)));
  vec4 fg = sRGB_to_linear(texture2DRect(palette, vec2(data.w,0)));
#if ALPHA == 0
# if COLOR == 0
  gl_FragColor = linear_to_sRGB(mix(bg,fg,texel.r));
# else // COLOR == 1
  gl_FragColor = linear_to_sRGB(mix(bg,fg,texel));
# endif
#else // ALPHA == 1
# if COLOR == 0
  gl_FragColor = linear_to_sRGB(mix(bg,fg*texel.r,texel.a));
# else // COLOR == 1
  gl_FragColor = linear_to_sRGB(mix(bg,fg*texel,texel.a));
# endif
#endif
}
