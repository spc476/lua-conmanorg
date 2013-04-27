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

#include <stdbool.h>
#include <assert.h>

#include <magic.h>

#include <lua.h>
#include <lauxlib.h>

#define TYPE_MAGIC	"org.conman.magic:It's Magic!"

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
  
  luaL_getmetatable(L,TYPE_MAGIC);
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
  
  pm = luaL_checkudata(L,1,TYPE_MAGIC);
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
  
  pm  = luaL_checkudata(L,1,TYPE_MAGIC);
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
  
  pm    = luaL_checkudata(L,1,TYPE_MAGIC);
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
  
  pm       = luaL_checkudata(L,1,TYPE_MAGIC);
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
  
  pm       = luaL_checkudata(L,1,TYPE_MAGIC);
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
  
  pm       = luaL_checkudata(L,1,TYPE_MAGIC);
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
  
  pm = luaL_checkudata(L,1,TYPE_MAGIC);
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
  
  pm       = luaL_checkudata(L,1,TYPE_MAGIC);
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
  pm = luaL_checkudata(L,1,TYPE_MAGIC);
  lua_pushfstring(L,"MAGIC (%p)",(void *)pm);
  return 1;
}

/**************************************************************************/

static int magiclua___gc(lua_State *const L)
{
  magic_t *pm;
  
  assert(L != NULL);
  pm = luaL_checkudata(L,1,TYPE_MAGIC);
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
  
  luaL_newmetatable(L,TYPE_MAGIC);
  luaL_register(L,NULL,mmagic_reg_meta);
  
  luaL_register(L,"org.conman.fsys.magic",mmagic_reg);
  lua_pushvalue(L,-1);
  lua_setfield(L,-3,"__index");
  
  return 1;
}

/*************************************************************************/
