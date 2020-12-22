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

local string = require "string"
local table  = require "table"
local math   = require "math"

local select    = select
local type      = type
local error     = error
local unpack    = table.unpack or unpack
local tonumber  = tonumber
local tostring  = tostring
local tointeger = math.tointeger or function(s) return tonumber(s) end

-- *******************************************************************
-- usage:       number = read_number(ios)
-- desc:        Read a number (in text form) to a number
-- input:       ios (table) Input/Output object
-- return:      number (number) data from source, nil on error
--
-- Note:        This is a direct translation of read_number() from
--              liolib.c
-- *******************************************************************

local function read_number(ios)
  local function nextc(rn)
    if #rn.buff >= 200 then
      return false
    else
      rn.buff = rn.buff .. rn.c
      rn.c    = ios:read(1)
      return true
    end
  end
  
  local function test2(rn,c1,c2)
    if rn.c == c1 or rn.c == c2 then
      return nextc(rn)
    else
      return false
    end
  end
  
  local function readdigits(rn,hex)
    local count = 0
    while rn.c and rn.c:match(hex) and nextc(rn) do
      count = count + 1
    end
    return count
  end
  
  local rn        = { buff = "" , c = "" }
  local hex       = "%d"
  local exp1,exp2 = 'e','E'
  local count     = 0
  local integer   = true
  
  repeat
    rn.c = ios:read(1)
  until not rn.c or not rn.c:match"%s"
  
  test2(rn,'-','+')
  if test2(rn,'0','0') then
    if test2(rn,'x','X') then
      hex = "%x"
      exp1,exp2 = 'p','P'
    else
      count = 1
    end
  end
  
  count = count + readdigits(rn,hex)
  if test2(rn,'.','.') then
    count   = count + readdigits(rn,hex)
    integer = false
  end
  
  if count > 0 and test2(rn,exp1,exp2) then
    test2(rn,'-','+')
    readdigits(rn,"%d")
    integer = false
  end
  
  if rn.c then
    ios._readbuf = rn.c .. ios._readbuf -- ungetc()
  end
  
  if integer then
    return tointeger(rn.buff)
  else
    return tonumber(rn.buff)
  end
end

-- *******************************************************************

local function refill(ios)
  local data = ios:_refill()
  if not data then
    return false
  end
  
  ios._readbuf = ios._readbuf .. data
  ios._eof     = #data == 0
  
  return true
end

-- *******************************************************************

local READER READER =
{
  ['*n'] = function(ios)
    return read_number(ios)
  end,
  
  -- ====================================================================
  
  ['*l'] = function(ios)
    if ios._eof then
      local data   = ios._readbuf
      ios._readbuf = nil
      return data
    end
    
    local s = ios._readbuf:find("[\r\n]",ios._ridx)
    if not s then
      ios._ridx = #ios._readbuf + 1
      if not refill(ios) then return nil end
      return READER['*l'](ios)
    end
    
    local s2,e2 = ios._readbuf:find("\r?\n",s)
    if not s2 then
      if not refill(ios) then return nil end
      return READER['*l'](ios)
    end
    
    local data   = ios._readbuf:sub(1,s2-1)
    ios._readbuf = ios._readbuf:sub(e2 + 1,-1)
    ios._ridx    = 1
    return data
  end,
  
  -- ====================================================================
  
  ['*L'] = function(ios)
    if ios._eof then
      local data = ios._readbuf
      ios._readbuf = nil
      return data
    end
    
    local s = ios._readbuf:find("[\r\n]",ios._ridx)
    if not s then
      ios._ridx = #ios._readbuf + 1
      if not refill(ios) then return nil end
      return READER['*L'](ios)
    end
    
    local s2,e2 = ios._readbuf:find("\r?\n",s)
    if not s2 then
      if not refill(ios) then return nil end
      return READER['*L'](ios)
    end
    
    local data   = ios._readbuf:sub(1,e2)
    ios._readbuf = ios._readbuf:sub(e2 + 1,-1)
    ios._ridx    = 1
    return data
  end,
  
  -- ====================================================================
  
  ['*a'] = function(ios)
    if ios._eof then
      return ""
    end
    
    repeat
      local data = ios:_refill()
      if data then
        ios._readbuf = ios._readbuf .. data
      end
    until not data or #data == 0
    
    ios._eof = true
    return ios._readbuf
  end,
  
  -- ====================================================================
  
  ['*h'] = function(ios)
    if ios._eof then
      local data = ios._readbuf
      ios._readbuf = nil
      return data
    end
    
    -- ------------------------
    -- Scan for first CR or LF
    -- ------------------------
    
    local s = ios._readbuf:find("[\r\n]",ios._ridx)
    if not s then
      ios._ridx = #ios._readbuf + 1
      if not refill(ios) then return nil end
      return READER['*h'](ios)
    end
    
    -- ---------------
    -- Check for CRLF
    -- ---------------
    
    local s2,e2 = ios._readbuf:find("\r?\n",s)
    if not s2 then
      if not refill(ios) then return nil end
      return READER['*h'](ios)
    end
    
    -- --------------------------------------------------
    -- Check for CRLFCRLF.  If not, then advance pointer
    -- --------------------------------------------------
    
    local s3,e3 = ios._readbuf:find("\r?\n\r?\n",s2)
    
    if not s3 then
      ios._ridx = e2 + 1
      if not refill(ios) then return nil end
      return READER['*h'](ios)
    end
    
    local data   = ios._readbuf:sub(1,e3)
    ios._readbuf = ios._readbuf:sub(e3+1,-1)
    ios._ridx    = 1
    return data
  end,
  
  -- ====================================================================
  
  ['*b'] = function(ios)
    if ios._eof then
      local data   = ios._readbuf
      ios._readbuf = nil
      return data
    end
    
    if ios._readbuf == "" then
      local data = ios:_refill()
      if not data then return nil end
      if #data == 0 then
        ios._eof = true
        return nil
      else
        return data
      end
    end
    
    local data   = ios._readbuf
    ios._readbuf = ""
    ios._ridx    = 1
    return data
  end,
}

