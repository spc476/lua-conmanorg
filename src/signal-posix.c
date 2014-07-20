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
* Module:	org.conman.signal	(POSIX)
*
* Desc:		POSIX interface to signal()/raise().
*
* Example:
*

        signal = require "org.conman.signal"
        
        signal.catch('term',function(sig)
            signal.raise('int')
            print(sig,"I can survive")
          end,
          signal.set('int')
        )
        
        signal.catch('int',function(sig) print("interrupt") end)
        
        signal.raise('term')
        
*
*********************************************************************/


/* http://pubs.opengroup.org/onlinepubs/7908799/xsh/signal.h.html */

#ifdef __linux
#  define _GNU_SOURCE
#endif

#ifndef __GNUC__
#  define __attribute__(x)
#endif

#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <sysexits.h>

#include <lua.h>
#include <lauxlib.h>

#define TYPE_SIGSET	"org.conman.signal:sigset"

/**********************************************************************/

struct datasig
{
  volatile sig_atomic_t triggered;
  int                   coderef;
  sigset_t              blocked;
};

/**********************************************************************/

static volatile sig_atomic_t  m_signal;
static volatile sig_atomic_t  m_bam;
static volatile int           m_hookcount;
static volatile int           m_hookmask;
static volatile lua_Hook      m_hook;
static lua_State             *m_L;
static struct datasig         m_handlers[NSIG];

/**********************************************************************
*
* SIGNAL HANDLER CODE
* 
**********************************************************************/

static void luasigstop(lua_State *L,lua_Debug *ar __attribute__((unused)))
{
  lua_sethook(L,NULL,0,0);
  
  while(m_signal)
  {
    m_signal = 0;
    for (size_t i = 0 ; i < NSIG ; i++)
    {
      if (m_handlers[i].triggered)
      {
        m_handlers[i].triggered = 0;
        lua_pushinteger(L,m_handlers[i].coderef);
        lua_gettable(L,LUA_REGISTRYINDEX);      
        lua_pushinteger(L,i);
        if (lua_pcall(L,1,0,0) != 0)
          lua_error(L);
        sigprocmask(SIG_UNBLOCK,&m_handlers[i].blocked,NULL);
      }
    }
  }
  
  lua_sethook(L,m_hook,m_hookmask,m_hookcount);
  m_bam = 0;
}

/**********************************************************************/

