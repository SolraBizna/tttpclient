assert(#arg == 2, "usage: glsl2h in.glsl out.h")

local i = assert(io.open(arg[1],"rb"))
local o = assert(io.open(arg[2],"wb"))
for l in i:lines() do
   if #l > 0 and l:sub(1,2) ~= "//" then
      o:write((("%q\n"):format(l.."\n"):gsub("\\\n","\\n")))
   end
end
