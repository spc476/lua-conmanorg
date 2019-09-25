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
-- luacheck: globals accept listens listena listen connecta connect
-- luacheck: ignore 611

local errno  = require "org.conman.errno"
local net    = require "org.conman.net"
local tls    = require "org.conman.tls"
local ios    = require "org.conman.net.ios"

local _VERSION = _VERSION

if _VERSION == "Lua 5.1" then
  module(...)
else
  _ENV = {} -- luacheck: ignore
end

-- *******************************************************************

local function make_ios(ctx)
  local state = ios()

  function state.close()
    return ctx:close()
  end
  
  function state._handshake()
    return ctx:handshake() == 0
  end
  
  function state._refill()
    return ctx:read(tls.BUFFERSIZE)
  end
  
  function state._drain(data)
    return ctx:write(data)
  end
  
  return state
end

-- *******************************************************************
-- Usage:       ios = accept(sock)
-- Desc:        Return a TCP connection
-- Input:       sock (userdata/socket) a listening socket
-- Return:      ios (table) an Input/Output object (nil on error)
-- *******************************************************************

function accept(sock)
  local conn,remote = sock:accept()
  if conn then
    return make_ios(conn,remote)
  end
end

-- *******************************************************************
-- Usage:       sock,errmsg = listens(sock,conf)
-- Desc:        Initialize a listening TCP socket
-- Input:       sock (userdata/socket) bound socket
--              conf (function) function for TLS configuration
-- Return:      sock (userdata) socket used for listening, false on error
--              errmsg (string) error message
-- *******************************************************************

function listens(sock,conf)
  local config = tls.config()
  local server = tls.server()
  
  if not conf(config) then return false,config:error() end
  if not server:configuration(config) then
    return false,server:error()
  end
  
  -- XXX finish ...
  
  return sock
end

-- *******************************************************************
-- Usage:       sock,errmsg = listena(addr,conf)
-- Desc:        Initialize a listening TCP socket
-- Input:       addr (userdata/address) IP address
--              conf (function) function for TLS configuration
-- Return:      sock (userdata) socket used for listening, false on error
--              errmsg (string) error message
-- *******************************************************************

function listena(addr,conf)
  local sock,err = net.socket(addr.family,'tcp')
  if not sock then
    return false,errno[err]
  end
  
  sock.reuseaddr = true
  sock:bind(addr)
  sock:listen()
  return listens(sock,conf)
end

-- *******************************************************************
-- Usage:       sock,errmsg = listen(host,port,conf)
-- Desc:        Initalize a listening TCP socket
-- Input:       host (string) address to bind to
--              port (string integer) port
--              conf (function) configuration options
-- Return:      sock (userdata) socket used for listening, false on error
--              errmsg (string) error message
-- *******************************************************************

function listen(host,port,conf)
  return listena(net.address2(host,'ip','tcp',port)[1],conf)
end

-- *******************************************************************
-- Usage:       ios = connecta(addr,hostname[,conf])
-- Desc:        Connect to a remote address
-- Input:       addr (userdata/address) IP address
--              hostname (string) hostname (required for TLS)
--              conf (function) configuration options
-- Return:      ios (table) Input/Output object (nil on error)
-- *******************************************************************

function connecta(addr,hostname,conf)
  local sock,err = net.socket(addr.family,'tcp')

  if not sock then
    return false,errno[err]
  end
  
  err = sock:connect(addr)

  if err ~= 0 then
    sock:close()
    return false,errno[err]
  end
  
  local config = tls.config()
  local ctx    = tls.client()
  
  if conf then
    if not conf(config) then return false,config:error() end
  else
    config:protocols("all")
  end
  
  ctx:configure(config)
  
  if not ctx:connect_socket(sock:_tofd(),hostname) then
    sock:close()
    return false,ctx:error()
  end
  
  return make_ios(ctx)
end

-- *******************************************************************
-- Usage:       ios = connect(host,port[,conf])
-- Desc:        Connect to a remote host
-- Input:       host (string) IP address
--              port (string number) port to connect to
--              conf (function) configuration options
-- Return:      ios (table) Input/Output object (nil on error)
-- *******************************************************************

function connect(host,port,conf)
  local addr = net.address2(host,'any','tcp',port)
  if addr then
    return connecta(addr[1],host,conf)
  end
end

-- *******************************************************************

if _VERSION >= "Lua 5.2" then
  return _ENV -- luacheck: ignore
end
