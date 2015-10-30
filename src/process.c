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
#include <limits.h>
#include <assert.h>

#include <lua.h>
#include <lauxlib.h>

#include <sysexits.h>
#include <syslog.h>
#include <libgen.h>

#include <sys/resource.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(__linux)
#  include <sched.h>
#elif defined(__SunOS)
#  include <sys/processor.h>
#  include <sys/procset.h>
#endif

#if !defined(LUA_VERSION_NUM) || LUA_VERSION_NUM < 501
#  error You need to compile against Lua 5.1 or higher
#endif

#define TYPE_LIMIT_HARD	"org.conman.process:rlimit_hard"
#define TYPE_LIMIT_SOFT	"org.conman.process:rlimit_soft"

#ifndef __GNUC__
#  define __attribute__(x)
#endif

#if ULONG_MAX > 4294967295uL
#  if ULONG_MAX > 9007199254740992uL
#    define MAX_RESOURCE_LIMIT	9007199254740992uL
#  else
#    define MAX_RESOURCE_LIMIT	RLIM_INFINITY
#  endif
#else
#  define MAX_RESOURCE_LIMIT	RLIM_INFINITY
#endif

/**********************************************************************/

struct strint
{
  const char *const text;
  const int         value;
};

/**********************************************************************
* PROCESS CREATION AND DESTRUCTION
**********************************************************************/

static int proclua_setsid(lua_State *const L)
{
  pid_t id = setsid();
  
  if (id == -1)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
  }
  else
  {
    lua_pushnumber(L,id);
    lua_pushinteger(L,0);
  }
  
  return 2;
}

/********************************************************************/

static int proclua_fork(lua_State *const L)
{
  pid_t child;
  
  child = fork();
  if (child < 0)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
  }
  else
  {
    lua_pushinteger(L,child);
    lua_pushinteger(L,0);
  }
  return 2;
}

/********************************************************************/

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

#if LUA_VERSION_NUM == 501  
  argc = lua_objlen(L,2) + 2;
#else
  argc = lua_rawlen(L,2) + 2;
#endif
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
    
    if (lua_type(L,3) != LUA_TTABLE)
    {
      free(argv);
      luaL_checktype(L,3,LUA_TTABLE);
    }

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

/******************************************************************/

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
    int rc = WEXITSTATUS(status);
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
  else if (WIFSTOPPED(status))
  {
    lua_pushinteger(L,WSTOPSIG(status));
    lua_setfield(L,-2,"rc");
    lua_pushstring(L,strsignal(WSTOPSIG(status)));
    lua_setfield(L,-2,"description");
    lua_pushliteral(L,"stopped");
    lua_setfield(L,-2,"status");
  }
  else if (WIFSIGNALED(status))
  {
    lua_pushinteger(L,WTERMSIG(status));
    lua_setfield(L,-2,"rc");
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

/***********************************************************************/

static int proclua_exit(lua_State *const L)
{
  assert(L != NULL);
  
  _exit(luaL_optinteger(L,1,EXIT_SUCCESS));
}

/***********************************************************************/

static int proclua_wait(lua_State *const L)
{
  pid_t child;
  int   status;
  int   flag;
  int   rc;
  
  child = luaL_optinteger(L,1,-1);
  flag  = lua_toboolean(L,2) 
  		? WUNTRACED | WCONTINUED | WNOHANG 
  		: WUNTRACED | WCONTINUED;
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
  lua_pushinteger(L,0);
  return 2;
}

/*********************************************************************/

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

/******************************************************************/

static int proclua_waitusage(lua_State *const L)
{
  struct rusage usage;
  pid_t         child;
  int           status;
  int           flag;
  int           rc;

  child = luaL_optinteger(L,1,-1);
  flag  = lua_toboolean(L,2) 
  		? WUNTRACED | WCONTINUED | WNOHANG 
  		: WUNTRACED | WCONTINUED ;
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
    lua_pushliteral(L,"normal");
    lua_setfield(L,-2,"status");
    lua_pushinteger(L,info.si_status);
    lua_setfield(L,-2,"rc");
  }
  else
  {
    switch(info.si_code)
    {
      case CLD_KILLED:    lua_pushliteral(L,"terminated");        break;
      case CLD_DUMPED:    lua_pushliteral(L,"terminated");        break;
      case CLD_STOPPED:   lua_pushliteral(L,"stopped");           break;
      case CLD_TRAPPED:   lua_pushliteral(L,"trapped");           break;
      case CLD_CONTINUED: lua_pushliteral(L,"continued");         break;
      default:            lua_pushfstring(L,"(%d)",info.si_code); break;
    }
    lua_setfield(L,-2,"status");
    lua_pushinteger(L,info.si_status);
    lua_setfield(L,-2,"rc");
    lua_pushstring(L,strsignal(info.si_status));
    lua_setfield(L,-2,"description");
    lua_pushboolean(L,info.si_code == CLD_DUMPED);
    lua_setfield(L,-2,"core");
  }
  
  lua_pushinteger(L,0);
  return 2;
}

