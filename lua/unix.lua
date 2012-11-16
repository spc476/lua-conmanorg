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

local str  = require "org.conman.string"
local fsys = require "org.conman.fsys"
local env  = require "org.conman.env"

module("org.conman.unix",package.seeall)

-- ************************************************************************

local function etcpasswd()
  local USERID = 1
  local PASS   = 2
  local UID    = 3
  local GID    = 4
  local NAME   = 5
  local HOME   = 6
  local SHELL  = 7
  
  local users = {}

  for line in io.lines('/etc/passwd') do
    local fields = str.split(line)
    local user   = {}

    user.userid = fields[USERID]
    user.uid    = tonumber(fields[UID])
    user.gid    = tonumber(fields[GID])
    user.name   = fields[NAME]
    user.home   = fields[HOME]
    user.shell  = fields[SHELL]
    
    users[user.userid] = user
    users[user.uid]    = user
  end
  
  return users
end

-- *************************************************************************

local function etcgroup()
  local NAME = 1
  local PASS = 2
  local GID  = 3
  local USERS = 4
  
  local groups = {}

  for line in io.lines('/etc/group') do
    local fields = str.split(line)
    local group  = {}
    
    group.name = fields[NAME]
    group.gid  = tonumber(fields[GID])
    
    if fields[USERS] then
      group.users = str.split(fields[USERS],"%,")
    else
      group.users = {}
    end
   
    groups[group.name] = group
    groups[group.gid]  = group
  end
  
  return groups
end

-- *************************************************************************

local function findexec(t,k)
  for _,path in ipairs(str.split(env.PATH)) do
    if fsys.access(path .. "/" .. k,"X") then
      t[k] = path .. "/" .. k
      return t[k]
    end
  end
  return nil
end

-- ************************************************************************

users        = etcpasswd()
groups       = etcgroup()
paths        = setmetatable({} , { __index = findexec })
