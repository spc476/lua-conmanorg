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
#  define END "big"
#  define CPU "sparcv9"
#elif defined(__sparc)
#  define END "big"
#  define CPU "sparc"
#elif defined(__x86)
#  define END "little"
#  define CPU "x86"
#elif defined(__i386)
#  define END "little"
#  define CPU "x86"
#elif defined(__x86_64)
#  define END "little"
#  define CPU "x86_64"
#else
#  define END "(undefined)"
#  define CPU "(undefined)"
#endif

#include <unistd.h>
#include <sys/utsname.h>
#include <sys/time.h>
#include <paths.h>

#include <lua.h>
#include <lauxlib.h>

#if !defined(LUA_VERSION_NUM) || LUA_VERSION_NUM < 501
#  error You need to compile against Lua 5.1 or higher
#endif

/*************************************************************************/

#if LUA_VERSION_NUM == 501
  static const struct luaL_Reg msys_reg[] =
  {
    { NULL , NULL }
  };
#endif

static const struct paths 
{
  const char *const name;
  const char *const value;
} mpaths[] =
{
  { "DEFPATH"	, _PATH_DEFPATH		} ,
  { "STDPATH"	, _PATH_STDPATH		} ,
  { "bshell"	, _PATH_BSHELL		} ,
  { "console"	, _PATH_CONSOLE		} ,
  { "cshell"	, _PATH_CSHELL		} ,
  { "devdb"	, _PATH_DEVDB		} ,
  { "devnull"	, _PATH_DEVNULL		} ,
  { "drum"	, _PATH_DRUM		} ,
  { "klog"	, _PATH_KLOG		} ,
  { "kmem"	, _PATH_KMEM		} ,
  { "lastlog"	, _PATH_LASTLOG		} ,
  { "maildir"	, _PATH_MAILDIR		} ,
  { "man"	, _PATH_MAN		} ,
  { "mnttab"	, _PATH_MNTTAB		} ,
  { "mounted"	, _PATH_MOUNTED		} ,
  { "nologin"	, _PATH_NOLOGIN		} ,
  { "preserve"	, _PATH_PRESERVE	} ,
  { "rwhodir"	, _PATH_RWHODIR		} ,
  { "sendmail"	, _PATH_SENDMAIL	} ,
  { "shadow"	, _PATH_SHADOW		} ,
  { "shells"	, _PATH_SHELLS		} ,
  { "tty"	, _PATH_TTY		} ,
  { "unix"	, _PATH_UNIX		} ,
  { "utmp"	, _PATH_UTMP		} ,
  { "vi"	, _PATH_VI		} ,
  { "wtmp"	, _PATH_WTMP		} ,
  { "dev"	, _PATH_DEV		} ,
  { "tmp"	, _PATH_TMP		} ,
  { "vardb"	, _PATH_VARDB		} ,
  { "varrun"	, _PATH_VARRUN		} ,
  { "vartmp"	, _PATH_VARTMP		} ,
};

#define MAXPATH	( sizeof(mpaths) / sizeof(struct paths))

/*************************************************************************/

int luaopen_org_conman_sys(lua_State *const L)
{
  struct utsname buffer;
  
  if (uname(&buffer) < 0)
    return 0;

#if LUA_VERSION_NUM == 501  
  luaL_register(L,"org.conman.sys",msys_reg);
#elif LUA_VERSION_NUM >= 502
  lua_createtable(L,0,10);
#endif

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
  lua_pushliteral(L,END);
  lua_setfield(L,-2,"_ENDIAN");
  lua_pushstring(L,CPU);
  lua_setfield(L,-2,"_CPU");

#ifdef _GNU_SOURCE
  lua_pushstring(L,buffer.domainname);
#else
  lua_pushliteral(L,"");
#endif
  lua_setfield(L,-2,"_DOMAINNAME");
  
#ifdef _SC_NPROCESSORS_ONLN
  lua_pushinteger(L,sysconf(_SC_NPROCESSORS_ONLN));
#else
  lua_pushinteger(L,1);
#endif
  lua_setfield(L,-2,"_CORES");

  lua_createtable(L,0,MAXPATH);
  for (size_t i = 0 ; i < MAXPATH ; i++)
  {
    lua_pushstring(L,mpaths[i].value);
    lua_setfield(L,-2,mpaths[i].name);
  }
  lua_setfield(L,-2,"_PATHS");
  
  return 1;
}

/*************************************************************************/
