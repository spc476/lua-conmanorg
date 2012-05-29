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

local net    = require "org.conman.net"
local dns    = require "org.conman.dns"
local math   = require "math"
local ipairs = ipairs

module("org.conman.dns.resolv")

-- ********************************************************************

local servers = 
{
  net.address("192.168.1.10","domain","udp"),
}

local searches =
{
  ".",
  ".roswell.area51."
}

servers.current = 1
servers.sock    = net.socket(servers[1].family,'udp')

-- ********************************************************************

local function query(host,type)
  local packet
  local result
  local err
  local id
  local sock
  local _
  local cycle
  
  id         = math.random()
  packet,err = dns.encode {
  		id       = id,
  		query    = true,
  		rd       = true,
  		opcode   = 'query',
  		question = 
  		{
  			name  = host,
  			type  = type,
  			class = 'in'
  		}
  	}
  
  if packet == nil then
    return packet,err
  end
  
  cycle = servers.current
  
  repeat
    servers.sock:write(servers[servers.current],packet)
  
    _,result,err = servers.sock:read(5)
    if err == 0 then
      return dns.decode(result)
    end
    
    servers.sock:close()
    cycle = cycle + 1
    if cycle > #servers then
      cycle = 1
    end
    servers.sock = net.socket(servers[cycle].family,'udp')
  until cycle == servers.current
  
  return nil
end

-- ********************************************************************

local function query_a(host,type)
  type = type:upper()
  local a,err = query(host,type)

  if a == nil then
    return nil
  end
  
  for i = 1,#a.answers do
    if a.answers[i].type == type then
      return a.answers[i].address
    end
  end
  return nil
end

-- ********************************************************************

function address(host)
  for _,suffix in ipairs(searches) do
    local hostname = host .. suffix
    local a,err    = query_a(hostname,'a')
    if a == nil then
      a,err = query_a(hostname,'aaaa')
    end
    if a ~= nil then
      return a
    end
  end
  return nil
end
