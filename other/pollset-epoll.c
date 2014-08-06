/***************************************************************************
*
* Copyright 2013 by Sean Conner.
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

#ifndef __GNUC__
#  define __attribute__(x)
#endif

#include <errno.h>

#include <unistd.h>
#include <sys/epoll.h>

#include <lua.h>
#include <lauxlib.h>

#if !defined(LUA_VERSION_NUM) || LUA_VERSION_NUM != 501
#  error This module is for Lua 5.1
#endif

#define TYPE_POLL	"org.conman.pollset"

/********************************************************************/

typedef struct
{
  int    efh;
  int    ref;
  size_t idx;
} epollset__t;

/**********************************************************************/

static int epollset_toevents(lua_State *const L,int idx)
{
  int events = 0;
  
  if (lua_istable(L,idx))
  {
    lua_getfield(L,idx,"read");
    events |= lua_isboolean(L,-1) ? EPOLLIN : 0;
    lua_getfield(L,idx,"write");
    events |= lua_isboolean(L,-1) ? EPOLLOUT : 0;
    lua_getfield(L,idx,"priority");
    events |= lua_isboolean(L,-1) ? EPOLLPRI : 0;
    lua_getfield(L,idx,"error");
    events |= lua_isboolean(L,-1) ? EPOLLERR : 0;
    lua_getfield(L,idx,"hangup");
    events |= lua_isboolean(L,-1) ? EPOLLHUP : 0;
    lua_getfield(L,idx,"trigger");
    events |= lua_isboolean(L,-1) ? EPOLLET : 0;
    lua_getfield(L,idx,"oneshot");
    events |= lua_isboolean(L,-1) ? EPOLLONESHOT : 0;
    lua_pop(L,7);
  }
  else if (lua_isstring(L,idx))
  {
    const char *flags = lua_tostring(L,idx);
    for ( ; *flags ; flags++)
    {
      switch(*flags)
      {
        case 'r': events |= EPOLLIN;      break;
        case 'w': events |= EPOLLOUT;     break;
        case 'p': events |= EPOLLPRI;     break;
        case 'e': events |= EPOLLERR;     break;
        case 'h': events |= EPOLLHUP;     break;
        case 't': events |= EPOLLET;      break;
        case '1': events |= EPOLLONESHOT; break;
        default:  break;
      }
    }
  }
  else if (lua_isnil(L,idx))
    events |= EPOLLIN;
  else
    return luaL_error(L,"expected table or string");
  
  return events;
}

/**********************************************************************/

static void epollset_pushevents(lua_State *const L,int events)
{
  lua_createtable(L,0,8);
  lua_pushboolean(L,(events & EPOLLIN)      != 0);
  lua_setfield(L,-2,"read");
  lua_pushboolean(L,(events & EPOLLOUT)     != 0);
  lua_setfield(L,-2,"write");
  lua_pushboolean(L,(events & EPOLLPRI)     != 0);
  lua_setfield(L,-2,"priority");
  lua_pushboolean(L,(events & EPOLLERR)     != 0);
  lua_setfield(L,-2,"error");
  lua_pushboolean(L,(events & EPOLLHUP)     != 0);
  lua_setfield(L,-2,"hangup");
  lua_pushboolean(L,(events & EPOLLET)      != 0);
  lua_setfield(L,-2,"trigger");
  lua_pushboolean(L,(events & EPOLLONESHOT) != 0);
  lua_setfield(L,-2,"oneshot");
}

/**********************************************************************/

static int epollset_lua(lua_State *const L)
{
  epollset__t *set;
  
  set      = lua_newuserdata(L,sizeof(epollset__t));
  set->idx = 0;
  set->efh = epoll_create(10);
  
  if (set->efh == -1)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    return 2;
  }
  
  lua_createtable(L,0,0);
  set->ref = luaL_ref(L,LUA_REGISTRYINDEX);
  
  luaL_getmetatable(L,TYPE_POLL);
  lua_setmetatable(L,-2);
  return 1;
}

/**********************************************************************/

static int epollmeta___tostring(lua_State *const L)
{
  lua_pushfstring(L,"pollset (%p)",lua_touserdata(L,1));
  return 1;
}

/**********************************************************************/

static int epollmeta___gc(lua_State *const L)
{
  epollset__t *set = luaL_checkudata(L,1,TYPE_POLL);
  luaL_unref(L,LUA_REGISTRYINDEX,set->ref);
  close(set->efh);
  return 0;
}

