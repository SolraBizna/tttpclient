--[[
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
]]

function linear2srgb(x)
   if x <= 0.0031308 then return x * 12.92
   else return 1.055 * math.pow(x, 1/2.4) - 0.055 end
end

function srgb2linear(x)
   if x <= 0.04045 then return x / 12.92
   else return math.pow((x + 0.055) / 1.055, 2.4) end
end

local out = assert(io.open("include/gen/blend_table.hh", "w"))

out:write("const uint8_t blend_table[16*64*64] = {\n")

for x=0,63 do
   local from = srgb2linear(x/63)
   for y=0,63 do
      local to = srgb2linear(y/63)
      for a=0,15 do
         out:write(("  %i,\n"):format(math.floor(linear2srgb(from + (to-from)*a/15)*255+0.5)))
      end
   end
end

out:write("};\n")
