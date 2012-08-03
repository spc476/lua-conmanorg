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
#include <lualib.h>
#include <lauxlib.h>

#define TCC_TYPE	"TCC"

/**************************************************************************/

static int	tcclua_new		(lua_State *const);
static int	tcclua_dispose		(lua_State *const);

static int	tcclua___tostring	(lua_State *const);
static int	tcclua___gc		(lua_State *const);
static int	tcclua_set_error_func	(lua_State *const);
static int	tcclua_set_warning	(lua_State *const);
static int	tcclua_include_path	(lua_State *const);
static int	tcclua_sysinclude_path	(lua_State *const);
static int	tcclua_define		(lua_State *const);
static int	tcclua_undef		(lua_State *const);
static int	tcclua_compile		(lua_State *const);
static int	tcclua_output_type	(lua_State *const);
static int	tcclua_library_path	(lua_State *const);
static int	tcclua_library		(lua_State *const);
static int	tcclua_set_symbol	(lua_State *const);
static int	tcclua_output		(lua_State *const);
static int	tcclua_run		(lua_State *const);
static int	tcclua_relocate		(lua_State *const);
static int	tcclua_get_symbol	(lua_State *const);
static int	tcclua_lib_path		(lua_State *const);

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
  { "set_error_func"	, tcclua_set_error_func		} ,
  { "set_warning"	, tcclua_set_warning		} ,
  
  { "include_path"	, tcclua_include_path		} ,
  { "sysinclude_path"	, tcclua_sysinclude_path	} ,
  { "define"		, tcclua_define			} ,
  { "undef"		, tcclua_undef			} ,
  
  { "compile"		, tcclua_compile		} ,

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
  luaL_newmetatable(L,TCC_TYPE);
  luaL_register(L,NULL,mtcc_meta);
  lua_pushvalue(L,-1);
  lua_setfield(L,-2,"__index");
  
  luaL_register(L,"tcc",mtcc_reg);
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
  
  ptcc = lua_newuserdata(L,sizeof(TCCState **));
  *ptcc = tcc;
  
  luaL_getmetatable(L,TCC_TYPE);
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
  lua_pushfstring(L,"TCC:%p",luaL_checkudata(L,1,TCC_TYPE));
  return 1;
}

/**************************************************************************/

static int tcclua___gc(lua_State *const L)
{
  syslog(LOG_DEBUG,"garbage collect TCC");
  tcc_delete(*(TCCState **)luaL_checkudata(L,1,TCC_TYPE));
  return 0;
}

/**************************************************************************/

static int tcclua_set_error_func(lua_State *const L __attribute__((unused)))
{
  return 0;
}

/**************************************************************************/

static int tcclua_set_warning(lua_State *const L)
{
  lua_pushinteger(
        L,
        tcc_set_warning(
                *(TCCState **)luaL_checkudata(L,1,TCC_TYPE),
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
               *(TCCState **)luaL_checkudata(L,1,TCC_TYPE),
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
                *(TCCState **)luaL_checkudata(L,1,TCC_TYPE),
                luaL_checkstring(L,2)
        )
  );
  return 1;
}

/**************************************************************************/

static int tcclua_define(lua_State *const L)
{
  tcc_define_symbol(
          *(TCCState **)luaL_checkudata(L,1,TCC_TYPE),
          luaL_checkstring(L,2),
          luaL_checkstring(L,3)
  );
  return 0;
}

/**************************************************************************/

static int tcclua_undef(lua_State *const L)
{
  tcc_undefine_symbol(
          *(TCCState **)luaL_checkudata(L,1,TCC_TYPE),
          luaL_checkstring(L,2)
  );
  return 0;
}

/**************************************************************************/

static int tcclua_compile(lua_State *const L)
{
  TCCState   **tcc  = luaL_checkudata(L,1,TCC_TYPE);
  const char  *text = luaL_checkstring(L,2);
  
  if (lua_toboolean(L,3))
    lua_pushboolean(L,tcc_add_file(*tcc,text) == 0);
  else
    lua_pushboolean(L,tcc_compile_string(*tcc,text) == 0);
  return 1;
}

/**************************************************************************/

static int tcclua_output_type(lua_State *const L)
{
  lua_pushinteger(
        L,
        tcc_set_output_type(
                *(TCCState **)luaL_checkudata(L,1,TCC_TYPE),
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
                  *(TCCState **)luaL_checkudata(L,1,TCC_TYPE),
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
                  *(TCCState **)luaL_checkudata(L,1,TCC_TYPE),
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
  
  tcc = luaL_checkudata(L,1,TCC_TYPE);
  size = tcc_relocate(*tcc,NULL);
  if (size == -1)
    return 0;
  
  mem = malloc(size);
  if (mem == NULL)
    return 0;
  
  tcc_relocate(*tcc,mem);
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
  
  tcc = luaL_checkudata(L,1,TCC_TYPE);
  max = lua_gettop(L);
  ret = 0;
  
  for (i = 2 ; i <= max ; i++)
  {
    if (!lua_checkstack(L,1))
      return ret;
    call = tcc_get_symbol(*tcc,luaL_checkstring(L,i));
    lua_pushcfunction(L,call);
    ret++;
  }

  return ret;
}

/**************************************************************************/

static int tcclua_lib_path(lua_State *const L)
{
  tcc_set_lib_path(
          *(TCCState **)luaL_checkudata(L,1,TCC_TYPE),
          luaL_checkstring(L,2)
  );
  return 0;
}

/**************************************************************************/
