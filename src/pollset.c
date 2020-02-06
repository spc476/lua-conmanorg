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

#include <stdlib.h>
#include <errno.h>

#include <lua.h>
#include <lauxlib.h>

#if !defined(LUA_VERSION_NUM) || LUA_VERSION_NUM < 501
#  error You need to compile against Lua 5.1 or higher
#endif

#if LUA_VERSION_NUM == 501
#  define lua_getuservalue(L,idx) lua_getfenv((L),(idx))
#  define lua_setuservalue(L,idx) lua_setfenv((L),(idx))
#  define luaL_setfuncs(L,reg,up) luaL_register((L),NULL,(reg))
#endif

#define TYPE_POLL       "org.conman.pollset"

#if !defined(POLLSET_IMPL_EPOLL) && !defined(POLLSET_IMPL_KQUEUE) && !defined(POLLSET_IMPL_POLL) && !defined(POLLSET_IMPL_SELECT)
#  if defined(__linux)
#    define POLLSET_IMPL_EPOLL
#  elif defined(__APPLE__)
#    define POLLSET_IMPL_KQUEUE
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
* Also, each of the implementations present the same API.  The
* org.conman.net module indicates which implementation is in use, but as
* long as you stick to select() type operations, you should be fine.
*
*	event	meaning
*	r	read ready
*	w	write ready
*	p	urgent data ready and/or error happened
*
* 	event	epoll	poll	select	kqueue
*	r	x	x	x	x
*	w	x	x	x	x
*	p	x	x	x	x
*
* Usage:	set,err = org.conman.pollset()
* Desc:		Return a file descriptor based event object
* Return:	set (userdata/set) event object, nil on error
*		err (integer) system error (0 - no error)
*
* Usage:	set._implemenation (string) one of:
*			* 'epoll'
*			* 'poll'
*			* 'select'
*			* 'kqueue'
*
* Usage:	err = set:insert(file,events[,obj])
* Desc:		Insert a file into the event object
* Input:	file (?) any object that has metatable field _tofd()
*		events (string) string of single character flag of events
*			| (see above)
*		obj (?/optional) value to associate with event
*			| (defaults to file object passed in)
* Return:	err (integer) system error (0 if no error)
*
* Usage:	err = set:update(file,events)
* Desc:		Update the flags for a file in event object
* Input:	file (?) any object that reponds to _tofd()
*		events (string) string of events (see above)
* Return:	err (integer) system error value
* Note:		file must have been added with set:insert(); othersise
*		results are undefined
*
* Usage:	err = set:remove(file)
* Desc:		Remove a file from the event object
* Input:	file (?) any object that reponds to _tofd()
* Return:	err (integer) system error value
* Note:		file must have been added with set:insert(); otherwise
*		results are undefined
*
* Usage:	events,err = set:events([timeout])
* Desc:		Return list of events
* Input:	timeout (number/optional) timeout in seconds, if not given,
*			| function will block until an event is received
* Return:	events (function) used in "for event in events do ... end"
*			| Each time the function is called, a table is
*			| returned with the following fields:
*			| * read (boolean) read event
*			| * write (boolean) write event
*			| * priority (boolean) priority data event
*			| * obj (?) value registered with set:insert()
*		err (integer) system error value
*
* Note:		Other events may be reported, such as 'error' or 'hangup'.
*		There is no exhaustive list, and the types of reports
*		are implementation dependent.
*
* LINUX implementation of pollset, using epoll()
*
*************************************************************************/

#ifdef POLLSET_IMPL_EPOLL
#define POLLSET_IMPL    "epoll"

#include <unistd.h>
#include <sys/epoll.h>

typedef struct
{
  int                 efh;
  size_t              idx;
  struct epoll_event *list;
  int                 max;
  int                 count;
} pollset__t;

/**********************************************************************/

static int pollset_toevents(lua_State *L,int idx)
{
  int events = 0;
  
  for (char const *flags = luaL_checkstring(L,idx) ; *flags ; flags++)
  {
    switch(*flags)
    {
      case 'r': events |= EPOLLIN;      break;
      case 'w': events |= EPOLLOUT;     break;
      case 'p': events |= EPOLLPRI;     break;
      default:  break;
    }
  }
  
  return events;
}

/**********************************************************************/

static void pollset_pushevents(lua_State *L,int events)
{
  lua_createtable(L,0,5);
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
}

