
local pairs    = pairs
local math     = math
local tostring = tostring
local string   = string
local type     = type
local print    = print

module("org.conman.table")

function show(l)
  local l = l or _G
  local maxkeylen = 0
  local maxvallen = 0
  
  local function conv_cntl(s)
    if s == "\n" then
      return "\\n"
    else
      return "\\?"
    end
  end
  
  for k,v in pairs(l)
  do
    maxkeylen = math.max(maxkeylen,string.len(tostring(k)))
    maxvallen = math.max(maxvallen,string.len(string.format("%q",tostring(v))))
  end
  
  ----------------------------------------------------
  -- clip the value portion to both the key and the
  -- value will display without wrapping in an 80
  -- column screen.  Also, the Lua runtime does not
  -- support formats longer than 99 characters wide.
  -----------------------------------------------------
  
  maxkeylen = math.min(30,maxkeylen)
  maxvallen = math.min(77 - maxkeylen,maxvallen)
  
  local format = string.format("%%-%ds %%-%ds",maxkeylen + 1,maxvallen + 1)
  
  for k,v in pairs(l)
  do
    local vt
    
    if type(v) == "string" then
      vt = string.format("%q",v)
    else
      vt = tostring(v)
    end
    
    vt = string.gsub(vt,"%c",conv_cntl)
    vt = string.sub(vt,1,maxvallen)
    print(string.format(format,tostring(k),vt))
  end
end

