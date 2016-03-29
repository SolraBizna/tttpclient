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

assert(#arg == 2, "usage: glsl2h in.glsl out.h")

local i = assert(io.open(arg[1],"rb"))
local o = assert(io.open(arg[2],"wb"))
for l in i:lines() do
   if #l > 0 and l:sub(1,2) ~= "//" then
      o:write((("%q\n"):format(l.."\n"):gsub("\\\n","\\n")))
   end
end
