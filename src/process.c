/***************************************************************************
*
* Copyright 2011 by Sean Conner.
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

#ifdef __linux
#  define _POSIX_SOURCE
#  define _SVID_SOURCE
#  define _GNU_SOURCE
#endif

#include <stddef.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <ctype.h>
#include <signal.h>
#include <assert.h>

#include <lua.h>
#include <lauxlib.h>

#include <syslog.h>
#include <libgen.h>

#include <sys/resource.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <unistd.h>

#define TYPE_LIMIT_HARD	"org.conman.process:rlimit_hard"
#define TYPE_LIMIT_SOFT	"org.conman.process:rlimit_soft"

#ifndef __GNUC__
#  define __attribute__(x)
#endif

/**********************************************************************/

struct strint
{
  const char *const text;
  const int         value;
};

/*************************************************************************/

static int	proclua_getuid		(lua_State *const);
static int	proclua_getgid		(lua_State *const);
static int	proclua_setuid		(lua_State *const);
static int	proclua_setgid		(lua_State *const);
static int	proclua_exit		(lua_State *const);
static int	proclua_fork		(lua_State *const);
static int	proclua_wait		(lua_State *const);
static int	proclua_waitusage	(lua_State *const);
static int	proclua_waitid		(lua_State *const);
static int	proclua_sleep		(lua_State *const);
static int	proclua_sleepres	(lua_State *const);
static int	proclua_kill		(lua_State *const);
static int	proclua_exec		(lua_State *const);
static int	proclua_times		(lua_State *const);
static int	proclua_getrusage	(lua_State *const);
static int	proclua_pause		(lua_State *const);
static int	proclua_itimer		(lua_State *const);
static int	proclua___index		(lua_State *const);
static int	proclua___newindex	(lua_State *const);
static bool	mlimit_trans		(int *const restrict,const char *const restrict);
static bool	mlimit_valid_suffix	(lua_Integer *const restrict,const int,const char *const restrict);
static int	mhlimitlua___index	(lua_State *const);
static int	mhlimitlua___newindex	(lua_State *const);
static int	mslimitlua___index	(lua_State *const);
static int	mslimitlua___newindex	(lua_State *const);
static int	sig_meta___index	(lua_State *const);
static int	siglua_caught		(lua_State *const);
static int	siglua_catch		(lua_State *const);
static int	siglua_ignore		(lua_State *const);
static int	siglua_default		(lua_State *const);
static int	siglua_block		(lua_State *const);
static int	siglua_unblock		(lua_State *const);
static int	siglua_mask		(lua_State *const);
static int	siglua_getmask		(lua_State *const);
static void	proc_pushstatus		(lua_State *const,const pid_t,int);
static void	proc_pushrusage		(lua_State *const restrict,struct rusage *const restrict);
static void	signal_handler		(int);
static int	set_signal_handler	(int,void (*)(int));

/************************************************************************/

static const struct luaL_Reg mprocess_reg[] =
{
  { "getuid"		, proclua_getuid	} ,
  { "getgid"		, proclua_getgid	} ,
  { "setuid"		, proclua_setuid	} ,
  { "setgid"		, proclua_setgid	} ,
  { "exit"		, proclua_exit		} ,
  { "fork"		, proclua_fork		} ,
  { "wait"		, proclua_wait		} ,
  { "waitusage"		, proclua_waitusage	} ,
  { "waitid"		, proclua_waitid	} ,
  { "sleep"		, proclua_sleep		} ,
  { "sleepres"		, proclua_sleepres	} ,
  { "kill"		, proclua_kill		} ,
  { "exec"		, proclua_exec		} ,
  { "times"		, proclua_times		} ,
  { "getrusage"		, proclua_getrusage	} ,
  { "pause"		, proclua_pause		} ,
  { "itimer"		, proclua_itimer	} ,
  { NULL		, NULL			} 
};

static const struct luaL_Reg mprocess_meta[] =
{
  { "__index"		, proclua___index	} ,
  { "__newindex"	, proclua___newindex	} ,
  { NULL		, NULL			}
};

static const struct luaL_Reg mhlimit_reg[] =
{
  { "__index" 		, mhlimitlua___index	} ,
  { "__newindex"	, mhlimitlua___newindex	} ,
  { NULL		, NULL			}
};

static const struct luaL_Reg mslimit_reg[] =
{
  { "__index"		, mslimitlua___index	} ,
  { "__newindex"	, mslimitlua___newindex	} ,
  { NULL		, NULL			}
};

static const struct luaL_Reg msig_reg[] =
{
  { "caught"	, siglua_caught		} ,
  { "catch"	, siglua_catch		} ,
  { "ignore"	, siglua_ignore		} ,
  { "default"	, siglua_default	} ,
  { "block"	, siglua_block		} ,
  { "unblock"	, siglua_unblock	} ,
  { "mask"	, siglua_mask		} ,
  { "getmask"	, siglua_getmask	} ,
  { NULL	, NULL			}
};

