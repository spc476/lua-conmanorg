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
#  define _DEFAULT_SOURCE
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

#if LUA_VERSION_NUM == 501
  static inline void lua_getuservalue(lua_State *L,int idx)
  {
    lua_getfenv(L,idx);
  }
  
  static inline void lua_setuservalue(lua_State *L,int idx)
  {
    lua_setfenv(L,idx);
  }
  
  static inline void luaL_newlib(lua_State *L,luaL_Reg const *list)
  {
    lua_createtable(L,0,0);
    luaL_register(L,NULL,list);
  }
  
  static inline size_t lua_rawlen(lua_State *L,int idx)
  {
    return lua_objlen(L,idx);
  }
#endif

/**********************************************************************
* PROCESS CREATION AND DESTRUCTION
**********************************************************************/

static int proclua_setsid(lua_State *L)
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

static int proclua_setpgid(lua_State *L)
{
  errno = 0;
  setpgid(luaL_optinteger(L,1,0),luaL_optinteger(L,2,0));
  lua_pushboolean(L,errno == 0);
  lua_pushinteger(L,errno);
  return 2;
}

/********************************************************************/

static int proclua_getpgid(lua_State *L)
{
  pid_t id = getpgid(luaL_optinteger(L,1,0));
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

static int proclua_setpgrp(lua_State *L)
{
  errno = 0;
  setpgid(0,0);
  lua_pushboolean(L,errno == 0);
  lua_pushinteger(L,errno);
  return 2;
}

/********************************************************************/

static int proclua_getpgrp(lua_State *L)
{
  pid_t id = getpgid(0);
  
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

static int proclua_fork(lua_State *L)
{
  pid_t child = fork();
  
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

static int proclua_exec(lua_State *L)
{
  extern char **environ;
  char const   *binary;
  char        **argv;
  size_t        argc;
  char        **envp;
  size_t        envc;
  size_t        envm;
  int           err;
  
  binary = luaL_checkstring(L,1);
  luaL_checktype(L,2,LUA_TTABLE);
  
  argc = lua_rawlen(L,2) + 2;
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
      char const *name;
      size_t      nsize;
      char const *value;
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
        lua_State   *L,
        const pid_t  pid,
        int          status
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
    lua_pushstring(L,(rc == EXIT_SUCCESS) ? "success" : "failure");
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

static int proclua_exit(lua_State *L)
{
  assert(L != NULL);
  
  _exit(luaL_optinteger(L,1,EXIT_SUCCESS));
}

/***********************************************************************/

static int proclua_wait(lua_State *L)
{
  int   status;
  pid_t child = luaL_optinteger(L,1,-1);
  int   flag  = lua_toboolean(L,2)
              ? WUNTRACED | WCONTINUED | WNOHANG
              : WUNTRACED | WCONTINUED;
  int   rc    = waitpid(child,&status,flag);
  
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
        lua_State     *L,
        struct rusage *usage
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

static int proclua_waitusage(lua_State *L)
{
  struct rusage usage;
  int           status;
  pid_t         child = luaL_optinteger(L,1,-1);
  int           flag  = lua_toboolean(L,2)
                      ? WUNTRACED | WCONTINUED | WNOHANG
                      : WUNTRACED | WCONTINUED ;
  int           rc    = wait4(child,&status,flag,&usage);
  
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

static int proclua_waitid(lua_State *L)
{
  siginfo_t info;
  pid_t    child  = luaL_checkinteger(L,1);
  int      flag   = lua_toboolean(L,2)
                  ? WEXITED | WSTOPPED | WCONTINUED | WNOHANG
                  : WEXITED | WSTOPPED | WCONTINUED;
  idtype_t idtype = (child == 0) ? P_ALL : P_PID;
  
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
    lua_pushinteger(L,info.si_status);
    lua_setfield(L,-2,"rc");
    lua_pushstring(L,(info.si_status == EXIT_SUCCESS) ? "success" : "failure");
    lua_setfield(L,-2,"description");
    lua_pushliteral(L,"normal");
    lua_setfield(L,-2,"status");
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

static int proclua_times(lua_State *L)
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

static int proclua_getrusage(lua_State *L)
{
  struct rusage  usage;
  char const    *twho;
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

static int proclua_meta___index(lua_State *L)
{
  char const *idx;
  
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

static int proclua_meta___newindex(lua_State *L)
{
  char const *idx;
  
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

/*********************************************************************
* PROCESS LIMITS
**********************************************************************/

static bool limit_valid_suffix(rlim_t *pval,int key,char const *unit)
{
  assert(pval != NULL);
  assert(unit != NULL);
  
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
#ifdef RLIMIT_NPROC
    case RLIMIT_NPROC:
#endif
#ifdef RLIMIT_LOCKS
    case RLIMIT_LOCKS:
#endif
#if defined RLIMIT_RSS && !defined(__APPLE__)
    case RLIMIT_RSS:
#endif
         return (*unit == '\0');
         
    case RLIMIT_AS:
    case RLIMIT_CORE:
    case RLIMIT_DATA:
    case RLIMIT_FSIZE:
    case RLIMIT_STACK:
#ifdef RLIMIT_MEMLOCK
    case RLIMIT_MEMLOCK:
#endif
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
         assert(0);
         return false;
  }
}

/******************************************************************/

static int hlimitlua_meta___index(lua_State *L)
{
  struct rlimit  limit;
  
  assert(L != NULL);
  
  lua_getuservalue(L,1);
  lua_replace(L,1);
  lua_rawget(L,1);
  
  if (lua_isnumber(L,-1))
  {
    getrlimit(lua_tointeger(L,-1),&limit);
#   if LUA_VERSION_NUM <= 502
      lua_pushnumber(L,limit.rlim_max);
#   else
      lua_pushinteger(L,limit.rlim_max);
#   endif
  }
  else
    lua_pushnil(L);
    
  return 1;
}

/************************************************************************/

static int hlimitlua_meta___newindex(lua_State *L)
{
  struct rlimit limit;
  int           key;
  
  lua_getuservalue(L,1);
  lua_pushvalue(L,2);
  lua_rawget(L,-2);
  
  if (!lua_isnumber(L,-1))
    return 0;
    
  key = lua_tointeger(L,-1);
  
  if (lua_isnumber(L,3))
    limit.rlim_max = lua_tonumber(L,3);
  else if (lua_isstring(L,3))
  {
    char const *tval;
    char       *unit;
    
    tval           = lua_tostring(L,3);
    limit.rlim_max = strtoul(tval,&unit,10);
    if (!limit_valid_suffix(&limit.rlim_max,key,unit))
      return luaL_error(L,"Illegal suffix: %c",*unit);
  }
  else
    return luaL_error(L,"Non-supported value for resource");
    
  limit.rlim_cur = limit.rlim_max;
  setrlimit(key,&limit);
  return 0;
}

/************************************************************************/

#if LUA_VERSION_NUM >= 502

  static int hlimit____next(lua_State *L)
  {
    struct rlimit limit;
    
    lua_getuservalue(L,1);
    lua_pushvalue(L,2);
    if (lua_next(L,-2))
    {
      if (lua_isnumber(L,-1))
      {
        getrlimit(lua_tointeger(L,-1),&limit);
        lua_pop(L,1);
        lua_pushinteger(L,limit.rlim_max);
        return 2;
      }
    }
    
    return 0;
  }
  
  /*==============================================================*/
  
  static int hlimitlua_meta___pairs(lua_State *L)
  {
    lua_pushcfunction(L,hlimit____next);
    lua_insert(L,1);
    lua_pushnil(L);
    return 3;
  }
  
#endif

/************************************************************************/

static int slimitlua_meta___index(lua_State *L)
{
  struct rlimit limit;
  
  lua_getuservalue(L,1);
  lua_replace(L,1);
  lua_rawget(L,1);
  
  if (lua_isnumber(L,-1))
  {
    getrlimit(lua_tointeger(L,-1),&limit);
#   if LUA_VERSION_NUM <= 502
      lua_pushnumber(L,limit.rlim_cur);
#   else
      lua_pushinteger(L,limit.rlim_cur);
#   endif
  }
  else
    lua_pushnil(L);
    
  return 1;
}

/************************************************************************/

static int slimitlua_meta___newindex(lua_State *L)
{
  struct rlimit limit;
  int           key;
  
  lua_getuservalue(L,1);
  lua_pushvalue(L,2);
  lua_rawget(L,-2);
  
  if (!lua_isnumber(L,-1))
    return 0;
    
  key = lua_tointeger(L,-1);
  getrlimit(key,&limit);
  
  if (lua_isnumber(L,3))
    limit.rlim_cur = lua_tonumber(L,3);
  else if (lua_isstring(L,3))
  {
    char const *tval;
    char       *unit;
    
    tval           = lua_tostring(L,3);
    limit.rlim_cur = strtoul(tval,&unit,10);
    if (!limit_valid_suffix(&limit.rlim_cur,key,unit))
      return luaL_error(L,"Illegal suffix: %c",*unit);
  }
  else
    return luaL_error(L,"Non-supported value for resource");
    
  setrlimit(key,&limit);
  return 0;
}

/************************************************************************/

#if LUA_VERSION_NUM >= 502

  static int slimit____next(lua_State *L)
  {
    struct rlimit limit;
    
    lua_getuservalue(L,1);
    lua_pushvalue(L,2);
    if (lua_next(L,-2))
    {
      if (lua_isnumber(L,-1))
      {
        getrlimit(lua_tointeger(L,-1),&limit);
        lua_pop(L,1);
        lua_pushinteger(L,limit.rlim_cur);
        return 2;
      }
    }
    
    return 0;
  }
  
  /*==============================================================*/
  
  static int slimitlua_meta___pairs(lua_State *L)
  {
    lua_pushcfunction(L,slimit____next);
    lua_insert(L,1);
    lua_pushnil(L);
    return 3;
  }
  
#endif

/************************************************************************
* PROCESS USER AND GROUP INFORMATION
************************************************************************/

static int proclua_getuid(lua_State *L)
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

static int proclua_getgid(lua_State *L)
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

static int proclua_setuid(lua_State *L)
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
  
  if (seteuid(euid) < 0)
  {
    lua_pushinteger(L,errno);
    return 1;
  }
  
  lua_pushinteger(L,0);
  return 1;
#endif
}

/*************************************************************************/

static int proclua_setgid(lua_State *L)
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

static int proclua_pause(lua_State *L)
{
  pause();
  lua_pushinteger(L,errno);
  return 1;
}

/*********************************************************************
* CPU AFINITY ROUTINES
*********************************************************************/

static int proclua_getaffinity(lua_State *L)
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

static int proclua_setaffinity(lua_State *L)
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

int luaopen_org_conman_process(lua_State *L)
{
  static struct luaL_Reg const m_process_reg[] =
  {
    { "setsid"            , proclua_setsid                } ,
    { "setpgid"           , proclua_setpgid               } ,
    { "getpgid"           , proclua_getpgid               } ,
    { "setpgrp"           , proclua_setpgrp               } ,
    { "getpgrp"           , proclua_getpgrp               } ,
    { "getuid"            , proclua_getuid                } ,
    { "getgid"            , proclua_getgid                } ,
    { "setuid"            , proclua_setuid                } ,
    { "setgid"            , proclua_setgid                } ,
    { "exit"              , proclua_exit                  } ,
    { "fork"              , proclua_fork                  } ,
    { "wait"              , proclua_wait                  } ,
    { "waitusage"         , proclua_waitusage             } ,
    { "waitid"            , proclua_waitid                } ,
    { "exec"              , proclua_exec                  } ,
    { "times"             , proclua_times                 } ,
    { "getrusage"         , proclua_getrusage             } ,
    { "pause"             , proclua_pause                 } ,
    { "getaffinity"       , proclua_getaffinity           } ,
    { "setaffinity"       , proclua_setaffinity           } ,
    { NULL                , NULL                          }
  };
  
  static struct luaL_Reg const m_process_meta[] =
  {
    { "__index"           , proclua_meta___index          } ,
    { "__newindex"        , proclua_meta___newindex       } ,
    { NULL                , NULL                          }
  };
  
  static struct luaL_Reg const m_hlimit_meta[] =
  {
    { "__index"           , hlimitlua_meta___index        } ,
    { "__newindex"        , hlimitlua_meta___newindex     } ,
  #if LUA_VERSION_NUM >= 502
    { "__pairs"           , hlimitlua_meta___pairs        } ,
  #endif
    { NULL                , NULL                          }
  };
  
  static struct luaL_Reg const m_slimit_meta[] =
  {
    { "__index"           , slimitlua_meta___index        } ,
    { "__newindex"        , slimitlua_meta___newindex     } ,
  #if LUA_VERSION_NUM >= 502
    { "__pairs"           , slimitlua_meta___pairs        } ,
  #endif
    { NULL                , NULL                          }
  };
  
  assert(L != NULL);
  
  lua_createtable(L,0,11);
  lua_pushinteger(L,RLIMIT_AS);      lua_setfield(L,-2,"vm");
  lua_pushinteger(L,RLIMIT_CORE);    lua_setfield(L,-2,"core");
  lua_pushinteger(L,RLIMIT_CPU);     lua_setfield(L,-2,"cpu");
  lua_pushinteger(L,RLIMIT_DATA);    lua_setfield(L,-2,"data");
  lua_pushinteger(L,RLIMIT_FSIZE);   lua_setfield(L,-2,"fsize");
  lua_pushinteger(L,RLIMIT_NOFILE);  lua_setfield(L,-2,"nofile");
  lua_pushinteger(L,RLIMIT_STACK);   lua_setfield(L,-2,"stack");
#ifdef RLIMIT_MEMLOCK
  lua_pushinteger(L,RLIMIT_MEMLOCK); lua_setfield(L,-2,"memlock");
#endif
#ifdef RLIMIT_NPROC
  lua_pushinteger(L,RLIMIT_NPROC);   lua_setfield(L,-2,"nproc");
#endif
#ifdef RLIMIT_LOCKS
  lua_pushinteger(L,RLIMIT_LOCKS);   lua_setfield(L,-2,"locks");
#endif
#if defined(RLIMIT_RSS) && !defined(__APPLE__)
  lua_pushinteger(L,RLIMIT_RSS);     lua_setfield(L,-2,"pages");
#endif

  lua_createtable(L,0,2);
  
  lua_newuserdata(L,0);
  lua_pushvalue(L,-3);
  lua_setuservalue(L,-2);
  luaL_newlib(L,m_hlimit_meta);
  lua_setmetatable(L,-2);
  lua_setfield(L,-2,"hard");
  
  lua_newuserdata(L,0);
  lua_pushvalue(L,-3);
  lua_setuservalue(L,-2);
  luaL_newlib(L,m_slimit_meta);
  lua_setmetatable(L,-2);
  lua_setfield(L,-2,"soft");
  
#if LUA_VERSION_NUM == 501
  luaL_register(L,"org.conman.process",m_process_reg);
#else
  luaL_newlib(L,m_process_reg);
#endif

  lua_pushvalue(L,-2);
  lua_setfield(L,-2,"limits");
  
  luaL_newlib(L,m_process_meta);
  lua_setmetatable(L,-2);
  
  return 1;
}

/************************************************************************/
