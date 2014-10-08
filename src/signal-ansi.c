/********************************************
*
* Copyright 2014 by Sean Conner.  All Rights Reserved.
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
* Module:	org.conman.signal
*
* Desc:		ANSI C interface to signal()/raise().  
*
* Example:
*
	signal = require "signal"

	signal.catch('term')
	signal.raise('term')

	if signal.caught('term') then
	  print("I will survive")
	end
      
	signal.catch('abrt',function(sig) print(sig,"I survived") end)
	signal.raise('abrt')
*
*********************************************************************/

#include <stddef.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <lua.h>
#include <lauxlib.h>

#if !defined(LUA_VERSION_NUM) || LUA_VERSION_NUM != 501
#  error This module is for Lua 5.1
#endif

#define MAX_SIG	6

/**********************************************************************/

struct datasig
{
  lua_State             *L;
  const char            *name;
  int                    fref;
  int                    cref;
  volatile sig_atomic_t  triggered;
};

/************************************************************************
*
* No assumpiton is made about the actual SIG* values (they could
* consecutive, they could not be).  So work around the fact that we can't
* assume the SIG* values are consecutive.
*
**************************************************************************/

static const int m_sigvalue[] =
{
  SIGABRT,
  SIGFPE,
  SIGILL,
  SIGINT,
  SIGSEGV,
  SIGTERM
};

struct mapstrint
{
  const char *const text;
  const int         value;
};

static const struct mapstrint sigs[] =
{
  { "abort"	, 0 } ,
  { "abrt"	, 0 } ,
  { "fpe"	, 1 } ,
  { "ill"	, 2 } ,
  { "illegal"	, 2 } ,
  { "int"	, 3 } ,
  { "interrupt"	, 3 } ,
  { "segfault"	, 4 } ,
  { "segv"	, 4 } ,
  { "term"	, 5 } ,
  { "terminate"	, 5 } ,
};

/*--------------------------------------------------------------------*/

static int mapstrintcmp(const void *needle,const void *haystack)
{
  const char             *key   = needle;
  const struct mapstrint *value = haystack;
  
  return strcmp(key,value->text);
}

/*--------------------------------------------------------------------*/

static int slua_tosignal(lua_State *L,int idx,const char **ps)
{
  const struct mapstrint *entry = bsearch(
          luaL_checkstring(L,idx),
          sigs,
          sizeof(sigs) / sizeof(struct mapstrint),
          sizeof(struct mapstrint),
          mapstrintcmp
  );
  
  if (entry == NULL)
    return luaL_error(L,"signal '%s' not supported",lua_tostring(L,idx));
  else
  {
    if (ps != NULL)
      *ps = entry->text;
    return entry->value;
  }
}

/**********************************************************************/

static volatile sig_atomic_t m_signal;
static struct datasig        m_handlers[MAX_SIG];

/***************************************************************************
*
* The signal handler backend---call the registered signal callback.
*
****************************************************************************/ 

static void luasigbackend(lua_State *L,lua_Debug *ar __attribute__((unused)))
{
  lua_sethook(L,NULL,0,0);
  
  while(m_signal)
  {
    m_signal = 0;
    
    for (size_t i = 0 ; i < MAX_SIG ; i++)
    {
      if (m_handlers[i].triggered)
      {
        if (m_handlers[i].fref != LUA_NOREF)
        {
          assert(m_handlers[i].L != NULL);
          m_handlers[i].triggered = 0;
          lua_pushinteger(m_handlers[i].L,m_handlers[i].fref);
          lua_gettable(m_handlers[i].L,LUA_REGISTRYINDEX);
          lua_pushstring(m_handlers[i].L,m_handlers[i].name);
          if (lua_pcall(m_handlers[i].L,1,0,0) != 0)
            lua_error(m_handlers[i].L);
        }
      }
    }
  }
}

