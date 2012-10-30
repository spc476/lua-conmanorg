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
*************************************************************************/

#ifdef __linux
#  define _GNU_SOURCE
#endif

#if defined(__sparcv9)
#  define CPU "sparcv9"
#elif defined(__sparc)
#  define CPU "sparc"
#elif defined(__x86)
#  define CPU "x86"
#elif defined(__i386)
#  define CPU "x86"
#elif defined(__x86_64)
#  define CPU "x86_64"
#else
#  error Define your CPU please
#endif

#include <sys/utsname.h>

#include <lua.h>
#include <lauxlib.h>

/*************************************************************************/

static const struct luaL_Reg msys_reg[] =
{
  { NULL , NULL }
};

/*************************************************************************/

int luaopen_org_conman_sys(lua_State *const L)
{
  struct utsname buffer;
  if (uname(&buffer) < 0)
    return 0;
  
  luaL_register(L,"org.conman.sys",msys_reg);
  lua_pushstring(L,buffer.sysname);
  lua_setfield(L,-2,"_SYSNAME");
  lua_pushstring(L,buffer.nodename);
  lua_setfield(L,-2,"_NODENAME");
  lua_pushstring(L,buffer.release);
  lua_setfield(L,-2,"_RELEASE");
  lua_pushstring(L,buffer.version);
  lua_setfield(L,-2,"_VERSION");
  lua_pushstring(L,buffer.machine);
  lua_setfield(L,-2,"_MACHINE");
#ifdef _GNU_SOURCE
  lua_pushstring(L,buffer.domainname);
  lua_setfield(L,-2,"_DOMAINNAME");
#endif
  lua_pushstring(L,CPU);
  lua_setfield(L,-2,"_CPU");
  return 1;
}

/*************************************************************************/
