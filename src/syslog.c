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
  { "auth"	, LOG_AUTH	} ,
  { "auth3"	, (13 << 3)	} ,
  { "auth4"	, (14 << 3)	} ,
#ifdef LOG_AUTHPRIV
  { "authpriv"	, LOG_AUTHPRIV	} ,
#else
  { "authpriv"	, (10 << 3)	} ,
#endif
  { "cron"	, LOG_CRON	} ,
  { "cron2"	, (15 << 3)	} ,
  { "daemon"	, LOG_DAEMON	} ,
#ifdef LOG_FTP
  { "ftp"	, LOG_FTP	} ,
#else
  { "ftp"	, (11 << 3)	} ,
#endif
  { "kernel"	, ( 0 << 3)	} ,
  { "local0"	, LOG_LOCAL0	} ,
  { "local1"	, LOG_LOCAL1	} ,
  { "local2"	, LOG_LOCAL2	} ,
  { "local3"	, LOG_LOCAL3	} ,
  { "local4"	, LOG_LOCAL4	} ,
  { "local5"	, LOG_LOCAL5	} ,
  { "local6"	, LOG_LOCAL6	} ,
  { "local7"	, LOG_LOCAL7	} ,
  { "lpr"	, LOG_LPR	} ,
  { "mail"	, LOG_MAIL	} ,
  { "news"	, LOG_NEWS	} ,
  { "ntp"	, (12 << 3)	} ,
  { "syslog"	, LOG_SYSLOG	} ,
  { "user"	, LOG_USER	} ,
  { "uucp"	, LOG_UUCP	} ,
};

#define MAX_FACILITY	(sizeof(m_facilities) / sizeof(struct strintmap))

static const struct strintmap m_levels[] = 
{
  { "alert"	, LOG_ALERT	} ,
  { "crit"	, LOG_CRIT	} ,
  { "critical"	, LOG_CRIT	} ,
  { "debug"	, LOG_DEBUG	} ,
  { "emerg"	, LOG_EMERG	} ,
  { "emergency"	, LOG_EMERG	} ,
  { "err"	, LOG_ERR	} ,
  { "error"	, LOG_ERR	} ,
  { "info"	, LOG_INFO	} ,
  { "notice"	, LOG_NOTICE	} ,
  { "warn"	, LOG_WARNING	} ,
  { "warning"	, LOG_WARNING	} ,
};

#define MAX_LEVEL	(sizeof(m_levels) / sizeof(struct strintmap))

static char m_ident[1024];

/************************************************************************/

static int sim_cmp(const void *needle,const void *haystack)
{
  const char             *key = needle;
  const struct strintmap *map = haystack;
  
  return strcmp(key,map->name);
}

/************************************************************************/

static int check_boolean(lua_State *L,int index,const char *field,int def)
{
  int b;
  
  lua_getfield(L,3,field);
  b = lua_toboolean(L,index);
  lua_pop(L,1);
  if (b)
    return def;
  else
    return 0;
}

/************************************************************************
*
* Usage:	syslog.open(ident,facility[,flags])
*
* Desc:		Establish the identyity string and default facility to
*		log information under.
*
* Input:	ident (string) identity string
*		facility (string number) facility to use.
*		flags (table/optional) flags, fields are:
*			| pid=true	log pid
*			| cons=true	log to console
*			| nodelay=true	open socket immediately
*			| ndelay=true		"
*			| odelay=true	wait before opening socket
*			| nowait=true	log immediately
*
************************************************************************/

static int syslog_open(lua_State *L)
{
  const char *ident;
  size_t      sident;
  int         options;
  int         facility;
  
  ident = luaL_checklstring(L,1,&sident);
  if (lua_type(L,2) == LUA_TNUMBER)
  {
    facility = lua_tointeger(L,2);
    if ((facility < 0) || (facility > 191))
      return luaL_error(L,"invalid facility %d",facility);
  }
  else if (lua_type(L,2) == LUA_TSTRING)
  {
    struct strintmap *map;
    const char       *name;
    
    name = lua_tostring(L,2);
    map  = bsearch(name,m_facilities,MAX_FACILITY,sizeof(struct strintmap),sim_cmp);
    if (map == NULL)
      return luaL_error(L,"invalid facility '%s'",name);
    facility = map->value;
  }
  else
    return luaL_error(L,"number or string expected, got %s",luaL_typename(L,2));
  
  options = 0;
  if (lua_type(L,3) == LUA_TTABLE)
  {
    options |= check_boolean(L , 3 , "pid"    , LOG_PID);
    options |= check_boolean(L , 3 , "cons"   , LOG_CONS);
    options |= check_boolean(L , 3 , "nodelay", LOG_NDELAY);
    options |= check_boolean(L , 3 , "ndelay" , LOG_NDELAY);
    options |= check_boolean(L , 3 , "odelay" , LOG_ODELAY);
    options |= check_boolean(L , 3 , "nowait" , LOG_NOWAIT);
  }
  
  if (sident > sizeof(m_ident) - 1)
    sident = sizeof(m_ident) - 1;
  
  memcpy(m_ident,ident,sident);
  m_ident[sident] = '\0';
  
  openlog(m_ident,options,facility);
  return 0;
}

/***********************************************************************
*
* Usage:	syslog.close()
*
* Desc:		Close the logging channel
*
* *********************************************************************/

static int syslog_close(lua_State *L __attribute__((unused)))
{
  closelog();
  return 0;
}

/***********************************************************************
*
* Usage:	syslog.log(level,format[,...])
*		| syslog(level,format[,...])
*
* Desc:		Log information at a given level
*
* Input:	level (string number) level number
*		format (string) format string
*
************************************************************************/

static int syslog_log(lua_State *L)
{
  int level;
  
  if (lua_type(L,1) == LUA_TNUMBER)
  {
    level = lua_tointeger(L,1);
    if ((level < 0) || (level > 7))
      return luaL_error(L,"invalid level %d",level);
  }
  else if (lua_type(L,1) == LUA_TSTRING)
  {
    struct strintmap *map;
    const char       *name;
    
    name = lua_tostring(L,1);
    map  = bsearch(name,m_levels,MAX_LEVEL,sizeof(struct strintmap),sim_cmp);
    if (map == NULL)
      return luaL_error(L,"invalid level '%s'",name);
    level = map->value;
  }
  else
    return luaL_error(L,"number or string expected, got %s",luaL_typename(L,1));

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
  size_t i;

#if LUA_VERSION_NUM == 501
  luaL_register(L,"org.conman.syslog",reg_syslog);
#else
  luaL_newlib(L,reg_syslog);
#endif

  lua_createtable(L,0,MAX_FACILITY);
  for (i = 0 ; i < MAX_FACILITY; i++)
  {
    lua_pushinteger(L,m_facilities[i].value);
    lua_setfield(L,-2,m_facilities[i].name);
  }
  lua_setfield(L,-2,"facility");

  lua_createtable(L,0,MAX_LEVEL);
  for (i = 0 ; i < MAX_LEVEL; i++)
  {
    lua_pushinteger(L,m_levels[i].value);
    lua_setfield(L,-2,m_levels[i].name);
  }
  lua_setfield(L,-2,"level");

  lua_newtable(L);
  lua_pushcfunction(L,syslog___call);
  lua_setfield(L,-2,"__call");
  lua_setmetatable(L,-2);

  return 1;
}

/************************************************************************/

