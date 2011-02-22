
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
  local f     = io.open("/etc/passwd","r")
  
  for line in f:lines() do
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
  
  f:close()
  return users
end

-- *************************************************************************

local function etcgroup()
  local NAME = 1
  local PASS = 2
  local GID  = 3
  local USERS = 4
  
  local groups = {}
  local f      = io.open("/etc/group","r")
  
  for line in f:lines() do
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
  
  f:close()
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

_DESCRIPTION = "The list of users, groups and programs on this Unix system"
_COPYRIGHT   = "Copyright 2010 by Sean Conner.  All Rights Reserved."
_VERSION     = "1.0"
users        = etcpasswd()
groups       = etcgroup()
paths        = setmetatable({} , { __index = findexec })
