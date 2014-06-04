-- ***************************************************************
--
-- Copyright 2012 by Sean Conner.  All Rights Reserved.
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
local floor    = math.floor
local os       = os

if _VERSION == "Lua 5.1" then
  module("org.conman.date")
else
  _ENV = {}
end

-- *********************************************************************

function tojulianday(date)
  local date = date or os.date("*t")
  
  local a = floor((14 - date.month) / 12)
  local y = date.year + 4800 - a
  local m = date.month + 12 * a - 3
  
  return date.day
       + floor((153 * m + 2) / 5)
       + y * 365
       + floor(y / 4)
       - floor(y / 100)
       + floor(y / 400)
       - 32045
end

-- **********************************************************************

function fromjulianday(jd)
  local a = jd + 32044
  local b = floor((4 * a + 3) / 146097)
  local c = a - floor((b * 146097) / 4)
  
  local d = floor((4 * c + 3) / 1461)
  local e = c - floor((1461 * d) / 4)
  local m = floor((5 * e + 2) / 153)
  
  return {
    day   = e - floor((153 * m + 2) / 5) + 1,
    month = m + 3 - 12 * floor((m / 10)),
    year  = b * 100 + d - 4800 + floor(m / 10)
  }

end

-- ************************************************************************

function dayofweek(date)
  local a = floor((14 - date.month) / 12)
  local y = date.year - a
  local m = date.month + 12 * a - 2

  local d = date.day
          + y
          + floor(y / 4)
          - floor(y / 100)
          + floor(y / 400)
          + floor(31 * m / 12)
  return (d % 7) + 1
end

-- ************************************************************************

if _VERSION >= "Lua 5.2" then
  return _ENV
end

