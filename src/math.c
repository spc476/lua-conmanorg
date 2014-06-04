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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <assert.h>

#include <lua.h>
#include <lauxlib.h>

#if !defined(LUA_VERSION_NUM) || LUA_VERSION_NUM != 501
#  error This module is for Lua 5.1
#endif

/************************************************************************/

static int math_randomseed(lua_State *const L)
{
  unsigned int  seed;
  
  assert(L != NULL);
  
  if (!lua_isboolean(L,1) && lua_isnumber(L,1))
    seed = lua_tonumber(L,1);
  else
  {
    FILE *fp;
    
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
  }
  
  srand(seed);
  lua_pushnumber(L,seed);
  return 1;
}

/**************************************************************************/

static int math_idiv(lua_State *const L)
{
  long   numer = luaL_checkinteger(L,1);
  long   denom = luaL_checkinteger(L,2);
  ldiv_t result;
  
  if (denom == 0)
  {
    if (numer < 0)
    {
      lua_pushnumber(L,-HUGE_VAL);
      lua_pushnumber(L,-HUGE_VAL);
    }
    else
    {
      lua_pushnumber(L,HUGE_VAL);
      lua_pushnumber(L,HUGE_VAL);
    }
  }
  else
  {
    result = ldiv(numer,denom);
    lua_pushinteger(L,result.quot);
    lua_pushinteger(L,result.rem);
  }
  
  return 2;
}

/**************************************************************************/

static int math_div(lua_State *const L)
{
  double numer = luaL_checknumber(L,1);
  double denom = luaL_checknumber(L,2);
  
  if (denom == 0.0)
  {
    if (numer < 0.0)
    {
      lua_pushnumber(L,-HUGE_VAL);
      lua_pushnumber(L,-HUGE_VAL);
    }
    else
    {
      lua_pushnumber(L,HUGE_VAL);
      lua_pushnumber(L,HUGE_VAL);
    }
  }
  else
  {
  
    lua_pushnumber(L,numer / denom);
    lua_pushnumber(L,fmod(numer,denom));
  }
  return 2;  
}

/************************************************************************/

static const struct luaL_Reg reg_math[] =
{
  { "randomseed"	, math_randomseed },
  { "idiv"		, math_idiv	  },
  { "div"		, math_div	  },
  { NULL		, NULL		  }
};

int luaopen_org_conman_math(lua_State *L)
{
  luaL_register(L,"org.conman.math",reg_math);
  return 1;
}

/**************************************************************************/

