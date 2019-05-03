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

local errno  = require "org.conman.errno"
local string = require "string"
local table  = require "table"
local lpeg   = require "lpeg"

local select = select
local type   = type
local error  = error
local unpack = table.unpack or unpack

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

local READER =
{
  ['*n'] = function()
    error "Not implemented"
  end,
  
  ['*l'] = function(ios)
    return read_data(ios,LINE)
  end,
  
  ['*L'] = function(ios)
    return read_data(ios,LINEcrlf)
  end,
  
  ['*a'] = function(ios)
    if ios._eof then
      return ""
    end
    
    repeat
      local data = ios:_refill()
      ios._readbuf = ios._readbuf .. data
    until #data == 0
    
    ios._eof = true
    return ios._readbuf
  end,
  
  ['*h'] = function(ios)
    return read_data(ios,HEADERS)
  end,
}

READER['a'] = READER['*a']
READER['l'] = READER['*l']
READER['n'] = READER['*n']
READER['L'] = READER['*L']
READER['h'] = READER['*h']

-- *******************************************************************
--
--	IO Routines
--
-- These mimic the Lua file:*() routines (duck typing and all that).  So
-- these *can* be used in places where you might otherwise use a file, but
-- beware that some of the functions night not do what you expect them to
-- do.
--
-- *******************************************************************
-- Usage:       okay[,errmsg,err] = ios:close()
-- Desc:        Close a TCP connection
-- Return:      okay (boolean) true if success, false if error
--              errmsg (string/optional) system error message
--              err (integer/optional) system error code
-- NOTE:	This should be overridden
-- *******************************************************************

local function close()
  return false,errno[errno.ENOSYS],errno.ENOSYS
end

-- *******************************************************************
-- Usage:       okay[,errmsg,err] = ios:flush()
-- Desc:        Flush any pending output
-- Return       okay (boolean) true if success, false if error
--              errmsg (string/optional) system error message
--              err (integer/optional) sytem error code
-- *******************************************************************

local function flush()
  return true
end

-- *******************************************************************
-- Usage:       intf = ios:lines()
-- Desc:        Interate through lines
-- Note:        This function is not implemented due to technical reasons
-- *******************************************************************

local function lines()
  -- XXX Lua 5.1 "dead: attempt to yield across metamethod/C-call boundary
  error "lines() not implemented"
end

-- *******************************************************************
-- Usage:       data... = ios:read([format...])
-- Desc:        Read data from a TCP connection
-- Input:       format (string/optionsl) format string per file:read()
-- Return:      data (strong number) data returned per format
-- *******************************************************************

local function read(ios,...)
  local function read_bytes(amount)
    if ios._eof    then return     end
    if amount == 0 then return ""  end
    
    if #ios._readbuf >= amount then
      local data   = ios._readbuf:sub(1,amount)
      ios._readbuf = ios._readbuf:sub(amount + 1,-1)
      return data
    end
    
    local data = ios:_refill()

    if data == "" then
      ios._eof = true
      assert(#ios._readbuf <= amount)
      return ios._readbuf
    end
    
    ios._readbuf = ios._readbuf .. data
    return read_bytes(amount)
  end
  
  -- -----------------------------------------------------
  
  local maxparam = select('#',...)
  
  if maxparam == 0 then
    return READER['l'](ios)
  end
  
  local res = {}
  
  for i = 1 , maxparam do
    local format = select(i,...)
    local data
    
    if type(format) == 'number' then
      data = read_bytes(format)
    else
      if READER[format] then
        data = READER[format](ios)
      else
        error(string.format("bad argument #%d to 'read' (invalid format)",i))
      end
    end
    
    table.insert(res,data)
  end
  
  return unpack(res)
end

-- *******************************************************************
-- Usage:       okay[,errmsg,err] = ios:seek([whence][,offset])
-- Desc:        Seek to an arbitrary position
-- Input:       whence (enum/optinal)
--                      * 'set'
--                      * 'cur' (default value)
--                      * 'end'
--              offset (integer/optional) offset to apply (0 default)
-- Return:      okay (boolean) true if success, false if error
--              errmsg (string/optional) system error message
--              err (integer/optional) system error code
--
-- NOTE:	This should be overriden.
-- *******************************************************************

local function seek()
  return nil,errno[errno.ESPIPE],errno.ESPIPE
end

-- *******************************************************************
-- Usage:       okay[,errmsg,err] = ios:setvbuf(mode[,size])
-- Desc:        Configure buffer mode
-- Input:       mode (enum)
--                      * 'no'
--                      * 'full'
--                      * 'line'
--              size (integer/optional) size of buffer
-- Return:      okay (boolean) true if success, false if error
--              errmsg (string/optional) system error message
--              err (integer/optional) system error code
-- *******************************************************************

local function setvbuf()
  return true
end

-- *******************************************************************
-- Usage:       okay[,errmsg,err] = ios:write([data...])
-- Desc:        Write data to TCP connection
-- Input:       data (string) data to write
-- Return:      okay (boolean) true if success, false if error
--              errmsg (string/optional) system error message
--              err (integer/optional) system error code
-- *******************************************************************

local function write(ios,...)
  if ios._eof then
    return false,errno[errno.ECONNRESET],errno.ECONNRESET
  end
  
  local output = ""
  
  for i = 1 , select('#',...) do
    local data = select(i,...)
    if type(data) ~= 'string' and type(data) ~= 'number' then
      error("string or number expected, got " .. type(data))
    end
    
    output = output .. data
  end
  
  ios:_drain(output)
  return true
end

-- *******************************************************************

return function()
  return {
    close   = close,
    flush   = flush,
    lines   = lines,
    read    = read,
    seek    = seek,
    setvbuf = setvbuf,
    write   = write,
    
    _readbuf = "",
    _rpos    = 1,
    _eof     = false,
    _refill  = function() error("failed to provide ios._refill()") end,
    _drain   = function() error("failed to provide ios._drain()")  end,
  }
end