/**********************************************************************/

static int pollset_next(lua_State *L)
{
  pollset__t *set = luaL_checkudata(L,lua_upvalueindex(1),TYPE_POLL);
  
  if (set->count < set->max)
  {
    lua_getuservalue(L,lua_upvalueindex(1));
    pollset_pushevents(L,set->list[set->count].events);
    lua_pushinteger(L,set->list[set->count].data.fd);
    lua_gettable(L,-3);
    lua_setfield(L,-2,"obj");
    set->count++;
  }
  else
  {
    free(set->list);
    set->list = NULL;
    lua_pushnil(L);
  }
  
  return 1;
}

/**********************************************************************/

static int pollset_lua(lua_State *L)
{
  pollset__t *set;
  int         efh;
  
  efh = epoll_create(10);
  if (efh == -1)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    return 2;
  }
  
  set        = lua_newuserdata(L,sizeof(pollset__t));
  set->efh   = efh;
  set->list  = NULL;
  set->idx   = 0;
  set->max   = 0;
  set->count = 0;
  
  lua_createtable(L,0,0);
  lua_setuservalue(L,-2);
  luaL_getmetatable(L,TYPE_POLL);
  lua_setmetatable(L,-2);
  lua_pushinteger(L,0);
  return 2;
}

/**********************************************************************/

static int polllua___len(lua_State *L)
{
  pollset__t *set = luaL_checkudata(L,1,TYPE_POLL);
  lua_pushinteger(L,set->idx);
  return 1;
}

/**********************************************************************/

static int polllua___tostring(lua_State *L)
{
  lua_pushfstring(L,"pollset (%p)",lua_touserdata(L,1));
  return 1;
}

/**********************************************************************/

static int polllua___gc(lua_State *L)
{
  pollset__t *set = luaL_checkudata(L,1,TYPE_POLL);
  if (set->list)
    free(set->list);
  close(set->efh);
  return 0;
}

/**********************************************************************/

static int polllua_insert(lua_State *L)
{
  pollset__t         *set = luaL_checkudata(L,1,TYPE_POLL);
  int                 fh;
  struct epoll_event  event;
  
  lua_settop(L,4);
  
  if (!luaL_callmeta(L,2,"_tofd"))
  {
    lua_pushinteger(L,EINVAL);
    return 1;
  }
  
  fh            = luaL_checkinteger(L,-1);
  event.events  = pollset_toevents(L,3);
  event.data.fd = fh;
  
  if (epoll_ctl(set->efh,EPOLL_CTL_ADD,event.data.fd,&event) < 0)
  {
    lua_pushinteger(L,errno);
    return 1;
  }
  
  lua_getuservalue(L,1);
  lua_pushinteger(L,fh);
  
  if (lua_isnil(L,4))
    lua_pushinteger(L,fh);
  else
    lua_pushvalue(L,4);
    
  lua_settable(L,-3);
  
  set->idx++;
  lua_pushinteger(L,0);
  return 1;
}

/**********************************************************************/

static int polllua_update(lua_State *L)
{
  pollset__t         *set = luaL_checkudata(L,1,TYPE_POLL);
  int                 fh;
  struct epoll_event  event;
  
  lua_settop(L,3);
  if (!luaL_callmeta(L,2,"_tofd"))
  {
    lua_pushinteger(L,EINVAL);
    return 1;
  }
  
  fh            = luaL_checkinteger(L,-1);
  event.events  = pollset_toevents(L,3);
  event.data.fd = fh;
  errno         = 0;
  
  epoll_ctl(set->efh,EPOLL_CTL_MOD,fh,&event);
  lua_pushinteger(L,errno);
  return 1;
}

/**********************************************************************/

static int polllua_remove(lua_State *L)
{
  pollset__t         *set = luaL_checkudata(L,1,TYPE_POLL);
  int                 fh;
  struct epoll_event  event;
  
  if (!luaL_callmeta(L,2,"_tofd"))
  {
    lua_pushinteger(L,EINVAL);
    return 1;
  }
  
  fh = luaL_checkinteger(L,-1);
  
  if (epoll_ctl(set->efh,EPOLL_CTL_DEL,fh,&event) < 0)
  {
    lua_pushinteger(L,errno);
    return 1;
  }
  
  lua_getuservalue(L,1);
  lua_pushinteger(L,fh);
  lua_pushnil(L);
  lua_settable(L,-3);
  
  set->idx--;
  lua_pushinteger(L,0);
  return 1;
}

