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

local _VERSION = _VERSION
local require  = require
local type     = type

local string = require "string"
local table  = require "table"
local io     = require "io"

if _VERSION == "Lua 5.1" then
  module("org.conman.string")
  require "org.conman.strcore"
else
  _ENV = {}
  local x   = require "org.conman.strcore"
  wrapt     = x.wrapt
  metaphone = x.metaphone
  soundex   = x.soundex
end

function split(s,delim)
  local results = {}
  local delim   = delim or "%:"
  local pattern = "([^" .. delim .. "]*)" .. delim .. "?"
  
  for segment in string.gmatch(s,pattern) do
    table.insert(results,segment)
  end
  
  return results
end

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

function filetemplate(temp,callbacks,data)
  local f = io.open(temp,"r")
  local d = f:read("*a")
  f:close()
  return template(d,callbacks,data)
end

function wrap(s,margin,lead)
  local lead = lead or ""
  local res  = wrapt(s,margin)
  
  -- -----------------------------------------------------------------------
  -- insert lead into the first position.  We then convert the table into a
  -- string, separated by a newline and the lead.  This conforms to the
  -- behavior of the C based version of this function.
  -- -----------------------------------------------------------------------
  
  table.insert(res,1,lead)
  return table.concat(res,"\n" .. lead)
end

if _VERSION >= "Lua 5.2" then
  return _ENV
end