static void signal_handler(int sig)
{
  if (!m_bam)
  {
    m_bam       = 1;
    m_hookcount = lua_gethookcount(m_L);
    m_hookmask  = lua_gethookmask (m_L);
    m_hook      = lua_gethook     (m_L);
    
    lua_sethook(
            m_L,
            luasigstop,
            LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT,
            1
    );
  }
  
  sigprocmask(SIG_BLOCK,&m_handlers[sig].blocked,NULL);
  m_signal = m_handlers[sig].triggered = 1;
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
  static const struct mapstrint sigs[] =
  {
    { "abort"		, SIGABRT	} ,	/* */
    { "abrt"		, SIGABRT	} ,	/* */
#ifdef SIGALRM
    { "alarm"		, SIGALRM	} ,
    { "alrm"		, SIGALRM	} ,
#endif
#ifdef SIGTRAP
    { "breakpoint"	, SIGTRAP	} ,
#endif
#ifdef SIGBUS
    { "bus"		, SIGBUS	} ,
#endif
#ifdef SIGCHLD
    { "child"		, SIGCHLD	} ,
    { "chld"		, SIGCHLD	} ,
#endif
#ifdef SIGCLD
    { "cld"		, SIGCLD	} ,
#endif
#ifdef SIGCONT
    { "cont"		, SIGCONT	} ,
    { "continue"	, SIGCONT	} ,
#endif
#ifdef SIGSTKFLT
    { "copstackfault"	, SIGSTKFLT	} ,
#endif
#ifdef SIGXCPU
    { "cputime"		, SIGXCPU	} ,
#endif
#ifdef SIGEMT
    { "emt"		, SIGEMT	} ,
#endif
#ifdef SIGLOST
    { "filelock"	, SIGLOST	} ,
#endif
#ifdef SIGXFSZ
    { "filesize"	, SIGXFSZ	} ,
#endif
    { "fpe"		, SIGFPE	} ,	/* */
#ifdef SIGHUP
    { "hangup"		, SIGHUP	} ,
    { "hup"		, SIGHUP	} ,
#endif
    { "ill"		, SIGILL	} ,	/* */
    { "illegal"		, SIGILL	} ,	/* */
#ifdef SIGINFO
    { "info"		, SIGINFO	} ,
    { "information"	, SIGINFO	} ,
#endif
    { "int"		, SIGINT	} ,	/* */
    { "interrupt"	, SIGINT	} ,	/* */
#ifdef SIGIO
    { "io"		, SIGIO		} ,
#endif
#ifdef SIGIOT
    { "iot"		, SIGIOT	} ,
#endif
#ifdef SIGKILL
    { "kill"		, SIGKILL	} ,
#endif
#ifdef SIGLOST
    { "lost"		, SIGLOST	} ,
#endif
#ifdef SIGPIPE
    { "pipe"		, SIGPIPE	} ,
#endif
#ifdef SIGPOLL
    { "poll"		, SIGPOLL	} ,
#endif
#ifdef SIGPWR
    { "power"		, SIGPWR	} ,
#endif
#ifdef SIGPROF
    { "prof"		, SIGPROF	} ,
    { "profile"		, SIGPROF	} ,
#endif
#ifdef SIGPWR
    { "pwr"		, SIGPWR	} ,
#endif
#ifdef SIGQUIT
    { "quit"		, SIGQUIT	} ,
#endif
    { "segv"		, SIGSEGV	} ,	/* */
#ifdef SIGSTKFLT
    { "stkflt"		, SIGSTKFLT	} ,
#endif
#ifdef SIGSTOP
    { "stop"		, SIGSTOP	} ,
#endif
#ifdef SIGSYS
    { "sys"		, SIGSYS	} ,
#endif
    { "term"		, SIGTERM	} ,	/* */
    { "terminate"	, SIGTERM	} ,	/* */
#ifdef SIGTRAP
    { "trap"		, SIGTRAP	} ,
#endif
#ifdef SIGTSTP
    { "tstp"		, SIGTSTP	} ,
#endif
#ifdef SIGTTIN
    { "ttin"		, SIGTTIN	} ,
#endif
#ifdef SIGTTO
    { "ttou"		, SIGTTOU	} ,
    { "ttout"		, SIGTTOU	} ,
#endif
#ifdef SIGTTIN
    { "ttyin"		, SIGTTIN	} ,
#endif
#ifdef SIGTTOU
    { "ttyout"		, SIGTTOU	} ,    
#endif
#ifdef SIGTSTP
    { "ttystop"		, SIGTSTP	} ,
#endif
#ifdef SIGUNUSED
    { "unused"		, SIGUNUSED	} ,
#endif
#ifdef SIGURG
    { "urg"		, SIGURG	} ,
    { "urgent"		, SIGURG	} ,
#endif
#ifdef SIGUSR1
    { "user1"		, SIGUSR1	} ,
#endif
#ifdef SIGUSR2
    { "user2"		, SIGUSR2	} ,
#endif
#ifdef SIGUSR1
    { "usr1"		, SIGUSR1	} ,
#endif
#ifdef SIGUSR2
    { "usr2"		, SIGUSR2	} ,
#endif
#ifdef SIGVTALRM
    { "vtalarm"		, SIGVTALRM	} ,
    { "vtalrm"		, SIGVTALRM	} ,
#endif
#ifdef SIGWINCH
    { "winch"		, SIGWINCH	} ,
    { "windowchange"	, SIGWINCH	} ,
#endif
#ifdef SIGXCPU
    { "xcpu"		, SIGXCPU	} ,
#endif
#ifdef SIGXFSZ
    { "xfsz"		, SIGXFSZ	} ,
#endif
  };

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
    return entry->value;
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

static int sigsetmeta___index(lua_State *const L)
{
  sigset_t *set = luaL_checkudata(L,1,TYPE_SIGSET);
  
  if (lua_isstring(L,2))
    lua_pushboolean(L,sigismember(set,slua_tosignal(L,2)));
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
    sigaddset(sig,slua_tosignal(L,2));
  else
    sigdelset(sig,slua_tosignal(L,2));
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

/**********************************************************************
* 
* Usage:	set = signal.set([fill,][signal...])
*
* Desc:		Create a set of signals.  If no parameters are given,
*		return an empty set.
*
* Input:	fill (boolean/optional) 			\
*			true - fill the set, remove following 	\
*				signals if any			\
*			false (default) - empty the set, add 	\
*				following signals if any
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
    return 1;
  
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
    (*setsig)(set,slua_tosignal(L,i));
  
  return 1;
}

/**********************************************************************
*
* Usage:	okay,err = signal.catch(signal,handler[,flags][,blocked])
*
* Desc:		Install a handler for a signal, and optionally block
*		other signals while handling the signal.
*
* Input:	signal (string) signal name
*		handler (function(sig) boolean) if handler is a 	\
*				function, then install as the signal	\
*				handler.  If ti's a boolean, then if	\
*				true, install the default handler, 	\
*				otherwise, ignore the signal.
*		flags (table/optional) various flags:			\
*			'nochildstop'	if signal is 'child', do not	\
*					receive notification when child \
*					prcess stops			\
*			'oneshot'	restore default action after handler \
*			'resethandler'		"			\
*			'onstack'	use alternative stack		\
*			'restart'	restart system calls		\
*			'nomask'	do not mask this signal		\
*			'nodefer'		"			\
*			'info'		receive additonal information
*		blocked (userdata(set)) signals to block during handler
*
* Return:	okay (boolean) true if successful, false if error
*		err (integer) system error, 0 if successful
*
*********************************************************************/

static int siglua_catch(lua_State *const L)
{
  struct sigaction act;
  int              sig;
  
  memset(&act,0,sizeof(act));
  sig = slua_tosignal(L,1);
  
  luaL_unref(L,LUA_REGISTRYINDEX,m_handlers[sig].coderef);
  
  if (lua_isboolean(L,2))
  {
    if (lua_toboolean(L,2))
      act.sa_handler = SIG_DFL;
    else
      act.sa_handler = SIG_IGN;
    
    errno = 0;
    sigaction(sig,&act,NULL);
    lua_pushboolean(L,errno == 0);
    lua_pushinteger(L,errno);
    return 2;
  }
  
  luaL_checktype(L,2,LUA_TFUNCTION);
  
  if (lua_isuserdata(L,3))
  {
    sigset_t *set           = luaL_checkudata(L,3,TYPE_SIGSET);
    act.sa_mask             = *set;
    m_handlers[sig].blocked = *set;
  }
  
  lua_pushvalue(L,2);
  m_handlers[sig].coderef = luaL_ref(L,LUA_REGISTRYINDEX);
  act.sa_handler  = signal_handler;
  
  errno = 0;
  sigaction(sig,&act,NULL);
  lua_pushboolean(L,errno == 0);
  lua_pushinteger(L,errno);
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
    int sig = slua_tosignal(L,i);
    luaL_unref(L,LUA_REGISTRYINDEX,m_handlers[sig].coderef);
    sigignore(sig);
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
    int sig = slua_tosignal(L,i);    
    luaL_unref(L,LUA_REGISTRYINDEX,m_handlers[sig].coderef);
    sigset(sig,SIG_DFL);
  }
  
  return 0;
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
    sighold(slua_tosignal(L,i));
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
    sigrelse(slua_tosignal(L,i));
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
* Usage:	err = signal.suspend(set)
*
* Desc:		Temporarily replace signal mask with set, then wait for
*		a signal.  Upon signal, restore the original signal mask
*
* Input:	set (userdata(set)) set of signals
*
* Return:	err (integer) system error, never 0.
*
**********************************************************************/

static int siglua_suspend(lua_State *const L)
{
  sigset_t *set = luaL_checkudata(L,1,TYPE_SIGSET);
  sigsuspend(set);
  lua_pushinteger(L,errno);
  return 1;
}

/**********************************************************************
*
* Usage:        okay,err = signal.raise(signal)
*
* Desc:         Triggers the given signal.
*
* Input:        signal (string) name of signal
*
* Return:       okay (boolean)  true of okay, false otherwise
*               err (integer)   error value
*
* See:          org.conman.errno
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
  { "set"		, siglua_set		} ,
  { "catch"		, siglua_catch		} ,
  { "ignore"		, siglua_ignore		} ,
  { "default"		, siglua_default	} ,
  { "allow"		, siglua_allow		} ,
  { "block"		, siglua_block		} ,
  { "mask"		, siglua_mask		} ,
  { "pending"		, siglua_pending	} ,
  { "suspend"		, siglua_suspend	} ,
  { "raise"		, siglua_raise		} ,
  { NULL		, NULL			}
};

static const struct luaL_Reg m_sigset_meta[] =
{
  { "__index"		, sigsetmeta___index	} ,
  { "__newindex"	, sigsetmeta___newindex	} ,
  { "__add"		, sigsetmeta___add	} ,
  { "__sub"		, sigsetmeta___sub	} ,
  { "__unm"		, sigsetmeta___unm	} ,
  { NULL		, NULL			}
};
 
int luaopen_signal(lua_State *const L)
{
  for (int i = 0 ; i < NSIG ; i++)
    m_handlers[i].coderef = LUA_NOREF;
  m_L = L;
  
  luaL_newmetatable(L,TYPE_SIGSET);
  luaL_register(L,NULL,m_sigset_meta); 
  luaL_register(L,"signal",m_sig_reg);
  return 1;
}
