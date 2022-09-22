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
-- luacheck: globals SOCKETS schedule spawn timeout info dump_info
-- luacheck: globals client_eventloop server_eventloop
-- luacheck: ignore 611

local pollset   = require "org.conman.pollset"
local syslog    = require "org.conman.syslog"
local signal    = require "org.conman.signal"
local clock     = require "org.conman.clock"
local errno     = require "org.conman.errno"
local coroutine = require "coroutine"
local table     = require "table"
local debug     = require "debug"
local math      = require "math"

local _VERSION     = _VERSION
local print        = print
local assert       = assert
local pairs        = pairs
local unpack       = table.unpack or unpack
local tostring     = tostring
local setmetatable = setmetatable

if _VERSION == "Lua 5.1" then
  module(...)
else
  _ENV = {} -- luacheck: ignore
end

-- **********************************************************************

local REFQUEUE = { _n = 0 } -- list of all coroutines (for strong reference)
local TOQUEUE  = setmetatable({},{ __mode = "k" }) -- TimeOut queue
local RUNQUEUE = {}         -- run queue
      SOCKETS  = pollset()  -- event generators
      
signal.ignore('pipe')

-- **********************************************************************
-- TOQUEUE:insert() and TOQUEUE:remove() are based, more or less directly,
-- up http://en.wikipedia.org/wiki/Binary_heap
-- ----------------------------------------------------------------------

function TOQUEUE:insert(when,co,...)
  local function bubble_up(parent,child)
    if parent > 0 then
      if self[parent].awake > self[child].awake then
        self[parent] , self[child] = self[child] , self[parent]
        return bubble_up(math.floor(parent / 2),parent)
      end
    end
  end
  
  local idx    = #self + 1
  local parent = math.floor(idx / 2)
  local info   =
  {
    awake   = clock.get('monotonic') + when,
    trigger = true,
    co      = co,
    ...
  }
  
  self[idx] = info
  self[co]  = info
  bubble_up(parent,idx)
end

-- **********************************************************************

function TOQUEUE:remove()
  local function bubble_down(parent,left,right)
    local smallest = parent
    
    if left <= #self and self[left].awake < self[smallest].awake then
      smallest = left
    end
    
    if right <= #self and self[right].awake < self[smallest].awake then
      smallest = right
    end
    
    if smallest ~= parent then
      self[parent] , self[smallest] = self[smallest] , self[parent]
      return bubble_down(smallest,smallest * 2,smallest * 2 + 1)
    end
  end
  
  local res    = self[1]
  local x      = table.remove(self)
  self[res.co] = nil
  
  if #self > 0 then
    self[1] = x
    bubble_down(1,2,3)
  end
  
  return res
end

-- **********************************************************************

function schedule(co , ... )
  if not RUNQUEUE[co] then
    RUNQUEUE[#RUNQUEUE + 1] = { co , ... }
    RUNQUEUE[co] = true
  end
end

-- **********************************************************************

function spawn(f, ...)
  local co = coroutine.create(f)
  
  if co then
    REFQUEUE[co] = true
    REFQUEUE._n = REFQUEUE._n + 1
    schedule(co,...)
  end
  
  return co
end

-- **********************************************************************

function timeout(when,...)
  local co = coroutine.running()
  
  if when == 0 then
    if TOQUEUE[co] then
      assert(TOQUEUE[co].co == co)
      TOQUEUE[co].trigger = false
    end
  else
    TOQUEUE:insert(when,co,...)
  end
end

-- **********************************************************************

local function eventloop(done_f)
  if done_f() then return end
  
  local timeout = -1
  local now     = clock.get('monotonic')
  
  while #TOQUEUE > 0 do
    local co = TOQUEUE[1]
    
    if co.trigger then
      timeout = co.awake - now
      if timeout > 0 then
        break
      end
      
      if coroutine.status(co.co) ~= 'dead' then
        schedule(co.co,unpack(co))
      end
    end
    
    TOQUEUE:remove()
  end
  
  if #RUNQUEUE > 0 then
    timeout = 0
  end
  
  local okay,err = SOCKETS:wait(timeout)
  if not okay then
    syslog('error',"SOCKETS:events() = %s",errno[err])
    return eventloop(done_f)
  end
  
  for event in SOCKETS:events() do
    event.obj(event)
  end
  
  while #RUNQUEUE > 0 do
    local co        = table.remove(RUNQUEUE,1)
    RUNQUEUE[co[1]] = nil
    
    local status = coroutine.status(co[1])
    
    if status == 'dead' then
      syslog('warning',"A dead coroutine was scheduled to run")
      
    elseif status == 'suspended' then
      local ret = { coroutine.resume(unpack(co)) }
      
      if not ret[1] then
        syslog('error',"CRASH: coroutine %s dead: %s",tostring(co[1]),ret[2])
        local msg = debug.traceback(co[1])
        for entry in msg:gmatch("[^%\n]+") do
          syslog('error',"CRASH: %s: %s",tostring(co[1]),entry)
        end
      end
      
      if coroutine.status(co[1]) == 'dead' then
        assert(REFQUEUE._n > 0)
        REFQUEUE[co[1]] = nil
        REFQUEUE._n     = REFQUEUE._n - 1
      end
    
    else
      -- =====================================================================
      -- There are two states not accounted for---'normal' and 'running'.
      -- Neither of these should happen.
      --
      -- 'running'
      --        Shouldn't happen because then it means we are trying to
      --        resume the coroutine that is currently running (namely, this
      --        coroutine), which not only doesn't make sense, but I can't
      --        see how this could happen.
      --
      -- 'normal'
      --        We're doing this check from a coroutine that isn't running
      --        and the coroutine we're checking is running, and I can't see
      --        how that can happen (and doesn't make much sense either).
      --
      -- So this path is one of those "This should never happen" paths,
      -- which should never happen.  And if it does, just remove the
      -- reference to the coroutine, and log what happened.
      -- =====================================================================
      
      syslog('critical',"unexpected coroutine state %q",status)
      assert(REFQUEUE._n > 0)
      REFQUEUE[co[1]] = nil
      REFQUEUE._n     = REFQUEUE._n - 1
    end
  end
  
  return eventloop(done_f)
end

-- **********************************************************************

function client_eventloop(done_f)
  done_f = done_f or function() return false end
  return eventloop(function() return REFQUEUE._n == 0 or done_f() end)
end

-- **********************************************************************

function server_eventloop(done_f)
  done_f = done_f or function() return false end
  return eventloop(done_f)
end

-- **********************************************************************

function info()
  return REFQUEUE._n,#RUNQUEUE,#TOQUEUE,#SOCKETS
end

-- **********************************************************************

function dump_info()
  print("SOCKETS:",#SOCKETS)
  for name,val in pairs(REFQUEUE) do print("REF",name,val) end
  for name,val in pairs(TOQUEUE)  do print("TO", name,val) end
  for name,val in pairs(RUNQUEUE) do print("RUN",name,val) end
end

-- **********************************************************************

if _VERSION >= "Lua 5.2" then
  return _ENV -- luacheck: ignore
end
