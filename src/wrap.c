/*********************************************************************
*
* Copyright 2010 by Sean Conner.  All Rights Reserved.
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
*********************************************************************/

#include <stddef.h>
#include <ctype.h>
#include <stdbool.h>
#include <assert.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#define DEF_MARGIN	78

/*************************************************************************/

static bool find_break_point(
	size_t     *const restrict pidx,
	const char *const restrict txt,
	const size_t               max __attribute__((unused))
)
{
  size_t idx;
  
  assert(pidx  != NULL);
  assert(*pidx >  0);
  assert(txt   != NULL);
  assert(max   >  0);
  assert(max   >  *pidx);

  for (idx = *pidx ; idx ; idx--)
    if (isspace(txt[idx])) break;
  
  if (idx)
  {
    *pidx = idx + 1;
    return true;
  }
  
  return false;
}

/**************************************************************************/

static int wrap(lua_State *L)
{
  const char *src;
  size_t      ssz;
  size_t      margin;
  size_t      breakp;
  luaL_Buffer buf;
  
  src    = luaL_checklstring(L,1,&ssz);
  margin = luaL_optinteger(L,2,DEF_MARGIN);
  breakp = margin;
  
  luaL_buffinit(L,&buf);
  
  while(true)
  {
    if (ssz < breakp)
      break;

    if (find_break_point(&breakp,src,ssz))
    {
      luaL_addlstring(&buf,src,breakp - 1);
      luaL_addchar(&buf,'\n');
      src    += breakp;
      ssz    -= breakp;
      breakp  = margin;
    }
    else
      break;
  }
  
  luaL_addlstring(&buf,src,ssz);
  luaL_addchar(&buf,'\n');
  luaL_pushresult(&buf);
  return 1;
}

/**************************************************************************/

int luaopen_org_conman_string_wrap(lua_State *L)
{
  assert(L != NULL);

  lua_pushcfunction(L,wrap);
  return 1;
}

/**************************************************************************/

