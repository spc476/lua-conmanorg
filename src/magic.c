
#include <stdbool.h>
#include <assert.h>

#include <magic.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#define MAGIC_TYPE	"It's MAGIC!"

/**************************************************************************/

static const char *m_magic_options[] =
{
  "debug",
  "symlink",
  "compress",
  "devices",
  "mime",
  "continue",
  "check",
  "preserve_atime",
  "raw",
  "error",
  NULL
};

static const int m_magic_flags[] =
{
  MAGIC_DEBUG,
  MAGIC_SYMLINK,
  MAGIC_COMPRESS,
  MAGIC_DEVICES,
  MAGIC_MIME,
  MAGIC_CONTINUE,
  MAGIC_CHECK,
  MAGIC_PRESERVE_ATIME,
  MAGIC_RAW,
  MAGIC_ERROR
};

/**************************************************************************/

static int magiclua_open(lua_State *const L)
{
  magic_t *pm;
  int      top;
  int      flags;
  
  assert(L != NULL);
  
  flags = 0;
  top   = lua_gettop(L);
  
  if (top)
    for(int i = 1 ; i <= top ; i++)
      flags |= m_magic_flags[luaL_checkoption(L,i,NULL,m_magic_options)];
      
  pm = lua_newuserdata(L,sizeof(magic_t));
  
  luaL_getmetatable(L,MAGIC_TYPE);
  lua_setmetatable(L,-2);
  
  *pm = magic_open(flags);
  if (*pm == NULL)
    lua_pushnil(L);
  return 1;
}

/**************************************************************************/

static int magiclua_close(lua_State *const L)
{
  magic_t *pm;
  
  assert(L != NULL);
  
  pm = luaL_checkudata(L,1,MAGIC_TYPE);
  magic_close(*pm);
  *pm = NULL;
  return 0; 
}

/**************************************************************************/
static int magiclua_error(lua_State *const L)
{
  const char *err;
  magic_t    *pm;
  
  assert(L != NULL);
  
  pm  = luaL_checkudata(L,1,MAGIC_TYPE);
  err = magic_error(*pm);
  lua_pushstring(L,err);
  return 1;
}

/**************************************************************************/

static int magiclua_setflags(lua_State *const L)
{
  magic_t *pm;
  int      top;
  int      flags;
  int      rc;
  
  pm    = luaL_checkudata(L,1,MAGIC_TYPE);
  flags = 0;
  top   = lua_gettop(L);
  rc    = 0;
  
  if (top)
    for (int i = 2 ; i <= top ; i++)
      flags |= m_magic_flags[luaL_checkoption(L,i,NULL,m_magic_options)];
  
  rc = magic_setflags(*pm,flags);

  if (rc != 0)
  {
    lua_pushboolean(L,false);
    lua_pushinteger(L,rc);
    return 2;
  }
  
  lua_pushboolean(L,true);
  return 1;
}

/**************************************************************************/

static int magiclua_load(lua_State *const L)
{
  const char *filename;
  magic_t    *pm;
  int         rc;
  
  assert(L != NULL);
  
  pm       = luaL_checkudata(L,1,MAGIC_TYPE);
  filename = luaL_optstring(L,2,NULL);
  rc       = magic_load(*pm,filename);
  
  if (rc != 0)
  {
    lua_pushboolean(L,false);
    lua_pushinteger(L,rc);
    return 2;
  }
  
  lua_pushboolean(L,true);
  return 1;
}

/**************************************************************************/

static int magiclua_compile(lua_State *const L)
{
  const char *filename;
  magic_t    *pm;
  int         rc;
  
  assert(L != NULL);
  
  pm       = luaL_checkudata(L,1,MAGIC_TYPE);
  filename = luaL_optstring(L,2,NULL);
  rc       = magic_compile(*pm,filename);
  
  if (rc != 0)
  {
    lua_pushboolean(L,false);
    lua_pushinteger(L,rc);
    return 2;
  }
  
  lua_pushboolean(L,true);
  return 1;
}

