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
-- --------------------------------------------------------------------
--
-- Some standards for options.  First, long options:
--	http://www.gnu.org/prep/standards/standards.html#Option-Table
--
-- And now for short options:
--	http://www.catb.org/~esr/writings/taoup/html/ch10s05.html
-- ********************************************************************

local _VERSION = _VERSION
local type     = type

if _VERSION == "Lua 5.1" then
  module("org.conman.getopt")
else
  _ENV = {}
end

local SHORT_OPT = 1
local LONG_OPT  = 2
local ARGUMENT  = 3
local CALLBACK  = 4

-- ************************************************************************

local function long_match(lnopt,arg,i,err)
  local tag
  local value
  
  tag,value = arg[i]:match("^%-%-([^%=]+)%=?(.*)")
  
  if lnopt == nil then 
    err(tag)
    return i + 1
  end

  if lnopt[ARGUMENT] then
    if value == "" then
      i = i + 1
      value = arg[i]
    end
  else
    value = nil
  end

  lnopt[CALLBACK](value)

  return i + 1
end

-- ***********************************************************************

local function short_match(shopt,arg,i,err)
  for n = 2 , #arg[i] do
    local opt = arg[i]:sub(n,n)
    
    if shopt[opt] == nil then
      err(opt)
    elseif not shopt[opt][ARGUMENT] then
      shopt[opt][CALLBACK]()
    else
      if n == #arg[i] then
        i = i + 1
        value = arg[i]
      else
        if arg[i]:sub(n+1,n+1) == '=' then
          value = arg[i]:sub(n+2)
        else
          value = arg[i]:sub(n+1)
        end
      end
      
      shopt[opt][CALLBACK](value)
      return i + 1
    end
  end
  return i + 1
end 

-- ***********************************************************************

function getopt(arg,options,err)
  local shopt     = {}
  local lnopt     = {}
  local err       = err or function(opt)
                             io.stderr:write("unsupported option: ",opt,"\n")
                           end
  
  for i = 1 , #options do
    shopt[options[i][SHORT_OPT]] = options[i]
    if options[i][LONG_OPT] ~= nil then
      if type(options[i][LONG_OPT]) == 'string' then
        lnopt[options[i][LONG_OPT]] = options[i]
      elseif type(options[i][LONG_OPT]) == 'table' then
        for j = 1 , #options[i][LONG_OPT] do
          lnopt[options[i][LONG_OPT][j]] = options[i]
        end
      end
    end
  end
  
  local i = 1
  
  while(i <= #arg) do
    local al
    local as
    
    al = arg[i]:match("^%-%-([^%=]+)")
    as = arg[i]:match("^%-([^%-])")
    
    if al ~= nil then
      i = long_match(lnopt[al],arg,i,err)
    elseif as ~= nil then
      i = short_match(shopt,arg,i,err)
    else
      return i
    end
  end
  
  return i
end

-- ***********************************************************************

if _VERSION >= "Lua 5.2" then
  return _ENV
end
