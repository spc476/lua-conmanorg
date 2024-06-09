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
-- luacheck: globals hexdump
-- luacheck: ignore 611

local io       = require "io"
local char     = require "org.conman.parsers.ascii.char"
local lpeg     = require "lpeg"
local _VERSION = _VERSION

if _VERSION == "Lua 5.1" then
  module("org.conman.debug")
else
  _ENV       = {}
end

-- *********************************************************************

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
