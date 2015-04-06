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

#include <limits.h>

#include <unistd.h>
#include <sys/utsname.h>
#include <sys/time.h>

#if defined(__linux)
#  include <paths.h>
#elif defined(__APPLE__)
#  include <paths.h>
#  define _PATH_KLOG            NULL
#  define _PATH_LASTLOG         NULL
#  define _PATH_MNTTAB          NULL
#  define _PATH_MOUNTED         NULL
#  define _PATH_PRESERVE        NULL
#  define _PATH_RWHODIR         NULL
#  define _PATH_SHADOW          NULL
#  define _PATH_UTMP            NULL
#  define _PATH_WTMP            NULL
#elif defined(__SunOS)
#  define _PATH_DEFPATH		"/usr/bin:/bin"
#  define _PATH_STDPATH		"/usr/bin:/bin:/usr/sbin:/sbin"
#  define _PATH_BSHELL		"/bin/sh"
#  define _PATH_CONSOLE		"/dev/console"
#  define _PATH_CSHELL		"/bin/csh"
#  define _PATH_DEVDB		NULL
#  define _PATH_DEVNULL		"/dev/null"
#  define _PATH_DRUM		NULL
#  define _PATH_KLOG		"/system/object/sysmsg"
#  define _PATH_KMEM		NULL
#  define _PATH_LASTLOG		"/var/adm/lastlog"
#  define _PATH_MAILDIR		"/var/mail"
#  define _PATH_MAN		"/usr/share/man"
#  define _PATH_MEM		"/dev/mem"
#  define _PATH_MNTTAB		"/etc/fstab"
#  define _PATH_MOUNTED		"/etc/mnttab"
#  define _PATH_NOLOGIN		NULL
#  define _PATH_PRESERVE	"/var/lib"
#  define _PATH_RWHODIR		NULL
#  define _PATH_SENDMAIL	"/usr/sbin/sendmail"
#  define _PATH_SHADOW		"/etc/shadow"
#  define _PATH_SHELLS		NULL
#  define _PATH_TTY		"/dev/tty"
#  define _PATH_UNIX		NULL
#  define _PATH_UTMP		"/var/adm/utmpx"
#  define _PATH_VI		"/usr/bin/vi"
#  define _PATH_WTMP		"/var/adm/wtmpx"
#  define _PATH_DEV		"/dev/"
#  define _PATH_TMP		"/tmp/"
#  define _PATH_VARDB		NULL
#  define _PATH_VARRUN		"/var/run/"
#  define _PATH_VARTMP		"/var/tmp/"
#endif

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

static const struct strint
{
  const char *const name;
  const int         value;
} mvars[] =
{
  { "VERSION"		, _SC_VERSION		} ,
  { "ARG_MAX"		, _SC_ARG_MAX		} ,
  { "CHILD_MAX"		, _SC_CHILD_MAX		} ,
  { "HOST_NAME_MAX"	, _SC_HOST_NAME_MAX	} ,
  { "LOGIN_NAME_MAX"	, _SC_LOGIN_NAME_MAX	} ,
  { "CLK_TCK"		, _SC_CLK_TCK		} ,
  { "OPEN_MAX"		, _SC_OPEN_MAX		} ,
  { "RE_DUP_MAX"	, _SC_RE_DUP_MAX	} ,
  { "STREAM_MAX"	, _SC_STREAM_MAX	} ,
  { "TTY_NAME_MAX"	, _SC_TTY_NAME_MAX	} ,
  { "TZNAME_MAX"	, _SC_TZNAME_MAX	} ,
  { "NGROUPS_MAX"	, _SC_NGROUPS_MAX	} ,
  { "RTSIG_MAX"		, _SC_RTSIG_MAX		} ,
  { "SIGQUEUE_MAX"	, _SC_SIGQUEUE_MAX	} ,
  { "NAME_MAX"		, _PC_NAME_MAX		} ,
  { "PATH_MAX"		, _PC_PATH_MAX		} ,
  { "PIPE_BUF"		, _PC_PIPE_BUF		} ,
  { "PAGESIZE"		, _SC_PAGESIZE		} ,
};

#define MAXVARS (sizeof(mvars) / sizeof(struct strint))

/*************************************************************************/