/***************************************************************************
*
* Set a Lua debug hook to get control at the next available time (a call, a
* return, one Lua VM cycle) so we can call the registered callback (if any). 
* We then re-establish the signal handler [1] and set a flag for which
* signal was triggered.
*
* [1]	The signal() handler is reset upon delivery.  Because of this, there
*	is a potential to lose a signal.  But what can you do with ANSI C
*	signal handling?
*
****************************************************************************/

static void signal_handler(int sig)
{
  size_t isig;
  
  for (isig = 0 ; isig < MAX_SIG ; isig++)
    if (m_sigvalue[isig] == sig) break;
  
  assert(isig               <  MAX_SIG);  
  assert(m_handlers[isig].L != NULL);
  
  lua_sethook(
          m_handlers[isig].L,
          luasigbackend,
          LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT,
          1
  );
  
  m_handlers[isig].triggered = m_signal = 1;  
  signal(sig,signal_handler);
}

/**********************************************************************
*
* Usage:	status = signal.caught([signal])
*
* Desc:		Tests to see if the given signal has been triggered.  If no
*		signal is given, tests to see if any signal has been
*		triggered.  Only signals that are caught with
*		org.conman.signal.catch() can be tested.
*
* Input:	signal	(string/optional) name of signal
* 
* Return:	status (boolean) true if signal has been caught.
*
***********************************************************************/

static int siglua_caught(lua_State *const L)
{
  if (lua_isnoneornil(L,1))
  {
    sig_atomic_t caught = 0;
    
    for (size_t i = 0 ; i < MAX_SIG ; i++)
    {
      caught |= m_handlers[i].triggered;
      m_handlers[i].triggered = 0;
    }
    
    lua_pushboolean(L,caught);
    return 1;
  }
  
  int sig = slua_tosignal(L,1,NULL);

  lua_pushboolean(L,m_handlers[sig].triggered);
  m_handlers[sig].triggered = 0;
  return 1;
}

/**********************************************************************
*
* Usage:	okay,err = signal.catch(signal[,handler])
*
* Desc:		Catches one or more signals.  If no handler is given, set
*		a flag that signal.caught() can check.
*
* Input:	signal (string) name of signal
*		handler (function/optional) handler for signal.
*
* Return:	okay (boolean) true if successful, false if failed
*		err (integer)  system error, 0 if successful
*
***********************************************************************/		

static int siglua_catch(lua_State *const L)
{
  const char     *name;
  int             sig = slua_tosignal(L,1,&name);
  struct datasig  ds;
  struct datasig  ods;
  
  ds.L         = L;
  ds.name      = name;
  ds.fref      = LUA_NOREF;
  ds.cref      = LUA_NOREF;
  ds.triggered = 0;
  
  if (lua_isfunction(L,2))
  {
    lua_pushvalue(L,2);
    ds.fref = luaL_ref(L,LUA_REGISTRYINDEX);
    lua_pushthread(L);
    ds.cref = luaL_ref(L,LUA_REGISTRYINDEX);
  }
  
  ods = m_handlers[sig];
  m_handlers[sig] = ds;
  
  if (signal(m_sigvalue[sig],signal_handler) == SIG_ERR)
  {
    m_handlers[sig] = ods;
    luaL_unref(L,LUA_REGISTRYINDEX,ds.cref);
    luaL_unref(L,LUA_REGISTRYINDEX,ds.fref);
    lua_pushboolean(L,0);
    lua_pushinteger(L,EINVAL);
  }
  else
  {
    luaL_unref(L,LUA_REGISTRYINDEX,ods.cref);
    luaL_unref(L,LUA_REGISTRYINDEX,ods.fref);
    lua_pushboolean(L,1);
    lua_pushinteger(L,0);
  }
  
  return 2;
}

/**********************************************************************
*
* Usage:	signal.ignore(signal[,signal...])
*
* Desc:		Causes system to ignore one or more signals
*
* Input:	signal (string) name of signal
*
**********************************************************************/

