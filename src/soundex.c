/*********************************************************************
*
* Copyright 2013 by Sean Conner.  All Rights Reserved.
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

#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include <lua.h>
#include <lauxlib.h>

/********************************************************************/

static int soundex_lua(lua_State *const L)
{
  static const char        ignore[] = "AEIOUWYH";
  static const char *const use[6]   =
  {
    "BPFV",
    "CSKGJQXZ",
    "DT",
    "L",
    "MN",
    "R"
  };
  
  char        sdx[4];
  const char *word = luaL_checkstring(L,1);
  char        c;
  char        last;
  size_t      idx;
  
  sdx[0] = last   = toupper(*word++);
  sdx[1] = sdx[2] = sdx[3] = '0';
  idx    = 1;
  
  while((idx < 4) && ((c = toupper(*word++)) != '\0'))
  {
    if (strchr(ignore,c) != NULL)
      continue;
    
    for (size_t i = 0 ; i < 6 ; i++)
    {
      if (strchr(use[i],c) != NULL)
      {
        if (strchr(use[i],last) != NULL)
          continue;
        last = c;
        sdx[idx++] = '1' + i;
      }
    }
  }
  
  lua_pushlstring(L,sdx,4);
  return 1;  
}

/********************************************************************/

int luaopen_org_conman_string_soundex(lua_State *const L)
{
  assert(L != NULL);
  
  lua_pushcfunction(L,soundex_lua);
  return 1;
}

/********************************************************************/