static const struct strint m_sigs[] =
{
  /*--------------------------------
  ; signals defined in C89
  ;---------------------------------*/
  
  { "ABRT"	, SIGABRT	} ,
  { "FPE"	, SIGFPE	} ,
  { "ILL"	, SIGILL	} ,
  { "INT"	, SIGINT	} ,
  { "SEGV"	, SIGSEGV	} ,
  { "TERM"	, SIGTERM	} ,
  
  /*-----------------------------
  ; other possible signals
  ;------------------------------*/

  { "ALRM"	, SIGALRM	} ,
  { "BUS"	, SIGBUS	} ,
  { "CHLD"	, SIGCHLD	} ,
  { "CONT"	, SIGCONT	} ,
  { "HUP"	, SIGHUP	} ,
  { "IO"	, SIGIO		} ,
  { "IOT"	, SIGIOT	} ,
  { "KILL"	, SIGKILL	} ,
  { "PIPE"	, SIGPIPE	} ,
  { "POLL"	, SIGPOLL	} ,
  { "PROF"	, SIGPROF	} ,
  { "PWR"	, SIGPWR	} ,
  { "QUIT"	, SIGQUIT	} ,
  { "SIGURG"	, SIGURG	} ,
#ifdef SIGSTKFLT
  { "STKFLT"	, SIGSTKFLT	} ,
#endif
  { "STOP"	, SIGSTOP	} ,
  { "SYS"	, SIGSYS	} ,
  { "TRAP"	, SIGTRAP	} ,
  { "TSTP"	, SIGTSTP	} ,
  { "TTIN"	, SIGTTIN	} ,
  { "TTOU"	, SIGTTOU	} ,
  { "USR1"	, SIGUSR1	} ,
  { "USR2"	, SIGUSR2	} ,
  { "VTALRM"	, SIGVTALRM	} ,
  { "WINCH"	, SIGWINCH	} ,
  { "XCPU"	, SIGXCPU	} ,
  { "XFSZ"	, SIGXFSZ	} ,
  { NULL	, 0		}
};

static volatile sig_atomic_t m_caught;
static volatile sig_atomic_t m_signal[32];

/*************************************************************************/

static int proclua_getuid(lua_State *const L)
{
#if defined(__linux)
  uid_t uid;
  uid_t euid;
  uid_t suid;

  getresuid(&uid,&euid,&suid);
  lua_pushinteger(L,uid);
  lua_pushinteger(L,euid);
  lua_pushinteger(L,suid);

#else

  lua_pushinteger(L,getuid());
  lua_pushinteger(L,geteuid());
  lua_pushinteger(L,-1);

#endif
  return 3;
}

/************************************************************************/

static int proclua_getgid(lua_State *const L)
{
#if defined(__linux)
  gid_t gid;
  gid_t egid;
  gid_t sgid;
  
  getresgid(&gid,&egid,&sgid);  
  lua_pushinteger(L,gid);
  lua_pushinteger(L,egid);
  lua_pushinteger(L,sgid);

#else

  lua_pushinteger(L,getgid());
  lua_pushinteger(L,getegid());
  lua_pushinteger(L,-1);

#endif
  return 3;
}

/********************************************************************/

static int proclua_setuid(lua_State *const L)
{
#if defined(__linux)
  uid_t uid;
  uid_t euid;
  uid_t suid;
  
  uid  = luaL_checkinteger(L,1);
  euid = luaL_optinteger(L,2,-1);
  suid = luaL_optinteger(L,3,-1);
  
  if (setresuid(uid,euid,suid) < 0)
    lua_pushinteger(L,errno);
  else
    lua_pushinteger(L,0);
  return 1;
#else
  uid_t uid;
  uid_t euid;

  uid  = luaL_checkinteger(L,1);
  euid = luaL_optinteger(L,2,-1);

  if (setuid(uid) < 0)
  {
    lua_pushinteger(L,errno);
    return 1;
  }

  if (seteuid(uid) < 0)
  {
    lua_pushinteger(L,errno);
    return 1;
  }

  lua_pushinteger(L,0);
  return 1;
#endif
}

/*************************************************************************/

static int proclua_setgid(lua_State *const L)
{
#if defined(__linux)
  gid_t gid;
  gid_t egid;
  gid_t sgid;
  
  gid  = luaL_checkinteger(L,1);
  egid = luaL_optinteger(L,2,-1);
  sgid = luaL_optinteger(L,3,-1);
  
  if (setresgid(gid,egid,sgid) < 0)
    lua_pushinteger(L,errno);
  else
    lua_pushinteger(L,0);
  return 1;
#else
  gid_t gid;
  gid_t egid;

  gid  = luaL_checkinteger(L,1);
  egid = luaL_optinteger(L,2,-1);

  if (setgid(gid) < 0)
  {
    lua_pushinteger(L,errno);
    return 1;
  }

  if (setegid(egid) < 0)
  {
    lua_pushinteger(L,errno);
    return 1;
  }

  lua_pushinteger(L,0);
  return 1;
#endif
}

/************************************************************************/

static int proclua_exit(lua_State *const L)
{
  assert(L != NULL);
  
  _exit(luaL_optint(L,1,0));
}