/**********************************************************************/

static int epollmeta_insert(lua_State *const L)
{
  epollset__t         *set = luaL_checkudata(L,1,TYPE_POLL);
  int                  fh  = luaL_checkinteger(L,2);
  struct epoll_event   event;
  
  lua_settop(L,4);
  
  event.events  = epollset_toevents(L,3);
  event.data.fd = fh;
  
  if (epoll_ctl(set->efh,EPOLL_CTL_ADD,fh,&event) < 0)
  {
    lua_pushinteger(L,errno);
    return 1;
  }
  
  lua_pushinteger(L,set->ref);
  lua_gettable(L,LUA_REGISTRYINDEX);
  lua_pushinteger(L,fh);
  
  if (lua_isnil(L,4))
    lua_pushvalue(L,2);
  else
    lua_pushvalue(L,4);
  
  lua_settable(L,-3);
  
  set->idx++;
  lua_pushinteger(L,0);
  return 1;
}

/**********************************************************************/

static int epollmeta_update(lua_State *const L)
{
  epollset__t         *set = luaL_checkudata(L,1,TYPE_POLL);
  int                  fh  = luaL_checkinteger(L,2);
  struct epoll_event   event;
  
  lua_settop(L,3);
  
  event.events  = epollset_toevents(L,3);
  event.data.fd = fh;  
  errno         = 0;
  
  epoll_ctl(set->efh,EPOLL_CTL_MOD,fh,&event);
  lua_pushinteger(L,errno);
  return 1;
}

/**********************************************************************/

static int epollmeta_remove(lua_State *const L)
{
  epollset__t         *set = luaL_checkudata(L,1,TYPE_POLL);
  int                  fh  = luaL_checkinteger(L,2);
  struct epoll_event   event;
  
  if (epoll_ctl(set->efh,EPOLL_CTL_DEL,fh,&event) < 0)
  {
    lua_pushinteger(L,errno);
    return 1;
  }
  
  lua_pushinteger(L,set->ref);
  lua_gettable(L,LUA_REGISTRYINDEX);
  lua_pushinteger(L,fh);
  lua_pushnil(L);
  lua_settable(L,-3);
  
  set->idx--;
  lua_pushinteger(L,0);
  return 1;
}

/**********************************************************************/

static int epollmeta_events(lua_State *const L)
{
  epollset__t *set      = luaL_checkudata(L,1,TYPE_POLL);
  lua_Number   dtimeout = luaL_optnumber(L,2,-1.0);
  int          timeout;
  int          count;
  size_t       idx;
  int          i;
  
  if (dtimeout < 0)
    timeout = -1;
  else
    timeout = (int)(dtimeout * 1000.0);
  
  struct epoll_event events[set->idx];
  
  count = epoll_wait(set->efh,events,set->idx,timeout);
  
  if (count < 0)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    return 2;
  }
  
  lua_pushinteger(L,set->ref);
  lua_gettable(L,LUA_REGISTRYINDEX);
  
  lua_createtable(L,set->idx,0);
  for (idx = 1 , i = 0 ; i < count ; i++)
  {
    lua_pushnumber(L,idx++);
    epollset_pushevents(L,events[i].events);
    lua_pushinteger(L,events[i].data.fd);
    lua_gettable(L,-5);
    lua_setfield(L,-2,"obj");
    lua_settable(L,-3);    
  }
  
  lua_pushinteger(L,0);
  return 2;
}

/**********************************************************************/

static int epollmeta__POLL(lua_State *const L)
{
  lua_pushliteral(L,"epoll");
  return 1;
}

/**********************************************************************/

static const luaL_Reg m_epollmeta[] =
{
  { "__tostring"	, epollmeta___tostring	} ,
  { "__gc"		, epollmeta___gc	} ,
  { "insert"		, epollmeta_insert	} ,
  { "update"		, epollmeta_update	} ,
  { "remove"		, epollmeta_remove	} ,
  { "events"		, epollmeta_events	} ,
  { "POLL"		, epollmeta__POLL	} ,
  { NULL		, NULL			}
};

/**********************************************************************/

int luaopen_epoll(lua_State *const L)
{
  luaL_newmetatable(L,TYPE_POLL);
  luaL_register(L,NULL,m_epollmeta);
  lua_pushvalue(L,-1);
  lua_setfield(L,-1,"__index");
  
  lua_pushcfunction(L,epollset_lua);
  return 1;
}

/**********************************************************************/
