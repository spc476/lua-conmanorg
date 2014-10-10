
#define _GNU_SOURCE
#include <time.h>
#include <math.h>

#include <unistd.h>
#include <sys/time.h>

#include <lua.h>
#include <lauxlib.h>

/**************************************************************************/

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
  return 1;
}