/***********************************************************************/

static int proclua_fork(lua_State *const L)
{
  pid_t child;
  
  child = fork();
  if (child > 0)
  {
    lua_pushinteger(L,child);
    return 1;
  }
  else if (child == 0)
  {
    lua_pushinteger(L,0);
    return 1;
  }
  else
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    return 2;
  }
}

/***********************************************************************/

static int proclua_wait(lua_State *const L)
{
  pid_t child;
  int   status;
  int   flag;
  int   rc;
  
  child = luaL_optinteger(L,1,-1);
  flag  = lua_toboolean(L,2) ? WNOHANG : 0;
  rc    = waitpid(child,&status,flag);
  if (rc == -1)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    return 2;
  }
  
  if (rc == 0)
  {
    lua_pushnil(L);
    lua_pushinteger(L,0);
    return 2;
  }

  proc_pushstatus(L,rc,status);
  return 1;
}

/*********************************************************************/

static int proclua_waitusage(lua_State *const L)
{
  struct rusage usage;
  pid_t         child;
  int           status;
  int           flag;
  int           rc;

  child = luaL_optinteger(L,1,-1);
  flag  = lua_toboolean(L,2) ? WNOHANG : 0;
  rc    = wait4(child,&status,flag,&usage);
  if (rc == -1)
  {
    lua_pushnil(L);
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    return 3;
  }
  
  if (rc == 0)
  {
    lua_pushnil(L);
    lua_pushnil(L);
    lua_pushinteger(L,0);
    return 3;
  }
  
  proc_pushstatus(L,rc,status);
  proc_pushrusage(L,&usage);  
  return 2;
}

/**********************************************************************/

static int proclua_waitid(lua_State *const L)
{
  siginfo_t info;
  pid_t     child;
  idtype_t  idtype;
  int       flag;
  
  child = luaL_checkinteger(L,1);
  flag  = lua_toboolean(L,2) 
  	? WEXITED | WSTOPPED | WCONTINUED | WNOHANG
  	: WEXITED | WSTOPPED | WCONTINUED;
  idtype = (child == 0) ? P_ALL : P_PID;
  
  memset(&info,0,sizeof(info));
  if (waitid(idtype,child,&info,flag) == -1)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    return 2;
  }
  
  lua_createtable(L,0,6);
  lua_pushinteger(L,info.si_pid);
  lua_setfield(L,-2,"pid");
  lua_pushinteger(L,info.si_uid);
  lua_setfield(L,-2,"uid");
  
  if (info.si_code == CLD_EXITED)
  {
    lua_pushliteral(L,"exit");
    lua_setfield(L,-2,"state");
    lua_pushinteger(L,info.si_status);
    lua_setfield(L,-2,"status");
  }
  else
  {
    switch(info.si_code)
    {
      case CLD_KILLED:    lua_pushliteral(L,"killed");    break;
      case CLD_DUMPED:    lua_pushliteral(L,"core");      break;
      case CLD_STOPPED:   lua_pushliteral(L,"stopped");   break;
      case CLD_TRAPPED:   lua_pushliteral(L,"trapped");   break;
      case CLD_CONTINUED: lua_pushliteral(L,"continued"); break;
      default:            lua_pushfstring(L,"(%d)",info.si_code); break;
    }
    lua_setfield(L,-2,"state");
    lua_pushinteger(L,info.si_status);
    lua_setfield(L,-2,"signal");
    lua_pushstring(L,strsignal(info.si_status));
    lua_setfield(L,-2,"description");
    lua_pushboolean(L,info.si_code == CLD_DUMPED);
    lua_setfield(L,-2,"core");
  }
  
  return 1;
}

/*********************************************************************/

static int proclua_getrusage(lua_State *const L)
{
  struct rusage  usage;
  const char    *twho;
  int            who;
  
  twho = luaL_optstring(L,1,"self");
  
  if (strcmp(twho,"self") == 0)
    who = RUSAGE_SELF;
  else if ((strcmp(twho,"children") == 0) || (strcmp(twho,"child") == 0))
    who = RUSAGE_CHILDREN;
  else
  {
    lua_pushnil(L);
    lua_pushinteger(L,EINVAL);
    return 2;
  }
  
  if (getrusage(who,&usage) < 0)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    return 2;
  }
  
  proc_pushrusage(L,&usage);
  return 1;
}

/**********************************************************************/

static int proclua_pause(lua_State *const L)
{
  pause();
  lua_pushinteger(L,errno);
  return 1;
}

/**********************************************************************/

