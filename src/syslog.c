/*********************************************************************
*
* Copyright 2010 by Sean Conner.  All Rights Reserved.
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
* Module:	org.conman.syslog
*
* Desc:		Lua interface to syslog()
*
* Example:
*

	syslog = require "org.conman.syslog"

	syslog.open("myprog",'local1') -- optional
	syslog('debug',"The time is now %s",os.date("%c"))
	syslog.close() -- optional

*
*********************************************************************/

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include <lua.h>
#include <lauxlib.h>

#include <syslog.h>

#if !defined(LUA_VERSION_NUM) || LUA_VERSION_NUM < 501
#  error You need to compile against Lua 5.1 or higher
#endif

#ifndef __GNUC__
#  define __attribute__(x)
#endif

struct strintmap
{
  const char *const name;
  const int         value;
};

/*************************************************************************/

static const struct strintmap m_facilities[] =
{
  { "auth"	, ( 4 << 3) } ,
  { "auth2"	, (10 << 3) } ,
  { "auth3"	, (13 << 3) } ,
  { "auth4"	, (14 << 3) } ,
  { "authpriv"	, (10 << 3) } ,
  { "cron"	, ( 9 << 3) } ,
  { "cron1"	, ( 9 << 3) } ,
  { "cron2"	, (15 << 3) } ,
  { "daemon"	, ( 3 << 3) } ,
  { "ftp"	, (11 << 3) } ,
  { "kernel"	, ( 0 << 3) } ,
  { "local0"	, (16 << 3) } ,
  { "local1"	, (17 << 3) } ,
  { "local2"	, (18 << 3) } ,
  { "local3"	, (19 << 3) } ,
  { "local4"	, (20 << 3) } ,
  { "local5"	, (21 << 3) } ,
  { "local6"	, (22 << 3) } ,
  { "local7"	, (23 << 3) } ,
  { "lpr"	, ( 6 << 3) } ,
  { "mail"	, ( 2 << 3) } ,
  { "news"	, ( 7 << 3) } ,
  { "ntp"	, (12 << 3) } ,
  { "syslog"	, ( 5 << 3) } ,
  { "user"	, ( 1 << 3) } ,
  { "uucp"	, ( 8 << 3) } ,
};

#define MAX_FACILITY	(sizeof(m_facilities) / sizeof(struct strintmap))

static const struct strintmap m_levels[] = 
{
  { "alert"	, 1	} ,
  { "crit"	, 2	} ,
  { "critical"	, 2	} ,
  { "debug"	, 7	} ,
  { "emerg"	, 0	} ,
  { "emergency"	, 0	} ,
  { "err"	, 3	} ,
  { "error"	, 3	} ,
  { "info"	, 6	} ,
  { "notice"	, 5	} ,
  { "warn"	, 4	} ,
  { "warning"	, 4	} ,
};

#define MAX_LEVEL	(sizeof(m_levels) / sizeof(struct strintmap))

static const struct strintmap m_opts[] =
{
  { "cons"	, LOG_CONS	} ,
  { "console"	, LOG_CONS	} ,
  { "ndelay"	, LOG_NDELAY	} ,
  { "nodelay"	, LOG_NDELAY	} ,
  { "nowait"	, LOG_NOWAIT	} ,
  { "odelay"	, LOG_ODELAY	} ,
#ifdef LOG_PERROR
  { "perror"	, LOG_PERROR	} ,
  { "pid"	, LOG_PID	} ,
  { "stderr"	, LOG_PERROR	} ,
#else
  { "perror"	, -1		} ,
  { "pid"	, LOG_PID	} ,
  { "stderr"	, -1		} ,
#endif
};

#define MAX_OPT		(sizeof(m_opts) / sizeof(struct strintmap))

/************************************************************************/

static int sim_cmp(const void *needle,const void *haystack)
{
  const char             *key = needle;
  const struct strintmap *map = haystack;
  
  return strcmp(key,map->name);
}

/************************************************************************
*
* Usage:	syslog.open(ident,facility[,flags])
*
* Desc:		Establish the identyity string and default facility to
*		log information under.
*
* Input:	ident (string) identity string
*		facility (string) facility to use.
*		flags (table/optional) array of enum
*			| pid		log pid
*			| cons		log to console
*			| nodelay	open socket immediately
*			| ndelay		"
*			| odelay	wait before opening socket
*			| nowait	log immediately
*			| perror	log to stderr as well
*			| stderr		"
*
************************************************************************/

