/*********************************************************************
*
* Copyright 2018 by Sean Conner.  All Rights Reserved.
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
* ---------------------------------------------------------------------
*
* This module is nothing more than an array of exit values (as found in
* sysexit.h).
*
*********************************************************************/

#include <stdlib.h>

#include <sysexits.h>
#include <lua.h>
#include <lauxlib.h>

#if !defined(LUA_VERSION_NUM) || LUA_VERSION_NUM < 501
#  error You need to compile against Lua 5.1 or higher
#endif

/***************************************************************************/

struct strint
{
  const char *const text;
  const int         value;
};

/***************************************************************************/

static const struct strint m_sysexits[] =
{
  { "SUCCESS"   , EXIT_SUCCESS  } ,
  { "FAILURE"   , EXIT_FAILURE  } ,
  
#ifdef EX_OK
  { "OK" , EX_OK } ,
#else
  { "OK" , EXIT_SUCCESS } ,
#endif
#ifdef EX_USAGE
  { "USAGE" , EX_USAGE } ,
#endif
#ifdef EX_DATAERR
  { "DATAERR" , EX_DATAERR } ,
#endif
#ifdef EX_NOINPUT
  { "NOINPUT" , EX_NOINPUT } ,
#endif
#ifdef EX_NOUSER
  { "NOUSER" , EX_NOUSER } ,
#endif
#ifdef EX_NOHOST
  { "NOHOST" , EX_NOHOST } ,
#endif
#ifdef EX_UNAVAILABLE
  { "UNAVAILABLE" , EX_UNAVAILABLE } ,
#endif
#ifdef EX_SOFTWARE
  { "SOFTWARE" , EX_SOFTWARE } ,
#endif
#ifdef EX_OSERR
  { "OSERR" , EX_OSERR } ,
#endif
#ifdef EX_OSFILE
  { "OSFILE" , EX_OSFILE } ,
#endif
#ifdef EX_CANTCREAT
  { "CANTCREATE" , EX_CANTCREAT } ,
#endif
#ifdef EX_IOERR
  { "IOERR" , EX_IOERR } ,
#endif
#ifdef EX_TEMPFAIL
  { "TEMPFAIL" , EX_TEMPFAIL } ,
#endif
#ifdef EX_PROTOCOL
  { "PROTOCOL" , EX_PROTOCOL } ,
#endif
#ifdef EX_NOPERM
  { "NOPERM" , EX_NOPERM } ,
#endif
#ifdef EX_CONFIG
  { "CONFIG" , EX_CONFIG } ,
#endif
#ifdef EX_NOTFOUND
  { "NOTFOUND" , EX_NOTFOUND } ,
#endif
  { NULL , 0 }
};

/***************************************************************************/

#if LUA_VERSION_NUM == 501
  static struct luaL_Reg const m_reg_exit[] =
  {
    { NULL , NULL }
  };
#endif

/***************************************************************************/

int luaopen_org_conman_exit(lua_State *L)
{
#if LUA_VERSION_NUM == 501
  luaL_register(L,"org.conman.exit",m_reg_exit);
#else
  lua_createtable(L,0,(sizeof(m_sysexits) / sizeof(struct strint)) - 1);
#endif

  for (size_t i = 0 ; m_sysexits[i].text != NULL ; i++)
  {
    lua_pushinteger(L,m_sysexits[i].value);
    lua_setfield(L,-2,m_sysexits[i].text);
  }
  
  return 1;
}

/***************************************************************************/
