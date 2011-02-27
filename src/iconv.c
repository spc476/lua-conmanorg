/*********************************************************************
*
* Copyright 2011 by Sean Conner.  All Rights Reserved.
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

#include <stdbool.h>
#include <assert.h>
#include <errno.h>

#include <iconv.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#define ICONV_TYPE	"ICONV"

/**************************************************************************/

static int luaiconv_open(lua_State *L)
{
  const char *fromcode;
  const char *tocode;
  iconv_t    *pic;
  iconv_t     ic;
  
  fromcode = luaL_checkstring(L,1);
  tocode   = luaL_checkstring(L,2);
  ic       = iconv_open(tocode,fromcode);
  
  if (ic == (iconv_t)-1)
  {
    int err = errno;
    
    lua_pushnil(L);
    lua_pushinteger(L,err);
    lua_pushfstring(L,"%s:%s",tocode,fromcode);
    return 3;
  }
  
  pic  = lua_newuserdata(L,sizeof(iconv_t));

  luaL_getmetatable(L,ICONV_TYPE);
  lua_setmetatable(L,-2);
    
  *pic = ic;
  return 1;  
}

/*************************************************************************/

static int luametaiconv_iconv(lua_State *L)
{
  const char *from;
  size_t      fsize;
  iconv_t    *pic;
  
  pic  = luaL_checkudata(L,1,ICONV_TYPE);
  from = luaL_checklstring(L,2,&fsize);
  
  size_t  rc;
  size_t  tsize    = fsize * 8;	/* a reasonable guess here */
  char    to[tsize];
  char   *pto = to;
  
  rc = iconv(*pic,(char **)&from,&fsize,&pto,&tsize);
  if (rc == (size_t)-1)
  {
    int err = errno;
    
    lua_pushnil(L);
    lua_pushinteger(L,err);
    
    if (err == E2BIG)
    {
      lua_pushnil(L);
      lua_pushnil(L);
    }
    else
    {
      lua_pushlstring(L,from,fsize);
      lua_pushlstring(L,to,sizeof(to) - tsize);
    }
    
    iconv(*pic,NULL,NULL,NULL,NULL);	/* reset conversion state */  
    return 4;
  }
  
  lua_pushlstring(L,to,sizeof(to) - tsize);
  return 1;
}

/*************************************************************************/

static int luametaiconv_close(lua_State *L)
{
  iconv_t *pic;
  
  pic = luaL_checkudata(L,1,ICONV_TYPE);
  
  assert(*pic != (iconv_t)-1);
  
  if (iconv_close(*pic) < 0)
  {
    int err = errno;
    
    lua_pushboolean(L,false);
    lua_pushinteger(L,err);
    lua_pushliteral(L,"how did this happen?");
    return 3;
  }
  
  *pic = (iconv_t)-1;
  lua_pushboolean(L,true);
  return 1;   
}

/*************************************************************************/

static int luametaiconv___tostring(lua_State *L)
{
  iconv_t *pic;
  
  pic = luaL_checkudata(L,1,ICONV_TYPE);

  if (*pic == (iconv_t)-1)
    lua_pushfstring(L,"iconv (closed)");
  else
    lua_pushfstring(L,"iconv (%p)",pic);

  return 1;
}

/*************************************************************************/

static int luametaiconv___gc(lua_State *L)
{
  iconv_t *pic;
  
  pic = luaL_checkudata(L,1,ICONV_TYPE);
  if (*pic != (iconv_t)-1)
  {
    iconv_close(*pic);
    *pic = (iconv_t)-1;
  }
  return 0;
}

/*************************************************************************/

static const struct luaL_reg reg_iconv[] = 
{
  { "open"	, luaiconv_open		} ,
  { NULL	, NULL			} 
};

static const struct luaL_reg reg_iconv_meta[] =
{
  { "iconv"		, luametaiconv_iconv		} ,
  { "close"		, luametaiconv_close		} ,
  { "__tostring"	, luametaiconv___tostring	} ,
  { "__gc"		, luametaiconv___gc		} ,
  { NULL		, NULL				}
};

int luaopen_org_conman_iconv(lua_State *L)
{
  luaL_newmetatable(L,ICONV_TYPE);
  luaL_register(L,NULL,reg_iconv_meta);
  lua_pushvalue(L,-1);
  lua_setfield(L,-2,"__index");
  
  luaL_register(L,"org.conman.iconv",reg_iconv);
  
  lua_pushliteral(L,"Copyright 2011 by Sean Conner.  All Rights Reserved.");
  lua_setfield(L,-2,"_COPYRIGHT");
  
  lua_pushliteral(L,"GNU-GPL 3");
  lua_setfield(L,-2,"_LICENSE");
  
  lua_pushliteral(L,"Interface to the Iconv codeset conversion routines");
  lua_setfield(L,-2,"_DESCRIPTION");
  
  lua_pushliteral(L,"0.1.0");
  lua_setfield(L,-2,"_VERSION");
  
  return 1;
}

/**************************************************************************/

