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
*********************************************/

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <lua.h>
#include <lauxlib.h>

#if defined(__GNUC__) && defined(__x86_64) && (__GLIBC__ == 2) && (__GLIBC_MINOR__ == 12)
#  include <limits.h>
#  define SIZET_MAX INT_MAX
#else
#  define SIZET_MAX (size_t)-1
#endif

extern char **environ;

static const struct luaL_reg env[] =
{
  { NULL , NULL }
};

int luaopen_org_conman_env(lua_State *L)
{
  luaL_register(L,"org.conman.env",env);
  
  for (int i = 0 ; environ[i] != NULL ; i++)
  {
    char   *value;
    char   *eos;
    
    value = memchr(environ[i],'=',SIZET_MAX);
    assert(value != NULL);
    eos   = memchr(value + 1,'\0',SIZET_MAX);
    assert(eos   != NULL);
    
    lua_pushlstring(L,environ[i],(size_t)(value - environ[i]));
    lua_pushlstring(L,value + 1,(size_t)(eos - (value + 1)));
    lua_settable(L,-3);
  }
    
  return 1;
}