/**********************************************************************/
  
static int polllua_events(lua_State *L)
{
  pollset__t         *set      = luaL_checkudata(L,1,TYPE_POLL);
  lua_Number          dtimeout = luaL_optnumber(L,2,-1.0);
  int                 timeout;
  
  if (dtimeout < 0)
    timeout = -1;
  else
    timeout = (int)(dtimeout * 1000.0);
  
  if (set->idx > 0)
  {
    set->list = calloc(set->idx,sizeof(struct epoll_event));
    if (set->list == NULL)
      set->max = -1;
    else
      set->max = epoll_wait(set->efh,set->list,set->idx,timeout);
  }
  else
    set->max = 0;
  
  if (set->max < 0)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    free(set->list);
    return 2;
  }
  
  set->count = 0;
  
  lua_pushvalue(L,1);
  lua_pushcclosure(L,pollset_next,1);
  lua_pushinteger(L,0);
  return 2;
}

#endif

/*********************************************************************
*
* kqueue() based version, used for *BSD and Mac OS-X
*
*********************************************************************/

#ifdef POLLSET_IMPL_KQUEUE
#define POLLSET_IMPL	"kqueue"

#include <math.h>
#include <stdbool.h>

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>

typedef struct
{
  int            qfh;
  size_t         idx;
  struct kevent *list;
  int            max;
  int            count;
} pollset__t;

/**********************************************************************/

static void pollset_toevents(lua_State *L,int idx,struct kevent filters[2])
{
  bool do_read  = false;
  bool do_write = false;
  bool do_pri   = false;
  
  for (char const *flags = luaL_checkstring(L,idx) ; *flags ; flags++)
  {
    switch(*flags)
    {
      case 'r': do_read  = true; break;
      case 'w': do_write = true; break;
      case 'p': do_pri   = true; break;
      default: break;
    }
  }
  
  filters[0].flags |= (do_read || do_pri) ? EV_ENABLE : EV_DISABLE;
  filters[1].flags |= (do_write)          ? EV_ENABLE : EV_DISABLE;
}

/**********************************************************************/

static void pollset_pushevents(lua_State *L,struct kevent const *event)
{
  bool read = event->filter == EVFILT_READ;
  bool pri  = event->flags & EV_OOBAND;
  bool eof  = event->flags & EV_EOF;
  
  lua_createtable(L,0,4);
  lua_pushboolean(L,read & pri);
  lua_setfield(L,-2,"priority");
  lua_pushboolean(L,read & !pri);
  lua_setfield(L,-2,"read");
  lua_pushboolean(L,!read);
  lua_setfield(L,-2,"write");
  lua_pushboolean(L,eof);
  lua_setfield(L,-2,"hangup");
}

/**********************************************************************/

static int pollset_next(lua_State *L)
{
  pollset__t *set = luaL_checkudata(L,lua_upvalueindex(1),TYPE_POLL);
  
  if (set->count < set->max)
  {
    lua_getuservalue(L,lua_upvalueindex(1));
    pollset_pushevents(L,&set->list[set->count]);
    lua_pushinteger(L,set->list[set->count].ident);
    lua_gettable(L,-3);
    lua_setfield(L,-2,"obj");
    set->count++;
  }
  else
  {
    free(set->list);
    set->list = NULL;
    lua_pushnil(L);
  }
  
  return 1;
}

/**********************************************************************/

static int pollset_lua(lua_State *L)
{
  pollset__t *set;
  int         qfh;
  
  qfh = kqueue();
  if (qfh == -1)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    return 2;
  }
  
  set        = lua_newuserdata(L,sizeof(pollset__t));
  set->qfh   = qfh;
  set->list  = NULL;
  set->idx   = 0;
  set->max   = 0;
  set->count = 0;
  
  lua_createtable(L,0,0);
  lua_setuservalue(L,1);
  luaL_getmetatable(L,TYPE_POLL);
  lua_setmetatable(L,-2);
  lua_pushinteger(L,0);
  return 2;
}

/**********************************************************************/

static int polllua___len(lua_State *L)
{
  pollset__t *set = luaL_checkudata(L,1,TYPE_POLL);
  lua_pushinteger(L,set->idx);
  return 1;
}

