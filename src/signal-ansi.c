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

#include <lua.h>
#include <lauxlib.h>

/**********************************************************************
*
* No assumptions are being made about the actual SIG* values (they could be
* consecutive, they could not be.  That's why I have separate variagbles for
* each signal.  Sure, it's a bit verbose, but it's a more portable approach.
*
***********************************************************************/

  /* modified in signal handler, thus the volatile modifier */
  
static volatile int           m_hookcount;
static volatile int           m_hookmask;
static volatile lua_Hook      m_hook;
static volatile sig_atomic_t  m_bam;
static volatile sig_atomic_t  m_signal;
static volatile sig_atomic_t  m_caught;
static volatile sig_atomic_t  m_caught_abrt;
static volatile sig_atomic_t  m_caught_fpe;
static volatile sig_atomic_t  m_caught_ill;
static volatile sig_atomic_t  m_caught_int;
static volatile sig_atomic_t  m_caught_segv;
static volatile sig_atomic_t  m_caught_term;

  /* set under more controlled circumstances */
  
static int                    m_ref_abrt = LUA_NOREF;
static int                    m_ref_fpe  = LUA_NOREF;
static int                    m_ref_ill  = LUA_NOREF;
static int                    m_ref_int  = LUA_NOREF;
static int                    m_ref_segv = LUA_NOREF;
static int                    m_ref_term = LUA_NOREF;
static lua_State             *m_L;

/***************************************************************************
*
* The signal handler backend. We remove any Lua debug hooks (how we got here
* in the first place) then figure out which signal was triggered.  If
* there's a function reference, call the function.  After all signals have
* been processed, restore any Lua debug hooks and continue.
*
****************************************************************************/ 

static void luasigbackend(lua_State *L,lua_Debug *ar __attribute__((unused)))
{
  volatile sig_atomic_t *ps;
  const char            *sig;
  int                    ref;
  
  lua_sethook(L,NULL,0,0);
  
  while(m_signal)
  {
    m_signal = 0;
    
    if (m_caught_abrt)
      ref = m_ref_abrt , sig = "abrt" , ps = &m_caught_abrt;
    else if (m_caught_fpe)
      ref = m_ref_fpe  , sig = "fpe"  , ps = &m_caught_fpe;
    else if (m_caught_ill)
      ref = m_ref_ill  , sig = "ill"  , ps = &m_caught_ill;
    else if (m_caught_int)
      ref = m_ref_int  , sig = "int"  , ps = &m_caught_int;
    else if (m_caught_segv)
      ref = m_ref_segv , sig = "segv" , ps = &m_caught_segv;
    else if (m_caught_term)
      ref = m_ref_term , sig = "term" , ps = &m_caught_term;
    else
      continue;
    
    if (ref != LUA_NOREF)
    {
      *ps = 0;
      lua_pushinteger(L,ref);
      lua_gettable(L,LUA_REGISTRYINDEX);
      lua_pushstring(L,sig);
      if (lua_pcall(L,1,0,0) != 0)
        lua_error(L);
    }
  }
  
  lua_sethook(L,m_hook,m_hookmask,m_hookcount);
  m_bam = 0;
}

/***************************************************************************
*
* If not already handling signals, save any Lua debug hooks, then set one so
* that anything that happens will trap to the signal backend.  We then
* re-establish the signal handler [1] and set a flag for which signal was
* triggered.
*
* [1]	The signal() handler is reset upon delivery.  Because of this, there
*	is a potential to lose a signal.  But what can you do with ANSI C
*	signal handling?
*
****************************************************************************/

static void signal_handler(int sig)
{
  if (!m_bam)
  {
    m_bam = 1;
    m_hookcount = lua_gethookcount(m_L);
    m_hookmask  = lua_gethookmask (m_L);
    m_hook      = lua_gethook     (m_L);
    
    lua_sethook(m_L,luasigbackend,LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT,1);
  }
  
  signal(sig,signal_handler);

  m_signal = m_caught = 1;

  switch(sig)
  {
    case SIGABRT: m_caught_abrt = 1; break;
    case SIGFPE:  m_caught_fpe  = 1; break;
    case SIGILL:  m_caught_ill  = 1; break;
    case SIGINT:  m_caught_int  = 1; break;
    case SIGSEGV: m_caught_segv = 1; break;
    case SIGTERM: m_caught_term = 1; break;
    default:                         break;
  }
}

