-- luacheck: ignore 611

package.path = "/home/spc/source/std/?.lua;" .. package.path

local tap    = require "tap14"
local base64 = require "org.conman.base64"

local data = "" do
  for i = 0 , 255 do
    data = data .. string.char(i)
  end
  for i = 255 , 0 , -1 do
    data = data .. string.char(i)
  end
  assert(#data == 512)
end

local tests =
{
  {
    name   = "RFC-1421 (deprecated)",
    last   = "+/",
    pad    = "=",
    len    = 64,
    ignore = false,
    strict = false,
    nocrlf = false,
  },

  {
    name   = "RFC-2045 (default)",
    last   = "+/",
    pad    = "=",
    len    = 76,
    ignore = true,
    strict = false,
    nocrlf = false,
  },
  
  {
    name   = "UTF-7",
    last   = "+/",
    pad    = "",
    len    = -1,
    ignore = false,
    strict = false,
    nocrlf = true,
  },
  
  {
    name   = "IMAP",
    last   = "+,",
    pad    = "",
    len    = -1,
    ignore = false,
    strict = false,
    nocrlf = true,
  },
  
  {
    name   = "RFC-4648 s4",
    last   = "+/",
    pad    = "",
    len    = -1,
    ignore = false,
    strict = false,
    nocrlf = true,
  },

  {
    name   = "RFC-4648 s5",
    last   = "-_",
    pad    = "",
    len    = -1,
    ignore = false,
    strict = false,
    nocrlf = true,
  },
}

tap:plan(#tests * 5 + 4)

for _,t in ipairs(tests) do
  local b = base64(t)
  local x = b:encode(data)
  tap:assert('fail',x,"%s: encoding",t.name)
  if t.len > 0 then
    tap:assert('fail',#x:match("^(.-)\n") == t.len,"%s: length check",t.name)
  else
    tap:assert('fail',#x == 683,"%s: length check",t.name)
  end
  if t.pad == "=" then
    tap:assert('fail',x:sub(-1,-1) == t.pad,"%s: pad check",t.name)
  else
    tap:assert('fail',x:sub(-1,-1) ~= t.pad,"%s: pad check",t.name)
  end
  local y = b:decode(x)
  tap:assert('fail',y,"%s: decoding",t.name)
  tap:assert('fail',y == data,"%s: decoding data check",t.name)
end

local b = base64 { strict = true }
local y = b:decode("QQ==")
tap:assert('fail',y,"strict mode: decoding good data")
tap:assert('fail',#y == 1,"strict mode: decoding length good data")
tap:assert('fail',y == 'A',"strict mode: decoding good data check")
y = b:decode("QR==")
tap:assert('fail',not y,"strict mode: decoding malicious data")

os.exit(tap:done())