/**************************************************************************/

static int magiclua_check(lua_State *const L)
{
  const char *filename;
  magic_t    *pm;
  int         rc;
  
  assert(L != NULL);
  
  pm       = luaL_checkudata(L,1,MAGIC_TYPE);
  filename = luaL_optstring(L,2,NULL);
  rc       = magic_check(*pm,filename);
  
  if (rc != 0)
  {
    lua_pushboolean(L,false);
    lua_pushinteger(L,rc);
    return 2;
  }
  
  lua_pushboolean(L,true);
  return 1;
}

/**************************************************************************/

static int magiclua_errno(lua_State *const L)
{
  magic_t *pm;
  
  assert(L != NULL);
  
  pm = luaL_checkudata(L,1,MAGIC_TYPE);
  lua_pushinteger(L,magic_errno(*pm));
  return 1;
}

/**************************************************************************/

static int magiclua___call(lua_State *const L)
{
  const char *filedata;
  magic_t    *pm;
  size_t      size;
  
  assert(L != NULL);
  
  pm       = luaL_checkudata(L,1,MAGIC_TYPE);
  filedata = luaL_checklstring(L,2,&size);
  
  if (lua_toboolean(L,3))
    lua_pushstring(L,magic_buffer(*pm,filedata,size));
  else
    lua_pushstring(L,magic_file(*pm,filedata));

  return 1;
}

/**************************************************************************/

static int magiclua___tostring(lua_State *const L)
{
  magic_t *pm;
  
  assert(L != NULL);
  pm = luaL_checkudata(L,1,MAGIC_TYPE);
  lua_pushfstring(L,"MAGIC (%p)",(void *)pm);
  return 1;
}

/**************************************************************************/

static int magiclua___gc(lua_State *const L)
{
  magic_t *pm;
  
  assert(L != NULL);
  pm = luaL_checkudata(L,1,MAGIC_TYPE);
  if (*pm != NULL)
  {
    magic_close(*pm);
    *pm = NULL;
  }
  
  return 0;
}

/**************************************************************************/

static const struct luaL_reg mmagic_reg[] =
{
  { "open"	, magiclua_open		} ,
  { "close"	, magiclua_close	} ,
  { "error"	, magiclua_error	} ,
  { "setflags"	, magiclua_setflags	} ,
  { "load"	, magiclua_load		} ,
  { "compile"	, magiclua_compile	} ,
  { "check"	, magiclua_check	} ,
  { "errno"	, magiclua_errno	} ,
  { NULL	, NULL			}
};

static const struct luaL_reg mmagic_reg_meta[] =
{
  { "__call"		, magiclua___call	} ,
  { "__tostring"	, magiclua___tostring	} ,
  { "__gc"		, magiclua___gc		} ,
  { NULL		, NULL			}
};

/*************************************************************************/

int luaopen_org_conman_fsys_magic(lua_State *const L)
{
  assert(L != NULL);
  
  luaL_newmetatable(L,MAGIC_TYPE);
  luaL_register(L,NULL,mmagic_reg_meta);
  
  luaL_register(L,"org.conman.fsys.magic",mmagic_reg);
  lua_pushvalue(L,-1);
  lua_setfield(L,-3,"__index");
  
  lua_pushliteral(L,"Copyright 2011 by Sean Conner.  All Rights Reserved.");
  lua_setfield(L,-2,"_COPYRIGHT");
  lua_pushliteral(L,"GNU-GPL 3");
  lua_setfield(L,-2,"_LICENSE");
  lua_pushliteral(L,"Interface to the magic file");
  lua_setfield(L,-2,"_DESCRIPTION");
  lua_pushliteral(L,"0.0.1");
  lua_setfield(L,-2,"_VERSION");
  
  return 1;
}

/*************************************************************************/
