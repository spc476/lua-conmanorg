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
* Desc:		Interface to signal()/raise().  
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

#ifdef __GNUC__
#  define _GNU_SOURCE
#else
#  define __attribute__(x)
#endif

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>

#include <lua.h>
#include <lauxlib.h>

#if !defined(LUA_VERSION_NUM) || LUA_VERSION_NUM < 501
#  error You need to compile against Lua 5.1 or higher
#endif

#if !defined(SIGNAL_ANSI) && !defined(SIGNAL_POSIX)
#  ifdef SIGCHLD
#    define SIGNAL_POSIX
#  else
#    define SIGNAL_ANSI
#  endif
#endif

/**********************************************************************
*
* The ANSI Implementatin of Lua signals.  
*
***********************************************************************/

#ifdef SIGNAL_ANSI
#define MAX_SIG	6

/**********************************************************************/

struct datasig
{
  const char            *name;
  int                    fref;
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

static volatile sig_atomic_t  m_signal;
static struct datasig         m_handlers[MAX_SIG];
static lua_State             *m_L;

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
          m_handlers[i].triggered = 0;
          lua_pushinteger(L,m_handlers[i].fref);
          lua_gettable(L,LUA_REGISTRYINDEX);
          lua_pushstring(L,m_handlers[i].name);
          lua_call(L,1,0);
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
  
  assert(isig <  MAX_SIG);  
  
