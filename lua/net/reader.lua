-- *******************************************************************
--
-- Copyright 2019 by Sean Conner.  All Rights Reserved.
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
-- *******************************************************************
-- luacheck: globals accept listena listen connecta connect
-- luacheck: ignore 611

local lpeg     = require "lpeg"
local _VERSION = _VERSION
local error    = error

if _VERSION == "Lua 5.1" then
  module("org.conman.net.reader")
  local _ENV = _M -- luacheck: ignore
else
  _ENV = {}
end

-- *******************************************************************

local CRLF     = lpeg.P"\r"^-1 * lpeg.P"\n"
local notCRLF  = lpeg.R("\0\9","\11\12","\14\255")

	-- -------------------------------------------------------
	-- The LPEG patterns LINE, LINEcrlf and HEADERS return the
	-- following values:
	--
	--	poscap	- maximum position of capture
	--	posres	- position to resume scanning
	--	eol	- true for full capture, false if need more
	-- --------------------------------------------------------

local LINE     = notCRLF^0 * lpeg.Cp()
               * (
                   CRLF * lpeg.Cp() * lpeg.Cc(true)
                        + lpeg.Cp() * lpeg.Cc(false)
                 )
local LINEcrlf = notCRLF^0
               * (
                   CRLF * lpeg.Cp() * lpeg.Cp() * lpeg.Cc(true)
                        + lpeg.Cp() * lpeg.Cp() * lpeg.Cc(false)
                 )
local restq    = CRLF       * lpeg.Cp()       * lpeg.Cp() * lpeg.Cc(true)
               + notCRLF^1  * lpeg.Cp()       * lpeg.Cp() * lpeg.Cc(false)
               + lpeg.P(-1) * lpeg.Cb('here') * lpeg.Cp() * lpeg.Cc(false)
local HEADERS  = CRLF * CRLF * lpeg.Cp() * lpeg.Cp() * lpeg.Cc(true)
               + CRLF * (notCRLF^1 * lpeg.Cg(lpeg.Cp(),'here') * CRLF)^1 * restq
               +        (notCRLF^1 * lpeg.Cg(lpeg.Cp(),'here') * CRLF)^1 * restq
               +         notCRLF^1 * lpeg.P(-1) * lpeg.Cp() * lpeg.Cp() * lpeg.Cc(false)
               +                                  lpeg.Cp() * lpeg.Cp() * lpeg.Cc(false)

               
-- *******************************************************************
-- usage:	data = read_data(ios,pattern)
-- desc:	Read data from a source
-- input:	ios (table) Input/Output object
--		pattern (userdata/LPEG) one of LINE, LINEcrlf or HEADERS
-- return:	data (string) data from data source
-- *******************************************************************

local function read_data(ios,pattern)
  if ios._eof then
    return nil
  end
  
  local poscap,posres,eol = pattern:match(ios._readbuf,ios._rpos)
  
  if not eol then
    local data = ios:_refill()
    
    if #data == 0 then
      ios._eof = true
      return ios._readbuf
    end
    
    ios._rpos    = posres
    ios._readbuf = ios._readbuf .. data
    return read_data(ios,pattern)
    
  else
    local data   = ios._readbuf:sub(1,poscap-1)
    ios._readbuf = ios._readbuf:sub(posres,-1)
    ios._rpos    = 1
    return data
  end
end

-- *******************************************************************

_ENV['*n'] = function()
  error "Not implemented"
end

-- *******************************************************************

_ENV['*l'] = function(ios)
  return read_data(ios,LINE)
end

-- *******************************************************************

_ENV['*L'] = function(ios)
  return read_data(ios,LINEcrlf)
end

-- *******************************************************************

_ENV['*a'] = function(ios)
  if ios._eof then
    return ""
  end
  
  repeat
    local data = ios:_refill()
    ios._readbuf = ios._readbuf .. data
  until #data == 0
  
  ios._eof = true
  return ios._readbuf
end

-- *******************************************************************
 
_ENV['*h'] = function(ios)
  return read_data(ios,HEADERS)
end

-- *******************************************************************
  
_ENV['a'] = _ENV['*a']
_ENV['l'] = _ENV['*l']
_ENV['n'] = _ENV['*n']
_ENV['L'] = _ENV['*L']
_ENV['h'] = _ENV['*h']

-- *******************************************************************

if _VERSION >= "Lua 5.2" then
  return _ENV
end
