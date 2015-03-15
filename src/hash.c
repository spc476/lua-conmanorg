/***************************************************************************
*
* Copyright 2011 by Sean Conner.
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
*************************************************************************/

#include <limits.h>
#include <stddef.h>
#include <stdbool.h>
#include <errno.h>

#include <openssl/evp.h>

#include <lua.h>
#include <lauxlib.h>

#if !defined(LUA_VERSION_NUM) || LUA_VERSION_NUM < 501
#  error You need to compile against Lua 5.1 or higher
#endif

#define TYPE_HASH	"org.conman.hash:hash"

/************************************************************************/

static int hash_hexa(
	lua_State  *const restrict L,
	const char *restrict       data,
	size_t                     size
)
{
  luaL_Buffer buf;
  char        hex[3];
  
  luaL_buffinit(L,&buf);
  while(size--)
  {
    snprintf(hex,3,"%02X",(unsigned char)*data++);
    luaL_addlstring(&buf,hex,2);
  }
  luaL_pushresult(&buf);
  return 1;
}

/************************************************************************/

static int hashlua_init(lua_State *const L)
{
  const EVP_MD *m;
  EVP_MD_CTX   *ctx;
  const char   *alg;
  
  alg = luaL_optstring(L,1,"md5");
  m   = EVP_get_digestbyname(alg);
  if (m == NULL)
  {
    lua_pushnil(L);
    return 1;
  }
  
  ctx = lua_newuserdata(L,sizeof(EVP_MD_CTX));
  EVP_DigestInit(ctx,m);
  luaL_getmetatable(L,TYPE_HASH);
  lua_setmetatable(L,-2);
  return 1;
}

/************************************************************************/

static int hashlua_update(lua_State *const L)
{
  EVP_MD_CTX *ctx;
  const char *data;
  size_t      size;
  
  ctx  = luaL_checkudata(L,1,TYPE_HASH);
  data = luaL_checklstring(L,2,&size);
  
  if (size > INT_MAX)
  {
    lua_pushboolean(L,false);
    lua_pushinteger(L,EFBIG);
    return 2;
  }
  
  EVP_DigestUpdate(ctx,data,size);
  lua_pushboolean(L,true);
  return 1;
}

/************************************************************************/

static int hashlua_final(lua_State *const L)
{
  EVP_MD_CTX    *ctx;
  unsigned char  hash[EVP_MAX_MD_SIZE];
  unsigned int   hashsize;
  
  ctx      = luaL_checkudata(L,1,TYPE_HASH);
  hashsize = sizeof(hash);
  
  EVP_DigestFinal(ctx,hash,&hashsize);
  lua_pushlstring(L,(char *)hash,hashsize);
  return 1;
}

/************************************************************************/

static int hashlua_finalhexa(lua_State *const L)
{
  const char *data;
  size_t      size;
  
  hashlua_final(L);
  data = lua_tolstring(L,-1,&size);
  return hash_hexa(L,data,size);
}

/************************************************************************/

static int hashlua_sum(lua_State *const L)
{
  const EVP_MD  *m;
  EVP_MD_CTX     ctx;
  const char    *data;
  size_t         size;
  const char    *alg;
  unsigned char  hash[EVP_MAX_MD_SIZE];
  unsigned int   hashsize;
  
  data = luaL_checklstring(L,1,&size);
  
  if (size > INT_MAX)
  {
    lua_pushnil(L);
    lua_pushinteger(L,EFBIG);
    return 2;
  }
  
  alg  = luaL_optstring(L,2,"md5");
  m    = EVP_get_digestbyname(alg);
  if (m == NULL)
  {
    lua_pushnil(L);
    lua_pushinteger(L,EINVAL);
    return 2;
  }
  
  hashsize = sizeof(hash);
  EVP_DigestInit(&ctx,m);
  EVP_DigestUpdate(&ctx,data,size);
  EVP_DigestFinal(&ctx,hash,&hashsize);
  lua_pushlstring(L,(char *)hash,hashsize);
  return 1;
}

/*****************************************************************/

static int hashlua_hexa(lua_State *const L)
{
  const char  *data;
  size_t       size;
  
  data = luaL_checklstring(L,1,&size);
  return hash_hexa(L,data,size);
}

/************************************************************************/

static int hashlua_sumhexa(lua_State *const L)
{
  const char *data;
  size_t      size;
  int         rc;
  
  rc = hashlua_sum(L);
  if (!lua_isstring(L,-1))
    return rc;
  
  data = lua_tolstring(L,-1,&size);
  return hash_hexa(L,data,size);  
}

/************************************************************************/

static int hashlua___tostring(lua_State *const L)
{
  lua_pushfstring(L,"hash (%p)",lua_touserdata(L,1));
  return 1;
}

/***********************************************************************/

static const struct luaL_Reg hashlua[] =
{
  { "new"	, hashlua_init		} ,
  { "update"	, hashlua_update	} ,
  { "final"	, hashlua_final		} ,
  { "sum"	, hashlua_sum		} ,
  { "hexa"	, hashlua_hexa		} ,
  { "sumhexa"	, hashlua_sumhexa	} ,
  { NULL	, NULL			}
};

static const struct luaL_Reg hashlua_meta[] =
{
  { "update"		, hashlua_update	} ,
  { "final"		, hashlua_final		} ,
  { "finalhexa"		, hashlua_finalhexa	} ,
  { "__tostring"	, hashlua___tostring 	} ,
  { NULL		, NULL			}
};

int luaopen_org_conman_hash(lua_State *const L)
{
  OpenSSL_add_all_digests();
  
  luaL_newmetatable(L,TYPE_HASH);
#if LUA_VERSION_NUM == 501
  luaL_register(L,NULL,hashlua_meta);
#else
  luaL_setfuncs(L,hashlua_meta,0);
#endif

  lua_pushvalue(L,-1);
  lua_setfield(L,-2,"__index");

#if LUA_VERSION_NUM == 501  
  luaL_register(L,"org.conman.hash",hashlua);
#else
  luaL_newlib(L,hashlua);
#endif

  lua_createtable(L,0,7);
  lua_pushinteger(L,128);
  lua_setfield(L,-2,"md2");
  lua_pushinteger(L,128);
  lua_setfield(L,-2,"md4");
  lua_pushinteger(L,128);
  lua_setfield(L,-2,"md5");
  lua_pushinteger(L,128);
  lua_setfield(L,-2,"mdc2");
  lua_pushinteger(L,160);
  lua_setfield(L,-2,"sha1");
  lua_pushinteger(L,160);
  lua_setfield(L,-2,"dss1");
  lua_pushinteger(L,160);
  lua_setfield(L,-2,"ripemd");
  lua_setfield(L,-2,"hashes");
  
  return 1;
}

/*************************************************************************/
