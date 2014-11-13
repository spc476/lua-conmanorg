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

local setmetatable = setmetatable
local tonumber     = tonumber

local fsys = require "org.conman.fsys"
local io   = require "io"
local os   = require "os"

module("org.conman.unix")

-- ************************************************************************

local function etcpasswd()
  local users = {}

  for line in io.lines('/etc/passwd') do
    if not line:match "^#" then
      local next  = line:gmatch "([^:]*):?" 
      local user  = {}
      
      user.userid = next()
      user.passwd = next()
      user.uid    = tonumber(next())
      user.gid    = tonumber(next())
      user.name   = next()
      user.home   = next()
      user.shell  = next()
      
      users[user.userid] = user
      users[user.uid]    = user
    end
  end
  
  return users
end

-- *************************************************************************

local function etcgroup()
  local groups = {}

  for line in io.lines('/etc/group') do
    if not line:match "^#" then
      local next   = line:gmatch "([^:]*):?"
      local group  = {}
      
      group.name   = next()
      group.passwd = next()
      group.gid    = tonumber(next())
      group.users  = {}
      
      local list = next()
      if list then
        for user in list:gmatch("([^,]*),?") do
          group.users[#group.users + 1] = user
        end
      end
      
      groups[group.name] = group
      groups[group.gid]  = group
    end
  end
    
  return groups
end

-- *************************************************************************

local function findexec(t,k)
  local paths = os.getenv "PATH"
  for path in paths:gmatch("([^:]*):?") do
    if fsys.access(path .. "/" .. k,"X") then
      t[k] = path .. "/" .. k
      return t[k]
    end
  end
end

-- ************************************************************************

users  = etcpasswd()
groups = etcgroup()
paths  = setmetatable({} , { __index = findexec })
