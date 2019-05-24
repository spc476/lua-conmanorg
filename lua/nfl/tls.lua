-- ***************************************************************
--
-- Copyright 2018 by Sean Conner.  All Rights Reserved.
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
-- luacheck: globals listena listen connecta connect
-- luacheck: ignore 611
--
-- We require org.conman.tls.LIBRESSL_VERSION >= 0x2050000f

local syslog    = require "org.conman.syslog"
local errno     = require "org.conman.errno"
local mkios     = require "org.conman.net.ios"
local net       = require "org.conman.net"
local tls       = require "org.conman.tls"
local nfl       = require "org.conman.nfl"
local coroutine = require "coroutine"

local _VERSION  = _VERSION
local assert    = assert

assert(tls.LIBRESSL_VERSION >= 0x2050000f,"too old a version of TLS")

if _VERSION == "Lua 5.1" then
  module(...)
else
  _ENV = {} -- luacheck: ignore
end

-- **********************************************************************

local function create_handler(conn,remote)
  local ios      = mkios()
  ios.__writebuf = ""
  ios.__rawbuf   = ""
  ios.__sock     = conn
  ios.__remote   = remote
  
  ios._refill = function(self)
    local str,len = self.__ctx:read(tls.BUFFERSIZE)
    if len == tls.ERROR then
      return nil
    elseif len == tls.WANT_INPUT or len == tls.WANT_OUTPUT then
      ios.__resume = true
      coroutine.yield()
      return self:_refill()
    else
      return str
    end
  end
  
  ios._drain = function(self,data)
    local bytes = self.__ctx:write(data)
    
    if bytes == tls.ERROR then
      return false
      
    elseif bytes == tls.WANT_INPUT or bytes == tls.WANT_OUTPUT then
      self.__resume = true
      coroutine.yield()
      return self:_drain(data)
    end
    return true
  end
  
  ios.close = function()
    nfl.SOCKETS:remove(conn)
    local err = conn:close()
    return err == 0,errno[err],err
  end
  
  ios.seek = function()
    return nil,errno[errno.ESPIPE],errno.ESPIPE
  end
  
  return ios,function(event)
    if event.read then
      local _,packet,err = conn:recv()
      if packet then
        if #packet == 0 then
          nfl.SOCKETS:remove(conn)
          ios._eof = true
        end
        
        ios.__rawbuf = ios.__rawbuf .. packet
        if ios.__resume then
          ios.__resume = false
          nfl.schedule(ios.__co,true)
        end
      else
        if err ~= errno.EAGAIN then
          syslog('error',"TLS.socket:recv() = %s",errno[err])
        end
      end
    end
    
    if event.write then
      nfl.SOCKETS:update(conn,"r")
      nfl.schedule(ios.__co,true)
    end
  end
end

-- **********************************************************************
--
-- Callbacks for accept_cbs() and connect_cbs().
--
-- **********************************************************************

local function tlscb_read(_,len,ios)
  if #ios.__rawbuf == 0 then
    if ios._eof then return "",0 end
    return nil,tls.WANT_INPUT
  end
  
  local ret    = ios.__rawbuf:sub(1,len)
  ios.__rawbuf = ios.__rawbuf:sub(len + 1,-1)
  return ret,0
end

-- **********************************************************************

local function tlscb_write(_,str,ios)
  local bytes = ios.__sock:send(nil,str)
  if not bytes then bytes = tls.ERROR end
  return bytes
end

-- **********************************************************************
-- Usage:       okay,errmsg = listena(addr,hostname,mainf)
-- Desc:        Initialize a listening TCP socket
-- Input:       addr (userdata/address) IP address
--              hostname (string) hostname
--              mainf (function) main handler for service
-- Return:      okay (boolean) true if okay, false if error
--              errmsg (strong) error message from TLS
-- **********************************************************************

function listena(addr,mainf,conf)
  local config = tls.config()
  local server = tls.server()
  
  if not conf(config) then return false,config:error() end
  if not server:configure(config) then
    return false,server:error()
  end
  
  local sock = net.socket(addr.family,'tcp')
  sock.reuseaddr = true
  sock.nonblock  = true
  sock:bind(addr)
  sock:listen()
  
  nfl.SOCKETS:insert(sock,'r',function()
    local conn,remote,err = sock:accept()
    
    if not conn then
      syslog('error',"sock:accept() = %s",errno[err])
      return
    end
    
    conn.nonblock            = true
    local ios,packet_handler = create_handler(conn,remote)
    ios.__ctx                = server:accept_cbs(ios,tlscb_read,tlscb_write)
    ios.__co                 = nfl.spawn(mainf,ios)
    nfl.SOCKETS:insert(conn,'r',packet_handler)
  end)
  return true
end

-- **********************************************************************
-- Usage:       listen(host,port,mainf,config)
-- Desc:        Initalize a listening TCP socket
-- Input:       host (string) address to bind to
--              port (string integer) port
--              mainf (function) main handler for service
--              config (table) configuration options
-- **********************************************************************

function listen(host,port,mainf,config)
  return listena(net.address2(host,'ip','tcp',port)[1],mainf,config)
end

-- **********************************************************************
-- Usage:       ios = tcp.connecta(addr,hostname,[,to])
-- Desc:        Connect to a remote address
-- Input:       addr (userdata/address) IP address
--              hostname (string) hostname (required for TLS)
--              to (number/optinal) timout the operation after to seconds
-- Return:      ios (table) Input/Output object (nil on error)
-- **********************************************************************

function connecta(addr,hostname,to)
  if not addr then return nil end
  
  local config = tls.config()
  local ctx    = tls.client()
  
  config:protocols("all")
  ctx:configure(config)
  
  local sock,err = net.socket(addr.family,'tcp')
  
  if not sock then
    syslog('error',"tls(TCP) = %s",errno[err])
    return
  end
  
  sock.nonblock            = true
  local ios,packet_handler = create_handler(sock,addr)
  ios.__ctx                = ctx
  ios.__co                 = coroutine.running()
  
  if not ctx:connect_cbs(hostname,ios,tlscb_read,tlscb_write) then
    return nil
  end
  
  -- ------------------------------------------------------------
  -- In POSIXland, a non-blocking socket doing a connect become available
  -- when it's ready for writing.  So we install a 'write' trigger, then
  -- call connect() and yield.  When we return, it's connected (unless we're
  -- optionally timing out the operation).
  -- ------------------------------------------------------------
  
  nfl.SOCKETS:insert(sock,'w',packet_handler)
  if to then nfl.timeout(to,false,errno.ETIMEDOUT) end
  
  sock:connect(addr)
  
  local okay,err1 = coroutine.yield()
  
  if to then nfl.timeout(0) end
  
  if not okay then
    syslog('error',"tls:connect(%s) = %s",hostname,errno[err1])
    nfl.SOCKETS:remove(sock)
    return nil
  end
  return ios
end

-- **********************************************************************
-- Usage:       ios = tcp.connect(host,port[,to])
-- Desc:        Connect to a remote host
-- Input:       host (string) IP address
--              port (string number) port to connect to
--              to (number/optioal) timeout the operation after to seconds
-- Return:      ios (table) Input/Output object (nil on error)
-- **********************************************************************

function connect(host,port,to)
  local addr = net.address2(host,'ip','tcp',port)
  if addr then
    return connecta(addr[1],host,to)
  end
end

-- **********************************************************************

if _VERSION >= "Lua 5.2" then
  return _ENV -- luacheck: ignore
end
