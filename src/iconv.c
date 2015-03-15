/*********************************************************************
*
* Copyright 2011 by Sean Conner.  All Rights Reserved.
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
* -------------------------------------------------------------------
*
* Use of this module is straightforward.  It returns a single function:
*
*       iconv(target_charset,source_charset)
*               -- type(target_charset) == 'string'
*               -- type(source_charset) == 'string'
*
* This returns a userdata can can be used to transform a string from
* one character set to another.
*
*       trans = iconv(...)
*       newstr,err,idx = trans("this is a test")
*
*       newstr = translated string, or nil if error
*       err    = 0 if okay, otherwise, the error (integer)
*       idx    = nil if okay, otherwise the index in the input where
*                the error happened.
*
* An example follows.
*

	iconv = require "org.conman.iconv"
	trans = iconv("utf-8","iso-8859-1") -- convert from ISO-8859-1
	x     = "This is \225 test"	-- string in ISO-8859-1
	y     = trans(x)		-- string now in UTF-8
	print(y)

*
*********************************************************************/

#include <stdbool.h>
#include <assert.h>
#include <errno.h>

#include <iconv.h>

#include <lua.h>
#include <lauxlib.h>

#if !defined(LUA_VERSION_NUM) || LUA_VERSION_NUM < 501
#  error You need to compile against Lua 5.1 or higher
#endif

#define TYPE_ICONV	"org.conman.iconv:iconv"

/**************************************************************************/

static int luaiconv_open(lua_State *L)
{
  const char *fromcode;
  const char *tocode;
  iconv_t    *pic;
  iconv_t     ic;
  
  tocode   = luaL_checkstring(L,1);
  fromcode = luaL_checkstring(L,2);
  ic       = iconv_open(tocode,fromcode);
  
  if (ic == (iconv_t)-1)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    lua_pushfstring(L,"%s:%s",tocode,fromcode);
    return 3;
  }
  
  pic  = lua_newuserdata(L,sizeof(iconv_t));
  *pic = ic;
  
  luaL_getmetatable(L,TYPE_ICONV);
  lua_setmetatable(L,-2);
  lua_pushinteger(L,0);  
  return 2;
}

/*************************************************************************/

static int luametaiconv___call(lua_State *L)
{
  const char *from;
  const char *ofrom;
  size_t      fsize;
  iconv_t    *pic;
  char        to[LUAL_BUFFERSIZE];
  size_t      tsize;
  char       *pto;
  luaL_Buffer buf;
  
  pic   = luaL_checkudata(L,1,TYPE_ICONV);
  from  = luaL_checklstring(L,2,&fsize);
  tsize = sizeof(to);
  pto   = to;
  ofrom = from;
  
  luaL_buffinit(L,&buf);
  
  while(fsize > 0)
  {
    if (iconv(*pic,(char **)&from,&fsize,&pto,&tsize) == (size_t)-1)
    {
      if (errno == E2BIG)
      {
        luaL_addlstring(&buf,to,sizeof(to) - tsize);
        pto   = to;
        tsize = sizeof(to);
      }
      else
      {
        iconv(*pic,NULL,NULL,NULL,NULL); /* reset state */
        lua_pushnil(L);
        lua_pushinteger(L,errno);
        lua_pushinteger(L,(int)(from - ofrom) + 1);
        return 3;
      }
    }
  }
  
  luaL_addlstring(&buf,to,sizeof(to) - tsize);
  luaL_pushresult(&buf);
  lua_pushinteger(L,0);
  return 2;
}  
      
/*************************************************************************/

static int luametaiconv___tostring(lua_State *L)
{
  lua_pushfstring(L,"iconv (%p)",lua_touserdata(L,1));
  return 1;
}

/*************************************************************************/

static int luametaiconv___gc(lua_State *L)
{
  iconv_close(*(iconv_t *)luaL_checkudata(L,1,TYPE_ICONV));
  return 0;
}

/*************************************************************************/

static const struct luaL_Reg reg_iconv_meta[] =
{
  { "__call"		, luametaiconv___call		} ,
  { "__tostring"	, luametaiconv___tostring	} ,
  { "__gc"		, luametaiconv___gc		} ,
  { NULL		, NULL				}
};

int luaopen_org_conman_iconv(lua_State *L)
{
  luaL_newmetatable(L,TYPE_ICONV);
#if LUA_VERSION_NUM == 501
  luaL_register(L,NULL,reg_iconv_meta);
#else
  luaL_setfuncs(L,reg_iconv_meta,0);
#endif
  lua_pushcfunction(L,luaiconv_open);
  return 1;
}

/**************************************************************************/

