/********************************************
*
* Copyright 2009 by Sean Conner.  All Rights Reserved.
*
* This library is free software; you can redistribute it and/or modify it
* under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation; either version 3 of the License, or (at your
* option) any later version.
*
* This library is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
* License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this library; if not, see <http://www.gnu.org/licenses/>.
*
* Comments, questions and criticisms can be sent to: sean@conman.org
*
* ---------------------------------------------------------------------
*
* This module is nothing more than an array of the environmental variables
* of the current process.  There's not much more to say about this than
*
	env = require "org.conman.env"
	print(env.LUA_PATH)
	print(env['LUA_CPATH'])

*
*********************************************/

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <lua.h>
#include <lauxlib.h>

#if !defined(LUA_VERSION_NUM) || LUA_VERSION_NUM < 501
#  error You need to compile against Lua 5.1 or higher
#endif

extern char **environ;

#if LUA_VERSION_NUM == 501
  static const struct luaL_Reg env[] =
  {
    { NULL , NULL }
  };
#endif

int luaopen_org_conman_env(lua_State *L)
{
  int i;

#if LUA_VERSION_NUM == 501 
  luaL_register(L,"org.conman.env",env);
#else
  lua_createtable(L,0,0);
#endif

  for (i = 0 ; environ[i] != NULL ; i++)
  {
    char *value = strchr(environ[i],'=');

    assert(value != NULL);

    lua_pushlstring(L,environ[i],(size_t)(value - environ[i]));
    lua_pushstring(L,value + 1);
    lua_settable(L,-3);
  }
  
  return 1;
}

