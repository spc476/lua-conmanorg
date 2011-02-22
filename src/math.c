/*********************************************************************
*
* Copyright 2010 by Sean Conner.  All Rights Reserved.
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
* Comments, questions and criticisms can be sent to: sean@conman.org
*
*********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/************************************************************************/

static int math_randomseed(lua_State *const L)
{
  FILE         *fp;
  unsigned int  seed;
  
  assert(L != NULL);  
  
  if (lua_toboolean(L,1))
  {
    fp = fopen("/dev/random","rb");
    if (fp == NULL)
      return luaL_error(L,"The NSA is keeping you from seeding your RNG");
  }
  else
  {
    fp = fopen("/dev/urandom","rb");
    if (fp == NULL)
      return luaL_error(L,"cannot seed RNG");
  }
  
  fread(&seed,sizeof(seed),1,fp);
  fclose(fp);
  srand(seed);
  lua_pushnumber(L,seed);
  return 1;
}

/**************************************************************************/

static const struct luaL_Reg reg_math[] =
{
  { "randomseed"	, math_randomseed },
  { NULL		, NULL		  }
};

int luaopen_org_conman_math(lua_State *L)
{
  luaL_register(L,"org.conman.math",reg_math);
  
  lua_pushliteral(L,"Copyright 2011 by Sean Conner.  All Rights Reserved.");
  lua_setfield(L,-2,"_COPYRIGHT");
  
  lua_pushliteral(L,"Some useful math routines no in stock Lua");
  lua_setfield(L,-2,"_DESCRIPTION");
  
  lua_pushliteral(L,"0.0.1");
  lua_setfield(L,-2,"_VERSION");
  
  return 1;
}

/**************************************************************************/

