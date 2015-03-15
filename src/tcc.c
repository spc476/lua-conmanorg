/***************************************************************************
*
* Copyright 2012 by Sean Conner.
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
* =======================================================================
*
* This module requires a customised TCC 0.9.25.  By default, TCC only
* provides a static library.  For this use, it needs to be compiled into
* a shared library.  That's easy enough---just copy the command line used
* to compile libtcc.a and make a few changes to make it libtcc.so.  
*
* Unfortunately, the resulting shared object doesn't work.  To get it to
* work, all functions not mentioned in libtcc.h need to converted to
* 'static', then compiled into a shared library.  Don't worry---converting
* all functions not mentioned in libtcc.h to 'static' won't affect the
* compilation of the normal library, but yes, it is a chore to change
* all the required functions.
*
* Also, the warning about ISO C forbidding assignment between a function
* pointer and void * is expected.  So don't freak out about it.
*
* You have been warned.
*
*************************************************************************/

#ifndef __GNUC__
#  define __attribute__(x)
#endif

#include <stdlib.h>
#include <errno.h>

#include <syslog.h>
#include <libtcc.h>

#include <lua.h>
#include <lauxlib.h>

#if !defined(LUA_VERSION_NUM) || LUA_VERSION_NUM < 501
#  error You need to compile against Lua 5.1 or higher
#endif

#define TYPE_TCC	"org.conman.tcc:TCC"

/**************************************************************************/

static int	tcclua_new		(lua_State *const);
static int	tcclua_dispose		(lua_State *const);

static int	tcclua___tostring	(lua_State *const);
static int	tcclua___gc		(lua_State *const);
static int	tcclua_enable_debug	(lua_State *const);
static int	tcclua_set_warning	(lua_State *const);
static int	tcclua_include_path	(lua_State *const);
static int	tcclua_sysinclude_path	(lua_State *const);
static int	tcclua_define		(lua_State *const);
static int	tcclua_undef		(lua_State *const);
static int	tcclua_compile		(lua_State *const);
static int	tcclua_add_file		(lua_State *const);
static int	tcclua_add_string	(lua_State *const);
static int	tcclua_output_type	(lua_State *const);
static int	tcclua_library_path	(lua_State *const);
static int	tcclua_library		(lua_State *const);
static int	tcclua_set_symbol	(lua_State *const);
static int	tcclua_output		(lua_State *const);
static int	tcclua_run		(lua_State *const);
static int	tcclua_relocate		(lua_State *const);
static int	tcclua_get_symbol	(lua_State *const);
static int	tcclua_lib_path		(lua_State *const);

static void	error_lua_handler	(void *,const char *);
static void	error_c_handler		(void *,const char *);

/************************************************************************/

static const struct luaL_Reg mtcc_reg[] =
{
  { "new"	, tcclua_new		} ,
  { "dispose"	, tcclua_dispose	} ,
  { NULL	, NULL			}
};

static const struct luaL_Reg mtcc_meta[] =
{
  { "__tostring"	, tcclua___tostring 		} ,
  { "__gc"		, tcclua___gc			} ,
  { "enable_debug"	, tcclua_enable_debug		} ,
  { "set_warning"	, tcclua_set_warning		} ,
  
  { "include_path"	, tcclua_include_path		} ,
  { "sysinclude_path"	, tcclua_sysinclude_path	} ,
  { "define"		, tcclua_define			} ,
  { "undef"		, tcclua_undef			} ,
  
  { "compile"		, tcclua_compile		} ,
  { "add_file"		, tcclua_add_file		} ,
  { "add_string"	, tcclua_add_string		} ,

  { "output_type"	, tcclua_output_type		} ,
  { "library_path"	, tcclua_library_path		} ,
  { "library"		, tcclua_library		} ,
  { "set_symbol"	, tcclua_set_symbol		} ,
  { "output"		, tcclua_output			} ,
  { "run"		, tcclua_run			} ,
  { "relocate"		, tcclua_relocate		} ,
  { "get_symbol"	, tcclua_get_symbol		} ,
  { "lib_path"		, tcclua_lib_path		} ,
  { NULL		, NULL				}
};

static const char *const moutput_type[] =
{
  "memory",
  "exec",
  "dll",
  "obj",
  "cpp",
  NULL
};

/**************************************************************************/

int luaopen_org_conman_tcc(lua_State *const L)
{
  luaL_newmetatable(L,TYPE_TCC);
#if LUA_VERSION_NUM == 501
  luaL_register(L,NULL,mtcc_meta);
#else
  luaL_setfuncs(L,mtcc_meta,0);
#endif

  lua_pushvalue(L,-1);
  lua_setfield(L,-2,"__index");

#if LUA_VERSION_NUM == 501  
  luaL_register(L,"org.conman.tcc",mtcc_reg);
#else
  luaL_newlib(L,mtcc_reg);
#endif
  return 1;
}

