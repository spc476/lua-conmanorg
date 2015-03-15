-- ***************************************************************
--
-- Copyright 2012 by Sean Conner.  All Rights Reserved.
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

local _VERSION     = _VERSION
local tcc          = require "org.conman.tcc"
local package      = package
local table        = table
local os           = os
local io           = io
local string       = string
local pairs        = pairs
local setmetatable = setmetatable

if _VERSION == "Lua 5.1" then
  module("org.conman.cc")
else
  _ENV = {}
end

package.ccpath = "/usr/loca/share/lua/5.1/?.c;/usr/local/lib/lua/5.1/?.c"

local DIR  = package.config:sub(1,1)
local SEP  = package.config:sub(3,3)
local MARK = package.config:sub(5,5)
local IGN  = package.config:sub(9,9)

do
  local ep = os.getenv("LUA_CCPATH")
  if ep ~= nil then
    package.ccpath = ep:gsub(";;",";" .. package.ccpath .. ";")
  end
  
  local function loader(name)
    local modulefile = name:gsub("%.",DIR)
    local errmsg     = ""
    local filename
    local file
    local entry
    local x
  
    for path in package.ccpath:gmatch("[^%" .. SEP .. "]+") do
      filename = path:gsub("[%" .. MARK .. "]",modulefile)
      file = io.open(filename)
      if file then break end
      errmsg = errmsg .. "\n\tno file '" .. filename .. "'"
    end
  
    if not file then
      return errmsg
    end
    
    file:close()
  
    local f,l = name:find(IGN,1,true)
    if l then
      entry = "luaopen_" .. name:sub(l + 1,-1):gsub("%.","_")
    else
      entry = "luaopen_" .. name:gsub("%.","_")
    end
  
    -- ---------------------------------------------------------------------
    -- Why not use compile() for this?  Because that caches the memory blob
    -- returned from relocate() via the symbol we pulled out.  Lua modules
    -- only use this function once, then it's not used, and thus, the module
    -- we just loaded stands a chance of being flushed out of memory,
    -- causing issues.
    --
    -- Doing it this way, we avoid that problem.  We still have the problem
    -- if the module is removed from package.loaded{} and reloaded, which
    -- will cause a memory leak, but that scenario is probably very rare. 
    -- I'm willing to live with this for now.
    -- ---------------------------------------------------------------------

    x = tcc.new()
    x:define("LUA_REQUIRE","1")
    x:output_type('memory')
    if x:compile(filename,true,function(msg)
                                 errmsg = errmsg .. "\n\t" .. msg
                               end) 
    then
      local blob = x:relocate()
      local f = x:get_symbol(entry)
      if f then
        return f
      end
      tcc.dispose(blob)
      errmsg = errmsg 
            .. string.format("\n\tno entry '%s' in file '%s'",entry,filename)
      return errmsg
    end
  
    return errmsg
  end
  
  table.insert(package.loaders,#package.loaders,loader) 
end    

-- ********************************************************************

do
  local code = [[

#include <syslog.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define CC_TYPE	"CC:type"

struct cclua
{
  void *data;
};

int cclua___tostring(lua_State *L)
{
  lua_pushfstring(L,"CC:%p",luaL_checkudata(L,1,CC_TYPE));
  return 1;
}

int cclua___gc(lua_State *L)
{
  struct cclua *obj = luaL_checkudata(L,1,CC_TYPE);
  luaL_getmetafield(L,1,"dispose");
  
  lua_pushlightuserdata(L,obj->data);
  lua_call(L,1,0);
  syslog(LOG_DEBUG,"garbage collect some C code");
    
  return 0;
}

int cclua_encap(lua_State *L)
{
  struct cclua *obj;
  
  luaL_checktype(L,1,LUA_TLIGHTUSERDATA);
  obj = lua_newuserdata(L,sizeof(struct cclua));
  obj->data = lua_touserdata(L,1);
  luaL_getmetatable(L,CC_TYPE);
  lua_setmetatable(L,-2);
  return 1;
}

const struct luaL_Reg mcc_meta[] =
{
  { "__tostring" 	, cclua___tostring	} ,
  { "__gc"		, cclua___gc		} ,
  { NULL		, NULL			}
};

int luaopen_cc(lua_State *L)
{
  luaL_newmetatable(L,CC_TYPE);
  luaL_register(L,NULL,mcc_meta);
  lua_pushvalue(L,-1);
  lua_setfield(L,-2,"__index");
  lua_pushvalue(L,-1);
  lua_setfield(L,-2,"__newindex");
  
  if (lua_isfunction(L,1))
  {
    lua_pushvalue(L,1);
    lua_setfield(L,-2,"dispose");
  }

  lua_pushcfunction(L,cclua_encap);
  return 1;
}
]]

  local x = tcc.new()
  x:output_type('memory')
  x:compile(code)
  x:relocate()
  local init = x:get_symbol('luaopen_cc')
  encap      = init(tcc.dispose)
end

-- **********************************************************************

CACHE = setmetatable( {} , { __mode = "k" })

function compile(fname,code,isfile,defines)
  local defines = defines or {}
  local x = tcc.new()
  
  for def,val in pairs(defines) do
    x:define(def,val)
  end
  x:output_type('memory')

  if not x:compile(code,isfile) then
    return nil
  end

  local blob = x:relocate()
  local f    = x:get_symbol(fname)
  
  if f then
    CACHE[f] = encap(blob)
    return f,blob
  end
  
  return nil
end

if _VERSION >= "Lua 5.2" then
  return _ENV
end