/**********************************************************************
* RESOURCE UTILITZATION
**********************************************************************/

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

/*********************************************************************/

static int proclua_meta___index(lua_State *const L)
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

static int proclua_meta___newindex(lua_State *const L)
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

static bool limit_trans(
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

static bool limit_valid_suffix(
	lua_Number *const restrict pval,
	const int                  key,
	const char *const restrict unit
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
  
  switch(key)
  {
    case RLIMIT_CPU:
         switch(toupper(*unit))
         {
           case 'S':                     break;
           case 'M':  *pval *=     60.0; break;
           case 'H':  *pval *=   3600.0; break;
           case 'D':  *pval *=  86400.0; break;
           case 'W':  *pval *= 604800.0; break;
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
           case 'K':  *pval *= (1024.0);                   break;
           case 'M':  *pval *= (1024.0 * 1024.0);          break;
           case 'G':  *pval *= (1024.0 * 1024.0 * 1024.0); break;
           case '\0':                                      break;
           default: return false;
         }
         return true;
         
    default:
         return false;
  }  
}

/******************************************************************/

static int hlimitlua_meta___index(lua_State *const L)
{
  struct rlimit  limit;
  const char    *tkey;
  int            key;
  int            rc;
  
  assert(L != NULL);
  
  luaL_checkudata(L,1,TYPE_LIMIT_HARD);
  tkey = luaL_checkstring(L,2);
  
  if (!limit_trans(&key,tkey))
    return luaL_error(L,"Illegal limit resource: %s",tkey);
  
  rc = getrlimit(key,&limit);
  if (rc == -1)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    return 2;
  }
  
  if (limit.rlim_max == RLIM_INFINITY)
    lua_pushnumber(L,HUGE_VAL);
  else
    lua_pushnumber(L,limit.rlim_max);

  return 1;
}

/************************************************************************/

static int hlimitlua_meta___newindex(lua_State *const L)
{
  struct rlimit  limit;
  const char    *tkey;
  int            key;
  lua_Number     ival;

  assert(L != NULL);
  
  luaL_checkudata(L,1,TYPE_LIMIT_HARD);
  tkey = luaL_checkstring(L,2);
  
  if (!limit_trans(&key,tkey))
    return luaL_error(L,"Illegal limit resource: %s",tkey);

  if (lua_isnumber(L,3))
    ival = lua_tonumber(L,3);
  else if (lua_isstring(L,3))
  {
    const char *tval;
    const char *unit;

    tval = lua_tostring(L,3);
    ival = strtod(tval,(char **)&unit);

    if (!limit_valid_suffix(&ival,key,unit))
      return luaL_error(L,"Illegal suffix: %c",*unit);
  } 
  else
    return luaL_error(L,"Non-supported type");
  
  if (ival >= MAX_RESOURCE_LIMIT)
  {
    limit.rlim_cur = RLIM_INFINITY;
    limit.rlim_max = RLIM_INFINITY;
  }
  else
  {
    limit.rlim_cur = ival;
    limit.rlim_max = ival;
  }
  
  setrlimit(key,&limit);
  return 0;
}

/************************************************************************/

static int slimitlua_meta___index(lua_State *const L)
{
  struct rlimit  limit;
  const char    *tkey;
  int            key;
  int            rc;
  
  assert(L != NULL);
  
  luaL_checkudata(L,1,TYPE_LIMIT_SOFT);
  tkey = luaL_checkstring(L,2);
  
  if (!limit_trans(&key,tkey))
    return luaL_error(L,"Illegal limit resource: %s",tkey);
  
  rc = getrlimit(key,&limit);
  if (rc == -1)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    return 2;
  }
  
  if (limit.rlim_cur == RLIM_INFINITY)
    lua_pushnumber(L,HUGE_VAL);
  else
    lua_pushnumber(L,limit.rlim_cur);
    
  return 1;
}

/************************************************************************/