/**************************************************************************/

static int tcclua_new(lua_State *const L)
{
  TCCState  *tcc;
  TCCState **ptcc;
  
  tcc = tcc_new();
  if (tcc == NULL)
  {
    lua_pushnil(L);
    lua_pushinteger(L,ENOMEM);
    return 2;
  }
  
  tcc_set_error_func(tcc,NULL,error_c_handler);
  ptcc = lua_newuserdata(L,sizeof(TCCState **));
  *ptcc = tcc;
  
  luaL_getmetatable(L,TYPE_TCC);
  lua_setmetatable(L,-2);
  
  return 1;
}

/**************************************************************************/

static int tcclua_dispose(lua_State *const L)
{
  if (!lua_islightuserdata(L,1))
    return luaL_error(L,"require a light user data");
  
  free(lua_touserdata(L,1));
  return 0;
}

/***********************************************************************/

static int tcclua___tostring(lua_State *const L)
{
  lua_pushfstring(L,"tcc (%p)",lua_touserdata(L,1));
  return 1;
}

/**************************************************************************/

static int tcclua___gc(lua_State *const L)
{
  syslog(LOG_DEBUG,"garbage collect TCC");
  tcc_delete(*(TCCState **)luaL_checkudata(L,1,TYPE_TCC));
  return 0;
}

/**************************************************************************/

static int tcclua_enable_debug(lua_State *const L)
{
  tcc_enable_debug(*(TCCState **)luaL_checkudata(L,1,TYPE_TCC));
  return 0;
}

/**************************************************************************/

static int tcclua_set_warning(lua_State *const L)
{
  lua_pushinteger(
        L,
        tcc_set_warning(
                *(TCCState **)luaL_checkudata(L,1,TYPE_TCC),
                luaL_checkstring(L,2),
                luaL_checkinteger(L,3)
        )
  );
  return 1;
}

/**************************************************************************/

static int tcclua_include_path(lua_State *const L)
{
  lua_pushinteger(
       L,
       tcc_add_include_path(
               *(TCCState **)luaL_checkudata(L,1,TYPE_TCC),
               luaL_checkstring(L,2)
       )
  );
  return 1;
}

/**************************************************************************/

static int tcclua_sysinclude_path(lua_State *const L)
{
  lua_pushinteger(
        L,
        tcc_add_sysinclude_path(
                *(TCCState **)luaL_checkudata(L,1,TYPE_TCC),
                luaL_checkstring(L,2)
        )
  );
  return 1;
}

/**************************************************************************/

static int tcclua_define(lua_State *const L)
{
  tcc_define_symbol(
          *(TCCState **)luaL_checkudata(L,1,TYPE_TCC),
          luaL_checkstring(L,2),
          luaL_checkstring(L,3)
  );
  return 0;
}

/**************************************************************************/

static int tcclua_undef(lua_State *const L)
{
  tcc_undefine_symbol(
          *(TCCState **)luaL_checkudata(L,1,TYPE_TCC),
          luaL_checkstring(L,2)
  );
  return 0;
}

/**************************************************************************/

struct error_data
{
  TCCState  *tcc;
  lua_State *L;
};

static int tcclua_compile(lua_State *const L)
{
  TCCState          **tcc  = luaL_checkudata(L,1,TYPE_TCC);
  const char         *text = luaL_checkstring(L,2);
  struct error_data   errdata;
  
  if (lua_isfunction(L,4))
  {
    errdata.tcc = *tcc;
    errdata.L   = L;
    
    lua_pushlightuserdata(L,*tcc);
    lua_pushvalue(L,4);
    lua_settable(L,LUA_REGISTRYINDEX);
    tcc_set_error_func(*tcc,&errdata,error_lua_handler);
  }
  
  if (lua_toboolean(L,3))
    lua_pushboolean(L,tcc_add_file(*tcc,text) == 0);
  else
    lua_pushboolean(L,tcc_compile_string(*tcc,text) == 0);
  
  tcc_set_error_func(*tcc,NULL,error_c_handler);
  lua_pushlightuserdata(L,*tcc);
  lua_pushnil(L);
  lua_settable(L,LUA_REGISTRYINDEX);
  
  return 1;
}

/**************************************************************************/

static int tcclua_add_file(lua_State *const L)
{
  TCCState          **tcc = luaL_checkudata(L,1,TYPE_TCC);
  const char         *file = luaL_checkstring(L,2);
  struct error_data   errdata;
  
  if (lua_isfunction(L,3))
  {
    errdata.tcc = *tcc;
    errdata.L   = L;
    
    lua_pushlightuserdata(L,*tcc);
    lua_pushvalue(L,3);
    lua_settable(L,LUA_REGISTRYINDEX);
    tcc_set_error_func(*tcc,&errdata,error_lua_handler);
  }
  
  lua_pushboolean(L,tcc_add_file(*tcc,file) == 0);
  
  tcc_set_error_func(*tcc,NULL,error_c_handler);
  lua_pushlightuserdata(L,*tcc);
  lua_pushnil(L);
  lua_settable(L,LUA_REGISTRYINDEX);
  
  return 1;
}

