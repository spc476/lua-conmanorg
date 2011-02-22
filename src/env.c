/********************************************
*
* Copyright 2009 by Sean Conner.  All Rights Reserved.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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
#include <lualib.h>

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
    size_t  len = strlen(environ[i]);
    char    buffer[len];
    size_t  namelen;
    size_t  valuelen;
    char   *p;
    
    memcpy(buffer,environ[i],len);
    buffer[len] = '\0';
    
    p = memchr(buffer,'=',len);
    if (p == NULL) continue;
    
    namelen = (size_t)(p - buffer);
    *p++ = '\0';
    valuelen = (len - namelen) - 1;
    
    lua_pushlstring(L,buffer,namelen);
    lua_pushlstring(L,p,valuelen);
    lua_settable(L,-3);
  }
  
  return 1;
}

