/********************************************
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
* ==================================================================
*
*	base64 = require "org.conman.base64" {
*			last   = "+/",
*			pad    = "=",
*			len    = 76,
*			ignore = true
*		      }
*
*	x = base64:encode("blahblahblah")
*	y = base64:decode("blahblahblah")
*
* The default encoder/decoder (if base64() is called with no parameters) is
* what people normally expect of base64.  The parameters are there to handle
* the other dozen variants of base64 that are defined.  The parameters given
* above are the current default values.

	base64 = require "org.conman.base64"()
	msg = "This is a test"
	enc = base64:encode(msg)
	dec = base64:decode(enc)
	print(msg == dec)

*
*********************************************************************/

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include <lua.h>
#include <lauxlib.h>

#if !defined(LUA_VERSION_NUM) || LUA_VERSION_NUM < 501
#  error You need to compile against Lua 5.1 or higher
#endif

#define TYPE_BASE64	"org.conman.base64:base64"

typedef struct
{
  size_t len;
  char   pad;
  bool   ignore;
  char   transtable[64];
} base64__s;

/************************************************************************/

static const char *const mbase = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                  "abcdefghijklmnopqrstuvwxyz"
                                  "0123456789+/";

/**********************************************************************/

static int b64meta_encode(lua_State *L)
{
  const base64__s *b64;
  uint8_t         *data;
  size_t           size;
  size_t           len;
  luaL_Buffer      b;
  uint8_t          A,B,C,D;
  
  b64  = luaL_checkudata(L,1,TYPE_BASE64);
  data = (uint8_t *)luaL_checklstring(L,2,&size);
  len  = 0;
  luaL_buffinit(L,&b);
  
  while(true)
  {
    switch(size)
    {
      case 0:
           luaL_pushresult(&b);
           return 1;
           
      case 1:
           A = (data[0] >> 2);
           B = (data[0] << 4) & 0x30;
           assert(A < 64);
           assert(B < 64);
      
           luaL_addchar(&b,b64->transtable[A]);
           luaL_addchar(&b,b64->transtable[B]);
           if (b64->pad)
           {
             luaL_addchar(&b,b64->pad);
             luaL_addchar(&b,b64->pad);
           }
           luaL_pushresult(&b);
           return 1;
      
      case 2:
           A =  (data[0] >> 2);
           B = ((data[0] << 4) & 0x30) | ((data[1] >> 4) );
           C =  (data[1] << 2) & 0x3C;
           
           assert(A < 64);
           assert(B < 64);
           assert(C < 64);
           
           luaL_addchar(&b,b64->transtable[A]);
           luaL_addchar(&b,b64->transtable[B]);
           luaL_addchar(&b,b64->transtable[C]);
           if (b64->pad)
             luaL_addchar(&b,b64->pad);       
           luaL_pushresult(&b);
           return 1;
      
      default:
           A =  (data[0] >> 2) ;
           B = ((data[0] << 4) & 0x30) | ((data[1] >> 4) );
           C = ((data[1] << 2) & 0x3C) | ((data[2] >> 6) );
           D =   data[2]       & 0x3F;
           
           assert(A < 64);
           assert(B < 64);
           assert(C < 64);
           assert(D < 64);
           
           luaL_addchar(&b,b64->transtable[A]);
           luaL_addchar(&b,b64->transtable[B]);
           luaL_addchar(&b,b64->transtable[C]);
           luaL_addchar(&b,b64->transtable[D]);
           len  += 4;
           size -= 3;
           data += 3;
           
           if (len >= b64->len)
           {
             luaL_addchar(&b,'\n');
             len = 0;
           }
           break;
    }
  }
}

/**********************************************************************/

