
#include <string.h>
#include <stdlib.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <syslog.h>

#ifndef __GNUC__
#  define __attribute__(x)
#endif

struct strintmap
{
  const char *const name;
  const int         value;
};

/*************************************************************************/

const struct strintmap m_facilities[] =
{
  { "auth"	, LOG_AUTH	} ,
  { "auth3"	, (13 << 3)	} ,
  { "auth4"	, (14 << 3)	} ,
  { "authpriv"	, LOG_AUTHPRIV	} ,
  { "cron"	, LOG_CRON	} ,
  { "cron2"	, (15 << 3)	} ,
  { "daemon"	, LOG_DAEMON	} ,
  { "ftp"	, LOG_FTP	} ,
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

const struct strintmap m_levels[] = 
{
  { "alert"	, LOG_ALERT	} ,
  { "crit"	, LOG_CRIT	} ,
  { "debug"	, LOG_DEBUG	} ,
  { "emerg"	, LOG_EMERG	} ,
  { "err"	, LOG_ERR	} ,
  { "info"	, LOG_INFO	} ,
  { "notice"	, LOG_NOTICE	} ,
  { "warn"	, LOG_WARNING	} ,
  { "warning"	, LOG_WARNING	} ,
};

#define MAX_LEVEL	(sizeof(m_levels) / sizeof(struct strintmap))

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

/************************************************************************/

static int syslog_open(lua_State *L)
{
  const char *ident;
  int         options;
  int         facility;
  
  ident = luaL_checkstring(L,1);
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
  
  openlog(ident,options,facility);
  return 0;
}

/***********************************************************************/

static int syslog_close(lua_State *L __attribute__((unused)))
{
  closelog();
  return 0;
}

/***********************************************************************/

static int syslog_log(lua_State *L)
{
  const char  *msg;
  int          level;
  int          top;
  
  top = lua_gettop(L);
  if (top < 2)
    return luaL_error(L,"too few paramters to syslog()");
  
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
  
  if (top > 2)
  {
    luaL_Buffer buf;
    
    luaL_buffinit(L,&buf);
    for (int i = 2 ; i <= top ; i++)
    {
      const char *text;
      size_t      size;
    
      if (i > 2) luaL_addchar(&buf,' ');
      text = lua_tolstring(L,i,&size);
      luaL_addlstring(&buf,text,size);
    }
    luaL_pushresult(&buf);
  }
  
  msg = lua_tostring(L,-1);
  syslog(level,"%s",msg);
  return 0;
}

/***********************************************************************/

static const struct luaL_reg reg_syslog[] =
{
  { "open"	, syslog_open   } ,
  { "close"	, syslog_close	} ,
  { "log"	, syslog_log	} ,
  { NULL	, NULL		} ,
};

int luaopen_org_conman_syslog(lua_State *L)
{
  luaL_register(L,"org.conman.syslog",reg_syslog);

  lua_pushliteral(L,"facility");
  lua_createtable(L,0,MAX_FACILITY);
  for (size_t i = 0 ; i < MAX_FACILITY; i++)
  {
    lua_pushstring(L,m_facilities[i].name);
    lua_pushinteger(L,m_facilities[i].value);
    lua_settable(L,-3);
  }
  lua_settable(L,-3);

  lua_pushliteral(L,"level");
  lua_createtable(L,0,MAX_LEVEL);
  for (size_t i = 0 ; i < MAX_LEVEL; i++)
  {
    lua_pushstring(L,m_levels[i].name);
    lua_pushinteger(L,m_levels[i].value);
    lua_settable(L,-3);
  }
  lua_settable(L,-3);

  lua_pushliteral(L,"COPYRIGHT");
  lua_pushliteral(L,"Copyright 2011 by Sean Conner.  All Rights Reserved.");
  lua_settable(L,-3);
  
  lua_pushliteral(L,"DESCRIPTION");
  lua_pushliteral(L,"Interface to Unix syslog");
  lua_settable(L,-3);
  
  lua_pushliteral(L,"VERSION");
  lua_pushliteral(L,"1.0.0");
  lua_settable(L,-3);

  return 1;
}

/************************************************************************/

