-- *******************************************************************
--
-- Copyright 2019 by Sean Conner.  All Rights Reserved.
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
-- *******************************************************************
-- luacheck: globals accept listena listen connecta connect
-- luacheck: ignore 611

local syslog = require "org.conman.syslog"
local net    = require "org.conman.net"
local errno  = require "org.conman.errno"
local ios    = require "org.conman.net.ios"

local _VERSION     = _VERSION
local setmetatable = setmetatable
local ipairs       = ipairs

if _VERSION == "Lua 5.1" then
  module("org.conman.net.tcp")
else
  _ENV = {}
end

-- *******************************************************************

local function make_ios(conn,remote)
  local state = ios()
  
  state.close = function(self)
    self:flush()
    self.__sock:close()
    return true
  end
  
  state._refill  = function(self)
    local _,data = self.__sock:recv()
    if data and #data > 0 then
      return data
    else
      return nil
    end
  end
  
  state._drain = function(self,buffer)
    local bytes,err = self.__sock:send(nil,buffer)
    return bytes >= 0,errno[err],err
  end
  
  state.__remote = remote
  state.__sock   = conn
  
  if _VERSION >= "Lua 5.2" then
    local mt = {}
    mt.__gc = state.close
    if _VERSION >= "Lua 5.4" then
      mt.__close = state.close
    end
    setmetatable(state,mt)
  end
  
  return state
end

-- *******************************************************************
-- Usage:	ios = tcp.accept(sock)
-- Desc:	Return a TCP connection
-- Input:	sock (userdata/socket) a listening socket
-- Return:	ios (table) an Input/Output object (nil on error)
-- *******************************************************************

function accept(sock)
  local conn,remote = sock:accept()
  if conn then
    sock.nodelay = true
    return make_ios(conn,remote)
  end
end

-- *******************************************************************
-- Usage:	sock,errmsg = tcp.listena(addr)
-- Desc:	Create a listening socket
-- Input:	addr (userdata/address) IP address and port
-- Return:	sock (userdata/socket) listening socket
--		errmsg (string) error message
-- *******************************************************************

function listena(addr)
  local sock,err = net.socket(addr.family,'tcp')
  if not sock then
    return false,errno[err]
  end
  
  sock.reuseaddr = true
  sock:bind()
  sock:listen()
  
  return sock
end

-- *******************************************************************
-- Usage:	ios = tcp.listen(host,port)
-- Desc:	Create a listening socket
-- Input:	host (string) hostname of interface to listen on
--		port (string number) port number
-- Return:	ios (table) Input/Output object (nil on error)
-- *******************************************************************

function listen(host,port)
  return listena(net.address2(host,'any','tcp',port)[1])
end

-- *******************************************************************
-- Usage:	ios = tcp.connecta(addr)
-- Desc:	Connect to a remote host
-- Input:	addr (userdata/address) IP address and port
-- Return:	ios (table) Input/Output object (nil on error)
-- *******************************************************************

function connecta(addr)
  if not addr then return end
  
  local sock,err = net.socket(addr.family,'tcp')
  
  if not sock then
    syslog('error',"socket(TCP) = %s",errno[err])
    return
  end
  
  local err1 = sock:connect(addr)

  if err1 ~= 0 then
    return
  end
  
  return make_ios(sock,addr)
end

-- *******************************************************************
-- Usage:	ios = tcp.connect(host,port)
-- Desc:	Connect to a remote host
-- Input:	host (string) hostname
--		port (string number) port to connect to
-- Return:	ios (table) Input/Output object (nil on error)
-- *******************************************************************
  
function connect(host,port)
  local addr = net.address2(host,'any','tcp',port)
  if addr then
    for _,a in ipairs(addr) do
      local conn = connecta(a)
      if conn then
        return conn
      end
    end
  end
end

-- *******************************************************************

if _VERSION >= "Lua 5.2" then
  return _ENV
end
