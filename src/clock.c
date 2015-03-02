/********************************************
*
* Copyright 2013 by Sean Conner.  All Rights Reserved.
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
* Module:	org.conman.clock
*
* Desc:		Wrapper for the POSIX time functions.
*
* Example:
*
                clock = require "org.conman.clock"
                clock.sleep(4) -- sleep for four seconds
                print(clock.resolution()) -- minimum sleep time
*
* Note:		The 'clocktype' parameter is ignored if the implementation
*		is 'gettimeofay'.  
*
* =========================================================================
*
* Usage:	remaining = clock.sleep(amount[,clocktype])
*
* Desc:		Pause the current process for amount seconds
*
* Input:	amount (number) number of seconds to pause
*		clocktype (enum/optional)
*			'realtime'  (default) walltime clock
*			'monotonic' a monotomically increasing clock
*
* Return:	remaining (number) number of seconds left of original
*			| amount (say, if process was interrupted by a
*			| signal).
*
* =========================================================================
*
* Usage:	now = clock.get([clocktype])
*
* Desc:		Get the current time or elaspsed time since boot
*
* Input:	clocktype (enum/optional)
*			'realtime'  (default) walltime
*			'monotonic' time since boot
*
* Return:	now (number) current time or elapsed time since boot
*
* =========================================================================
*
* Usage:	clock.set(time[,clocktype])
*
* Desc:		Set the current time (must be root)
*
* Input:	time (number) current time
*		clocktype (enum/optional)
*			'realtime' (default) walltime
*			'monotonic' time since boot
* 
* Note:		only 'realtime' clock supports this call.
*
* =========================================================================
*
* Usage:	amount = clock.resolution([clocktype])
*
* Desc:		Return the minimum "tick time" of the given clock
*
* Input:	clocktype (enum/optional)
*			'realtime' (default) walltime clock
*			'monotonic' time since boot
*
* Return:	amount (number) amount of time (in seconds) of minimal
*			| tick.
*
*****************************************************************************/

#ifdef __GNUC__
#  define _GNU_SOURCE
#endif

#include <time.h>
#include <math.h>

#include <unistd.h>
#include <sys/time.h>

#include <lua.h>
#include <lauxlib.h>


/**************************************************************************
*
* Implementation based on POSIX.1-2008, using the clock_*() functions.
*
**************************************************************************/

#ifdef CLOCK_REALTIME
#define IMPLEMENTATION	"clock_gettime"

const char *m_clocks[] =
{
  "realtime",
  "monotonic",
  NULL
};

const clockid_t m_clockids[] =
{
  CLOCK_REALTIME,
  CLOCK_MONOTONIC,
};

/**************************************************************************/

static int clocklua_sleep(lua_State *const L)
{
  clockid_t       theclock;
  struct timespec interval;
  struct timespec left;
  double          param;
  double          seconds;
  double          fract;

  param            = luaL_checknumber(L,1);
  fract            = modf(param,&seconds);
  interval.tv_sec  = (time_t)seconds;
  interval.tv_nsec = (long)(fract * 1000000000.0);
  theclock         = m_clockids[luaL_checkoption(L,2,"realtime",m_clocks)];
  left.tv_sec      = 0;
  left.tv_nsec     = 0;
  
  clock_nanosleep(theclock,0,&interval,&left);  
  lua_pushnumber(L,(double)left.tv_sec + ((double)left.tv_nsec / 1000000000.0));
  return 1;
}

/**************************************************************************/

static int clocklua_get(lua_State *const L)
{
  struct timespec now;
  
  clock_gettime(m_clockids[luaL_checkoption(L,1,"realtime",m_clocks)],&now);
  lua_pushnumber(L,(double)now.tv_sec + ((double)now.tv_nsec / 1000000000.0));
  return 1;
}

/**************************************************************************/

static int clocklua_set(lua_State *const L)
{
  clockid_t       theclock;
  struct timespec now;
  double          param;
  double          seconds;
  double          fract;

  param       = luaL_checknumber(L,1);
  fract       = modf(param,&seconds);
  now.tv_sec  = (time_t)seconds;
  now.tv_nsec = (long)(fract * 1000000000.0);
  theclock    = m_clockids[luaL_checkoption(L,2,"realtime",m_clocks)];
  
  clock_settime(theclock,&now);
  return 0;
}

/**************************************************************************/

static int clocklua_resolution(lua_State *const L)
{
  struct timespec res;
  
  clock_getres(m_clockids[luaL_checkoption(L,1,"realtime",m_clocks)],&res);
  lua_pushnumber(L,(double)res.tv_sec + ((double)res.tv_nsec / 1000000000.0));
  return 1;
}


/**************************************************************************
*
* Implementation based on older Unix standards, gettimeofday(); these have
* been obsoleted in the latest POSIX standards but some OSes (*cough* OS-X
* *cough*) haven't caught up yet.
*
**************************************************************************/

#else
#define IMPLEMENTATION	"gettimeofday"

static int clocklua_sleep(lua_State *L)
{
  struct timespec interval;
  struct timespec left;
  double          param;
  double          seconds;
  double          fract;
  
  param            = luaL_checknumber(L,1);
  fract            = modf(param,&seconds);
  interval.tv_sec  = (time_t)seconds;
  interval.tv_nsec = (long)(fract * 1000000000.0);

  nanosleep(&interval,&left);
  lua_pushnumber(L,(double)left.tv_sec + (((double)left.tv_nsec) / 1000000000.0));
  return 1;
}

/**************************************************************************/

static int clocklua_get(lua_State *L)
{
  struct timeval now;
  
  gettimeofday(&now,NULL);
  lua_pushnumber(L,(double)now.tv_sec + ((double)now.tv_usec / 1000000.0));
  return 1;
}

/**************************************************************************/

static int clocklua_set(lua_State *L)
{
  struct timeval  now;
  double          param;
  double          seconds;
  double          fract;
  
  param       = luaL_checknumber(L,1);
  fract       = modf(param,&seconds);
  now.tv_sec  = (time_t)seconds;
  now.tv_usec = (long)(fract * 1000000.0);
  settimeofday(&now,NULL);
  return 0;
}

/**************************************************************************/

static int clocklua_resolution(lua_State *L)
{
  lua_pushnumber(L,0.1); /* just a guess ... */
  return 1;
}

#endif

/**************************************************************************/

static const struct luaL_Reg m_clock_reg[] =
{
  { "sleep"		, clocklua_sleep	} ,
  { "get"		, clocklua_get		} ,
  { "set"		, clocklua_set		} ,
  { "resolution"	, clocklua_resolution	} ,  
  { NULL		, NULL			}
};

int luaopen_org_conman_clock(lua_State *const L)
{
  luaL_register(L,"org.conman.clock",m_clock_reg);
  lua_pushliteral(L,IMPLEMENTATION);
  lua_setfield(L,-2,"IMPLEMENTATION");
  return 1;
}

