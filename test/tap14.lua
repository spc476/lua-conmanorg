-- *************************************************************************
--
-- Copyright 2022 by Sean Conner.  All Rights Reserved.
--
-- This program is free software: you can redistribute it and/or modify it
-- under the terms of the GNU General Public License as published by the
-- Free Software Foundation, either version 3 of the License, or (at your
-- option) any later version.
--
-- This program is distributed in the hope that it will be useful, but
-- WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
-- Public License for more details.
--
-- You should have received a copy of the GNU General Public License along
-- with this program.  If not, see <http://www.gnu.org/licenses/>.
--
-- Comments, questions and criticisms can be sent to: sean@conman.org
--
-- *************************************************************************
-- luacheck: ignore 611
-- luacheck: globals plan comment assert assertB done

local io     = require "io"
local string = require "string"
local os     = require "os"
local table  = require "table"

local _VERSION   = _VERSION

if _VERSION == "Lua 5.1" then
  module("tap14")
else
  _ENV = {}
end

local mtaps = {}

-- *************************************************************************

local function ok(msg,...)
  local tap = mtaps[#mtaps]
  io.stdout:write(tap.pad,"ok ",tap.id)
  if msg then
    io.stdout:write(" - ",string.format(msg,...))
  end
  io.stdout:write("\n")
  tap.id = tap.id + 1
end

-- *************************************************************************

local function notok(msg,...)
  local tap = mtaps[#mtaps]
  io.stdout:write(tap.pad,"not ok ",tap.id)
  if msg then
    io.stdout:write(" - ",string.format(msg,...))
  end
  io.stdout:write("\n")
  tap.pass = false
  tap.id   = tap.id + 1
end

-- *************************************************************************

local function bail(msg,...)
  io.stdout:write("Bail out!")
  if msg then
    io.stdout:write(" ",string.format(msg,...))
  end
  io.stdout:write("\n")
  os.exit(1,true)
end

-- *************************************************************************

function plan(plan,fmt,...)
  local tap =
  {
    name = (fmt and string.format(fmt,...))                or "",
    pad  = (mtaps[#mtaps] and mtaps[#mtaps].pad .. "    ") or "",
    plan = plan or 0,
    id   = 1,
    pass = true,
  }
  
  if tap.name and tap.name ~= "" then
    io.stdout:write(mtaps[#mtaps].pad,"# Subtest: ",tap.name,"\n")
  end
  
  if tap.plan ~= 0 then
    io.stdout:write(tap.pad,"1..",tap.plan,"\n")
  end
  
  table.insert(mtaps,tap)
end

-- *************************************************************************

function comment(fmt,...)
  io.stdout:write(mtaps[#mtaps].pad,"# ",string.format(fmt,...),"\n")
end

-- *************************************************************************

function assert(cond,...) -- luacheck: ignore
  if cond then
    ok(...)
  else
    notok(...)
  end
  return cond
end

-- *************************************************************************

function assertB(cond,...)
  if cond then
    ok(...)
  else
    bail(...)
  end
  return cond
end

-- *************************************************************************

function done()
  local tap = table.remove(mtaps)
  
  if tap.plan == 0 then
    io.stdout:write(tap.pad,"1..",tap.id - 1,"\n")
  end
  
  if #mtaps > 0 then
    if tap.pass then
      if tap.name ~= "" then
        ok(tap.name)
      else
        ok()
      end
    else
      if tap.name ~= "" then
        notok(tap.name)
      else
        notok()
      end
    end
  end
  return tap.pass and 0 or 1
end

-- *************************************************************************

io.stdout:write("TAP version 14\n")

if _VERSION >= "Lua 5.2" then
  return _ENV
end