/**********************************************************************/

static int polllua___tostring(lua_State *L)
{
  lua_pushfstring(L,"pollset (%p)",luaL_checkudata(L,1,TYPE_POLL));
  return 1;
}

/**********************************************************************/

static int polllua___gc(lua_State *L)
{
  pollset__t *set = luaL_checkudata(L,1,TYPE_POLL);
  if (set->list)
    free(set->list);
  close(set->qfh);
  return 0;
}

/**********************************************************************/

static int polllua_insert(lua_State *L)
{
  pollset__t    *set;
  int            backlog;
  struct kevent  filters[2];
  int            fh;
  
  lua_settop(L,4);
  
  if (!luaL_callmeta(L,2,"_tofd"))
  {
    lua_pushinteger(L,EINVAL);
    return 1;
  }

  set     = luaL_checkudata(L,1,TYPE_POLL);
  fh      = luaL_checkinteger(L,-1);
  backlog = luaL_optinteger(L,5,0);
  
  filters[0].ident  = fh;
  filters[0].filter = EVFILT_READ;
  filters[0].flags  = EV_ADD;
  filters[0].fflags = 0;
  filters[0].data   = backlog;
  filters[0].udata  = NULL;
  
  filters[1].ident  = fh;
  filters[1].filter = EVFILT_WRITE;
  filters[1].flags  = EV_ADD;
  filters[1].fflags = 0;
  filters[1].data   = 0;
  filters[1].udata  = NULL;
  
  pollset_toevents(L,3,filters);
  
  if (kevent(set->qfh,filters,2,NULL,0,NULL) == -1)
  {
    lua_pushinteger(L,errno);
    return 1;
  }
  
  lua_getuservalue(L,1);
  lua_pushinteger(L,fh);
  
  if (lua_isnil(L,4))
    lua_pushinteger(L,4);
  else
    lua_pushvalue(L,4);
  
  lua_settable(L,-3);
  
  set->idx++;
  lua_pushinteger(L,0);
  return 1;
}

/**********************************************************************/

static int polllua_update(lua_State *L)
{
  pollset__t    *set;
  struct kevent  filters[2];
  int            fh;
  
  lua_settop(L,3);
  if (!luaL_callmeta(L,2,"_tofd"))
  {
    lua_pushinteger(L,EINVAL);
    return 1;
  }
  
  set    = luaL_checkudata(L,1,TYPE_POLL);
  fh     = luaL_checkinteger(L,-1);
  
  filters[0].ident  = fh;
  filters[0].filter = EVFILT_READ;
  filters[0].flags  = EV_ADD;
  filters[0].fflags = 0;
  filters[0].data   = 0;
  filters[0].udata  = NULL;
  
  filters[1].ident  = fh;
  filters[1].filter = EVFILT_WRITE;
  filters[1].flags  = EV_ADD;
  filters[1].fflags = 0;
  filters[1].data   = 0;
  filters[1].udata  = NULL;
  
  pollset_toevents(L,3,filters);
  errno = 0;
  kevent(set->qfh,filters,2,NULL,0,NULL);
  lua_pushinteger(L,errno);
  return 1;
}

/**********************************************************************/

static int polllua_remove(lua_State *L)
{
  pollset__t    *set;
  struct kevent  filters[2];
  int            fh;
  
  if (!luaL_callmeta(L,2,"_tofd"))
  {
    lua_pushinteger(L,EINVAL);
    return 1;
  }

  set = luaL_checkudata(L,1,TYPE_POLL);
  fh  = luaL_checkinteger(L,-1);
  
  filters[0].ident  = fh;
  filters[0].filter = EVFILT_READ;
  filters[0].flags  = EV_DELETE;
  filters[0].fflags = 0;
  filters[0].data   = 0;
  filters[0].udata  = NULL;
  
  filters[1].ident  = fh;
  filters[1].filter = EVFILT_WRITE;
  filters[1].flags  = EV_DELETE;
  filters[1].fflags = 0;
  filters[1].data   = 0;
  filters[1].udata  = NULL;
  
  if (kevent(set->qfh,filters,2,NULL,0,NULL) == -1)
  {
    lua_pushinteger(L,errno);
    return 1;
  }
  
  lua_getuservalue(L,1);
  lua_pushinteger(L,fh);
  lua_pushnil(L);
  lua_settable(L,-3);
  
  set->idx--;
  lua_pushinteger(L,0);
  return 1;
}

