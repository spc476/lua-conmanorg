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
#include <math.h>

#include <sys/select.h>

#include <lua.h>
#include <lauxlib.h>

#if !defined(LUA_VERSION_NUM) || LUA_VERSION_NUM != 501
#  error This module is for Lua 5.1
#endif

#define TYPE_POLL	"org.conman.pollset"

/********************************************************************/

typedef struct
{
  fd_set read;
  fd_set write;
  fd_set except;
  int    min;
  int    max;
  size_t idx;
  int    ref;
} selectset__t;

/**********************************************************************/

static void selectset_toevents(
        lua_State *const L,
        int              idx,
        selectset__t    *set,
        int              fd
)
{
  if (lua_istable(L,idx))
  {
    lua_getfield(L,idx,"read");
    if (lua_isboolean(L,-1)) FD_SET(fd,&set->read);
    lua_getfield(L,idx,"write");
    if (lua_isboolean(L,-1)) FD_SET(fd,&set->write);
    lua_getfield(L,idx,"except");
    if (lua_isboolean(L,-1)) FD_SET(fd,&set->except);
    lua_pop(L,3);
  }
  else if (lua_isstring(L,idx))
  {
    const char *flags = lua_tostring(L,idx);
    for ( ; *flags ; flags++)
    {
      switch(*flags)
      {
        case 'r': FD_SET(fd,&set->read);   break;
        case 'w': FD_SET(fd,&set->write);  break;
        case 'e': FD_SET(fd,&set->except); break;
        default:  break;
      }
    }
  }
  else if (lua_isnil(L,idx))
    FD_SET(fd,&set->read);
  else
    luaL_error(L,"expected table or string");
}

/**********************************************************************/

static void selectset_pushevents(lua_State *const L,selectset__t *set,int fd)
{
  lua_createtable(L,0,4);
  lua_pushboolean(L,FD_ISSET(fd,&set->read));
  lua_setfield(L,-2,"read");
  lua_pushboolean(L,FD_ISSET(fd,&set->write));
  lua_setfield(L,-2,"write");
  lua_pushboolean(L,FD_ISSET(fd,&set->except));
  lua_setfield(L,-2,"except");
}

/**********************************************************************/

static int selectset_lua(lua_State *const L)
{
  selectset__t *set;
  
  set = lua_newuserdata(L,sizeof(selectset__t));
  FD_ZERO(&set->read);
  FD_ZERO(&set->write);
  FD_ZERO(&set->except);
  set->idx = 0;
  set->min = INT_MAX;
  set->max = 0;
  
  lua_createtable(L,0,0);
  set->ref = luaL_ref(L,LUA_REGISTRYINDEX);
  
  luaL_getmetatable(L,TYPE_POLL);
  lua_setmetatable(L,-2);
  return 1;
}

/**********************************************************************/

static int selectmeta___tostring(lua_State *const L)
{
  lua_pushfstring(L,"selectset (%p)",lua_touserdata(L,1));
  return 1;
}

/**********************************************************************/

static int selectmeta___gc(lua_State *const L)
{
  selectset__t *set = luaL_checkudata(L,1,TYPE_POLL);
  luaL_unref(L,LUA_REGISTRYINDEX,set->ref);
  return 0;
}

/**********************************************************************/

static int selectmeta_insert(lua_State *const L)
{
  selectset__t *set = luaL_checkudata(L,1,TYPE_POLL);
  int           fh  = luaL_checkinteger(L,2);
  
  lua_settop(L,4);
  
  if (fh > FD_SETSIZE)
  {
    lua_pushinteger(L,EINVAL);
    return 1;
  }
  
  if (fh < set->min) set->min = fh;
  if (fh > set->max) set->max = fh;
  
  selectset_toevents(L,3,set,fh);
  
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

static int selectmeta_update(lua_State *const L)
{
  selectset__t *set = luaL_checkudata(L,1,TYPE_POLL);
  int           fh  = luaL_checkinteger(L,2);
  
  FD_CLR(fh,&set->read);
  FD_CLR(fh,&set->write);
  FD_CLR(fh,&set->except);
  
  selectset_toevents(L,3,set,fh);
  lua_pushinteger(L,0);
  return 1;
}

/**********************************************************************/

static int selectmeta_remove(lua_State *const L)
{
  selectset__t *set = luaL_checkudata(L,1,TYPE_POLL);
  int           fh  = luaL_checkinteger(L,2);
  
  FD_CLR(fh,&set->read);
  FD_CLR(fh,&set->write);
  FD_CLR(fh,&set->except);
  
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

static int selectmeta_events(lua_State *const L)
{
  selectset__t   *set     = luaL_checkudata(L,1,TYPE_POLL);
  double          timeout = luaL_optnumber(L,2,-1.0);
  struct timeval  tout;
  struct timeval *ptout;
  fd_set          read;
  fd_set          write;
  fd_set          except;
  int             rc;
  int             i;
  size_t          idx;
  
  if (timeout < 0)
    ptout = NULL;
  else
  {
    double seconds;
    double fract;
    
    fract        = modf(timeout,&seconds);
    tout.tv_sec  = (long)seconds;
    tout.tv_usec = (long)(fract * 1000000.0);
    ptout        = &tout;
  }
  
  read   = set->read;
  write  = set->write;
  except = set->except;
  
  rc = select(FD_SETSIZE,&read,&write,&except,ptout);
  if (rc < 0)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    return 2;
  }
  
  lua_pushinteger(L,set->ref);
  lua_gettable(L,LUA_REGISTRYINDEX);
  
  lua_createtable(L,set->idx,0);  
  for (idx = 1 , i = set->min ; i <= set->max ; i++)
  {
    if (FD_ISSET(i,&read) || FD_ISSET(i,&write) || FD_ISSET(i,&except))
    {
      lua_pushnumber(L,idx++);
      selectset_pushevents(L,set,i);
      lua_pushinteger(L,i);
      lua_gettable(L,-5);
      lua_setfield(L,-2,"obj");
      lua_settable(L,-3);
    }
  }
  
  lua_pushinteger(L,0);
  return 2;
}

/**********************************************************************/

static int selectmeta__POLL(lua_State *const L)
{
  lua_pushliteral(L,"select");
  return 1;
}

/**********************************************************************/

static const luaL_Reg m_selectmeta[] =
{
  { "__tostring"	, selectmeta___tostring	} ,
  { "__gc"		, selectmeta___gc	} ,
  { "insert"		, selectmeta_insert	} ,
  { "update"		, selectmeta_update	} ,
  { "remove"		, selectmeta_remove	} ,
  { "events"		, selectmeta_events	} ,
  { "POLL"		, selectmeta__POLL	} ,
  { NULL		, NULL			}
};

/**********************************************************************/

int luaopen_select(lua_State *const L)
{
  luaL_newmetatable(L,TYPE_POLL);
  luaL_register(L,NULL,m_selectmeta);
  lua_pushvalue(L,-1);
  lua_setfield(L,-1,"__index");
  
  lua_pushcfunction(L,selectset_lua);
  return 1;
}

/**********************************************************************/
