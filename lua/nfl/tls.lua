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
-- luacheck: globals listens listena listen connecta connect
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
  local ios        = mkios()
  ios.__writebuf   = ""
  ios.__rawbuf     = ""
  ios.__sock       = conn
  ios.__remote     = remote
  
  ios._handshake = function(self)
    local rc = ios.__ctx:handshake()
    if rc == tls.WANT_INPUT then
      ios.__resume = true
      coroutine.yield()
      return self:_handshake()
    
    elseif rc == tls.WANT_OUTPUT then
      nfl.SOCKETS:update(conn,"rw")
      ios.__resume = true
      coroutine.yield()
      return self:_handshake()
    
    else
      return rc == 0
    end
  end
  
  ios._refill = function(self)
    local str,len = self.__ctx:read(tls.BUFFERSIZE)

    if len == tls.ERROR then
      return nil,self.__ctx:error()
    elseif len == tls.WANT_INPUT then
      ios.__resume = true
      coroutine.yield()
      return self:_refill()
    elseif len == tls.WANT_OUTPUT then
      nfl.SOCKETS:update(conn,"rw")
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
    
      -- --------------------------------------------------------------------
      -- I was receiving "Resource temporarily unavailable" and trying again,
      -- but that strategy fails upon failure to read a certificate.  So now
      -- I'm back to returning an error.  Let's hope this works this time.
      -- --------------------------------------------------------------------
      
      return false,self.__ctx:error()
      
    elseif bytes == tls.WANT_INPUT then
      self.__resume = true
      coroutine.yield()
      return self:_drain(data)
      
    elseif bytes == tls.WANT_OUTPUT then
      nfl.SOCKETS:update(conn,"rw")
      self.__resume = true
      coroutine.yield()
      return self:_drain(data)
      
    elseif bytes < #data then
      nfl.SOCKETS:update(conn,"rw")
      self.__resume = true
      coroutine.yield()
      return self:_drain(data:sub(bytes+1,-1))
    end
    
    return true
  end
  
  ios.close = function(self)
    local rc = ios.__ctx:close()
    if rc == tls.WANT_INPUT then
      ios.__resume = true
      coroutine.yield()
      return self:close()
    elseif rc == tls.WANT_OUTPUT then
      nfl.SOCKETS:update(conn,"rw")
      ios.__resume = true
      return self:close()
    end
    
    nfl.SOCKETS:remove(conn)
    local err = conn:close()
    return err == 0,errno[err],err
  end
  
  return ios,function(event)
    if event.hangup then
      nfl.SOCKETS:remove(conn)
      ios._eof = true
      nfl.schedule(ios.__co,false,errno.ECONNREFUSED)
    end
    
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
  if not bytes then
    bytes = tls.ERROR
  end
  
  return bytes
end

-- **********************************************************************
-- Usage:       sock,errmsg = listens(sock,mainf,conf)
-- Desc:        Initialize a listening TCP socket
-- Input:       sock (userdata/socket) bound socket
--              mainf (function) main handler for service
--              conf (function) function for TLS configuration
-- Return:      sock (userdata) socket used for listening, false on error
--              errmsg (string) error message
-- **********************************************************************

function listens(sock,mainf,conf)
  local config = tls.config()
  local server = tls.server()
  
  if not conf(config) then return false,config:error() end
  if not server:configure(config) then
    return false,server:error()
  end
  
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
  
  return sock
end

-- **********************************************************************
-- Usage:       sock,errmsg = listena(addr,mainf,conf)
-- Desc:        Initialize a listening TCP socket
-- Input:       addr (userdata/address) IP address
--              mainf (function) main handler for service
--              conf (function) function for TLS configuration
-- Return:      sock (userdata) socket used for listening, false on error
--              errmsg (string) error message
-- **********************************************************************

function listena(addr,mainf,conf)
  local sock,err = net.socket(addr.family,'tcp')
  
  if not sock then
    return false,errno[err]
  end
  
  sock.reuseaddr = true
  sock.nonblock  = true
  sock:bind(addr)
  sock:listen()
  return listens(sock,mainf,conf)
end

-- **********************************************************************
-- Usage:       sock,errmsg = listen(host,port,mainf,config)
-- Desc:        Initalize a listening TCP socket
-- Input:       host (string) address to bind to
--              port (string integer) port
--              mainf (function) main handler for service
--              config (function) configuration options
-- Return:      sock (userdata) socket used for listening, false on error
--              errmsg (string) error message
-- **********************************************************************

function listen(host,port,mainf,config)
  return listena(net.address2(host,'any','tcp',port)[1],mainf,config)
end

-- **********************************************************************
-- Usage:       ios = tcp.connecta(addr,hostname[,to[,config]])
-- Desc:        Connect to a remote address
-- Input:       addr (userdata/address) IP address
--              hostname (string) hostname (required for TLS)
--              to (number/optinal) timout the operation after to seconds
--              config (function) configuration options
-- Return:      ios (table) Input/Output object (nil on error)
-- **********************************************************************

function connecta(addr,hostname,to,conf)
  if not addr then return nil end
  
  local config = tls.config()
  local ctx    = tls.client()
  
  if conf then
    if not conf(config) then return false,config:error() end
  else
    config:protocols("all")
  end
  
  ctx:configure(config)
  
  local sock,err = net.socket(addr.family,'tcp')
  
  if not sock then
    syslog('error',"tls(TCP) = %s",errno[err])
    return false,errno[err]
  end
  
  sock.nonblock            = true
  local ios,packet_handler = create_handler(sock,addr)
  ios.__ctx                = ctx
  ios.__co                 = coroutine.running()
  
  if not ctx:connect_cbs(hostname,ios,tlscb_read,tlscb_write) then
    syslog('error',"connect_cbs() = %s",ctx:error())
    return false,ctx:error()
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
    return false,errno[err1]
  end
  return ios
end

-- **********************************************************************
-- Usage:       ios = tcp.connect(host,port[,to[,conf]])
-- Desc:        Connect to a remote host
-- Input:       host (string) IP address
--              port (string number) port to connect to
--              to (number/optioal) timeout the operation after to seconds
--              conf (function) configuration options
-- Return:      ios (table) Input/Output object (nil on error)
-- **********************************************************************

function connect(host,port,to,conf)
  local addr = net.address2(host,'any','tcp',port)
  if addr then
    return connecta(addr[1],host,to,conf)
  end
end

-- **********************************************************************

if _VERSION >= "Lua 5.2" then
  return _ENV -- luacheck: ignore
end