static int proclua_itimer(lua_State *const L)
{
  struct itimerval set;
  double           interval;
  double           seconds;
  double           fract;
  
  if (lua_isnumber(L,1))
    interval = lua_tonumber(L,1);
  else if (lua_isstring(L,1))
  {
    const char *v = lua_tostring(L,1);
    char       *p;
    
    interval = strtod(v,&p);
    switch(*p)
    {
      case 's': break;
      case 'm': interval *=    60.0; break;
      case 'h': interval *=  3600.0; break;
      case 'd': interval *= 86400.0; break;
      default:  break;
    }
  }
  else
    interval = 0.0;
  
  fract = modf(interval,&seconds);
  
  set.it_value.tv_sec  = set.it_interval.tv_sec  = seconds;
  set.it_value.tv_usec = set.it_interval.tv_usec = fract * 1000000.0;
  
  if (setitimer(ITIMER_REAL,&set,NULL) < 0)
  {
    lua_pushboolean(L,false);
    lua_pushinteger(L,errno);
  }
  else
  {
    lua_pushboolean(L,true);
    lua_pushinteger(L,0);
  }
  
  return 2;
}

/**********************************************************************/

static int proclua_sleep(lua_State *const L)
{
  struct timespec interval;
  struct timespec left;
  double          param;
  double          seconds;
  double          fract;
  
  assert(L != NULL);
  
  param            = luaL_checknumber(L,1);
  fract            = modf(param,&seconds);
  interval.tv_sec  = (time_t)seconds;
  interval.tv_nsec = (long)(fract * 1000000000.0);
  
  if (nanosleep(&interval,&left) < 0)
  {
    lua_pushnumber(L,param);
    lua_pushinteger(L,errno);
    return 2;
  }
  
  lua_pushnumber(L,(double)left.tv_sec + (((double)left.tv_nsec) / 1000000000.0));
  lua_pushinteger(L,0);
  return 2;    
}

/***********************************************************************/

static int proclua_sleepres(lua_State *const L)
{
  struct timespec res;

  assert(L != NULL);
  
  clock_getres(CLOCK_REALTIME,&res);
  lua_pushnumber(L,(double)res.tv_sec + (((double)res.tv_nsec) / 1000000000.0));
  return 1;  
}

/**********************************************************************/

static int proclua_kill(lua_State *const L)
{
  pid_t child;
  int   sig;
  
  assert(L != NULL);

  child = luaL_checkinteger(L,1);
  sig   = luaL_optint(L,2,SIGTERM);
  
  if (kill(child,sig) < 0)
  {
    lua_pushboolean(L,false);
    lua_pushinteger(L,errno);
    return 2;
  }
  
  lua_pushboolean(L,1);
  return 1;
}

/*********************************************************************/

static int proc_execfree(char **argv,char **envp,size_t envc)
{
  for (size_t i = 0 ; i < envc ; i++)
    free(envp[i]);
  
  free(envp);
  free(argv);
  
  return ENOMEM;
}

/********************************************************************/

static bool proc_growenvp(char ***penvp,size_t *psize)
{
  char   **new;
  size_t   nsize;
  
  assert(penvp != NULL);
  assert(psize != NULL);
  
  nsize = *psize + 64;
  new   = realloc(*penvp,nsize * sizeof(char *));
  
  if (new == NULL)
    return false;
  
  *penvp = new;
  *psize = nsize;
  return true;
}

/******************************************************************/  

static int proclua_exec(lua_State *const L)
{
  extern char **environ;
  const char   *binary;
  char        **argv;
  size_t        argc;
  char        **envp;
  size_t        envc;
  size_t        envm;
  int           err;
  
  binary = luaL_checkstring(L,1);
  luaL_checktype(L,2,LUA_TTABLE);
  
  argc = lua_objlen(L,2) + 2;
  argv = malloc(argc * sizeof(char *));
  if (argv == NULL)
  {
    lua_pushinteger(L,ENOMEM);
    return 1;
  }
  
  argv[0] = basename((char *)binary);
  for (size_t i = 1 ; i < argc - 1 ; i++)
  {
    lua_pushinteger(L,i);
    lua_gettable(L,2);
    argv[i] = (char *)lua_tostring(L,-1);
    lua_pop(L,1);
  }
  argv[argc - 1] = NULL;
  
  envc = 0;
  envm = 0;

  if (lua_isnoneornil(L,3))
    envp = environ;
  else
  {
    envp = NULL;
    envc = 0;
    envm = 0;
  
    luaL_checktype(L,3,LUA_TTABLE);

    lua_pushnil(L);
    while(lua_next(L,3) != 0)
    {
      const char *name;
      size_t      nsize;
      const char *value;
      size_t      vsize;
    
      name  = lua_tolstring(L,-2,&nsize);
      value = lua_tolstring(L,-1,&vsize);
    
      if ((envc == envm) && !proc_growenvp(&envp,&envm))
      {
        proc_execfree(argv,envp,envc);
        lua_pushinteger(L,ENOMEM);
        return 1;
      }

      envp[envc] = malloc(nsize + vsize + 3);
      if (envp[envc] == NULL)
      {
        proc_execfree(argv,envp,envc);
        lua_pushinteger(L,ENOMEM);
        return 1;
      }
    
      sprintf(envp[envc++],"%s=%s",name,value);
      lua_pop(L,1);
    }
  
    if ((envc == envm) && !proc_growenvp(&envp,&envm))
    {
      proc_execfree(argv,envp,envc);
      lua_pushinteger(L,ENOMEM);
      return 1;
    }
    
    envp[envc++] = NULL;
  }
  
  execve(binary,argv,envp);
  err = errno;
  if (envp != environ) proc_execfree(argv,envp,envc);
  lua_pushinteger(L,err);
  return 1;
}

