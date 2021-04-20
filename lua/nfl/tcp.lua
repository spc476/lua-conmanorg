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

local syslog    = require "org.conman.syslog"
local errno     = require "org.conman.errno"
local net       = require "org.conman.net"
local mkios     = require "org.conman.net.ios"
local nfl       = require "org.conman.nfl"
local coroutine = require "coroutine"

local _VERSION     = _VERSION
local tostring     = tostring
local setmetatable = setmetatable
local assert       = assert
local ipairs       = ipairs

if _VERSION == "Lua 5.1" then
  module(...)
else
  _ENV = {} -- luacheck: ignore
end

-- **********************************************************************
-- usage:       ios,handler = create_handler(conn,remote)
-- desc:        Create the event handler for handing network packets
-- input:       conn (userdata/socket) connected socket
--              remote (userdata/address) remote connection
-- return:      ios (table) I/O object (similar to what io.open() returns)
--              handler (function) event handler
-- **********************************************************************

local function create_handler(conn,remote)
  local ios    = mkios()
  ios.__socket = conn
  ios.__remote = remote
  ios.__output = ""
  ios.__rbytes = 0
  ios.__wbytes = 0
  
  ios._refill = function()
    return coroutine.yield()
  end
  
  ios._drain = function(self,data)
    ios.__output = data
    nfl.SOCKETS:update(self.__socket,'w')
    return coroutine.yield()
  end
  
  ios.close = function(self)
    -- ---------------------------------------------------------------------
    -- It might be a bug *somewhere*, but on Linux, this is required to get
    -- Unix domain sockets to work with the NFL driver.  There's a race
    -- condition where writting data then calling close() may cause the
    -- other side to receive no data.  This does NOT appoear to happen with
    -- TCP sockets, but this doesn't hurt the TCP side in any case.
    -- ---------------------------------------------------------------------
    
    while self.__socket.sendqueue and ios.__socket.sendqueue > 0 do
      nfl.SOCKETS:update(self.__socket,'w')
      coroutine.yield()
    end
    
    nfl.SOCKETS:remove(self.__socket)
    local err = self.__socket:close()
    return err == 0,errno[err],err
  end
  
  if _VERSION >= "Lua 5.2" then
    local mt = {}
    mt.__gc = ios.close
    if _VERSION >= "Lua 5.4" then
      mt.__close = ios.close
    end
    setmetatable(ios,mt)
  end
  
  return ios,function(event)
    assert(not (event.read and event.write))
    
    if event.hangup then
      if not ios._eof then
        nfl.SOCKETS:remove(ios.__socket)
        ios._eof = true
        nfl.schedule(ios.__co,false,"disconnect")
      end
      return
    end
    
    if event.read then
      local _,packet,err = ios.__socket:recv()
      if packet then
        if #packet == 0 then
          nfl.SOCKETS:remove(ios.__socket)
          ios._eof    = true
          nfl.schedule(ios.__co,false,"disconnect")
        else
          ios.__rbytes = ios.__rbytes + #packet
          nfl.schedule(ios.__co,packet)
        end
      else
        if err ~= errno.EAGAIN then
          syslog('error',"socket:recv() = %s",errno[err])
          nfl.SOCKETS:remove(ios.__socket)
          ios._eof     = true
          nfl.schedule(ios.__co,false,errno[err],err)
        end
      end
    end
    
    if event.write then
      if #ios.__output > 0 then
        local bytes,err = ios.__socket:send(nil,ios.__output)
        if err == 0 then
          ios.__wbytes = ios.__wbytes + bytes
          ios.__output = ios.__output:sub(bytes + 1,-1)
        else
          syslog('error',"socket:send() = %s",errno[err])
          nfl.SOCKETS:remove(ios.__socket)
          ios._eof    = true
          nfl.schedule(ios.__co,false,errno[err],err)
        end
      end
      
      if #ios.__output == 0 then
        nfl.SOCKETS:update(ios.__socket,'r')
        nfl.schedule(ios.__co,true)
      end
    end
  end
end

-- **********************************************************************
-- Usage:       sock,errmsg = listens(sock,mainf)
-- Desc:        Initialize a listening TCP socket
-- Input:       sock (userdata/socket) bound socket
--              mainf (function) main handler for service
-- Return:      sock (userdata) socket used for listening, false on error
--              errmsg (string) error message
-- **********************************************************************

function listens(sock,mainf)
  nfl.SOCKETS:insert(sock,'r',function()
    local conn,remote,err = sock:accept()
    
    if not conn then
      syslog('error',"sock:accept() = %s",errno[err])
      return
    end
    
    conn.nonblock = true
    local ios,packet_handler = create_handler(conn,remote)
    ios.__co = nfl.spawn(mainf,ios)
    nfl.SOCKETS:insert(conn,'r',packet_handler)
  end)
  
  return sock
end

-- **********************************************************************
-- Usage:       sock,errmsg = listena(addr,mainf)
-- Desc:        Initalize a listening TCP socket
-- Input:       addr (userdata/address) IP address
--              mainf (function) main handler for service
-- Return:      sock (userdata) socket used for listening, false on error
--              errmsg (string) error message
-- **********************************************************************

function listena(addr,mainf)
  local sock,err = net.socket(addr.family,'tcp')
  
  if not sock then
    return false,errno[err]
  end
  
  sock.reuseaddr = true
  sock.nonblock  = true
  sock:bind(addr)
  sock:listen()
  return listens(sock,mainf)
end

-- **********************************************************************
-- Usage:       listen(host,port,mainf)
-- Desc:        Initalize a listening TCP socket
-- Input:       host (string) address to bind to
--              port (string integer) port
--              mainf (function) main handler for service
-- **********************************************************************

function listen(host,port,mainf)
  return listena(net.address2(host,'any','tcp',port)[1],mainf)
end

-- **********************************************************************
-- Usage:       ios = tcp.connecta(addr[,to])
-- Desc:        Connect to a remote address
-- Input:       addr (userdata/address) IP address
--              to (number/optinal) timout the operation after to seconds
-- Return:      ios (table) Input/Output object (nil on error)
-- **********************************************************************

function connecta(addr,to)
  if not addr then return nil end
  
  local sock,err = net.socket(addr.family,'tcp')
  
  if not sock then
    syslog('error',"socket(TCP) = %s",errno[err])
    return
  end
  
  sock.nonblock            = true
  local ios,packet_handler = create_handler(sock,addr)
  ios.__co                 = coroutine.running()
  
  -- ------------------------------------------------------------
  -- In POSIXland, a non-blocking socket doing a connect become available
  -- when it's ready for writing.  So we install a 'write' trigger, then
  -- call connect() and yield.  When we return, it's connected (unless we're
  -- optionally timing out the operation).
  -- ------------------------------------------------------------
  
  nfl.SOCKETS:insert(sock,'w',packet_handler)
  if to then nfl.timeout(to,false,errno[errno.ETIMEDOUT]) end
  sock:connect(addr)
  local okay,err1 = coroutine.yield()
  if to then nfl.timeout(0) end
  
  if not okay then
    nfl.SOCKETS:remove(sock)
    sock:close()
    syslog('error',"sock:connect(%s) = %s",tostring(addr),err1)
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
  local addr = net.address2(host,'any','tcp',port)
  if addr then
    for _,a in ipairs(addr) do
      local conn = connecta(a,to)
      if conn then
        return conn
      end
    end
  end
end

-- **********************************************************************

if _VERSION >= "Lua 5.2" then
  return _ENV -- luacheck: ignore
end
