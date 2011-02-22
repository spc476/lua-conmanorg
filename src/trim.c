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
#include <ctype.h>
#include <assert.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/************************************************************************/

int trim(lua_State *L)
{
  const char *front;
  const char *end;
  size_t      size;
  
  assert(L != NULL);
  
  front = lua_tolstring(L,1,&size);
  end   = &front[size - 1];
  
  lua_pop(L,1);
  
  for ( ; size && isspace(*front) ; size-- , front++)
    ;
  
  for ( ; size && isspace(*end) ; size-- , end--)
    ;
  
  lua_pushlstring(L,front,(size_t)(end - front) + 1);
  return 1;
}

/************************************************************************/

int luaopen_org_conman_string_trim(lua_State *L)
{
  assert(L != NULL);
  
  lua_pushcfunction(L,trim);
  return 1;
}

/*************************************************************************/