/*********************************************************************/

static int proclua_times(lua_State *const L)
{
  struct tms tms;
  
  assert(L != NULL);
  
  if (times(&tms) == (clock_t)-1)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    return 2;
  }

  lua_createtable(L,0,4);
  lua_pushnumber(L,tms.tms_utime);
  lua_setfield(L,-2,"utime");
  lua_pushnumber(L,tms.tms_stime);
  lua_setfield(L,-2,"stime");
  lua_pushnumber(L,tms.tms_cutime);
  lua_setfield(L,-2,"cutime"); 
  lua_pushnumber(L,tms.tms_cstime);
  lua_setfield(L,-2,"cstime");
  
  return 1;
}

/*********************************************************************/

static int proclua___index(lua_State *const L)
{
  const char *idx;
  
  assert(L != NULL);
  
  idx = luaL_checkstring(L,2);
  if (strcmp(idx,"PID") == 0)
    lua_pushinteger(L,getpid());
  else if (strcmp(idx,"PRI") == 0)
    lua_pushinteger(L,getpriority(PRIO_PROCESS,0));
  else
    lua_pushnil(L);
  return 1;
}

/*********************************************************************/

static int proclua___newindex(lua_State *const L)
{
  const char *idx;
  
  assert(L != NULL);
  
  idx = luaL_checkstring(L,2);
  if (strcmp(idx,"PID") == 0)
    lua_pushnil(L);
  else if (strcmp(idx,"PRI") == 0)
  {
    int rc;
    
    rc = setpriority(PRIO_PROCESS,0,luaL_checkinteger(L,3));
    if (rc == -1)
      lua_pushnil(L);
    else
      lua_pushinteger(L,rc);
  }
  else
    lua_pushnil(L);
  return 1;
}

/*********************************************************************/

static bool mlimit_trans(
	int        *const restrict pret,
	const char *const restrict tag
)
{
  assert(pret != NULL);
  assert(tag  != NULL);
  
  if (strcmp(tag,"core") == 0)
    *pret = RLIMIT_CORE; 
  else if (strcmp(tag,"cpu") == 0)
    *pret = RLIMIT_CPU;
  else if (strcmp(tag,"data") == 0)
    *pret = RLIMIT_DATA;
  else if (strcmp(tag,"fsize") == 0)
    *pret = RLIMIT_FSIZE;
  else if (strcmp(tag,"nofile") == 0)
    *pret = RLIMIT_NOFILE;
  else if (strcmp(tag,"stack") == 0)
    *pret = RLIMIT_STACK;
  else if (strcmp(tag,"vm") == 0)
    *pret = RLIMIT_AS;
  else
    return false;
    
  return true;
}

/**********************************************************************/

static bool mlimit_valid_suffix(
	lua_Integer *const restrict pval,
	const int                   key,
	const char *const restrict  unit
)
{
  assert(pval != NULL);
  assert(unit != NULL);
  assert(
             (key == RLIMIT_CORE)
          || (key == RLIMIT_CPU)
          || (key == RLIMIT_DATA)
          || (key == RLIMIT_FSIZE)
          || (key == RLIMIT_NOFILE)
          || (key == RLIMIT_STACK)
          || (key == RLIMIT_AS)
        );
  
  if ((strcmp(unit,"inf") == 0) || (strcmp(unit,"infinity") == 0))
  {
    *pval = RLIM_INFINITY;
    return true;
  }
  
  switch(key)
  {
    case RLIMIT_CPU:
         switch(toupper(*unit))
         {
           case 'S':                     break;
           case 'M':  *pval *=     60uL; break;
           case 'H':  *pval *=   3600uL; break;
           case 'D':  *pval *=  86400uL; break;
           case 'W':  *pval *= 604800uL; break;
           case '\0':                    break;
           default: return false;
         }
         return true;
           
    case RLIMIT_NOFILE:
         return (*unit == '\0');
           
    case RLIMIT_CORE:
    case RLIMIT_DATA:
    case RLIMIT_FSIZE:
    case RLIMIT_STACK:
    case RLIMIT_AS:
         switch(toupper(*unit))
         {
           case 'B':                                       break;
           case 'K':  *pval *= (1024uL);                   break;
           case 'M':  *pval *= (1024uL * 1024uL);          break;
           case 'G':  *pval *= (1024uL * 1024uL * 1024uL); break;
           case '\0':                                      break;
           default: return false;
         }
         return true;
         
    default:
         return false;
  }  
}

/******************************************************************/

