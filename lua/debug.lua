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

module("org.conman.debug",package.seeall)

EDITOR = os.getenv("EDITOR") or "/bin/vi"

-- *********************************************************************

local function create()
  local name = os.tmpname()
  local src
  local f
  
  os.execute(EDITOR .. " " .. name)
  f = io.open(name,"r")
  src = f:read("*a")
  f:close()
  os.remove(name)
  local code = loadstring(src)
  code()  
end

-- ********************************************************************

function edit(f)
  if f == nil then
    create()
    return
  end
  
  local info = debug.getinfo(f)
  local src
  
  if info.source:byte(1) == 64 then
    local f = io.open(info.source:sub(2),"r")
    local count = 0
    local line
    
    repeat
      line  = f:read("*l")
      count = count + 1
    until count == info.linedefined
    
    src = line .. "\n"
    
    repeat
      src = src .. f:read("*l") .. "\n"
      count = count + 1
    until count == info.lastlinedefined
    
    f:close()
  else
    src = info.source
  end
  
  local name = os.tmpname()
  local f    = io.open(name,"w")
  f:write(src)
  f:close()
  
  os.execute(EDITOR .. " " .. name)
  
  f = io.open(name,"r")
  src = f:read("*a")
  f:close()
  os.remove(name)
  
  local code = loadstring(src)
  code()
end

-- **********************************************************************

function editf(f)
  if f == nil then
    create()
    return
  end
  
  local info = debug.getinfo(f)
  
  if info.source:byte(1) == 64 then
    local editor = os.getenv("EDITOR")
    local file   = info.source:sub(2)
    
    os.execute(string.format("%s +%d %s",
    		EDITOR,
    		info.linedefined,
    		file
    	))
    dofile(file)
  elseif info.source:byte(1) == 61 then
    print("can't edit---no source")
  else
    local name   = os.tmpname()
    local f      = io.open(name,"w")
    
    f:write(info.source)
    f:close()
    
    os.execute(EDITOR .. " " .. name)
    f = io.open(name,"r")
    src = f:read("*a")
    f:close()
    os.remove(name)
    
    assert(loadstring(src))
  end
end

-- ********************************************************************

function hexdump(t,f)
  local offset=1
  local out = f or io.stdout
  
  while true do
    if offset > string.len(t) then return end
    local s = string.sub(t,offset,offset + 15)
    out:write(string.format("%08X: ",offset - 1))
    string.gsub(s,"(.)",
            function (c) out:write(string.format("%02X ",string.byte(c))) end)
    out:write(string.rep(" ",3*(16-string.len(s))))
    
    string.gsub(s,"(.)",
    	    function (c)
    	      local x = string.byte(c)
    	      if x < 32 then
    	        out:write(".")
    	      elseif x > 127 then
    	        out:write(".")
    	      else
    	        out:write(c)
    	      end
    	    end
    	    )
    out:write("\n")
    offset = offset+16
  end
end
