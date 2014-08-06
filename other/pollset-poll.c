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

#include <string.h>
#include <poll.h>

#include <lua.h>
#include <lauxlib.h>

#if !defined(LUA_VERSION_NUM) || LUA_VERSION_NUM != 501
#  error This module is for Lua 5.1
#endif

#define TYPE_POLL	"org.conman.pollset"

/********************************************************************/

typedef struct
{
  struct pollfd *set;
  size_t         idx;
  size_t         max;
  int            ref;
} pollset__t;

/**********************************************************************/

static int pollset_toevents(lua_State *const L,int idx)
{
  int events = 0;
  
  if (lua_istable(L,idx))
  {
    lua_getfield(L,idx,"read");
    events |= lua_isboolean(L,-1) ? POLLIN : 0;
    lua_getfield(L,idx,"write");
    events |= lua_isboolean(L,-1) ? POLLOUT : 0;
    lua_getfield(L,idx,"priority");
    events |= lua_isboolean(L,-1) ? POLLPRI : 0;
    lua_getfield(L,idx,"error");
    lua_pop(L,3);
  }
  else if (lua_isstring(L,idx))
  {
    const char *flags = lua_tostring(L,idx);
    for ( ; *flags ; flags++)
    {
      switch(*flags)
      {
        case 'r': events |= POLLIN;  break;
        case 'w': events |= POLLOUT; break;
        case 'p': events |= POLLPRI; break;
        default:  break;
      }
    }
  }
  else if (lua_isnil(L,idx))
    events |= POLLIN;
  else
    return luaL_error(L,"expected table or string");
  
  return events;
}

/**********************************************************************/

static void pollset_pushevents(lua_State *const L,int events)
{
  lua_createtable(L,0,7);
  lua_pushboolean(L,(events & POLLIN)   != 0);
  lua_setfield(L,-2,"read");
  lua_pushboolean(L,(events & POLLOUT)  != 0);
  lua_setfield(L,-2,"write");
  lua_pushboolean(L,(events & POLLPRI)  != 0);
  lua_setfield(L,-2,"priority");
  lua_pushboolean(L,(events & POLLERR)  != 0);
  lua_setfield(L,-2,"error");
  lua_pushboolean(L,(events & POLLHUP)  != 0);
  lua_setfield(L,-2,"hangup");
  lua_pushboolean(L,(events & POLLNVAL) != 0);
  lua_setfield(L,-2,"invalid");
}

/**********************************************************************/

static int pollset_lua(lua_State *const L)
{
  pollset__t *set;
  
  set = lua_newuserdata(L,sizeof(pollset__t));
  set->set = NULL;
  set->idx = 0;
  set->max = 0;
  
  lua_createtable(L,0,0);
  set->ref = luaL_ref(L,LUA_REGISTRYINDEX);
  
  luaL_getmetatable(L,TYPE_POLL);
  lua_setmetatable(L,-2);
  return 1;
}

/**********************************************************************/

static int pullmeta___tostring(lua_State *const L)
{
  lua_pushfstring(L,"pollset (%p)",lua_touserdata(L,1));
  return 1;
}

/**********************************************************************/

static int pullmeta___gc(lua_State *const L)
{
  lua_Alloc  allocf;
  void      *ud;
  
  pollset__t *set = luaL_checkudata(L,1,TYPE_POLL);
  luaL_unref(L,LUA_REGISTRYINDEX,set->ref);
  allocf = lua_getallocf(L,&ud);
  (*allocf)(ud,set->set,set->max * sizeof(struct pollfd),0);  
  return 0;
}

/**********************************************************************/