static int mhlimitlua___index(lua_State *const L)
{
  struct rlimit  limit;
  const char    *tkey;
  int            key;
  int            rc;
  
  assert(L != NULL);
  
  luaL_checkudata(L,1,TYPE_LIMIT_HARD);
  tkey = luaL_checkstring(L,2);
  
  if (!mlimit_trans(&key,tkey))
    return luaL_error(L,"Illegal limit resource: %s",tkey);
  
  rc = getrlimit(key,&limit);
  if (rc == -1)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    return 2;
  }
  
  if (limit.rlim_max == RLIM_INFINITY)
    lua_pushliteral(L,"inf");
  else
    lua_pushinteger(L,limit.rlim_max);

  return 1;
}

/************************************************************************/

static int mhlimitlua___newindex(lua_State *const L)
{
  struct rlimit  limit;
  const char    *tkey;
  int            key;
  lua_Integer    ival;

  assert(L != NULL);
  
  luaL_checkudata(L,1,TYPE_LIMIT_HARD);
  tkey = luaL_checkstring(L,2);
  
  if (!mlimit_trans(&key,tkey))
    return luaL_error(L,"Illegal limit resource: %s",tkey);

  if (lua_isnumber(L,3))
    ival = lua_tointeger(L,3);
  else if (lua_isstring(L,3))
  {
    const char *tval;
    const char *unit;
    
    tval = lua_tostring(L,3);
    ival = strtoul(tval,(char **)&unit,10);

    if (!mlimit_valid_suffix(&ival,key,unit))
      return luaL_error(L,"Illegal suffix: %c",*unit);
  } 
  else
    return luaL_error(L,"Non-supported type");

  limit.rlim_cur = ival;
  limit.rlim_max = ival;
  
  setrlimit(key,&limit);
  return 0;
}

/************************************************************************/

static int mslimitlua___index(lua_State *const L)
{
  struct rlimit  limit;
  const char    *tkey;
  int            key;
  int            rc;
  
  assert(L != NULL);
  
  luaL_checkudata(L,1,TYPE_LIMIT_SOFT);
  tkey = luaL_checkstring(L,2);
  
  if (!mlimit_trans(&key,tkey))
    return luaL_error(L,"Illegal limit resource: %s",tkey);
  
  rc = getrlimit(key,&limit);
  if (rc == -1)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    return 2;
  }
  
  if (limit.rlim_cur == RLIM_INFINITY)
    lua_pushliteral(L,"inf");
  else
    lua_pushinteger(L,limit.rlim_cur);
    
  return 1;
}

/************************************************************************/

static int mslimitlua___newindex(lua_State *const L)
{
  struct rlimit  climit;
  struct rlimit  limit;
  const char    *tkey;
  int            key;
  lua_Integer    ival;
  int            rc;
  
  assert(L != NULL);
  
  luaL_checkudata(L,1,TYPE_LIMIT_SOFT);
  tkey = luaL_checkstring(L,2);
  
  if (!mlimit_trans(&key,tkey))
    return luaL_error(L,"Illegal limit resource: %s",tkey);

  if (lua_isnumber(L,3))
    ival = lua_tointeger(L,3);
  else if (lua_isstring(L,3))
  {
    const char *tval;
    const char *unit;
    
    tval = lua_tostring(L,3);
    ival = strtoul(tval,(char **)&unit,10);

    if (!mlimit_valid_suffix(&ival,key,unit))
      return luaL_error(L,"Illegal suffix: %c",*unit);
  } 
  else
    return luaL_error(L,"Non-supported type");

  limit.rlim_cur = ival;
  
  rc = getrlimit(key,&climit);
  if (rc == -1)
    return 0;
  
  limit.rlim_max = climit.rlim_max;
  setrlimit(key,&limit);
  return 0;
}

/***********************************************************************/

static int siglua_caught(lua_State *const L)
{
  int sig;
  
  assert(L != NULL);
  
  if (lua_isnoneornil(L,1))
  {
    lua_pushboolean(L,m_caught);
    m_caught = 0;
    return 1;
  }
  
  sig = luaL_checkint(L,1);
  if (sig < 32)
  {
    lua_pushboolean(L,m_signal[sig]);
    m_signal[sig] = 0;
  }
  else
    lua_pushboolean(L,false);
  m_caught = 0;
  return 1;
}

/**********************************************************************/

static void sig_table(lua_State *const L,int idx,void (*handler)(int))
{
  size_t max;
  size_t i;
  
  assert(L   != NULL);
  assert(idx != 0);
  assert(handler);
  
  idx = (idx < 0) 
  	? lua_gettop(L) + idx + 1
  	: idx;
	
  max = lua_objlen(L,idx);
  for (i = 1 ; i <= max ; i++)
  {
    lua_pushinteger(L,i);
    lua_gettable(L,idx);
    if (lua_isnumber(L,-1))
      set_signal_handler(lua_tointeger(L,-1),handler);
    else if (lua_istable(L,-1))
      sig_table(L,-1,handler);
    lua_pop(L,1);
  }
}

/************************************************************************/

