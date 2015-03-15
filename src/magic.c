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
* ==================================================================
*
* Module:	org.conman.fsys.magic
*
* Desc:		Wrapper for the file magic library
*
* Example:
*
		magic = require "org.conman.fsys.magic"
		print(magic ".")
		magic:flags('mime')
		print(magic ".")
*
*************************************************************************/

#include <stdbool.h>
#include <assert.h>
#include <errno.h>

#include <magic.h>

#include <lua.h>
#include <lauxlib.h>

#if !defined(LUA_VERSION_NUM) || LUA_VERSION_NUM < 501
#  error You need to compile against Lua 5.1 or higher
#endif

#define TYPE_MAGIC	"org.conman.fsys.magic:It's Magic!"

/**************************************************************************
*
* Usage:	filetype = magic(filedata[,isbuffer])
*
* Desc:		Return filetype
*
* Input:	filedata (string) the filename, or contents of a file
*		isbuffer (boolean/optional) true if filedata is contents of
*				| a file, otherwise, nil or false
*
* Return:	filetype (string) description of file contents
*
**************************************************************************/

static int magicmeta___call(lua_State *const L)
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

static int magicmeta___tostring(lua_State *const L)
{
  lua_pushfstring(L,"magic (%p)",lua_touserdata(L,1));
  return 1;
}

/**************************************************************************/

static int magicmeta___gc(lua_State *const L)
{
  magic_close(*(magic_t *)luaL_checkudata(L,1,TYPE_MAGIC));
  return 0;
}

/**************************************************************************
*
* Usage:	error = magicdb:error()
*
* Desc:		Return the last error
*
* Return:	error (string) error message
*
***************************************************************************/

static int magicmeta_error(lua_State *const L)
{
  lua_pushstring(L,magic_error(*(magic_t *)luaL_checkudata(L,1,TYPE_MAGIC)));
  return 1;
}

/**************************************************************************
*
* Usage:	okay,err = magicdb:setflags([flag[,flag...]])
*
* Desc:		Set flags on an open file magic database
*
* Input:	flag (enum(flag)) flags to set
*
* Return:	okay (boolean) true if successful, false on error
*		err (integer) system error, 0 if success
*
*************************************************************************/

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

/*------------------------------------------------------------------------*/

static int magicmeta_flags(lua_State *const L)
{
  magic_t *pm;
  int      flags;
  
  pm    = luaL_checkudata(L,1,TYPE_MAGIC);
  flags = 0;
  
  for (int top = lua_gettop(L) , i = 2 ; i <= top ; i++)
    flags |= m_magic_flags[luaL_checkoption(L,i,NULL,m_magic_options)];
  
  int rc = magic_setflags(*pm,flags);
  if (rc == 0)
    rc = magic_load(*pm,NULL);
  
  lua_pushboolean(L,rc == 0);
  lua_pushinteger(L,rc);
  return 2;
}

/**************************************************************************
*
* Usage:	okay,err = magicdb:load([dbpath])
*
* Desc:		Load the file magic databases
*
* Input:	dbpath (string/optional) colon delimeted list of databse files
*
* Return:	okay (boolean) true if success, false if failre
*		err (integer) system error, 0 if success
*
***************************************************************************/

static int magicmeta_load(lua_State *const L)
{
  int rc = magic_load(
            *(magic_t *)luaL_checkudata(L,1,TYPE_MAGIC),
            luaL_optstring(L,2,NULL)
          );
  
  lua_pushboolean(L,rc == 0);
  lua_pushinteger(L,rc);
  return 2;
}

/**************************************************************************
*
* Usage:	okay,err = magic:compile([dbpath])
*
* Desc:		Compile the colon delimeted list of database files.  
*
* Input:	dbpath (string/optional) colon delimeted list of db files
*
* Return:	okay (boolean) true if okay, false if error
*		err (integer) system errir, 0 on success
*
**************************************************************************/

static int magicmeta_compile(lua_State *const L)
{
  int rc = magic_compile(
            *(magic_t *)luaL_checkudata(L,1,TYPE_MAGIC),
            luaL_optstring(L,2,NULL)
          );
  
  lua_pushboolean(L,rc == 0);
  lua_pushinteger(L,rc);
  return 2;
}

/**************************************************************************
*
* Usage:	okay,err = magic:check([dbpath])
*
* Desc:		Check validity of entries in the colon delimeted list of
*		database files.
*
* Input:	dbpath (string/optional) colon delimeted list of databse files
*
* Return:	okay (boolean) true if okay, false if error
*		err (integer) system error, 0 on success
*
***************************************************************************/

static int magicmeta_check(lua_State *const L)
{
  int rc = magic_check(
              *(magic_t *)luaL_checkudata(L,1,TYPE_MAGIC),
              luaL_optstring(L,2,NULL)
            );
  
  lua_pushboolean(L,rc == 0);
  lua_pushinteger(L,rc);
  return 2;
}

/**************************************************************************
*
* Usage:	error = magic:errno()
*
* Desc:		Return the error from the last operation
*
* Return:	error (integer) system error, 0 if no error
*
***************************************************************************/

static int magicmeta_errno(lua_State *const L)
{
  lua_pushinteger(L,magic_errno(*(magic_t *)luaL_checkudata(L,1,TYPE_MAGIC)));
  return 1;
}

/**************************************************************************/

static const struct luaL_Reg mmagic_reg_meta[] =
{
  { "__call"		, magicmeta___call	} ,
  { "__tostring"	, magicmeta___tostring	} ,
  { "__gc"		, magicmeta___gc	} ,
  { "close"		, magicmeta___gc	} ,
  { "error"		, magicmeta_error	} ,
  { "flags"		, magicmeta_flags	} ,
  { "load"		, magicmeta_load	} ,
  { "compile"		, magicmeta_compile	} ,
  { "check"		, magicmeta_check	} ,
  { "errno"		, magicmeta_errno	} ,
  { NULL		, NULL			}
};

/*************************************************************************/

int luaopen_org_conman_fsys_magic(lua_State *const L)
{
  magic_t  m;
  magic_t *pm;
  
  assert(L != NULL);
  
  m = magic_open(0);
  
  if (m == NULL)
    return 0;

  if (magic_load(m,NULL) != 0)
  {
    magic_close(m);
    return 0;
  }
  
  luaL_newmetatable(L,TYPE_MAGIC);
#if LUA_VERSION_NUM == 501
  luaL_register(L,NULL,mmagic_reg_meta);
#else
  luaL_setfuncs(L,mmagic_reg_meta,0);
#endif
  lua_pushvalue(L,-1);
  lua_setfield(L,-2,"__index");
  
  pm  = lua_newuserdata(L,sizeof(magic_t));
  *pm = m;
    
  lua_pushvalue(L,-2);
  lua_setmetatable(L,-2);
  return 1;
}

/*************************************************************************/