static int slimitlua_meta___newindex(lua_State *const L)
{
  struct rlimit  climit;
  struct rlimit  limit;
  const char    *tkey;
  int            key;
  lua_Number     ival;
  int            rc;
  
  assert(L != NULL);
  
  luaL_checkudata(L,1,TYPE_LIMIT_SOFT);
  tkey = luaL_checkstring(L,2);
  
  if (!limit_trans(&key,tkey))
    return luaL_error(L,"Illegal limit resource: %s",tkey);
  
  if (lua_isnumber(L,3))
    ival = lua_tonumber(L,3);
  else if (lua_isstring(L,3))
  {
    const char *tval;
    const char *unit;
    
    tval = lua_tostring(L,3);
    ival = strtod(tval,(char **)&unit);
    if (!limit_valid_suffix(&ival,key,unit))
      return luaL_error(L,"Illegal suffix: %c",*unit);
  } 
  else
    return luaL_error(L,"Non-supported type");

  if (ival >= MAX_RESOURCE_LIMIT)
    limit.rlim_cur = RLIM_INFINITY;
  else
    limit.rlim_cur = ival;
  
  rc = getrlimit(key,&climit);
  if (rc == -1)
    return 0;
  
  limit.rlim_max = climit.rlim_max;
  setrlimit(key,&limit);
  return 0;
}

/************************************************************************
* PROCESS USER AND GROUP INFORMATION
************************************************************************/

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

/************************************************************************
* MISC PROCESS ROUTINES
*************************************************************************/

static int proclua_pause(lua_State *const L)
{
  pause();
  lua_pushinteger(L,errno);
  return 1;
}

/*********************************************************************
* CPU AFINITY ROUTINES
*********************************************************************/

static int proclua_getaffinity(lua_State *const L)
{
#if defined(__linux)

  cpu_set_t set;
  long      cpus;
  
  if (sched_getaffinity(luaL_optinteger(L,1,0),sizeof(set),&set) < 0)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    return 2;
  }
  
  cpus = sysconf(_SC_NPROCESSORS_ONLN);
  
  lua_createtable(L,sizeof(set),0);
  for (long i = 0 ; i < cpus ; i++)
  {
    lua_pushinteger(L,i+1);
    lua_pushboolean(L,CPU_ISSET(i,&set));
    lua_settable(L,-3);
  }
  lua_pushinteger(L,0);
  return 2;

#elif defined(__SunOS)

  processorid_t id;
  
  if (processor_bind(P_PID,luaL_optinteger(L,1,P_MYID),PBIND_QUERY,&id) < 0)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
  }
  else
  {
    lua_pushnumber(L,id);
    lua_pushinteger(L,0);
  }
  return 2;
  
#else

  lua_pushnil(L);
  lua_pushinteger(L,ENOSYS);
  return 2;

#endif
}

/************************************************************************/

static int proclua_setaffinity(lua_State *const L)
{
#if defined(__linux)

  cpu_set_t set;
  pid_t     pid;
  long      max;
  long      i;
  
  CPU_ZERO(&set);
  pid = luaL_optinteger(L,1,0);
  max = sysconf(_SC_NPROCESSORS_ONLN);
  
  if (lua_istable(L,2))
  {
    for (i = 1 ; max > 0 ; i++ , max--)
    {
      lua_pushinteger(L,i);
      lua_gettable(L,2);
      if (lua_toboolean(L,-1))
        CPU_SET(i - 1,&set);
      lua_pop(L,1);
    }
  }
  else
  {
    for (i = 2 ; max > 0 ; i++ , max--)
      if (lua_isnumber(L,i))
        CPU_SET(lua_tointeger(L,i) - 1,&set);
  } 
  
  errno = 0;
  sched_setaffinity(pid,sizeof(set),&set);
  lua_pushboolean(L,errno == 0);
  lua_pushinteger(L,errno);
  return 2;

#elif defined(__SunOS)

  errno = 0;
  processor_bind(
  	P_PID,
  	luaL_optinteger(L,1,P_MYID),
  	luaL_checkinteger(L,2),
  	NULL
  );
  
  lua_pushboolean(L,errno == 0);
  lua_pushinteger(L,errno);
  return 2;
  
#else

  lua_pushnil(L);
  lua_pushinteger(L,ENOSYS);
  return 2;
  
#endif
}

/***********************************************************************/