#ifdef __linux__
# include <string.h>
# include <errno.h>
# include <sys/sysinfo.h>

  static int sys_sysinfo(lua_State *L)
  {
    struct sysinfo info;
    
    memset(&info,0,sizeof(info));
    if (sysinfo(&info) < 0)
    {
      lua_pushnil(L);
      lua_pushinteger(L,errno);
      return 2;
    }
    
    lua_createtable(L,0,12);
    lua_createtable(L,3,0);
    lua_pushinteger(L,1);
    lua_pushinteger(L,info.loads[0]);
    lua_settable(L,-3);
    lua_pushinteger(L,2);
    lua_pushinteger(L,info.loads[1]);
    lua_settable(L,-3);
    lua_pushinteger(L,3);
    lua_pushinteger(L,info.loads[2]);
    lua_settable(L,-3);
    lua_setfield(L,-2,"load");
    
    lua_pushnumber(L,info.uptime);
    lua_setfield(L,-2,"uptime");
    lua_pushnumber(L,info.totalram);
    lua_setfield(L,-2,"totalram");
    lua_pushnumber(L,info.freeram);
    lua_setfield(L,-2,"freeram");
    lua_pushnumber(L,info.sharedram);
    lua_setfield(L,-2,"sharedram");
    lua_pushnumber(L,info.bufferram);
    lua_setfield(L,-2,"bufferram");
    lua_pushnumber(L,info.totalswap);
    lua_setfield(L,-2,"totalswap");
    lua_pushnumber(L,info.freeswap);
    lua_setfield(L,-2,"freeswap");
    lua_pushinteger(L,info.procs);
    lua_setfield(L,-2,"procs");
    lua_pushnumber(L,info.totalhigh);
    lua_setfield(L,-2,"totalhigh");
    lua_pushnumber(L,info.freehigh);
    lua_setfield(L,-2,"freehigh");
    lua_pushinteger(L,info.mem_unit);
    lua_setfield(L,-2,"mem_unit");
    
    lua_pushinteger(L,0);
    return 2;
  }
#endif

/*************************************************************************/

int luaopen_org_conman_sys(lua_State *const L)
{
  struct utsname buffer;
  size_t         i;
  
  if (uname(&buffer) < 0)
    return 0;

#if LUA_VERSION_NUM == 501  
  luaL_register(L,"org.conman.sys",msys_reg);
#elif LUA_VERSION_NUM >= 502
  lua_createtable(L,0,11);
#endif

  lua_pushstring(L,buffer.sysname);
  lua_setfield(L,-2,"SYSNAME");
  lua_pushstring(L,buffer.nodename);
  lua_setfield(L,-2,"NODENAME");
  lua_pushstring(L,buffer.release);
  lua_setfield(L,-2,"RELEASE");
  lua_pushstring(L,buffer.version);
  lua_setfield(L,-2,"VERSION");
  lua_pushstring(L,buffer.machine);
  lua_setfield(L,-2,"MACHINE");
  lua_pushliteral(L,END);
  lua_setfield(L,-2,"ENDIAN");
  lua_pushstring(L,CPU);
  lua_setfield(L,-2,"CPU");

#ifdef _GNU_SOURCE
  lua_pushstring(L,buffer.domainname);
#else
  lua_pushliteral(L,"");
#endif
  lua_setfield(L,-2,"DOMAINNAME");
  
#ifdef _SC_NPROCESSORS_ONLN
  lua_pushinteger(L,sysconf(_SC_NPROCESSORS_ONLN));
#else
  lua_pushinteger(L,1);
#endif
  lua_setfield(L,-2,"CORES");

  lua_createtable(L,0,0);
  for (i = 0 ; i < MAXVARS ; i++)
  {
    lua_pushinteger(L,sysconf(mvars[i].value));
    lua_setfield(L,-2,mvars[i].name);
  }
  lua_setfield(L,-2,"POSIX");
  
  lua_createtable(L,0,MAXPATH);
  for (i = 0 ; i < MAXPATH ; i++)
  {
    lua_pushstring(L,mpaths[i].value);
    lua_setfield(L,-2,mpaths[i].name);
  }
  lua_setfield(L,-2,"PATHS");

#ifdef __linux__
  lua_pushcfunction(L,sys_sysinfo);
  lua_setfield(L,-2,"getinfo");
#endif
  
  return 1;
}

/*************************************************************************/
