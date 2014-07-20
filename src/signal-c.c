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
* Desc:		ANSI C interface to signal()/raise().  Given the baroque
*		nature of signals, this is a pretty simplisitic interface,
*		it pretty much just flags any caught signal, which can then
*		be tested.
*
* Example:
*

      signal = require "org.conman.signal"
      signal.catch('term')
      signal.raise('term')
      if signal.caught('term') then
        print("I will survive")
      end
      
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
* No assumptions are being made about the actual SIG* values.  As such, we
* will have a variable for each possible signal (as defined by C89) that
* will be set in the signal handler.  There is also one overall signal flag
* that will be set for every signal handled.
*
***********************************************************************/

static volatile sig_atomic_t m_caught;
static volatile sig_atomic_t m_caught_abrt;
static volatile sig_atomic_t m_caught_fpe;
static volatile sig_atomic_t m_caught_ill;
static volatile sig_atomic_t m_caught_int;
static volatile sig_atomic_t m_caught_segv;
static volatile sig_atomic_t m_caught_term;

/**********************************************************************/

static void signal_handler(int sig)
{
  signal(sig,signal_handler);
  m_caught = sig;
  
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

/**********************************************************************/

struct mapstrint
{
  const char *const text;
  const int         value;
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
  /*-------------------------------------------------
  ; NOTE: the following list must be kept sorted.
  ;--------------------------------------------------*/
  
  static const struct mapstrint sigs[] =
  {
    { "abort"		, SIGABRT	} ,
    { "abrt"		, SIGABRT	} ,
    { "fpe"		, SIGFPE	} ,
    { "ill"		, SIGILL	} ,
    { "illegal"		, SIGILL	} ,
    { "int"		, SIGINT	} ,
    { "interrupt"	, SIGINT	} ,
    { "segv"		, SIGSEGV	} ,
    { "term"		, SIGTERM	} ,
    { "terminate"	, SIGTERM	} ,
  };
  
# define MAXSIG (sizeof(sigs) / sizeof(struct mapstrint))

  struct mapstrint *entry = bsearch(
          luaL_checkstring(L,idx),
          sigs,
          MAXSIG,
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
* Return:	boolean
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
* Usage:	signal.catch(signal[,signal...])
*
* Desc:		Catches one or more signals.
*
* Input:	signal (string) name of signal
*
***********************************************************************/		

static int siglua_catch(lua_State *const L)
{
  int top = lua_gettop(L);
  int i;
  
  for (i = 1 ; i <= top ; i++)
    signal(slua_tosignal(L,i),signal_handler);
  return 0;
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

/**********************************************************************/

static const struct luaL_Reg m_sig_reg[] =
{
  { "caught"	, siglua_caught		} ,
  { "catch"	, siglua_catch		} ,
  { "ignore"	, siglua_ignore		} ,
  { "default"	, siglua_default	} ,
  { "raise"	, siglua_raise		} ,
  { NULL	, NULL			}
};

int luaopen_signal(lua_State *const L)
{
  luaL_register(L,"signal",m_sig_reg);
  return 1;
}

/**********************************************************************/