static int siglua_ignore(lua_State *const L)
{
  int top = lua_gettop(L);
  int i;
  
  for (i = 1 ; i <= top ; i++)
  {
    
    int sig = slua_tosignal(L,1,NULL);
    signal(m_sigvalue[sig],SIG_IGN);
    luaL_unref(L,LUA_REGISTRYINDEX,m_handlers[sig].fref);
    luaL_unref(L,LUA_REGISTRYINDEX,m_handlers[sig].cref);
    m_handlers[sig].fref      = LUA_NOREF;
    m_handlers[sig].cref      = LUA_NOREF;
    m_handlers[sig].triggered = 0;
    m_handlers[sig].L         = NULL;
    m_handlers[sig].name      = NULL;
  }
  return 0;
}

/**********************************************************************
*
* Usage:	signal.default(signal[,signal...])
*
* Desc:		Set the default action for the given signals
*
* Input:	signal (string)	name of signal
*
***********************************************************************/

static int siglua_default(lua_State *const L)
{
  int top = lua_gettop(L);
  int i;
  
  for (i = 1 ; i <= top ; i++)
  {
    int sig = slua_tosignal(L,1,NULL);
    signal(m_sigvalue[sig],SIG_DFL);
    luaL_unref(L,LUA_REGISTRYINDEX,m_handlers[sig].fref);
    luaL_unref(L,LUA_REGISTRYINDEX,m_handlers[sig].cref);
    m_handlers[sig].fref      = LUA_NOREF;
    m_handlers[sig].cref      = LUA_NOREF;
    m_handlers[sig].triggered = 0;
    m_handlers[sig].L         = NULL;
    m_handlers[sig].name      = NULL;
  }
  return 0;
}

/**********************************************************************
*
* Usage:	okay,err = signal.raise(signal)
*
* Desc:		Triggers the given signal.
*
* Input:	signal (string) name of signal
*
* Return:	okay (boolean)	true of okay, false otherwise
*		err (integer)	error value
*
* See:		org.conman.errno
*
**********************************************************************/

static int siglua_raise(lua_State *const L)
{
  errno = 0;
  raise(m_sigvalue[slua_tosignal(L,1,NULL)]);
  lua_pushboolean(L,errno == 0);
  lua_pushinteger(L,errno);
  return 2;
}

/**********************************************************************
*
* Usage:        defined = signal.defined(signal)
*
* Desc:         Return true or false if the given signal is defined
*
* Input:        signal (string) name of signal
*
* Return:       defined (boolean)
*
**********************************************************************/

static int siglua_defined(lua_State *const L)
{
  lua_pushboolean(
          L,
          bsearch(
                   luaL_checkstring(L,1),
                   sigs,
                   sizeof(sigs) / sizeof(struct mapstrint),
                   sizeof(struct mapstrint),
                   mapstrintcmp
                ) != NULL
  );
  
  return 1;
}

/**********************************************************************
*
* Usage:	implementation = signal.SIGNAL()
*
* Desc:		Return the implementation of this module.
*
* Return:	implementation (string) "ANSI"
*
**********************************************************************/

static int siglua_SIGNAL(lua_State *const L)
{
  lua_pushliteral(L,"ANSI");
  return 1;
}

/**********************************************************************/

static const struct luaL_Reg m_sig_reg[] =
{
  { "caught"	, siglua_caught		} ,
  { "catch"	, siglua_catch		} ,
  { "ignore"	, siglua_ignore		} ,
  { "default"	, siglua_default	} ,
  { "raise"	, siglua_raise		} ,
  { "defined"	, siglua_defined	} ,
  { "SIGNAL"	, siglua_SIGNAL		} ,
  { NULL	, NULL			}
};

int luaopen_ansi(lua_State *const L)
{
  for (size_t i = 0 ; i < MAX_SIG ; i++)
  {
    m_handlers[i].fref      = LUA_NOREF;
    m_handlers[i].cref      = LUA_NOREF;
    m_handlers[i].triggered = 0;
    m_handlers[i].L         = NULL;
    m_handlers[i].name      = NULL;
  }
  
  luaL_register(L,"org.conman.signal-ansi",m_sig_reg);
  return 1;
}

/**********************************************************************/