static bool Ib64_readout(
	const base64__s  *b64,
	uint8_t          *pr,
	size_t           *skip,
	const char      **pdata
)
{
  const char *data = *pdata;
  char       *p;
  
  while(true)
  {
    if (*data == '\0')
    {
      (*skip)++;
      *pr = 0;
      return true;
    }
  
    if (*data == b64->pad)
    {
      (*skip)++;
      *pdata = data + 1;
      *pr    = b64->pad;
      return true;
    }
  
    p = strchr(b64->transtable,*data);
    if (p)
    {
      *pr    = (p - b64->transtable);
      *pdata = data + 1;
      return true;
    }
    
    if (!b64->ignore)
      return false;
  }
}

/**********************************************************************/

static bool Ib64_readout4(
	const base64__s  *b64,
	uint8_t          *pr,
	size_t           *skip,
	const char      **pdata
)
{
  size_t i;
  
  for (i = 0 ; i < 4 ; i++)
    if (!Ib64_readout(b64,&pr[i],skip,pdata))
      return false;
  return true;
}

/**********************************************************************/

static int b64meta_decode(lua_State *L)
{
  const base64__s *b64;
  const char       *data;
  uint8_t           buf[4];
  luaL_Buffer       b;
  size_t            skip;
  
  b64  = luaL_checkudata(L,1,TYPE_BASE64);
  data = luaL_checkstring(L,2);
  skip = 0;
 
  luaL_buffinit(L,&b);

  while(*data)
  {
    if (!Ib64_readout4(b64,buf,&skip,&data))
      return luaL_error(L,"invalid character '%c'",*data);
      
    luaL_addchar(&b,(buf[0] << 2) | (buf[1] >> 4));
    if (skip < 2) luaL_addchar(&b,(buf[1] << 4) | (buf[2] >> 2));
    if( skip < 1) luaL_addchar(&b,(buf[2] << 6) | (buf[3]     ));
  }
  
  luaL_pushresult(&b);
  return 1;
}

/************************************************************************/

static int b64meta___tostring(lua_State *L)
{
  lua_pushfstring(L,"base64 (%p)",lua_touserdata(L,1));
  return 1;
}

/************************************************************************/

static int b64lua(lua_State *L)
{
  base64__s *b64;
  
  lua_settop(L,1);

  b64         = lua_newuserdata(L,sizeof(base64__s));
  b64->len    = 76;
  b64->pad    = '=';
  b64->ignore = true;
  memcpy(b64->transtable,mbase,64);
  
  if (!lua_isnil(L,1))
  {
    luaL_checktype(L,1,LUA_TTABLE);
    lua_getfield(L,1,"last");
    if (!lua_isnil(L,-1))
    {
      size_t      size;
      const char *last = luaL_checklstring(L,-1,&size);
      if (size != 2) luaL_error(L,"need two characters only");
      b64->transtable[62] = last[0];
      b64->transtable[63] = last[1];
    }
    
    lua_getfield(L,1,"pad");
    b64->pad = *luaL_optstring(L,-1,"=");
    
    lua_getfield(L,1,"len");
    b64->len = luaL_optinteger(L,-1,64);
    
    lua_getfield(L,1,"ignore");
    if (lua_isboolean(L,-1))
      b64->ignore = lua_toboolean(L,-1);
    
    lua_pop(L,4);
  }
  
  luaL_getmetatable(L,TYPE_BASE64);
  lua_setmetatable(L,-2);
  return 1;
}

/**********************************************************************/

static const struct luaL_Reg mb64_meta[] =
{
  { "encode"		, b64meta_encode 	} ,
  { "decode"		, b64meta_decode 	} ,
  { "__tostring"	, b64meta___tostring	} ,
  { NULL		, NULL 			}
};

int luaopen_org_conman_base64(lua_State *L)
{
  luaL_newmetatable(L,TYPE_BASE64);
#if LUA_VERSION_NUM == 501
  luaL_register(L,NULL,mb64_meta);
#else
  luaL_setfuncs(L,mb64_meta,0);
#endif
  lua_pushvalue(L,-1);
  lua_setfield(L,-2,"__index");
  
  lua_pushcfunction(L,b64lua);
  return 1;
}

/**********************************************************************/
