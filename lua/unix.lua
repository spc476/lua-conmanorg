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
-- luacheck: globals users groups paths
-- luacheck: ignore 611

local _VERSION     = _VERSION
local setmetatable = setmetatable
local tonumber     = tonumber

local fsys = require "org.conman.fsys"
local lpeg = require "lpeg"
local io   = require "io"
local os   = require "os"

if _VERSION == "Lua 5.1" then
  module("org.conman.unix")
else
  _ENV = {}
end

local text   = lpeg.C((lpeg.R" ~" - lpeg.P":")^0)
local number = lpeg.R"09"^1 / tonumber

-- ************************************************************************

users = {} do
  local entry = lpeg.Ct(
                           lpeg.Cg(text,  "userid") * lpeg.P":"
                         * lpeg.Cg(text,  "passwd") * lpeg.P":"
                         * lpeg.Cg(number,"uid")    * lpeg.P":"
                         * lpeg.Cg(number,"gid")    * lpeg.P":"
                         * lpeg.Cg(text,  "name")   * lpeg.P":"
                         * lpeg.Cg(text,  "home")   * lpeg.P":"
                         * lpeg.Cg(text,  "shell")
                       )
  
  for line in io.lines("/etc/passwd") do
    local user = entry:match(line)
    if user then
      users[user.userid] = user
      users[user.uid]    = user
    end
  end
end

-- *************************************************************************

groups = {} do
  local name  = lpeg.C((lpeg.R" ~" - lpeg.S":,")^1)
  local users = lpeg.Ct(name * (lpeg.P"," * name)^0)
              + lpeg.P(true) / function() return {} end
  local entry = lpeg.Ct(
                           lpeg.Cg(text,  "name")   * lpeg.P":"
                         * lpeg.Cg(text,  "passwd") * lpeg.P":"
                         * lpeg.Cg(number,"gid")    * lpeg.P":"
                         * lpeg.Cg(users, "users")
                       )
  
  for line in io.lines("/etc/group") do
    local group = entry:match(line)
    if group then
      groups[group.name] = group
      groups[group.gid]  = group
    end
  end
end

-- ************************************************************************

paths = setmetatable({},{ __index = function(t,k)
  for path in os.getenv("PATH"):gmatch "[^:]+" do
    if fsys.access(path .. "/" .. k,"X") then
      t[k] = path .. '/' .. k
      return t[k]
    end
  end
end})

if _VERSION >= "Lua 5.2" then
  return _ENV
end