/**********************************************************************
*
* Function to map a string to a signal value.
*
***********************************************************************/

struct mapstrint
{
  const char *const text;
  const int         value;
};

/*-------------------------------------------------
; NOTE: the following list must be kept sorted.
;--------------------------------------------------*/

static const struct mapstrint sigs[] =
{
  { "abort"	, SIGABRT	} ,
  { "abrt"	, SIGABRT	} ,
  { "fpe"	, SIGFPE	} ,
  { "ill"	, SIGILL	} ,
  { "illegal"	, SIGILL	} ,
  { "int"	, SIGINT	} ,
  { "interrupt"	, SIGINT	} ,
  { "segv"	, SIGSEGV	} ,
  { "term"	, SIGTERM	} ,
  { "terminate"	, SIGTERM	} ,
};

/*--------------------------------------------------------------------*/

static int mapstrintcmp(const void *needle,const void *haystack)
{
  const char             *key   = needle;
  const struct mapstrint *value = haystack;
  
  return strcmp(key,value->text);
}

/*--------------------------------------------------------------------*/

static int slua_tosignal(lua_State *const L,int idx)
{  
  struct mapstrint *entry = bsearch(
          luaL_checkstring(L,idx),
          sigs,
          sizeof(sigs) / sizeof(struct mapstrint),
          sizeof(struct mapstrint),
          mapstrintcmp
        );
  
  if (entry == NULL)
    return luaL_error(L,"signal '%s' not supported",lua_tostring(L,idx));
  else
    return entry->value;
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
  int caught;
  
  if (lua_isnoneornil(L,1))
  {
    lua_pushboolean(L,m_caught != 0);
    m_caught = 0;
    return 1;
  }
  
  switch(slua_tosignal(L,1))
  {
    case SIGABRT: caught = (m_caught_abrt != 0); m_caught_abrt = 0; break;
    case SIGFPE:  caught = (m_caught_fpe  != 0); m_caught_fpe  = 0; break;
    case SIGILL:  caught = (m_caught_ill  != 0); m_caught_ill  = 0; break;
    case SIGINT:  caught = (m_caught_int  != 0); m_caught_int  = 0; break;
    case SIGSEGV: caught = (m_caught_segv != 0); m_caught_segv = 0; break;
    case SIGTERM: caught = (m_caught_term != 0); m_caught_term = 0; break;
    default:      caught = 0;                                       break;
  }
  
  lua_pushboolean(L,caught);
  m_caught = 0;
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
  int sig;
  int ref;
  
  sig = slua_tosignal(L,1);
  
  switch(sig)
  {
    case SIGABRT: ref = m_ref_abrt; m_ref_abrt = LUA_NOREF; break;
    case SIGFPE:  ref = m_ref_fpe;  m_ref_fpe  = LUA_NOREF; break;
    case SIGILL:  ref = m_ref_ill;  m_ref_ill  = LUA_NOREF; break;
    case SIGINT:  ref = m_ref_int;  m_ref_int  = LUA_NOREF; break;
    case SIGSEGV: ref = m_ref_segv; m_ref_segv = LUA_NOREF; break;
    case SIGTERM: ref = m_ref_term; m_ref_term = LUA_NOREF; break;
    default:                                                break;
  }
  
  luaL_unref(L,LUA_REGISTRYINDEX,ref);
  
  if (lua_isfunction(L,2))
  {
    lua_pushvalue(L,2);
    ref = luaL_ref(L,LUA_REGISTRYINDEX);
      
    switch(sig)
    {
      case SIGABRT: m_ref_abrt = ref; break;
      case SIGFPE:  m_ref_fpe  = ref; break;
      case SIGILL:  m_ref_ill  = ref; break;
      case SIGINT:  m_ref_int  = ref; break;
      case SIGSEGV: m_ref_segv = ref; break;
      case SIGTERM: m_ref_term = ref; break;
      default:                        break;
    } 
  }
  
  signal(sig,signal_handler);
  lua_pushboolean(L,1);
  lua_pushinteger(L,0);
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
    signal(slua_tosignal(L,i),SIG_IGN);
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
    signal(slua_tosignal(L,i),SIG_DFL);
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
  raise(slua_tosignal(L,1));
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
  m_L = L;
  luaL_register(L,"org.conman.signal-ansi",m_sig_reg);
  return 1;
}

/**********************************************************************/