static void sig_mask(lua_State *const L,int idx,sigset_t *set)
{
  size_t max;
  size_t i;
  
  assert(L   != NULL);
  assert(idx != 0);
  assert(set != NULL);
  
  idx = (idx < 0)
      ? lua_gettop(L) + idx + 1
      : idx;
  max = lua_objlen(L,idx);
  for (i = 1 ; i <= max ; i++)
  {
    lua_pushinteger(L,i);
    lua_gettable(L,idx);
    if (lua_isnumber(L,-1))
      sigaddset(set,lua_tointeger(L,-1));
    else if (lua_istable(L,-1))
      sig_mask(L,-1,set);
    lua_pop(L,1);
  }
}

/************************************************************************/

static int siglua_catch(lua_State *const L)
{
  int top = lua_gettop(L);
  
  assert(L != NULL);
  
  for (int i = 1 ; i <= top ; i++)
  {
    if (lua_isnumber(L,i))
      set_signal_handler(lua_tointeger(L,i),signal_handler);
    else if (lua_istable(L,i))
      sig_table(L,i,signal_handler);
    else
      luaL_error(L,"expected number or table");
  }  
  return 0;
}

/**********************************************************************/

int siglua_ignore(lua_State *const L)
{
  int top = lua_gettop(L);
  
  assert(L != NULL);
  
  for (int i = 1 ; i <= top ; i++)
  {
    if (lua_isnumber(L,i))
      set_signal_handler(lua_tointeger(L,i),SIG_IGN);
    else if (lua_istable(L,i))
      sig_table(L,i,SIG_IGN);
    else
      luaL_error(L,"expected number or table");
  }
  return 0;
}

/**********************************************************************/

int siglua_default(lua_State *const L)
{
  int top = lua_gettop(L);
  
  assert(L != NULL);
  
  for (int i = 1 ; i <= top ; i++)
  {
    if (lua_isnumber(L,i))
      set_signal_handler(lua_tointeger(L,i),SIG_DFL);
    else if (lua_istable(L,i))
      sig_table(L,i,SIG_DFL);
    else
      luaL_error(L,"expected number or table");
  }
  return 0;
}

/**********************************************************************/

int siglua_block(lua_State *const L)
{
  int      top = lua_gettop(L);
  sigset_t set;
  
  if (top == 0)
    sigfillset(&set);
  else
  {
    sigemptyset(&set);
  
    for (int i = 1 ; i <= top ; i++)
    {
      if (lua_isnumber(L,i))
        sigaddset(&set,lua_tointeger(L,i));
      else if (lua_istable(L,i))
        sig_mask(L,i,&set);
      else
        luaL_error(L,"expected number or table");
    }
  }
  
  sigprocmask(SIG_BLOCK,&set,NULL);
  return 0;
}

/*********************************************************************/

int siglua_unblock(lua_State *const L)
{
  int      top = lua_gettop(L);
  sigset_t set;
  
  if (top == 0)
    sigfillset(&set);
  else
  {
    sigemptyset(&set);
  
    for (int i = 1 ; i <= top ; i++)
    {
      if (lua_isnumber(L,i))
        sigaddset(&set,lua_tointeger(L,i));
      else if (lua_istable(L,i))
        sig_mask(L,i,&set);
      else
        luaL_error(L,"expected number or table");
    }
  }
  
  sigprocmask(SIG_UNBLOCK,&set,NULL);
    
  return 0;
}

/*******************************************************************/

int siglua_mask(lua_State *const L)
{
  int      top = lua_gettop(L);
  sigset_t set;
  
  sigemptyset(&set);
  
  for (int i = 1 ; i <= top ; i++)
  {
    if (lua_isnumber(L,i))
      sigaddset(&set,lua_tointeger(L,i));
    else if (lua_istable(L,i))
      sig_mask(L,i,&set);
    else
      luaL_error(L,"expected number or table");
  }
  
  sigprocmask(SIG_SETMASK,&set,NULL);
  return 0;
}

/********************************************************************/

int siglua_getmask(lua_State *const L)
{
  sigset_t set;
  size_t   i;
  int      idx;
  
  sigprocmask(0,NULL,&set);
  lua_createtable(L,0,0);
  for (idx = 1 , i = 0 ; m_sigs[i].text != NULL ; i++)
  {
    if (sigismember(&set,m_sigs[i].value))
    {
      lua_pushinteger(L,idx++);
      lua_pushinteger(L,m_sigs[i].value);
      lua_settable(L,-3);
    }
  }
      
  return 1;
}

/*********************************************************************/

int sig_meta___index(lua_State *const L)
{
  assert(L != NULL);
  lua_pushstring(L,strsignal(luaL_checkinteger(L,2)));
  return 1;
}

/**********************************************************************/

