
#include <limits.h>
#include <stddef.h>
#include <stdbool.h>
#include <errno.h>

#include <openssl/evp.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#define HASH_TYPE	"org.conman.hash"

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
  luaL_getmetatable(L,HASH_TYPE);
  lua_setmetatable(L,-2);
  return 1;
}

/************************************************************************/

static int hashlua_update(lua_State *const L)
{
  EVP_MD_CTX *ctx;
  const char *data;
  size_t      size;
  
  ctx  = luaL_checkudata(L,1,HASH_TYPE);
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
  
  ctx      = luaL_checkudata(L,1,HASH_TYPE);
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
  EVP_MD_CTX *ctx;
  
  ctx = luaL_checkudata(L,1,HASH_TYPE);
  lua_pushfstring(L,"HASH:%p",(void *)ctx);
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
  
  luaL_newmetatable(L,HASH_TYPE);
  luaL_register(L,NULL,hashlua_meta);
  lua_pushvalue(L,-1);
  lua_setfield(L,-2,"__index");
  
  luaL_register(L,"org.conman.hash",hashlua);
  
  lua_pushliteral(L,"Copyright 2011 by Sean Conner.  All Rights Reserved.");
  lua_setfield(L,-2,"_COPYRIGHT");
  
  lua_pushliteral(L,"GNU-GPL 3");
  lua_setfield(L,-2,"_LICENSE");
  
  lua_pushliteral(L,"0.0.1");
  lua_setfield(L,-2,"_VERSION");
  
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
