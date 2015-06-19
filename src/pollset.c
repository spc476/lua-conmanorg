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

#include <lua.h>
#include <lauxlib.h>

#if !defined(LUA_VERSION_NUM) || LUA_VERSION_NUM < 501
#  error You need to compile against Lua 5.1 or higher
#endif

#define TYPE_POLL	"org.conman.pollset"

#if !defined(POLLSET_IMPL_EPOLL) && !defined(POLLSET_IMPL_POLL) && !defined(POLLSET_IMPL_SELECT)
#  ifdef __linux
#    define POLLSET_IMPL_EPOLL
#  else
#    define POLLSET_IMPL_POLL
#  endif
#endif

/************************************************************************
*
* The various implementations of pollsets.  The reason they're here and not
* in org.conman.fsys is because I don't need this code all the time, so why
* include it if I don't need it.
*
* So they're here in this module.
*
* Also, each of the three implementations present the same API, but some
* like epoll() might have more functionality than others, like select() that
* can be used.  The org.conman.net module indicates which implementation is
* in use, but as long as you stick to select() type operations, you should
* be fine.
*
* LINUX implementation of pollset, using epoll()
*
*************************************************************************/

#ifdef POLLSET_IMPL_EPOLL
#define POLLSET_IMPL	"epoll"

#include <unistd.h>
#include <sys/epoll.h>

typedef struct
{
  int    efh;
  int    ref;
  size_t idx;
} pollset__t;

/**********************************************************************/

static int pollset_toevents(lua_State *const L,int idx)
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

static void pollset_pushevents(lua_State *const L,int events)
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

