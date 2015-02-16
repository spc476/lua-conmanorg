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

local _VERSION     = _VERSION
local pairs        = pairs
local math         = math
local tostring     = tostring
local string       = string
local type         = type
local io           = io
local print        = print

local type         = type
local pcall        = pcall
local getmetatable = getmetatable

if _VERSION == "Lua 5.1" then
  module("org.conman.table")
else
  _ENV = {}
end

-- *******************************************************************

function show(l,fout)
  local fout = fout or io.stdout
  local maxkeylen = 0
  local maxvallen = 0
  
  if type(l) ~= 'table' then
    return
  end
  
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
    fout:write(string.format(format,tostring(k),vt),"\n")
  end
end

-- *************************************************************

function dump_value(name,value,path,level,marked)

  local function conv_cntl(s)
    if s == "\n" then
      return "\\n"
    else
      return "\\?"
    end
  end

  local path   = path   or ""
  local level  = level  or 0
  local marked = marked or {}
  local lead   = string.rep(" ",level)
  
  if type(name) == "nil" then
    return ""
  elseif type(name) == "number" then
    name = string.format("[%d]",name)
  end
  
  if type(value) == "nil" then 
    return "" 
  elseif type(value) == "boolean" then
    return string.format("%s%s = %s,\n",lead,name,tostring(value))
  elseif type(value) == "number" then
    return string.format("%s%s = %f,\n",lead,name,value)
  elseif type(value) == "string" then
    value = string.gsub(value,"%c",conv_cntl)
    return string.format("%s%s = %q,\n",lead,name,value)
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
      return ""
    else
      return string.format("%s%s = loadstring(%q),\n",lead,name,func)
    end    
  elseif type(value) == "thread" then
    return string.format("%s%s = THREAD\n",lead,name)
  elseif type(value) == "userdata" then
    return string.format("%s%s = %s\n",lead,name,tostring(value))
  else
    error("unsupported data type!")
  end
end

-- **********************************************************

function dump(name,value)
  print(dump_value(name,value))
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