static int pullmeta_insert(lua_State *const L)
{
  pollset__t *set = luaL_checkudata(L,1,TYPE_POLL);
  int         fh  = luaL_checkinteger(L,2);
  
  lua_settop(L,4);
  
  if (set->idx == set->max)
  {
    struct pollfd *new;
    size_t         newmax;
    lua_Alloc      allocf;
    void          *ud;
    
    allocf = lua_getallocf(L,&ud);
    newmax = set->max + 10;
    new    = (*allocf)(
    		ud,
    		set->set,
    		set->max * sizeof(struct pollfd),
    		newmax   * sizeof(struct pollfd)
    	);
    
    if (new == NULL)
    {
      lua_pushinteger(L,ENOMEM);
      return 1;
    }
    
    set->set = new;
    set->max = newmax;
  }
  
  set->set[set->idx].events = pollset_toevents(L,3);
  set->set[set->idx].fd     = fh;
  
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

static int pullmeta_update(lua_State *const L)
{
  pollset__t *set = luaL_checkudata(L,1,TYPE_POLL);
  int         fh  = luaL_checkinteger(L,2);
  
  lua_settop(L,3);
  
  for (size_t i = 0 ; i < set->idx ; i++)
  {
    if (set->set[i].fd == fh)
    {
      set->set[i].events = pollset_toevents(L,3);
      lua_pushinteger(L,0);
      return 1;
    }
  }
  
  lua_pushinteger(L,EINVAL);
  return 1;
}

/**********************************************************************/

static int pullmeta_remove(lua_State *const L)
{
  pollset__t *set = luaL_checkudata(L,1,TYPE_POLL);
  int         fh  = luaL_checkinteger(L,2);
  
  for (size_t i = 0 ; i < set->idx ; i++)
  {
    if (set->set[i].fd == fh)
    {
      lua_pushinteger(L,set->ref);
      lua_gettable(L,LUA_REGISTRYINDEX);
      lua_pushinteger(L,fh);
      lua_pushnil(L);
      lua_settable(L,-3);
    
      memmove(
               &set->set[i],
               &set->set[i+1],
               (set->idx - i - 1) * sizeof(struct pollfd)
            );
            
      set->idx--;
      lua_pushinteger(L,0);
      return 1;
    }
  }
  
  lua_pushinteger(L,EINVAL);
  return 1;
}

/**********************************************************************/

static int pullmeta_events(lua_State *const L)
{
  pollset__t *set      = luaL_checkudata(L,1,TYPE_POLL);
  lua_Number  dtimeout = luaL_optnumber(L,2,-1.0);
  int         timeout;

  if (dtimeout < 0)
    timeout = -1;
  else
    timeout = (int)(dtimeout * 1000.0);  
  
  if (poll(set->set,set->idx,timeout) < 0)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    return 2;
  }
  
  lua_pushinteger(L,set->ref);
  lua_gettable(L,LUA_REGISTRYINDEX);
  
  lua_createtable(L,set->idx,0);
  for (size_t idx = 1 , i = 0 ; i < set->idx ; i++)
  {
    if (set->set[i].revents != 0)
    {
      lua_pushnumber(L,idx++);
      pollset_pushevents(L,set->set[i].events);
      lua_pushinteger(L,set->set[i].fd);
      lua_gettable(L,-5);
      lua_setfield(L,-2,"obj");
      lua_settable(L,-3);      
    }
  }
  
  lua_pushinteger(L,0);
  return 2;
}

/**********************************************************************/

static int pullmeta__POLL(lua_State *const L)
{
  lua_pushliteral(L,"poll");
  return 1;
}

/**********************************************************************/

static const luaL_Reg m_pullmeta[] =
{
  { "__tostring"	, pullmeta___tostring	} ,
  { "__gc"		, pullmeta___gc		} ,
  { "insert"		, pullmeta_insert	} ,
  { "update"		, pullmeta_update	} ,
  { "remove"		, pullmeta_remove	} ,
  { "events"		, pullmeta_events	} ,
  { "POLL"		, pullmeta__POLL		} ,
  { NULL		, NULL			}
};

/**********************************************************************/

int luaopen_poll(lua_State *const L)
{
  luaL_newmetatable(L,TYPE_POLL);
  luaL_register(L,NULL,m_pullmeta);
  lua_pushvalue(L,-1);
  lua_setfield(L,-1,"__index");
  
  lua_pushcfunction(L,pollset_lua);
  return 1;
}

/**********************************************************************/