static int pollset_lua(lua_State *const L)
{
  pollset__t *set;
  
  set      = lua_newuserdata(L,sizeof(pollset__t));
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

static int polllua___tostring(lua_State *const L)
{
  lua_pushfstring(L,"pollset (%p)",lua_touserdata(L,1));
  return 1;
}

/**********************************************************************/

static int polllua___gc(lua_State *const L)
{
  pollset__t *set = luaL_checkudata(L,1,TYPE_POLL);
  luaL_unref(L,LUA_REGISTRYINDEX,set->ref);
  close(set->efh);
  return 0;
}

/**********************************************************************/

static int polllua_insert(lua_State *const L)
{
  pollset__t         *set = luaL_checkudata(L,1,TYPE_POLL);
  int                 fh  = luaL_checkinteger(L,2);
  struct epoll_event  event;
  
  lua_settop(L,4);
  
  event.events  = pollset_toevents(L,3);
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

static int polllua_update(lua_State *const L)
{
  pollset__t         *set = luaL_checkudata(L,1,TYPE_POLL);
  int                 fh  = luaL_checkinteger(L,2);
  struct epoll_event  event;
  
  lua_settop(L,3);
  
  event.events  = pollset_toevents(L,3);
  event.data.fd = fh;  
  errno         = 0;
  
  epoll_ctl(set->efh,EPOLL_CTL_MOD,fh,&event);
  lua_pushinteger(L,errno);
  return 1;
}

/**********************************************************************/

static int polllua_remove(lua_State *const L)
{
  pollset__t         *set = luaL_checkudata(L,1,TYPE_POLL);
  int                 fh  = luaL_checkinteger(L,2);
  struct epoll_event  event;
  
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

static int polllua_events(lua_State *const L)
{
  pollset__t *set      = luaL_checkudata(L,1,TYPE_POLL);
  lua_Number  dtimeout = luaL_optnumber(L,2,-1.0);
  int         timeout;
  int         count;
  size_t      idx;
  int         i;
  
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
    pollset_pushevents(L,events[i].events);
    lua_pushinteger(L,events[i].data.fd);
    lua_gettable(L,-5);
    lua_setfield(L,-2,"obj");
    lua_settable(L,-3);    
  }
  
  lua_pushinteger(L,0);
  return 2;
}

#endif

/*********************************************************************
*
* poll() based version, used for Unix systems other than Linux
*
* This implementation uses the memory allocation functions for the
* given Lua state.  I felt this was a Good Idea(TM), since if a Lua
* instance is using a particular memory allocation scheme, there is
* probably a good reason for it.  
*********************************************************************/

#ifdef POLLSET_IMPL_POLL
#define POLLSET_IMPL	"poll"

#include <string.h>
#include <poll.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>

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
    events |= lua_isboolean(L,-1) ? POLLERR : 0;
    lua_getfield(L,idx,"hangup");
    events |= lua_isboolean(L,-1) ? POLLHUP : 0;
    lua_getfield(L,idx,"invalid");
    events |= lua_isboolean(L,-1) ? POLLNVAL : 0;
    lua_pop(L,6);
  }
  else if (lua_isstring(L,idx))
  {
    const char *flags = lua_tostring(L,idx);
    for ( ; *flags ; flags++)
    {
      switch(*flags)
      {
        case 'r': events |= POLLIN;   break;
        case 'w': events |= POLLOUT;  break;
        case 'p': events |= POLLPRI;  break;
        case 'e': events |= POLLERR;  break;
        case 'h': events |= POLLHUP;  break;
        case 'i': events |= POLLNVAL; break;
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

static int polllua___tostring(lua_State *const L)
{
  lua_pushfstring(L,"pollset (%p)",lua_touserdata(L,1));
  return 1;
}

/**********************************************************************/

static int polllua___gc(lua_State *const L)
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

static int polllua_insert(lua_State *const L)
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
    struct rlimit  limit;
    
    if (getrlimit(RLIMIT_NOFILE,&limit) < 0)
    {
      lua_pushinteger(L,errno);
      return 1;
    }
    
    if (set->max == limit.rlim_cur)
    {
      lua_pushinteger(L,ENOMEM);
      return 1;
    }
    
    allocf = lua_getallocf(L,&ud);
    newmax = set->max + 10;
    
    if (newmax > limit.rlim_cur)
      newmax = limit.rlim_cur;
    
    new = (*allocf)(
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

static int polllua_update(lua_State *const L)
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

static int polllua_remove(lua_State *const L)
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

static int polllua_events(lua_State *const L)
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

#endif

/**********************************************************************
*
* And finally the version based on select(), for those really old Unix
* systems that just can't handle poll().
*
***********************************************************************/

#ifdef POLLSET_IMPL_SELECT
#define POLLSET_IMPL	"select"

#include <math.h>
#include <sys/select.h>

typedef struct
{
  fd_set read;
  fd_set write;
  fd_set except;
  int    min;
  int    max;
  size_t idx;
  int    ref;
} pollset__t;

/**********************************************************************/

static void pollset_toevents(lua_State *const L,int idx,pollset__t *set,int fd)
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

static void pollset_pushevents(lua_State *const L,pollset__t *set,int fd)
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

static int pollset_lua(lua_State *const L)
{
  pollset__t *set;
  
  set = lua_newuserdata(L,sizeof(pollset__t));
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

static int polllua___tostring(lua_State *const L)
{
  lua_pushfstring(L,"pollset (%p)",lua_touserdata(L,1));
  return 1;
}

/**********************************************************************/

static int polllua___gc(lua_State *const L)
{
  pollset__t *set = luaL_checkudata(L,1,TYPE_POLL);
  luaL_unref(L,LUA_REGISTRYINDEX,set->ref);
  return 0;
}

/**********************************************************************/

static int polllua_insert(lua_State *const L)
{
  pollset__t *set = luaL_checkudata(L,1,TYPE_POLL);
  int         fh  = luaL_checkinteger(L,2);
  
  lua_settop(L,4);
  
  if (fh > FD_SETSIZE)
  {
    lua_pushinteger(L,EINVAL);
    return 1;
  }
  
  if (fh < set->min) set->min = fh;
  if (fh > set->max) set->max = fh;
  
  pollset_toevents(L,3,set,fh);
  
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

static int polllua_update(lua_State *const L)
{
  pollset__t *set = luaL_checkudata(L,1,TYPE_POLL);
  int         fh  = luaL_checkinteger(L,2);
  
  FD_CLR(fh,&set->read);
  FD_CLR(fh,&set->write);
  FD_CLR(fh,&set->except);
  
  pollset_toevents(L,3,set,fh);
  lua_pushinteger(L,0);
  return 1;
}

/**********************************************************************/

static int polllua_remove(lua_State *const L)
{
  pollset__t *set = luaL_checkudata(L,1,TYPE_POLL);
  int         fh  = luaL_checkinteger(L,2);
  
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

static int polllua_events(lua_State *const L)
{
  pollset__t     *set     = luaL_checkudata(L,1,TYPE_POLL);
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
      pollset_pushevents(L,set,i);
      lua_pushinteger(L,i);
      lua_gettable(L,-5);
      lua_setfield(L,-2,"obj");
      lua_settable(L,-3);
    }
  }
  
  lua_pushinteger(L,0);
  return 2;
}

#endif

/**********************************************************************/

static const luaL_Reg m_polllua[] =
{
  { "__tostring"	, polllua___tostring	} ,
  { "__gc"		, polllua___gc		} ,
  { "insert"		, polllua_insert	} ,
  { "update"		, polllua_update	} ,
  { "remove"		, polllua_remove	} ,
  { "events"		, polllua_events	} ,
  { NULL		, NULL			}
};

int luaopen_org_conman_pollset(lua_State *const L)
{
  luaL_newmetatable(L,TYPE_POLL);
#if LUA_VERSION_NUM == 501
  luaL_register(L,NULL,m_polllua);
#else
  luaL_setfuncs(L,m_polllua,0);
#endif
  lua_pushliteral(L,POLLSET_IMPL);
  lua_setfield(L,-2,"_implementation");
  lua_pushvalue(L,-1);
  lua_setfield(L,-1,"__index");
  
  lua_pushcfunction(L,pollset_lua);
  return 1;
}

/**********************************************************************/
