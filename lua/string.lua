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
-- luacheck: globals wrapt metaphone compare comparen mksplit split
-- luacheck: globals template filetemplate wrap safeascii safeutf8
-- luacheck: globals comparei
-- luacheck: ignore 611

local strcore  = require "org.conman.strcore"
local lpeg     = require "lpeg"
local string   = require "string"
local table    = require "table"
local io       = require "io"

local _VERSION = _VERSION
local type     = type
local tostring = tostring

if _VERSION == "Lua 5.1" then
  module("org.conman.string")
else
  _ENV = {}
end

metaphone = strcore.metaphone
compare   = strcore.compare
comparen  = strcore.comparen
comparei  = strcore.comparei
wrapt     = strcore.wrapt
safeascii = strcore.safeascii
safeutf8  = strcore.safeutf8

-- ********************************************************************

function mksplit(delim)
  delim      = lpeg.P(delim or ':')
  local char = lpeg.C((lpeg.P(1) - delim)^0)
  
  return lpeg.Ct(char * (delim * char)^0)
end

-- ********************************************************************

function split(s,delim)
  local sp = mksplit(delim)
  return sp:match(s)
end

-- ********************************************************************

function template(temp,callbacks,data)
  local function cmd(tag)
    local word = string.sub(tag,3,-3)
    
    if type(callbacks[word]) == 'string' then
      return callbacks[word]
    elseif type(callbacks[word]) == 'function' then
      return callbacks[word](data)
    else
      return tostring(callbacks[word])
    end
  end
  
  local s = string.gsub(temp,"%%{[%w%.]+}%%",cmd)
  return s
end

-- ********************************************************************

function filetemplate(temp,callbacks,data)
  local f = io.open(temp,"r")
  local d = f:read("*a")
  f:close()
  return template(d,callbacks,data)
end

-- ********************************************************************

function wrap(s,margin,lead)
  lead      = lead or ""
  local res = wrapt(s,margin)
  
  -- -----------------------------------------------------------------------
  -- insert lead into the first position.  We then convert the table into a
  -- string, separated by a newline and the lead.  This conforms to the
  -- behavior of the C based version of this function.
  -- -----------------------------------------------------------------------
  
  table.insert(res,1,lead)
  return table.concat(res,"\n" .. lead)
end

-- ********************************************************************

if _VERSION >= "Lua 5.2" then
  return _ENV
end
