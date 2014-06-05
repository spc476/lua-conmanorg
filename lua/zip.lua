-- ***************************************************************
--
-- Copyright 2014 by Sean Conner.  All Rights Reserved.
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
-- To effectively use this module, you should understand the ZIP file
-- format.  The definitive guide to this format is at
--
--		http://www.pkware.com/appnote
--
-- ********************************************************************

local _VERSION     = _VERSION
local pairs        = pairs
local setmetatable = setmetatable

local table = require "table"
local math  = require "math"

if _VERSION == "Lua 5.1" then
  module("org.conman.zip")
else
  _ENV = {}
end

-- ************************************************************************

magic = 
{
  EOCD      = "PK\005\006",
  DIR       = "PK\001\002",
  FILE      = "PK\003\004",
  DATA      = "PK\007\008",
  ARCHIVE   = "PK\006\008",
  SIGNATURE = "PK\005\005",
  EOCD64    = "PK\006\006",
  EOCDL64   = "PK\006\007",
  
}

local function reverse_index(tab)
  local keys = {}
  for name in pairs(tab) do
    table.insert(keys,name)
  end
  
  for i = 1 , #keys do
    local k = keys[i]
    local v = tab[k]
    tab[v] = k
  end
  
  return setmetatable(tab,{
    __index = function(t,key)
      if type(key) == 'number' then
        return '-'
      elseif type(key) == 'string' then
        return 0
      end
    end
  })
end

os = reverse_index {
  ["MS-DOS"]          =  0,
  ["Amiga"]           =  1,
  ["OpenVMS"]         =  2,
  ["UNIX"]            =  3,
  ["VM/CMS"]          =  4,
  ["Atari ST"]        =  5,
  ["OS/2"]            =  6,
  ["Macintosh"]       =  7,
  ["Z-System"]        =  8,
  ["CP/M"]            =  9,
  ["Windows"]         = 10,
  ["MVS"]             = 11,
  ["VSE"]             = 12,
  ["Acorn Risc"]      = 13,
  ["VFAT"]            = 14,
  ["alternative MVS"] = 15,
  ["BeOS"]            = 16,
  ["Tandem"]          = 17,
  ["OS/400"]          = 18,
  ["Darwin"]          = 19,
}

-- ************************************************************************

function idiv(up,down)
  local q = math.floor(up/down)
  local r = up % down
  return q,r
end

-- ************************************************************************

if _VERSION >= "Lua 5.2" then
  return _ENV
end