/**********************************************************************/

static int polllua_events(lua_State *L)
{
  pollset__t      *set      = luaL_checkudata(L,1,TYPE_POLL);
  lua_Number       dtimeout = luaL_optnumber(L,2,-1.0);
  struct timespec *ptimeout;
  struct timespec  timeout;
  
  if (dtimeout >= 0)
  {
    double seconds;
    double fract    = modf(dtimeout,&seconds);
    timeout.tv_sec  = (time_t)seconds;
    timeout.tv_nsec = (long)(fract * 1000000000.0);
    ptimeout        = &timeout;
  }
  else
    ptimeout = NULL;
  
  if (set->idx > 0)
  {
    set->list = calloc(set->idx,sizeof(struct kevent));
    if (set->list == NULL)
      count = -1;
    else
      count = kevent(set->qfh,NULL,0,set->list,set->idx,ptimeout);
  }
  else
    count = 0;
  
  if (count < 0)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    free(set->list);
    return 2;
  }
  
  set->count = 0;
  
  lua_pushvalue(L,1);
  lua_pushcclosure(L,pollset_next,1);
  lua_pushinteger(L,0);
  return 2;
}

#endif

/*********************************************************************
*
* poll() based version, used for Unix systems that don't have a better
* event mechanism.
*
* This implementation uses the memory allocation functions for the
* given Lua state.  I felt this was a Good Idea(TM), since if a Lua
* instance is using a particular memory allocation scheme, there is
* probably a good reason for it.
*********************************************************************/

#ifdef POLLSET_IMPL_POLL
#define POLLSET_IMPL    "poll"

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
  size_t         count;
} pollset__t;

/**********************************************************************/

static int pollset_toevents(lua_State *L,int idx)
{
  int events = 0;
  
  for (char const *flags = luaL_checkstring(L,idx) ; *flags ; flags++)
  {
    switch(*flags)
    {
      case 'r': events |= POLLIN;   break;
      case 'w': events |= POLLOUT;  break;
      case 'p': events |= POLLPRI;  break;
      default:  break;
    }
  }
  
  return events;
}

/**********************************************************************/

static void pollset_pushevents(lua_State *L,int events)
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

static int pollset_next(lua_State *L)
{
  pollset__t *set = luaL_checkudata(L,lua_upvalueindex(1),TYPE_POLL);
  
  while(set->count < set->idx)
  {
    if (set->set[set->count].revents != 0)
    {
      lua_getuservalue(L,lua_upvalueindex(1));
      pollset_pushevents(L,set->set[set->count].revents);
      lua_pushinteger(L,set->set[set->count].fd);
      lua_gettable(L,-3);
      lua_setfield(L,-2,"obj");
      set->count++;
      return 1;
    }
    set->count++;
  }
  
  lua_pushnil(L);
  return 1;
}

/**********************************************************************/

static int pollset_lua(lua_State *L)
{
  pollset__t *set;
  
  set = lua_newuserdata(L,sizeof(pollset__t));
  set->set = NULL;
  set->idx = 0;
  set->max = 0;
  
  lua_createtable(L,0,0);
  lua_setuservalue(L,1);
  luaL_getmetatable(L,TYPE_POLL);
  lua_setmetatable(L,-2);
  return 1;
}

/**********************************************************************/

static int polllua___len(lua_State *L)
{
  pollset__t *set = luaL_checkudata(L,1,TYPE_POLL);
  lua_pushinteger(L,set->idx);
  return 1;
}

/**********************************************************************/

static int polllua___tostring(lua_State *L)
{
  lua_pushfstring(L,"pollset (%p)",lua_touserdata(L,1));
  return 1;
}

/**********************************************************************/

static int polllua___gc(lua_State *L)
{
  void       *ud;
  pollset__t *set    = luaL_checkudata(L,1,TYPE_POLL);
  lua_Alloc   allocf = lua_getallocf(L,&ud);
  (*allocf)(ud,set->set,set->max * sizeof(struct pollfd),0);
  return 0;
}

/**********************************************************************/