/************************************************************************/

static int tcclua_add_string(lua_State *const L)
{
  TCCState          **tcc = luaL_checkudata(L,1,TYPE_TCC);
  const char         *file = luaL_checkstring(L,2);
  struct error_data   errdata;
  
  if (lua_isfunction(L,3))
  {
    errdata.tcc = *tcc;
    errdata.L   = L;
    
    lua_pushlightuserdata(L,*tcc);
    lua_pushvalue(L,3);
    lua_settable(L,LUA_REGISTRYINDEX);
    tcc_set_error_func(*tcc,&errdata,error_lua_handler);
  }
  
  lua_pushboolean(L,tcc_compile_string(*tcc,file) == 0);
  
  tcc_set_error_func(*tcc,NULL,error_c_handler);
  lua_pushlightuserdata(L,*tcc);
  lua_pushnil(L);
  lua_settable(L,LUA_REGISTRYINDEX);
  
  return 1;
}

/*************************************************************************/

static void error_lua_handler(
	void       *opaque,
	const char *msg
)
{
  struct error_data *errdata = opaque;
  int                rc;
  
  lua_pushlightuserdata(errdata->L,errdata->tcc);
  lua_gettable(errdata->L,LUA_REGISTRYINDEX);
  lua_pushstring(errdata->L,msg);
  rc = lua_pcall(errdata->L,1,0,0);
  if (rc != 0)
    syslog(LOG_DEBUG,"lua_pcall() = %d: %s\n",rc,lua_tostring(errdata->L,-1));
}

/***********************************************************************/

static void error_c_handler(
	void       *opaque __attribute__((unused)),
	const char *msg
)
{
  fprintf(stderr,"tcc: %s\n",msg);
}

/************************************************************************/

static int tcclua_output_type(lua_State *const L)
{
  lua_pushinteger(
        L,
        tcc_set_output_type(
                *(TCCState **)luaL_checkudata(L,1,TYPE_TCC),
                luaL_checkoption(L,2,"memory",moutput_type)
        )
  );
  return 1;
}

/**************************************************************************/

static int tcclua_library_path(lua_State *const L)
{
  lua_pushinteger(
          L,
          tcc_add_library_path(
                  *(TCCState **)luaL_checkudata(L,1,TYPE_TCC),
                  luaL_checkstring(L,2)
          )
  );               
  return 1;
}

/**************************************************************************/

static int tcclua_library(lua_State *const L)
{
  lua_pushinteger(
          L,
          tcc_add_library(
                  *(TCCState **)luaL_checkudata(L,1,TYPE_TCC),
                  luaL_checkstring(L,2)
          )
  );
  return 1;
}

/**************************************************************************/

static int tcclua_set_symbol(lua_State *const L __attribute__((unused)))
{
  return 0;
}

/**************************************************************************/

static int tcclua_output(lua_State *const L __attribute__((unused)))
{
  return 0;
}

/**************************************************************************/

static int tcclua_run(lua_State *const L __attribute__((unused)))
{
  return 0;
}

/**************************************************************************/

static int tcclua_relocate(lua_State *const L)
{
  TCCState **tcc;
  void      *mem;
  int        size;
  int        rc;
  
  tcc = luaL_checkudata(L,1,TYPE_TCC);
  size = tcc_relocate(*tcc,NULL);
  if (size == -1)
    return 0;
  
  mem = malloc(size);
  if (mem == NULL)
    return 0;
  
  rc = tcc_relocate(*tcc,mem);
  if (rc == -1)
  {
    free(mem);
    return 0;
  }

  lua_pushlightuserdata(L,mem);
  return 1;
}

/**************************************************************************/

static int tcclua_get_symbol(lua_State *const L __attribute__((unused)))
{
  TCCState   **tcc;
  int          max;
  int          i;
  int          ret;
  int        (*call)(lua_State *);
  
  tcc = luaL_checkudata(L,1,TYPE_TCC);
  max = lua_gettop(L);
  ret = 0;
  
  for (i = 2 ; i <= max ; i++)
  {
    if (!lua_checkstack(L,1))
      return ret;

    call = tcc_get_symbol(*tcc,luaL_checkstring(L,i));

    if (call == NULL)
      lua_pushnil(L);
    else
      lua_pushcfunction(L,call);
    ret++;
  }

  return ret;
}

/**************************************************************************/

static int tcclua_lib_path(lua_State *const L)
{
  tcc_set_lib_path(
          *(TCCState **)luaL_checkudata(L,1,TYPE_TCC),
          luaL_checkstring(L,2)
  );
  return 0;
}

/**************************************************************************/