  lua_sethook(
          m_L,
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
  
  ds.name      = name;
  ds.fref      = LUA_NOREF;
  ds.triggered = 0;
  
  if (lua_isfunction(L,2))
  {
    lua_pushvalue(L,2);
    ds.fref = luaL_ref(L,LUA_REGISTRYINDEX);
  }
  
  ods = m_handlers[sig];
  m_handlers[sig] = ds;
  
  if (signal(m_sigvalue[sig],signal_handler) == SIG_ERR)
  {
    m_handlers[sig] = ods;
    luaL_unref(L,LUA_REGISTRYINDEX,ds.fref);
    lua_pushboolean(L,0);
    lua_pushinteger(L,EINVAL);
  }
  else
  {
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
    m_handlers[sig].fref      = LUA_NOREF;
    m_handlers[sig].triggered = 0;
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
    m_handlers[sig].fref      = LUA_NOREF;
    m_handlers[sig].triggered = 0;
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
  lua_pushboolean(L,raise(m_sigvalue[slua_tosignal(L,1,NULL)]) == 0);
  return 1;
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

/**********************************************************************/

static const struct luaL_Reg m_sig_reg[] =
{
  { "caught"	, siglua_caught		} ,
  { "catch"	, siglua_catch		} ,
  { "ignore"	, siglua_ignore		} ,
  { "default"	, siglua_default	} ,
  { "raise"	, siglua_raise		} ,
  { "defined"	, siglua_defined	} ,
  { NULL	, NULL			}
};

int luaopen_org_conman_signal(lua_State *const L)
{
  for (size_t i = 0 ; i < MAX_SIG ; i++)
  {
    m_handlers[i].fref      = LUA_NOREF;
    m_handlers[i].triggered = 0;
    m_handlers[i].name      = NULL;
  }
  
  luaL_register(L,"org.conman.signal-ansi",m_sig_reg);
  lua_pushliteral(L,"ANSI");
  lua_setfield(L,-2,"_implementation");
  
  m_L = L;
  return 1;
}

#endif

/**********************************************************************
*
* The POSIX Implementation of Lua signals.
*
***********************************************************************/

#ifdef SIGNAL_POSIX

#include <stdbool.h>
#include <unistd.h>
#ifdef __SunOS
#  include <sys/wait.h>
#endif

#define TYPE_SIGSET	"org.conman.signal:sigset"

/**********************************************************************/

struct datasig
{
  const char            *name;
  int                    fref;
  volatile sig_atomic_t  triggered;
  sigset_t               blocked;
  siginfo_t              info;
  bool                   use_info;
};

/**********************************************************************/

static volatile sig_atomic_t  m_signal;
static struct datasig         m_handlers[NSIG];
static lua_State             *m_L;
/************************************************************************
*
* Map a reason code to a string.  Again, because of implementation details,
* I'm not going to assume the various codes don't overlap, so I'm going to
* bake this cake by the book!  I certainly don't want a messy recipe.
*
*************************************************************************/

static const char *codetostr(int sig,int code,char *dst,size_t dstsiz)
{
  switch(sig)
  {
    case SIGILL:
         switch(code)
         {
           case ILL_ILLOPC: return "ILL_ILLOPC";
           case ILL_ILLOPN: return "ILL_ILLOPN";
           case ILL_ILLADR: return "ILL_ILLADR";
           case ILL_ILLTRP: return "ILL_ILLTRP";
           case ILL_PRVOPC: return "ILL_PRVOPC";
           case ILL_PRVREG: return "ILL_PRVREG";
           case ILL_COPROC: return "ILL_COPROC";
           case ILL_BADSTK: return "ILL_BADSTK";
           default:         break;
         }
         break;
         
    case SIGFPE:
         switch(code)
         {
           case FPE_INTDIV: return "FPE_INTDIV";
           case FPE_INTOVF: return "FPE_INTOVF";
           case FPE_FLTDIV: return "FPE_FLTDIV";
           case FPE_FLTOVF: return "FPE_FLTOVF";
           case FPE_FLTUND: return "FPE_FLTUND";
           case FPE_FLTRES: return "FPE_FLTRES";
           case FPE_FLTINV: return "FPE_FLTINV";
           case FPE_FLTSUB: return "FPE_FLTSUB";
           default:         break;
         }
         break;
    
    case SIGSEGV:
         switch(code)
         {
           case SEGV_MAPERR: return "SEGV_MAPERR";
           case SEGV_ACCERR: return "SEGV_ACCERR";
           default:          break;
         }
         break;
         
    case SIGBUS:
         switch(code)
         {
           case BUS_ADRALN:    return "BUS_ADRALN";
           case BUS_ADRERR:    return "BUS_ADRERR";
           case BUS_OBJERR:    return "BUS_OBJERR";
#  ifdef BUS_MCEERR_AO
           case BUS_MCEERR_AO: return "BUS_MCEEFF_AO";
#  endif
#  ifdef BUS_MCEERR_AR
           case BUS_MCEERR_AR: return "BUS_MCEERR_AR";
#  endif
           default:            break;
         }
         break;

    case SIGTRAP:
         switch(code)
         {
           case TRAP_BRKPT:  return "TRAP_BRKPT";
           case TRAP_TRACE:  return "TRAP_TRACE";
#  ifdef TRAP_BRANCH
           case TRAP_BRANCH: return "TRAP_BRANCH";
#  endif
#  ifdef TRAP_HWBKPT
           case TRAP_HWBKPT: return "TRAP_HWBKPT";
#  endif
           default:          break;
         }
         break;

    case SIGCHLD:
         switch(code)
         {
           case CLD_EXITED:    return "CLD_EXITED";
           case CLD_KILLED:    return "CLD_KILLED";
           case CLD_DUMPED:    return "CLD_DUMPED";
           case CLD_TRAPPED:   return "CLD_TRAPPED";
           case CLD_STOPPED:   return "CLD_STOPPED";
           case CLD_CONTINUED: return "CLD_CONTINUED";
           default:            break;
         }
         break;

#ifdef SIGPOLL
    case SIGPOLL:
         switch(code)
         {
           case POLL_IN:    return "POLL_IN";
           case POLL_OUT:   return "POLL_OUT";
           case POLL_MSG:   return "POLL_MSG";
           case POLL_ERR:   return "POLL_ERR";
           case POLL_PRI:   return "POLL_PRI";
           case POLL_HUP:   return "POLL_HUP";
           default:         break;
         }
         break;
#endif

    default:
         break;
  }
  
  switch(code)
  {
    case SI_USER:    return "SI_USER";
    case SI_QUEUE:   return "SI_QUEUE";
    case SI_TIMER:   return "SI_TIMER";
    case SI_ASYNCIO: return "SI_ASYNCIO";
    case SI_MESGQ:   return "SI_MESGQ";
    default:         break;
  }
  
  snprintf(dst,dstsiz,"%d",code);
  return dst;
}

/***************************************************************************
*
* Push additional signal information onto the Lua stack.  This is a table
* with the following fields:
*
*	signal (enum)	signal name
*	errno  (number)	system error value
*	code   (enum)	signal code
*	status (table)	SIGCHLD only---child process status
*		pid (number)	Process ID
*		uid (number)	User ID
*		rc  (number)	exit value or signal
*		description (string/optional) signal description
*		status (enum)	'terminated' | 'stopped' | 'normal'
*		core (boolean/optional) core file dumped
*	addr    (number/optional) address of faulting instruction
*	band	(number/optional) band event (SIGPOLL or SIGIO)
*
**************************************************************************/

static void slua_pushinfo(
        lua_State  *const L,
        siginfo_t  *const info,
        const char *const name
)
{
  char code[65];
  
  lua_createtable(L,0,6);
  lua_pushstring(L,name);
  lua_setfield(L,-2,"signal");
  lua_pushinteger(L,info->si_errno);
  lua_setfield(L,-2,"errno");
  lua_pushstring(L,codetostr(info->si_signo,info->si_code,code,sizeof(code)));
  lua_setfield(L,-2,"code");
  
  switch(info->si_signo)
  {
    case SIGBUS:
    case SIGILL:
    case SIGFPE:
    case SIGSEGV:
         lua_pushinteger(L,(intptr_t)info->si_addr);
         lua_setfield(L,-2,"addr");
         break;

#ifdef SIGPOLL
    case SIGPOLL:
         lua_pushnumber(L,info->si_band);
         lua_setfield(L,-2,"band");
         break;
#endif

    case SIGCHLD:
         lua_createtable(L,0,8);
         lua_pushinteger(L,info->si_pid);
         lua_setfield(L,-2,"pid");
         lua_pushinteger(L,info->si_uid);
         lua_setfield(L,-2,"uid");
         
         if (WIFEXITED(info->si_status))
         {
           int rc = WEXITSTATUS(info->si_status);
           lua_pushinteger(L,rc);
           lua_setfield(L,-2,"rc");
           lua_pushfstring(
                L,
                "%s %d",
                (rc == EXIT_SUCCESS)
                        ? "success"
                        : "failure",
                rc
           );
           lua_setfield(L,-2,"description");
           lua_pushliteral(L,"normal");
           lua_setfield(L,-2,"status");
         }
         else if (WIFSTOPPED(info->si_status))
         {
           lua_pushinteger(L,WSTOPSIG(info->si_status));
           lua_setfield(L,-2,"rc");
           lua_pushstring(L,strsignal(WSTOPSIG(info->si_status)));
           lua_setfield(L,-2,"description");
           lua_pushliteral(L,"stopped");
           lua_setfield(L,-2,"status");
         }
         else if (WIFSIGNALED(info->si_status))
         {
           lua_pushinteger(L,WTERMSIG(info->si_status));
           lua_setfield(L,-2,"rc");
           lua_pushstring(L,strsignal(WTERMSIG(info->si_status)));
           lua_setfield(L,-2,"description");
           lua_pushliteral(L,"terminated");
           lua_setfield(L,-2,"status");
#ifdef WCOREDUMP
           lua_pushboolean(L,WCOREDUMP(info->si_status));
           lua_setfield(L,-2,"core");
#endif
         }
         lua_setfield(L,-2,"status");
         break;

    default:
         break;
  }
}
	
/***************************************************************************
*
* The signal handler backend. We remove any Lua debug hooks (how we got here
* in the first place) then figure out which signal was triggered.  If
* there's a function reference, call the function.  After all signals have 
* been processed, restore any Lua debug hooks and continue.
*
* When the handler is finished, unblock the signals.  If there's an error in
* the handler, signals will still be blocked.  Buyer beware.
*
****************************************************************************/

static void luasigbackend(lua_State *L,lua_Debug *ar __attribute__((unused)))
{
  lua_sethook(L,NULL,0,0);
  
  while(m_signal)
  {
    m_signal = 0;
    
    for (size_t i = 0 ; i < NSIG ; i++)
    {
      if (m_handlers[i].triggered)
      {
        if (m_handlers[i].fref != LUA_NOREF)
        {
          m_handlers[i].triggered = 0;
          lua_pushinteger(L,m_handlers[i].fref);
          lua_gettable(L,LUA_REGISTRYINDEX);
          lua_pushstring(L,m_handlers[i].name);
          
          if (m_handlers[i].use_info)
            slua_pushinfo(L,&m_handlers[i].info,m_handlers[i].name);
          else
            lua_pushnil(L);
          
          lua_call(L,2,0);
          sigprocmask(SIG_UNBLOCK,&m_handlers[i].blocked,NULL);
        }
      }
    }
  }
}

/***************************************************************************
*
* If not already handling signals, save any Lua debug hooks, then set one so
* that anything that happens will trap to the signal backend.
*
* Also, even though the handler for this signal may have other signals
* blocked that doesn't mean they'll be blocked when the actual handler runs
* (since it runs in non-signal time).  So, in the handler, we block the set
* of signals to ensure the behavior of blocked signals during the handler.
*
****************************************************************************/

static void signal_handler(int sig)
{
  lua_sethook(
          m_L,
          luasigbackend,
          LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT,
          1
  );
  
  m_handlers[sig].triggered = m_signal = 1;
  sigprocmask(SIG_BLOCK,&m_handlers[sig].blocked,NULL);
}

/**********************************************************************
*
* Pretty much the same as the above signal handler, only with addtional
* information
*
***********************************************************************/

static void signal_action(int sig,siginfo_t *info,void *_ __attribute__((unused)))
{
  lua_sethook(
          m_L,
          luasigbackend,
          LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT,
          1
  );
  
  m_handlers[sig].info      = *info;
  m_handlers[sig].triggered = m_signal = 1;
  sigprocmask(SIG_BLOCK,&m_handlers[sig].blocked,NULL);
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
  { "abort"		, SIGABRT	} ,	/* ANSI */
  { "abrt"		, SIGABRT	} ,	/* ANSI */
  { "alarm"		, SIGALRM	} ,
  { "alrm"		, SIGALRM	} ,
  { "breakpoint"	, SIGTRAP	} ,
  { "bus"		, SIGBUS	} ,
  { "child"		, SIGCHLD	} ,
  { "chld"		, SIGCHLD	} ,

#ifndef __linux__
#  ifdef SIGCLD
  { "cld"		, SIGCLD	} ,
#  endif
#endif

  { "cont"		, SIGCONT	} ,
  { "continue"		, SIGCONT	} ,

#ifdef SIGSTKFLT
  { "copstackfault"	, SIGSTKFLT	} ,
#endif

  { "cputime"		, SIGXCPU	} ,

#ifdef SIGEMT
  { "emt"		, SIGEMT	} ,
#endif

#ifdef SIGLOST
  { "filelock"		, SIGLOST	} ,
#endif

  { "filesize"		, SIGXFSZ	} ,
  { "fpe"		, SIGFPE	} ,	/* ANSI */
  { "hangup"		, SIGHUP	} ,
  { "hup"		, SIGHUP	} ,
  { "ill"		, SIGILL	} ,	/* ANSI */
  { "illegal"		, SIGILL	} ,	/* ANSI */

#ifdef SIGINFO
  { "info"		, SIGINFO	} ,
  { "information"	, SIGINFO	} ,
#endif

  { "int"		, SIGINT	} ,	/* ANSI */
  { "interrupt"		, SIGINT	} ,	/* ANSI */

#ifndef __linux__
#  ifdef SIGIO
  { "io"		, SIGIO		} ,
#  endif
#endif

#ifndef __linux__
#  ifdef SIGIOT
  { "iot"		, SIGIOT	} ,
#  endif
#endif

  { "kill"		, SIGKILL	} ,

#ifdef SIGLOST
  { "lost"		, SIGLOST	} ,
#endif

  { "pipe"		, SIGPIPE	} ,
  
#ifdef SIGPOLL
  { "poll"		, SIGPOLL	} ,
#endif

#ifdef SIGPWR
  { "power"		, SIGPWR	} ,
#endif

  { "prof"		, SIGPROF	} ,
  { "profile"		, SIGPROF	} ,

#ifdef SIGPWR
  { "pwr"		, SIGPWR	} ,
#endif

  { "quit"		, SIGQUIT	} ,
  { "segfault"		, SIGSEGV	} ,
  { "segv"		, SIGSEGV	} ,	/* ANSI */

#ifdef SIGSTKFLT
  { "stkflt"		, SIGSTKFLT	} ,
#endif

  { "stop"		, SIGSTOP	} ,
  { "sys"		, SIGSYS	} ,
  { "term"		, SIGTERM	} ,	/* ANSI */
  { "terminate"		, SIGTERM	} ,	/* ANSI */
  { "trap"		, SIGTRAP	} ,
  { "tstp"		, SIGTSTP	} ,
  { "ttin"		, SIGTTIN	} ,
  { "ttou"		, SIGTTOU	} ,
  { "ttout"		, SIGTTOU	} ,
  { "ttyin"		, SIGTTIN	} ,
  { "ttyout"		, SIGTTOU	} ,    
  { "ttystop"		, SIGTSTP	} ,
  { "urg"		, SIGURG	} ,
  { "urgent"		, SIGURG	} ,
  { "user1"		, SIGUSR1	} ,
  { "user2"		, SIGUSR2	} ,
  { "usr1"		, SIGUSR1	} ,
  { "usr2"		, SIGUSR2	} ,
  { "vtalarm"		, SIGVTALRM	} ,
  { "vtalrm"		, SIGVTALRM	} ,

#ifdef SIGWINCH
  { "winch"		, SIGWINCH	} ,
  { "windowchange"	, SIGWINCH	} ,
#endif

  { "xcpu"		, SIGXCPU	} ,
  { "xfsz"		, SIGXFSZ	} ,
};

/*--------------------------------------------------------------------*/

static int mapstrintcmp(const void *needle,const void *haystack)
{
  const char             *key   = needle;
  const struct mapstrint *value = haystack;
  
  return strcmp(key,value->text);
}

/*--------------------------------------------------------------------*/

static int slua_tosignal(lua_State *const L,int idx,const char **ps)
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

/**********************************************************************
*
* Usage:	status = signal.caught([signal])
*
* Desc:         Tests to see if the given signal has been triggered.  If no
*               signal is given, tests to see if any signal has been
*               triggered.  Only signals that are caught with
*               org.conman.signal.catch() can be tested.
*
* Input:	signal  (string/optional) name of signal
*
* Return:	status (boolean) true if signal has been caught.
*
***********************************************************************/

static int siglua_caught(lua_State *const L)
{
  if (lua_isnoneornil(L,1))
  {
    sig_atomic_t caught = 0;
    
    for (size_t i = 1 ; i < NSIG ; i++)
    {
      caught |= m_handlers[i].triggered;
      m_handlers[i].triggered = 0;
    }
    
    lua_pushboolean(L,caught);
    return 1;
  }
  
  int sig = slua_tosignal(L,1,NULL);  
  
  lua_pushboolean(L,m_handlers[sig].triggered != 0);
  m_handlers[sig].triggered = 0;
  return 1;  
}

/**********************************************************************
*
* Usage:	okay,err = signal.catch(signal[,handler[,flags][,blocked]])
*
* Desc:		Install a handler for a signal, and optionally block
*		other signals while handling the signal.
*
* Input:	signal (string) signal name
*		handler (function(sig)/optional) handler for signal
*		flags (string array/optional) various flags:
*			* 'nochildstop'	if signal is 'child', do not
*			|		receive notification when child
*			|		prcess stops
*			* 'oneshot'	restore default action after handler
*			* 'resethandler'		"
*			* 'restart'	restart system calls
*			* 'nomask'	do not mask this signal
*			* 'nodefer'		"
*		blocked (userdata(set)/optional) signals to block during
*			| handler
*
* Return:	okay (boolean) true if successful, false if error
*		err (integer) system error, 0 if successful
*
*********************************************************************/

static int slua_toflags(lua_State *const L,int idx)
{
  static const struct mapstrint tflags[] =
  {
    { "info"		, SA_SIGINFO	} ,
    { "nochildstop"	, SA_NOCLDSTOP	} ,
    { "nodefer"		, SA_NODEFER	} ,
#ifdef SA_NOMASK
    { "nomask"		, SA_NOMASK	} ,
#endif
#ifdef SA_ONESHOT
    { "oneshot"		, SA_ONESHOT	} ,
#endif
/*  { "onstack"		, SA_ONSTACK	} , */ /* need to think about this */
    { "resethandler"	, SA_RESETHAND	} ,
    { "restart"		, SA_RESTART	} ,
  };
  
  if (lua_isnil(L,idx))
    return 0;
  
  if (lua_isstring(L,idx))
  {
    const struct mapstrint *entry = bsearch(
            lua_tostring(L,idx),
            tflags,
            sizeof(tflags) / sizeof(struct mapstrint),
            sizeof(struct mapstrint),
            mapstrintcmp
    );
    if (entry == NULL)
      return luaL_error(L,"flag '%s' not supported",lua_tostring(L,idx));
    else
      return entry->value;
  }
  else if (lua_istable(L,idx))
  {
    const struct mapstrint *entry;
    int                     flags;
    int                     len;
    int                     i;
    
#if LUA_VERSION_NUM == 501
    len = lua_objlen(L,idx);
#else
    len = lua_rawlen(L,idx);
#endif
    flags = 0;
    
    for (i = 1 ; i <= len; i++)
    {
      lua_pushinteger(L,i);
      lua_gettable(L,idx);
      entry = bsearch(
                lua_tostring(L,-1),
                tflags,
                sizeof(tflags) / sizeof(struct mapstrint),
                sizeof(struct mapstrint),
                mapstrintcmp
              );
      if (entry == NULL)
        return luaL_error(L,"flag '%s' not supported",lua_tostring(L,-1));
      else
        flags |= entry->value;
    }
    
    return flags;
  }
  else
    return luaL_error(L,"wrong type");
}

/*--------------------------------------------------------------------*/      

#if defined(__APPLE__) || defined(__SunOS)
#  define FLAGSIG	SA_NODEFER
#else
#  define FLAGSIG	(SA_NODEFER | SA_NOMASK)
#endif

static int siglua_catch(lua_State *const L)
{
  const char       *name;
  int               sig = slua_tosignal(L,1,&name);
  struct datasig    ds;
  struct datasig    ods;
  struct sigaction  act;
  
  ds.name      = name;
  ds.fref      = LUA_NOREF;
  ds.triggered = 0;
  ds.use_info  = false;
  sigemptyset(&ds.blocked);
  memset(&act,0,sizeof(act));
  
  lua_settop(L,4);
  
  if (lua_isfunction(L,2))
  {
    lua_pushvalue(L,2);
    ds.fref = luaL_ref(L,LUA_REGISTRYINDEX);
  }
  
  if (lua_isuserdata(L,3))
  {
    sigset_t *set = luaL_checkudata(L,3,TYPE_SIGSET);
    act.sa_mask   = *set;
    ds.blocked    = *set;
  }
  else
    act.sa_flags |= slua_toflags(L,3);
  
  if (lua_isuserdata(L,4))
  {
    sigset_t *set = luaL_checkudata(L,4,TYPE_SIGSET);
    act.sa_mask   = *set;
    ds.blocked    = *set;
  }
  else
    act.sa_flags |= slua_toflags(L,4);
  
  if ((act.sa_flags & FLAGSIG) == 0)
    sigaddset(&ds.blocked,sig);  
  
  if ((act.sa_flags & SA_SIGINFO) == SA_SIGINFO)
  {
    ds.use_info      = true;
    act.sa_sigaction = signal_action;
  }
  else
  {
    ds.use_info    = false;
    act.sa_handler = signal_handler;
  }
  
  ods = m_handlers[sig];
  m_handlers[sig] = ds;
  
  if (sigaction(sig,&act,NULL) < 0)
  {
    m_handlers[sig] = ods;
    luaL_unref(L,LUA_REGISTRYINDEX,ds.fref);
    lua_pushboolean(L,0);
    lua_pushinteger(L,errno);
  }
  else
  {
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
  for (int top = lua_gettop(L) , i = 1 ; i <= top ; i++)
  {
    int sig = slua_tosignal(L,i,NULL);
    sigignore(sig);
    luaL_unref(L,LUA_REGISTRYINDEX,m_handlers[sig].fref);
    sigemptyset(&m_handlers[sig].blocked);
    m_handlers[sig].fref      = LUA_NOREF;
    m_handlers[sig].triggered = 0;
    m_handlers[sig].name      = NULL;
    m_handlers[sig].use_info  = false;
  }
  
  return 0;
}

/**********************************************************************
*
* Usage:        signal.default(signal[,signal...])
*
* Desc:         Set the default action for the given signals
*
* Input:        signal (string) name of signal
*
**********************************************************************/

static int siglua_default(lua_State *const L)
{
  for (int top = lua_gettop(L) , i = 1 ; i <= top ; i++)
  {
    int sig = slua_tosignal(L,i,NULL);
    sigset(sig,SIG_DFL);
    luaL_unref(L,LUA_REGISTRYINDEX,m_handlers[sig].fref);
    sigemptyset(&m_handlers[sig].blocked);
    m_handlers[sig].fref      = LUA_NOREF;
    m_handlers[sig].triggered = 0;
    m_handlers[sig].name      = NULL;
    m_handlers[sig].use_info  = false;
  }
  
  return 0;
}

/**********************************************************************
*
* Usage:        okay,err = signal.raise(signal[,pid])
*
* Desc:         Triggers the given signal.
*
* Input:        signal (string) name of signal
*		pid (integer/optional)	process ID to send signal to
* 
* Return:       okay (boolean)  true of okay, false otherwise
*               err (integer)   error value
*
* See:          org.conman.errno
*
**********************************************************************/

static int siglua_raise(lua_State *const L)
{
  if (lua_gettop(L) == 1)
    lua_pushboolean(L,raise(slua_tosignal(L,1,NULL)) == 0);
  else
    lua_pushboolean(L,kill(luaL_checkinteger(L,2),slua_tosignal(L,1,NULL)) == 0);
    
  return 1;
}

/**********************************************************************
*
* Usage:	defined = signal.defined(signal)
*
* Desc:		Return true or false if the given signal is defined
*
* Input:	signal (string) name of signal
*
* Return:	defined (boolean)
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
* Usage:        signal.allow(signal[,signal...])
*
* Desc:         Allow the given signals to be sent
*
* Input:        signal (string) name of signal
*
**********************************************************************/

static int siglua_allow(lua_State *const L)
{
  for (int top = lua_gettop(L) , i = 1 ; i <= top ; i++)
    sigrelse(slua_tosignal(L,i,NULL));
  return 0;
}    

/**********************************************************************
*
* Usage:	signal.block(signal[,signal...])
*
* Desc:		Block the signal from being sent
*
* Input:	signal (string) name of signal
*
***********************************************************************/

static int siglua_block(lua_State *const L)
{
  for (int top = lua_gettop(L) , i = 1 ; i <= top ; i++)
    sighold(slua_tosignal(L,i,NULL));
  return 0;
}

/**********************************************************************
*
* Usage:	oldset,err = signal.mask([how,]newset)
*
* Desc:		Change the set of signals being blocked.
*
* Input:	how (enum/optional)
*			'block'		- add sigals to block 
*			'unblock'	- remove signals from being blocked
*			'set'		* set the blocked signals
*		newset (userdata(set))	signals to use
*
* Return:	oldset (userdata(set)) previous set of signals, nil on errors
*		err (integer) system error, 0 on success
*
**********************************************************************/

static int siglua_mask(lua_State *const L)
{
  sigset_t *new;
  sigset_t *old;
  int       how = SIG_SETMASK;
  
  if (lua_isstring(L,1))
  {
    const char *thow = lua_tostring(L,1);
    if (strcmp(thow,"block") == 0)
      how = SIG_BLOCK;
    else if (strcmp(thow,"unblock") == 0)
      how = SIG_UNBLOCK;
    else if (strcmp(thow,"set") == 0)
      how = SIG_SETMASK;
    else
    {
      lua_pushnil(L);
      lua_pushinteger(L,EINVAL);
      return 2;
    }
    lua_remove(L,1);
  }
  
  new = luaL_checkudata(L,1,TYPE_SIGSET);
  old = lua_newuserdata(L,sizeof(sigset_t));
  luaL_getmetatable(L,TYPE_SIGSET);
  lua_setmetatable(L,-2);
  
  errno = 0;
  sigprocmask(how,new,old);
  lua_pushinteger(L,errno);
  return 2;
}

/**********************************************************************
*
* Usage:	set,err = signal.pending()
*
* Desc:		Return a set of pending signals that are blocked
*
* Return:	set (userdata(set)) set of signals, nil on error
*		err (integer) system error, 0 on success
*
*********************************************************************/

static int siglua_pending(lua_State *const L)
{
  sigset_t *set = lua_newuserdata(L,sizeof(sigset_t));
  luaL_getmetatable(L,TYPE_SIGSET);
  lua_setmetatable(L,-2);
  
  errno = 0;
  sigpending(set);
  if (errno != 0)
    lua_pushnil(L);
  lua_pushinteger(L,errno);
  return 2;
}

/**********************************************************************
*
* Usage:	okay,err = signal.suspend(set)
*
* Desc:		Temporarily replace signal mask with set, then wait for
*		a signal.  Upon signal, restore the original signal mask
*
* Input:	set (userdata(set)) set of signals
*
* Return:	okay (boolean) true if no error
*		err (integer) system error, never 0.
*
**********************************************************************/

static int siglua_suspend(lua_State *const L)
{
  sigset_t *set = luaL_checkudata(L,1,TYPE_SIGSET);
  errno         = 0;
  sigsuspend(set);
  lua_pushboolean(L,errno == 0);
  lua_pushinteger(L,errno);
  return 2;
}
                
/**********************************************************************
* 
* Usage:	set = signal.set([fill,][signal...])
*
* Desc:		Create a set of signals.  If no parameters are given,
*		return an empty set.
*
* Input:	fill (boolean/optional)
*			* true - fill the set, remove following
*			|	signals if any
*			* false (default) - empty the set, add
*			|	following signals if any
*		signal (string) name of signal
*
* Return:	set (userdata(set)) a set of signals
*
**********************************************************************/

static int siglua_set(lua_State *const L)
{
  int        top = lua_gettop(L);
  sigset_t  *set = lua_newuserdata(L,sizeof(sigset_t));
  int      (*setsig)(sigset_t *,int);
  int        start;
  
  luaL_getmetatable(L,TYPE_SIGSET);
  lua_setmetatable(L,-2);
  
  if (top == 0)
  {
    sigemptyset(set);
    return 1;
  }
  
  if (lua_isboolean(L,1))
  {
    start = 2;
    
    if (lua_toboolean(L,1))
    {
      sigfillset(set);
      setsig = (sigdelset);
    }
    else
    {
      sigemptyset(set);
      setsig = (sigaddset);
    }
  }
  else
  {
    start = 1;
    sigemptyset(set);
    setsig = (sigaddset);
  }
  
  for (int i = start ; i <= top ; i++)
    (*setsig)(set,slua_tosignal(L,i,NULL));
  
  return 1;
}

/**********************************************************************
*
*	SIGNAL SET OPERATIONS
*
* A set is a set of signals maintained as a userdata.  The following
* operations are defined:
*
*	dest signal.set('segv','term','abort')
*	src  signal.set('illegal','fpe')
*
*	isset =  dest['segv']	-- true if 'segv' signal is in set
*	dest  =  dest + src	-- add the signals from src to dest
*	dest  =  dest - src	-- remove signals in src from dest
*	dest  = -dest		-- reverse set
*
***********************************************************************/

static int sigsetmeta___tostring(lua_State *const L)
{
  lua_pushfstring(L,"sigset (%p)",lua_touserdata(L,1));
  return 1;
}

/**********************************************************************/

static int sigsetmeta___index(lua_State *const L)
{
  sigset_t *set = luaL_checkudata(L,1,TYPE_SIGSET);
  
  if (lua_isstring(L,2))
    lua_pushboolean(L,sigismember(set,slua_tosignal(L,2,NULL)));
  else
    lua_pushnil(L);
  return 1;
}

/**********************************************************************/

static int sigsetmeta___newindex(lua_State *const L)
{
  sigset_t   *sig    = luaL_checkudata(L,1,TYPE_SIGSET);
  bool        set    = lua_toboolean(L,3);
  
  if (set)
    sigaddset(sig,slua_tosignal(L,2,NULL));
  else
    sigdelset(sig,slua_tosignal(L,2,NULL));
  return 0;
}

/**********************************************************************/

static int sigsetmeta___add(lua_State *const L)
{
  sigset_t *s1 = luaL_checkudata(L,1,TYPE_SIGSET);
  sigset_t *s2 = luaL_checkudata(L,2,TYPE_SIGSET);
  sigset_t *d  = lua_newuserdata(L,sizeof(sigset_t));
  
  luaL_getmetatable(L,TYPE_SIGSET);
  lua_setmetatable(L,-2);

  *d = *s1;
  
  for (int i = 0 ; i < NSIG ; i++)
    if (sigismember(s2,i))
      sigaddset(d,i);

  return 1;
}

/**********************************************************************/

static int sigsetmeta___sub(lua_State *const L)
{
  sigset_t *s1 = luaL_checkudata(L,1,TYPE_SIGSET);
  sigset_t *s2 = luaL_checkudata(L,2,TYPE_SIGSET);
  sigset_t *d  = lua_newuserdata(L,sizeof(sigset_t));
  
  luaL_getmetatable(L,TYPE_SIGSET);
  lua_setmetatable(L,-2);

  *d = *s1;
    
  for (int i = 0 ; i < NSIG ; i++)
    if (sigismember(s2,i))
      sigdelset(d,i);
  
  return 1;
}

/**********************************************************************/

static int sigsetmeta___unm(lua_State *const L)
{
  sigset_t *s = luaL_checkudata(L,1,TYPE_SIGSET);
  sigset_t *d = lua_newuserdata(L,sizeof(sigset_t));
  
  luaL_getmetatable(L,TYPE_SIGSET);
  lua_setmetatable(L,-2);
  
  *d = *s;
  
  for (int i = 0 ; i < NSIG ; i++)
    if (sigismember(s,i))
      sigdelset(d,i);
    else
      sigaddset(d,i);
  
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
  { "allow"	, siglua_allow		} ,
  { "block"	, siglua_block		} ,
  { "mask"	, siglua_mask		} ,
  { "pending"	, siglua_pending	} ,
  { "suspend"	, siglua_suspend	} ,
  { "set"	, siglua_set		} ,
  { NULL	, NULL			}
};

static const struct luaL_Reg m_sigset_meta[] =
{
  { "__tostring", sigsetmeta___tostring	} ,
  { "__index"	, sigsetmeta___index	} ,
  { "__newindex", sigsetmeta___newindex	} ,
  { "__add"	, sigsetmeta___add	} ,
  { "__sub"	, sigsetmeta___sub	} ,
  { "__unm"	, sigsetmeta___unm	} ,
  { NULL	, NULL			}
};
 
int luaopen_org_conman_signal(lua_State *const L)
{
  for (int i = 0 ; i < NSIG ; i++)
  {
    m_handlers[i].fref      = LUA_NOREF;
    m_handlers[i].triggered = 0;
    m_handlers[i].name      = NULL;
    m_handlers[i].use_info  = false;
    sigemptyset(&m_handlers[i].blocked);
  }

  luaL_newmetatable(L,TYPE_SIGSET);
#if LUA_VERSION_NUM == 501
  luaL_register(L,NULL,m_sigset_meta); 
  luaL_register(L,"org.conman.signal-posix",m_sig_reg);
#else
  luaL_setfuncs(L,m_sigset_meta,0);
  luaL_newlib(L,m_sig_reg);
#endif
  lua_pushliteral(L,"POSIX");
  lua_setfield(L,-2,"_implementation");
  m_L = L;
  return 1;
}

#endif
