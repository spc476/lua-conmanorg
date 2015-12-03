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
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <time.h>
#include <assert.h>

#include <lua.h>
#include <lauxlib.h>

#if !defined(LUA_VERSION_NUM) || LUA_VERSION_NUM < 501
#  error You need to compile against Lua 5.1 or higher
#endif

/************************************************************************/

static int math_randomseed(lua_State *const L)
{
  char         *seedbank;
  char         *seedbank_file;
  char         *seedbank_log;
  unsigned int  seed;
  
  assert(L != NULL);
  
  /*--------------------------------------------------------------------
  ; The idea for this is from https://github.com/catseye/seedbank
  ;
  ; But slightly modified.  If SEEDBANK_SEED is an integer, that's used
  ; as the seeed.  If SEEDBANK_FILE is set and SEEDBANK_SEED=LAST, then
  ; the contents of $SEEDBANK_FILE is used.  Otherwise, the older methods
  ; are used (so this is new functionality).
  ;
  ; In either case, if SEEDBANK_FILE is set, then the seed (however
  ; selected) will be saved in that file.  I'm not sure of the security
  ; implications of this---a malicious user can set either environment
  ; variable to cause problems, but if a malicious user can do that, I
  ; suspect you have other issues.
  ;--------------------------------------------------------------------*/
  
  seedbank = getenv("SEEDBANK_SEED");
  
  if (seedbank != NULL)
  {
    char          *ep;
    unsigned long  val;
    char           buffer[64];
    
    if (strcmp(seedbank,"LAST") == 0)
    {
      seedbank_file = getenv("SEEDBANK_FILE");
      if (seedbank_file != NULL)
      {
        FILE *fp = fopen(seedbank_file,"r");
        
        /*------------------------------------------------------------------
        ; most common error---file doesn't exist.  But there's not real easy
        ; way to check for that condition only.  So in some bizarre cases,
        ; not reporting an error may fail us here.  Well, if we had issues
        ; reading, we might have issues writing, so let's hope the check
        ; below finds an issue.
        ;
        ; But we still need a seed.  So let's skip ahead (GOTO WARNING!).
        ;------------------------------------------------------------------*/
        
        if (fp != NULL)
        {
          memset(buffer,0,sizeof(buffer));
          fgets(buffer,sizeof(buffer),fp);
          fclose(fp);
          seedbank = buffer;
        }
        else
          goto math_randomseed_normal;
      }
    }
    
    errno = 0;
    val   = strtoul(seedbank,&ep,10);
    if (errno != 0)
      return luaL_error(L,"SEEDBANK_SEED not a valid number");
    if (val > UINT_MAX)
      return luaL_error(L,"SEEDBANK_SEED range exceeded");
    
    seed = val;
  }
  else
  {
math_randomseed_normal:
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
  }
  
  seedbank_file = getenv("SEEDBANK_FILE");
  if (seedbank_file)
  {
    FILE *fp = fopen(seedbank_file,"w");
    if (fp != NULL)
    {
      fprintf(fp,"%u\n",seed);
      fclose(fp);
    }
    else
      return luaL_error(L,"SEEDBANK_FILE: %s",strerror(errno));
  }
  
  seedbank_log = getenv("SEEDBANK_LOG");
  if (seedbank_log != NULL)
  {
    FILE *fp = fopen(seedbank_log,"a");
    if (fp != NULL)
    {
      time_t now = time(NULL);
      char   buffer[20];
      
      strftime(buffer,sizeof(buffer),"%Y-%m-%dT%H:%M:%S",localtime(&now));
      fprintf(fp,"%s: %u\n",buffer,seed);
      fclose(fp);
    }
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
#if LUA_VERSION_NUM == 501
  luaL_register(L,"org.conman.math",reg_math);
#else
  luaL_newlib(L,reg_math);
#endif
  return 1;
}

/**************************************************************************/

