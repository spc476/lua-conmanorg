-- ***************************************************************
--
-- Copyright 2010 by Sean Conner.  All Rights Reserved.
--
-- This library is free software; you can redistribute it and/or modify it
-- under the terms of the GNU Lesser General Public License as published by
-- the Free Software Foundation; either version 3 of the License, or (at your
-- option) any later version.
--
-- This library is distributed in the hope that it will be useful, but
-- WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
-- or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
-- License for more details.
--
-- You should have received a copy of the GNU Lesser General Public License
-- along with this library; if not, see <http://www.gnu.org/licenses/>.
--
-- Comments, questions and criticisms can be sent to: sean@conman.org
--
-- ********************************************************************
-- luacheck: globals EDITOR edit editf hexdump
-- luacheck: ignore 611

local os       = require "os"
local debug    = require "debug"
local io       = require "io"
local string   = require "string"
local char     = require "org.conman.parsers.ascii.char"
local lpeg     = require "lpeg"

local _VERSION = _VERSION
local dofile   = dofile
local loadstring

if _VERSION == "Lua 5.1" then
  loadstring = _G.loadstring
  module("org.conman.debug")
else
  loadstring = load
  _ENV       = {}
end

EDITOR = os.getenv("VISUAL") or os.getenv("EDITOR") or "/bin/vi"

-- *********************************************************************

local function create()
  local name = os.tmpname()
  local src
  local f
  
  os.execute(EDITOR .. " " .. name)
  f = io.open(name,"r")
  src = f:read("*a")
  f:close()
  os.remove(name)
  local code = loadstring(src)
  code()
end

-- ********************************************************************

function edit(f)
  if f == nil then
    create()
    return
  end
  
  local info = debug.getinfo(f)
  local src
  
  if info.source:byte(1) == 64 then
    local fin   = io.open(info.source:sub(2),"r")
    local count = 0
    local line
    
    repeat
      line  = fin:read("*l")
      count = count + 1
    until count == info.linedefined
    
    src = line .. "\n"
    
    while count < info.lastlinedefined do
      src = src .. fin:read("*l") .. "\n"
      count = count + 1
    end
    
    fin:close()
  else
    src = info.source
  end
  
  local name = os.tmpname()
  local fin  = io.open(name,"w")
  fin:write(src)
  fin:close()
  
  os.execute(EDITOR .. " " .. name)
  
  f = io.open(name,"r")
  src = f:read("*a")
  f:close()
  os.remove(name)
  
  local code = loadstring(src)
  code()
end

-- **********************************************************************

function editf(f)
  if f == nil then
    create()
    return
  end
  
  local info = debug.getinfo(f)
  
  if info.source:byte(1) == 64 then
    local file   = info.source:sub(2)
    
    os.execute(string.format("%s +%d %s",
                EDITOR,
                info.linedefined,
                file
        ))
    dofile(file)
  elseif info.source:byte(1) == 61 then
    print("can't edit---no source")
  else
    local name   = os.tmpname()
    local fin    = io.open(name,"w")
    
    fin:write(info.source)
    fin:close()
    
    os.execute(EDITOR .. " " .. name)
    fin = io.open(name,"r")
    local src = fin:read("*a")
    fin:close()
    os.remove(name)
    
    assert(loadstring(src))
  end
end

-- ********************************************************************

local toascii = lpeg.Cs((char + lpeg.P(1) / ".")^0)
local tohex   = lpeg.Cs((lpeg.P(1) / function(c)
  return ("%02X "):format(c:byte())
end)^0)

function hexdump(bin,delta,bias,out)
  delta = delta or 16
  bias  = bias  or 0
  out   = out   or function(off,hex,ascii,d)
    io.stderr:write(
        ("%08X: "):format(off),
        hex,
        (" "):rep(3 * (d - (#hex / 3))),
        ascii,
        "\n"
    )
  end
  
  local offset = 1
  
  while offset <= #bin do
    local s = bin:sub(offset,offset + (delta - 1))
    local h = tohex:match(s)
    local a = toascii:match(s)
    
    out((offset - 1) + bias,h,a,delta)
    offset = offset + delta
  end
end

-- ********************************************************************

if _VERSION >= "Lua 5.2" then
  return _ENV
end
