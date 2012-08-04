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

local tcc          = require "org.conman.tcc"
local setmetatable = setmetatable

module("org.conman.cc")

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
  syslog(LOG_DEBUG,"Hello from %s(%d)",__FILE__,__LINE__);
  
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

_CACHE = setmetatable( {} , { __mode = "k" })

function compile(fname,code,isfile)
  local x = tcc.new()
  
  if not x:output_type('memory') then
    return nil
  end
  
  x:compile(code,isfile)

  local blob = x:relocate()
  local f    = x:get_symbol(fname)
  
  if f then
    _CACHE[f] = encap(blob)
    return f
  end
  
  return nil
end