static int polllua_insert(lua_State *L)
{
  pollset__t *set = luaL_checkudata(L,1,TYPE_POLL);
  int         fh;
  
  lua_settop(L,4);
  
  if (!luaL_callmeta(L,2,"_tofd"))
  {
    lua_pushinteger(L,EINVAL);
    return 1;
  }
  
  fh = luaL_checkinteger(L,-1);
  
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
  
  lua_getuservalue(L,1);
  lua_pushinteger(L,fh);
  
  if (lua_isnil(L,4))
    lua_pushinteger(L,fh);
  else
    lua_pushvalue(L,4);
    
  lua_settable(L,-3);
  
  set->idx++;
  lua_pushinteger(L,0);
  return 1;
}

/**********************************************************************/

static int polllua_update(lua_State *L)
{
  pollset__t *set = luaL_checkudata(L,1,TYPE_POLL);
  int         fh;
  
  lua_settop(L,3);
  
  if (!luaL_callmeta(L,2,"_tofd"))
  {
    lua_pushinteger(L,EINVAL);
    return 1;
  }
  
  fh = luaL_checkinteger(L,-1);
  
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

static int polllua_remove(lua_State *L)
{
  pollset__t *set = luaL_checkudata(L,1,TYPE_POLL);
  int         fh;
  
  if (!luaL_callmeta(L,2,"_tofd"))
  {
    lua_pushinteger(L,EINVAL);
    return 1;
  }
  
  fh = luaL_checkinteger(L,-1);
  
  for (size_t i = 0 ; i < set->idx ; i++)
  {
    if (set->set[i].fd == fh)
    {
      lua_getuservalue(L,1);
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

static int polllua_events(lua_State *L)
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
  
  set->count = 0;
  
  lua_pushvalue(L,1);
  lua_pushcclosure(L,pollset_next,1);
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
#define POLLSET_IMPL    "select"

#include <math.h>
#include <sys/select.h>

typedef struct
{
  fd_set read;
  fd_set write;
  fd_set except;
  fd_set sread;
  fd_set swrite;
  fd_set sexcept;
  int    min;
  int    max;
  int    count;
  size_t idx;
} pollset__t;

/**********************************************************************/

static void pollset_toevents(lua_State *L,int idx,pollset__t *set,int fd)
{
  for (char const *flags = luaL_checkstring(L,idx) ; *flags ; flags++)
  {
    switch(*flags)
    {
      case 'r': FD_SET(fd,&set->read);   break;
      case 'w': FD_SET(fd,&set->write);  break;
      case 'p': FD_SET(fd,&set->except); break;
      default:  break;
    }
  }
}

/**********************************************************************/

static void pollset_pushevents(lua_State *L,pollset__t *set,int fd)
{
  lua_createtable(L,0,4);
  lua_pushboolean(L,FD_ISSET(fd,&set->read));
  lua_setfield(L,-2,"read");
  lua_pushboolean(L,FD_ISSET(fd,&set->write));
  lua_setfield(L,-2,"write");
  lua_pushboolean(L,FD_ISSET(fd,&set->except));
  lua_setfield(L,-2,"priority");
}

/**********************************************************************/

static int pollset_next(lua_State *L)
{
  pollset__t *set = luaL_checkudata(L,lua_upvalueindex(1),TYPE_POLL);
  
  while(set->count <= set->max)
  {
    if (FD_ISSET(set->count,&set->sread) || FD_ISSET(set->count,&set->swrite) || FD_ISSET(set->count,&set->sexcept))
    {
      lua_getuservalue(L,lua_upvalueindex(1));
      pollset_pushevents(L,set,set->count);
      lua_pushinteger(L,set->count);
      lua_gettable(L,-3);
      lua_setfield(L,-2,"obj");
      set->count++;
      return 1;
    }
    set->count++;
  }
  lua_pushnil(L);
  return 1;
}

/**********************************************************************/

static int pollset_lua(lua_State *L)
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
  lua_setuservalue(L,1);
  luaL_getmetatable(L,TYPE_POLL);
  lua_setmetatable(L,-2);
  return 1;
}

/**********************************************************************/

static int polllua___len(lua_State *L)
{
  pollset__t *set = luaL_checkudata(L,1,TYPE_POLL);
  lua_pushinteger(L,set->idx);
  return 1;
}

/**********************************************************************/

static int polllua___tostring(lua_State *L)
{
  lua_pushfstring(L,"pollset (%p)",lua_touserdata(L,1));
  return 1;
}

/**********************************************************************/

static int polllua___gc(lua_State *L)
{
  (void)L;
  return 0;
}

/**********************************************************************/

static int polllua_insert(lua_State *L)
{
  pollset__t *set = luaL_checkudata(L,1,TYPE_POLL);
  int         fh;
  
  lua_settop(L,4);
  
  if (!luaL_callmeta(L,2,"_tofd"))
  {
    lua_pushinteger(L,EINVAL);
    return 1;
  }
  
  fh = luaL_checkinteger(L,-1);
  
  if (fh > FD_SETSIZE)
  {
    lua_pushinteger(L,EINVAL);
    return 1;
  }
  
  if (fh < set->min) set->min = fh;
  if (fh > set->max) set->max = fh;
  
  pollset_toevents(L,3,set,fh);
  
  lua_getuservalue(L,1);
  lua_pushinteger(L,fh);
  
  if (lua_isnil(L,4))
    lua_pushinteger(L,fh);
  else
    lua_pushvalue(L,4);
    
  lua_settable(L,-3);
  
  set->idx++;
  lua_pushinteger(L,0);
  return 1;
}

/**********************************************************************/

static int polllua_update(lua_State *L)
{
  pollset__t *set = luaL_checkudata(L,1,TYPE_POLL);
  int         fh;
  
  if (!luaL_callmeta(L,2,"_tofd"))
  {
    lua_pushinteger(L,EINVAL);
    return 1;
  }
  
  fh = luaL_checkinteger(L,-1);
  
  FD_CLR(fh,&set->read);
  FD_CLR(fh,&set->write);
  FD_CLR(fh,&set->except);
  
  pollset_toevents(L,3,set,fh);
  lua_pushinteger(L,0);
  return 1;
}

/**********************************************************************/

static int polllua_remove(lua_State *L)
{
  pollset__t *set = luaL_checkudata(L,1,TYPE_POLL);
  int         fh;
  
  if (!luaL_callmeta(L,2,"_tofd"))
  {
    lua_pushinteger(L,EINVAL);
    return 1;
  }
  
  fh = luaL_checkinteger(L,-1);
  
  if (FD_ISSET(fh,&set->read) || FD_ISSET(fh,&set->write) || FD_ISSET(fh,&set->except))
  {
    FD_CLR(fh,&set->read);
    FD_CLR(fh,&set->write);
    FD_CLR(fh,&set->except);
    
    lua_getuservalue(L,1);
    lua_pushinteger(L,fh);
    lua_pushnil(L);
    lua_settable(L,-3);
    
    set->idx--;
    lua_pushinteger(L,0);
  }
  else
    lua_pushinteger(L,EINVAL);
    
  return 1;
}

/**********************************************************************/

static int polllua_events(lua_State *L)
{
  pollset__t     *set     = luaL_checkudata(L,1,TYPE_POLL);
  double          timeout = luaL_optnumber(L,2,-1.0);
  struct timeval  tout;
  struct timeval *ptout;
  int             rc;
  
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
  
  set->sread   = set->read;
  set->swrite  = set->write;
  set->sexcept = set->except;
  
  rc = select(FD_SETSIZE,&set->sread,&set->swrite,&set->sexcept,ptout);
  if (rc < 0)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    return 2;
  }
  
  set->count = 0;
  lua_pushvalue(L,1);
  lua_pushcclosure(L,pollset_next,1);
  lua_pushinteger(L,0);
  return 2;
}

#endif

/**********************************************************************/

static luaL_Reg const m_polllua[] =
{
  { "__len"             , polllua___len         } ,
  { "__tostring"        , polllua___tostring    } ,
  { "__gc"              , polllua___gc          } ,
  { "insert"            , polllua_insert        } ,
  { "update"            , polllua_update        } ,
  { "remove"            , polllua_remove        } ,
  { "events"            , polllua_events        } ,
  { NULL                , NULL                  }
};

int luaopen_org_conman_pollset(lua_State *L)
{
  luaL_newmetatable(L,TYPE_POLL);
  luaL_setfuncs(L,m_polllua,0);
  lua_pushliteral(L,POLLSET_IMPL);
  lua_setfield(L,-2,"_implementation");
  lua_pushvalue(L,-1);
  lua_setfield(L,-1,"__index");
  
  lua_pushcfunction(L,pollset_lua);
  return 1;
}

/**********************************************************************/
