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

local f = assert(io.open("src/cp437_to_utf8.utxt","rb"))

local char_map = {}
local char_table = {}
local c = 0
for l in f:lines() do
   char_map[c] = #char_table
   for n=1,#l do table.insert(char_table, l:byte(n)) end
   table.insert(char_table, 0)
   c = c + 1
end
assert(c == 256, "cp437_to_utf8.utxt wasn't 256 lines long!")
f:close()
assert(char_table[255] < 65536, "cp437_to_utf8.utxt was too complicated!")

f = assert(io.open("include/gen/char_table.hh","wb"))
f:write("static const uint8_t char_table["..#char_table.."] = {")
for n=1,#char_table do f:write(char_table[n]..",") end
f:write("};\nstatic const uint16_t char_map[256] = {")
for n=0,255 do f:write(char_map[n]..",") end
f:write("};\n")
f:close()