static const struct strint m_sysexits[] =
{
  { "SUCCESS"	, EXIT_SUCCESS	} ,
  { "FAILURE"	, EXIT_FAILURE	} ,

#ifdef EX_OK
  { "OK" , EX_OK } ,
#else
  { "OK" , EXIT_SUCCESS } ,
#endif
#ifdef EX_USAGE
  { "USAGE" , EX_USAGE } ,
#endif
#ifdef EX_DATAERR
  { "DATAERR" , EX_DATAERR } ,
#endif
#ifdef EX_NOINPUT
  { "NOINPUT" , EX_NOINPUT } ,
#endif
#ifdef EX_NOUSER
  { "NOUSER" , EX_NOUSER } ,
#endif
#ifdef EX_NOHOST
  { "NOHOST" , EX_NOHOST } ,
#endif
#ifdef EX_UNAVAILABLE
  { "UNAVAILABLE" , EX_UNAVAILABLE } ,
#endif
#ifdef EX_SOFTWARE
  { "SOFTWARE" , EX_SOFTWARE } ,
#endif
#ifdef EX_OSERR
  { "OSERR" , EX_OSERR } ,
#endif
#ifdef EX_OSFILE
  { "OSFILE" , EX_OSFILE } ,
#endif
#ifdef EX_CANTCREAT
  { "CANTCREATE" , EX_CANTCREAT } ,
#endif
#ifdef EX_IOERR
  { "IOERR" , EX_IOERR } ,
#endif
#ifdef EX_TEMPFAIL
  { "TEMPFAIL" , EX_TEMPFAIL } ,
#endif
#ifdef EX_PROTOCOL
  { "PROTOCOL" , EX_PROTOCOL } ,
#endif
#ifdef EX_NOPERM
  { "NOPERM" , EX_NOPERM } ,
#endif
#ifdef EX_CONFIG
  { "CONFIG" , EX_CONFIG } ,
#endif
#ifdef EX_NOTFOUND
  { "NOTFOUND" , EX_NOTFOUND } ,
#endif
  { NULL , 0 }
};

static const struct luaL_Reg m_process_reg[] =
{
  { "setsid"		, proclua_setsid		} ,
  { "getuid"		, proclua_getuid		} ,
  { "getgid"		, proclua_getgid		} ,
  { "setuid"		, proclua_setuid		} ,
  { "setgid"		, proclua_setgid		} ,
  { "exit"		, proclua_exit			} ,
  { "fork"		, proclua_fork			} ,
  { "wait"		, proclua_wait			} ,
  { "waitusage"		, proclua_waitusage		} ,
  { "waitid"		, proclua_waitid		} ,
  { "exec"		, proclua_exec			} ,
  { "times"		, proclua_times			} ,
  { "getrusage"		, proclua_getrusage		} ,
  { "pause"		, proclua_pause			} ,
  { "getaffinity"	, proclua_getaffinity		} ,
  { "setaffinity"	, proclua_setaffinity		} ,
  { NULL		, NULL				} 
};

static const struct luaL_Reg m_process_meta[] =
{
  { "__index"		, proclua_meta___index		} ,
  { "__newindex"	, proclua_meta___newindex	} ,
  { NULL		, NULL				}
};

static const struct luaL_Reg m_hlimit_meta[] =
{
  { "__index" 		, hlimitlua_meta___index	} ,
  { "__newindex"	, hlimitlua_meta___newindex	} ,
  { NULL		, NULL				}
};

static const struct luaL_Reg m_slimit_meta[] =
{
  { "__index"		, slimitlua_meta___index	} ,
  { "__newindex"	, slimitlua_meta___newindex	} ,
  { NULL		, NULL				}
};

int luaopen_org_conman_process(lua_State *const L)
{
  assert(L != NULL);

#if LUA_VERSION_NUM == 501  
  luaL_newmetatable(L,TYPE_LIMIT_HARD);
  luaL_register(L,NULL,m_hlimit_meta);
  
  luaL_newmetatable(L,TYPE_LIMIT_SOFT);
  luaL_register(L,NULL,m_slimit_meta);
  
  luaL_register(L,"org.conman.process",m_process_reg);
#else
  luaL_newmetatable(L,TYPE_LIMIT_HARD);
  luaL_setfuncs(L,m_hlimit_meta,0);
  
  luaL_newmetatable(L,TYPE_LIMIT_SOFT);
  luaL_setfuncs(L,m_slimit_meta,0);

  luaL_newlib(L,m_process_reg);  
#endif

  lua_createtable(L,0,0);
  for (size_t i = 0 ; m_sysexits[i].text != NULL ; i++)
  {
    lua_pushinteger(L,m_sysexits[i].value);
    lua_setfield(L,-2,m_sysexits[i].text);
  }
  lua_setfield(L,-2,"EXIT");

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
#if LUA_VERSION_NUM == 501
  luaL_register(L,NULL,m_process_meta);
#else
  luaL_newlib(L,m_process_meta);
#endif
  lua_setmetatable(L,-2);
    
  return 1;
}

/************************************************************************/