int luaopen_org_conman_process(lua_State *const L)
{
  assert(L != NULL);
  
  luaL_newmetatable(L,TYPE_LIMIT_HARD);
  luaL_register(L,NULL,mhlimit_reg);
  
  luaL_newmetatable(L,TYPE_LIMIT_SOFT);
  luaL_register(L,NULL,mslimit_reg);
  
  luaL_register(L,"org.conman.process",mprocess_reg);

  lua_createtable(L,0,0);
  luaL_register(L,NULL,msig_reg);
  for (size_t i = 0 ; m_sigs[i].text != NULL ; i++)
  {
    lua_pushinteger(L,m_sigs[i].value);
    lua_setfield(L,-2,m_sigs[i].text);
  }
  lua_createtable(L,0,1);
  lua_pushcfunction(L,sig_meta___index);
  lua_setfield(L,-2,"__index");
  lua_setmetatable(L,-2);
  lua_setfield(L,-2,"sig");
  
  lua_createtable(L,0,2);
  lua_newuserdata(L,sizeof(int));
  luaL_getmetatable(L,TYPE_LIMIT_HARD);
  lua_setmetatable(L,-2);
  lua_setfield(L,-2,"hard");
  
  lua_newuserdata(L,sizeof(int));
  luaL_getmetatable(L,TYPE_LIMIT_SOFT);
  lua_setmetatable(L,-2);
  lua_setfield(L,-2,"soft");
  
  lua_setfield(L,-2,"limits");
  
  lua_createtable(L,0,0);
  luaL_register(L,NULL,mprocess_meta);
  lua_setmetatable(L,-2);
  
  return 1;
}

/************************************************************************/

static void proc_pushstatus(
	lua_State *const L,
	const pid_t      pid,
	int              status
)
{
  lua_createtable(L,0,0);
  lua_pushinteger(L,pid);
  lua_setfield(L,-2,"pid");
  
  if (WIFEXITED(status))
  {
    lua_pushinteger(L,WEXITSTATUS(status));
    lua_setfield(L,-2,"rc");
    lua_pushliteral(L,"normal");
    lua_setfield(L,-2,"status");
  }
  else if (WIFSTOPPED(status))
  {
    lua_pushinteger(L,WSTOPSIG(status));
    lua_setfield(L,-2,"signal");
    lua_pushstring(L,strsignal(WSTOPSIG(status)));
    lua_setfield(L,-2,"description");
    lua_pushliteral(L,"stopped");
    lua_setfield(L,-2,"status");
  }
  else if (WIFSIGNALED(status))
  {
    lua_pushinteger(L,WTERMSIG(status));
    lua_setfield(L,-2,"signal");
    lua_pushstring(L,strsignal(WTERMSIG(status)));
    lua_setfield(L,-2,"description");
    lua_pushliteral(L,"terminated");
    lua_setfield(L,-2,"status");
#ifdef WCOREDUMP
    lua_pushboolean(L,WCOREDUMP(status));
    lua_setfield(L,-2,"core");
#endif
  }
}

/************************************************************************/

static void proc_pushrusage(
	lua_State     *const restrict L,
	struct rusage *const restrict usage
)
{
  lua_createtable(L,0,0);
  
  lua_pushnumber(L,usage->ru_utime.tv_sec + ((double)usage->ru_utime.tv_usec / 1000000.0));
  lua_setfield(L,-2,"utime");
  
  lua_pushnumber(L,usage->ru_stime.tv_sec + ((double)usage->ru_stime.tv_usec / 1000000.0));
  lua_setfield(L,-2,"stime");
  
  lua_pushnumber(L,usage->ru_maxrss);
  lua_setfield(L,-2,"maxrss");
  
  lua_pushnumber(L,usage->ru_ixrss);
  lua_setfield(L,-2,"text");
  
  lua_pushnumber(L,usage->ru_idrss);
  lua_setfield(L,-2,"data");
  
  lua_pushnumber(L,usage->ru_isrss);
  lua_setfield(L,-2,"stack");
  
  lua_pushnumber(L,usage->ru_minflt);
  lua_setfield(L,-2,"softfaults");
  
  lua_pushnumber(L,usage->ru_majflt);
  lua_setfield(L,-2,"hardfaults");

  lua_pushnumber(L,usage->ru_nswap);
  lua_setfield(L,-2,"swapped");
  
  lua_pushnumber(L,usage->ru_inblock);
  lua_setfield(L,-2,"inblock");
  
  lua_pushnumber(L,usage->ru_oublock);
  lua_setfield(L,-2,"outblock");
  
  lua_pushnumber(L,usage->ru_msgsnd);
  lua_setfield(L,-2,"ipcsend");
  
  lua_pushnumber(L,usage->ru_msgrcv);
  lua_setfield(L,-2,"ipcreceive");
  
  lua_pushnumber(L,usage->ru_nsignals);
  lua_setfield(L,-2,"signals");
  
  lua_pushnumber(L,usage->ru_nvcsw);
  lua_setfield(L,-2,"coopcs");
  
  lua_pushnumber(L,usage->ru_nivcsw);
  lua_setfield(L,-2,"preemptcs");
}

/************************************************************************/

static void signal_handler(int sig)
{
  m_caught = 1;
  if (sig < 32)
    m_signal[sig] = 1;
}

/************************************************************************/

static int set_signal_handler(int sig,void (*handler)(int))
{
  struct sigaction act;
  struct sigaction oact;
  
  sigemptyset(&act.sa_mask);
  act.sa_handler = handler;
  act.sa_flags   = 0;

  if (sigaction(sig,&act,&oact) == -1)
    return errno;
  return 0;
}

/************************************************************************/
