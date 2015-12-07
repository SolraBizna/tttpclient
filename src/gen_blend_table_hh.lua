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