READER['a'] = READER['*a']
READER['l'] = READER['*l']
READER['n'] = READER['*n']
READER['L'] = READER['*L']
READER['h'] = READER['*h'] -- extension
READER['b'] = READER['*b'] -- extension

-- *******************************************************************
--
--      IO Routines
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
-- NOTE:        This should be overridden
-- *******************************************************************

local function close()
  return false,"Not implemented",-1
end

-- *******************************************************************
-- Usage:       okay[,errmsg,err] = ios:flush()
-- Desc:        Flush any pending output
-- Return       okay (boolean) true if success, false if error
--              errmsg (string/optional) system error message
--              err (integer/optional) sytem error code
-- NOTE:        This should be overridden
-- *******************************************************************

local function flush()
  return false,"Not implemented",-1
end

-- *******************************************************************
-- Usage:       intf = ios:lines()
-- Desc:        Interate through lines
-- Return:      intf (function) interator
-- Note:        This function doesn't work in Lua 5.1
-- *******************************************************************

local function lines(ios)
  -- XXX Lua 5.1 "dead: attempt to yield across metamethod/C-call boundary
  return function()
    return ios:read("*l")
  end
end

-- *******************************************************************
-- Usage:       data... = ios:read([format...])
-- Desc:        Read data from a TCP connection
-- Input:       format (string/optionsl) format string per file:read()
-- Return:      data (strong number) data returned per format
-- *******************************************************************

local function read(ios,...)
  local function read_bytes(amount)
  
    if #ios._readbuf >= amount then
      local data   = ios._readbuf:sub(1,amount)
      ios._readbuf = ios._readbuf:sub(amount + 1,-1)
      return data
    end
    
    if ios.eof then
      if #ios._readbuf > 0 then
        local data = ios._readbuf
        ios._readbuf = ""
        return data
      else
        return nil
      end
    end
    
    local data = ios:_refill()
    
    if not data then
      return nil
    else
      ios.eof = #data == 0
      ios._readbuf = ios._readbuf .. data
      return read_bytes(amount)
    end
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
    
    if not data then break end
    
    table.insert(res,data)
    ios._rbytes = ios._rbytes + #tostring(data)
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
-- NOTE:        This should be overriden.
-- *******************************************************************

local function seek()
  return nil,"Not implemented",-1
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
-- NOTE:        This should be overridden
-- *******************************************************************

local function setvbuf()
  return false,"Not implemented",-1
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
    return false,"stream closed",-3
  end
  
  for i = 1 , select('#',...) do
    local data = select(i,...)
    if type(data) ~= 'string' and type(data) ~= 'number' then
      error(string.format("bad argument #%d to 'write' (string expected, got %s)",i,type(data)))
    end
    
    data = tostring(data)
    
    local okay,err,ev = ios:_drain(data)
    if not okay then
      return okay,err,ev
    end
    
    ios._wbytes = ios._wbytes + #data
  end
  return true
end

-- *******************************************************************

return function()
  return {
    close   = close,   -- override
    flush   = flush,   -- override
    lines   = lines,
    read    = read,
    seek    = seek,    -- override
    setvbuf = setvbuf, -- override
    write   = write,
    
    _readbuf = "",
    _rpos    = 1,
    _eof     = false,
    _ridx    = 1,
    _wbytes  = 0,
    _rbytes  = 0,
    _refill  = function() error("failed to provide ios._refill()") end,
    _drain   = function() error("failed to provide ios._drain()")  end,
  }
end