static int syslog_open(lua_State *L)
{
  struct strintmap *map;
  const char       *name;
  const char       *ident;
  int               options;
  int               facility;
  size_t            i;
  
  ident = luaL_checkstring(L,1);
  name  = luaL_checkstring(L,2);
  map   = bsearch(name,m_facilities,MAX_FACILITY,sizeof(struct strintmap),sim_cmp);
  if (map == NULL)
    return luaL_error(L,"invalid facility '%s'",name);

  facility = map->value;
  
  options = 0;
  if (lua_type(L,3) == LUA_TTABLE)
  {
    size_t max;
    
#if LUA_VERSION_NUM == 501
    max = lua_objlen(L,3);
#else
    lua_len(L,3);
    max = lua_tointeger(L,-1);
    lua_pop(L,1);
#endif

    for (i = 1 ; i <= max ; i++)
    {
      lua_pushinteger(L,i);
      lua_gettable(L,3);
      map = bsearch(
                     luaL_checkstring(L,-1),
                     m_opts,
                     MAX_OPT,
                     sizeof(struct strintmap),
                     sim_cmp
                    );
     
      if (map != NULL)
      {
#ifndef LOG_PERROR
        if (map->value == -1)
        {
          lua_pushboolean(L,true);
          lua_setfield(L,LUA_REGISTRYINDEX,"org.conman.syslog:perror");
        }
        else
#endif
        {
          options |= map->value;
        }
      }
      
      lua_pop(L,1);
    }    
  }
  
  lua_pushvalue(L,1);
  lua_setfield(L,LUA_REGISTRYINDEX,"org.conman.syslog:ident");
  openlog(ident,options,facility);
  return 0;
}

/***********************************************************************
*
* Usage:	syslog.close()
*
* Desc:		Close the logging channel
*
* *********************************************************************/

static int syslog_close(lua_State *L)
{
  closelog();
  lua_pushnil(L);
  lua_setfield(L,LUA_REGISTRYINDEX,"org.conman.syslog:ident");
#ifndef LOG_PERROR
  lua_pushnil(L);
  lua_setfield(L,LUA_REGISTRYINDEX,"org.conman.syslog:perror");
#endif
  return 0;
}

/***********************************************************************
*
* Usage:	syslog.log(level,format[,...])
*		| syslog(level,format[,...])
*
* Desc:		Log information at a given level
*
* Input:	level (string) level
*		format (string) format string
*
************************************************************************/

static int syslog_log(lua_State *L)
{
  struct strintmap *map;
  const char       *name;
  int               level;
  
  name = luaL_checkstring(L,1);
  map  = bsearch(name,m_levels,MAX_LEVEL,sizeof(struct strintmap),sim_cmp);
  if (map == NULL)
    return luaL_error(L,"invalid level '%s'",name);
  level = map->value;

  /*------------------------------------------------------------------------
  ; shove string.format() onto the stack to format the message, which
  ; comprises the rest of the paramters.  Call it, then print the resulting
  ; string via syslog()
  ;--------------------------------------------------------------------------*/
  
  luaL_checktype(L,2,LUA_TSTRING);
  lua_getfield(L,2,"format");
  lua_insert(L,2);
  lua_call(L,lua_gettop(L) - 2,1);
  syslog(level,"%s",lua_tostring(L,-1));

#ifndef LOG_PERROR
  lua_getfield(L,LUA_REGISTRYINDEX,"org.conman.syslog:perror");
  if (lua_toboolean(L,-1))
    fprintf(stderr,"%s %s\n",name,lua_tostring(L,-2));
#endif

  return 0;
}

/***********************************************************************/

static int syslog___call(lua_State *L)
{
  lua_remove(L,1);	/* remove table */
  return syslog_log(L);
}

/**********************************************************************/

static const struct luaL_Reg reg_syslog[] =
{
  { "open"	, syslog_open   } ,
  { "close"	, syslog_close	} ,
  { "log"	, syslog_log	} ,
  { NULL	, NULL		} ,
};

int luaopen_org_conman_syslog(lua_State *L)
{
#if LUA_VERSION_NUM == 501
  luaL_register(L,"org.conman.syslog",reg_syslog);
#else
  luaL_newlib(L,reg_syslog);
#endif

  lua_newtable(L);
  lua_pushcfunction(L,syslog___call);
  lua_setfield(L,-2,"__call");
  lua_setmetatable(L,-2);

  return 1;
}

/************************************************************************/

