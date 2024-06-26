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
-- luacheck: globals safestring dump_value dump keepset
-- luacheck: ignore 611

local _VERSION     = _VERSION
local math         = require "math"
local string       = require "string"
local io           = require "io"
local ipairs       = ipairs
local pairs        = pairs
local tostring     = tostring
local type         = type
local pcall        = pcall
local getmetatable = getmetatable

if _VERSION == "Lua 5.1" then
  module("org.conman.table")
else
  _ENV = {}
end

-- *******************************************************************

local char_trans =
{
  ['\a'] = '\\a',
  ['\b'] = '\\b',
  ['\t'] = '\\t',
  ['\n'] = '\\n',
  ['\v'] = '\\v',
  ['\f'] = '\\f',
  ['\r'] = '\\r',
  ['"']  = '\\"',
  ['\\'] = '\\\\',
}

function safestring(v)
  if type(v) == 'string' then
    return '"' .. v:gsub(".",function(c)
      if char_trans[c] then
        return char_trans[c]
      end
      
      local b = c:byte()
      
      if b < 32 or b > 126 then
        return string.format("\\%03d",b)
      else
        return c
      end
    end) .. '"'
  else
    return tostring(v)
  end
end

-- *************************************************************

function dump_value(name,value,path,level,marked)
  path       = path   or ""
  level      = level  or 0
  marked     = marked or {}
  local lead = string.rep(" ",level)
  
  if type(name) == "nil" then
    return ""
  elseif type(name) == "number" then
    if _VERSION >= "Lua 5.3" then
      if math.type(name) == 'integer' then
        name = string.format("[%d]",name)
      else
        name = string.format("[%f]",name)
      end
    else
      name = string.format("[%f]",name)
    end
  elseif type(name) == 'string' then
    if not name:match "^[A-Za-z_][A-Za-z0-9_]*$" then
      name = "[" .. safestring(name) .. "]"
    end
  else
    name = "[" .. safestring(name) .. "]"
  end
  
  if type(value) == "nil" then
    return ""
  elseif type(value) == "boolean" then
    return string.format("%s%s = %s,\n",lead,name,tostring(value))
  elseif type(value) == "number" then
    return string.format("%s%s = %f,\n",lead,name,value)
  elseif type(value) == "string" then
    value = safestring(value)
    return string.format("%s%s = %s,\n",lead,name,value)
  elseif type(value) == "table" then
  
    if marked[tostring(value)] ~= nil then
      return string.format("%s%s = %s,\n",lead,name,marked[tostring(value)])
    else
      if path == "" then
        marked[tostring(value)] = name
      else
        marked[tostring(value)] = path .. "." .. name
      end
    end
    
    local s = string.format("%s%s =\n%s{\n",lead,name,lead)
    
    for k,v in pairs(value) do
      s = s .. dump_value(k,v,marked[tostring(value)],level + 2,marked)
    end
    
    s = s .. string.format("%s}",lead)
    if level > 0 then s = s .. string.format(",") end
    
    if getmetatable(value) ~= nil then
      s = s .. string.format("--METATABLE\n",lead)
    else
      s = s .. string.format("\n",lead)
    end
    return s
    
  elseif type(value) == "function" then
    local err,func = pcall(string.dump,value)
    if err == false then
      return string.format("%s%s = C_FUNCTION\n",lead,name)
    else
      return string.format("%s%s = loadstring(%q),\n",lead,name,func)
    end
  elseif type(value) == "thread" then
    return string.format("%s%s = THREAD\n",lead,name)
  elseif type(value) == "userdata" then
    local mt = getmetatable(value)
    
    if mt and (mt.__pairs or mt.__ipairs) then
      local s = string.format("%s%s =\n%s{\n",lead,name,lead)
      
      if mt.__ipairs then
        for i,v in ipairs(value) do
          s = s .. dump_value(i,v,marked[tostring(value)],level + 2,marked)
        end
      end
      
      if mt.__pairs then
        for k,v in pairs(value) do
          s = s .. dump_value(k,v,marked[tostring(value)],level + 2,marked)
        end
      end
      
      return s .. string.format("%s} --USERDATA\n",lead)
    else
      return string.format("%s%s = %s\n",lead,name,tostring(value))
    end
  else
    error("unsupported data type!")
  end
end

-- **********************************************************

function dump(name,value)
  io.stdout:write(dump_value(name,value),'\n')
end

-- ***********************************************************

function keepset(t,name,value)
  if t[name] == nil then
    t[name] = value
  elseif type(t[name]) == 'table' then
    t[name][#t[name] + 1] = value
  else
    t[name] = { t[name] , value }
  end
  
  return t
end

-- ************************************************************

if _VERSION >= "Lua 5.2" then
  return _ENV
end

