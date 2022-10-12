-- luacheck: ignore 611

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

tap.plan(#tests * 5 + 4)

for _,t in ipairs(tests) do
  local b = base64(t)
  local x = b:encode(data)
  tap.assert(x,"%s: encoding",t.name)
  if t.len > 0 then
    tap.assert(#x:match("^(.-)\n") == t.len,"%s: length check",t.name)
  else
    tap.assert(#x == 683,"%s: length check",t.name)
  end
  if t.pad == "=" then
    tap.assert(x:sub(-1,-1) == t.pad,"%s: pad check",t.name)
  else
    tap.assert(x:sub(-1,-1) ~= t.pad,"%s: pad check",t.name)
  end
  local y = b:decode(x)
  tap.assert(y,"%s: decoding",t.name)
  tap.assert(y == data,"%s: decoding data check",t.name)
end

local b = base64 { strict = true }
local y = b:decode("QQ==")
tap.assert(y,"strict mode: decoding good data")
tap.assert(#y == 1,"strict mode: decoding length good data")
tap.assert(y == 'A',"strict mode: decoding good data check")
y = b:decode("QR==")
tap.assert(not y,"strict mode: decoding malicious data")

os.exit(tap.done())
