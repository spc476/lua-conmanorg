
#define _GNU_SOURCE

#include <stddef.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include <ctype.h>
#include <signal.h>
#include <assert.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <syslog.h>

#include <sys/resource.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <unistd.h>

#define SYS_LIMIT_HARD	"rlimit_hard"
#define SYS_LIMIT_SOFT	"rlimit_soft"

/*************************************************************************/

static int	proclua_ignorechild	(lua_State *const);
static int	proclua_getuid		(lua_State *const);
static int	proclua_getgid		(lua_State *const);
static int	proclua_setuid		(lua_State *const);
static int	proclua_setgid		(lua_State *const);
static int	proclua_exit		(lua_State *const);
static int	proclua_fork		(lua_State *const);
static int	proclua_wait		(lua_State *const);
static int	proclua_waitusage	(lua_State *const);
static int	proclua_sleep		(lua_State *const);
static int	proclua_sleepres	(lua_State *const);
static int	proclua_kill		(lua_State *const);
static int	proclua_exec		(lua_State *const);
static int	proclua_times		(lua_State *const);
static int	proclua___index		(lua_State *const);
static int	proclua___newindex	(lua_State *const);
static bool	mlimit_trans		(int *const restrict,const char *const restrict);
static bool	mlimit_valid_suffix	(lua_Integer *const restrict,const int,const char *const restrict);
static int	mhlimitlua___index	(lua_State *const);
static int	mhlimitlua___newindex	(lua_State *const);
static int	mslimitlua___index	(lua_State *const);
static int	mslimitlua___newindex	(lua_State *const);

/************************************************************************/

static const struct luaL_reg mprocess_reg[] =
{
  { "ignorechild"	, proclua_ignorechild	} ,
  { "getuid"		, proclua_getuid	} ,
  { "getgid"		, proclua_getgid	} ,
  { "setuid"		, proclua_setuid	} ,
  { "setgid"		, proclua_setgid	} ,
  { "exit"		, proclua_exit		} ,
  { "fork"		, proclua_fork		} ,
  { "wait"		, proclua_wait		} ,
  { "waitusage"		, proclua_waitusage	} ,
  { "sleep"		, proclua_sleep		} ,
  { "sleepres"		, proclua_sleepres	} ,
  { "kill"		, proclua_kill		} ,
  { "exec"		, proclua_exec		} ,
  { "times"		, proclua_times		} ,
  { NULL		, NULL			} 
};

static const struct luaL_reg mprocess_meta[] =
{
  { "__index"		, proclua___index	} ,
  { "__newindex"	, proclua___newindex	} ,
  { NULL		, NULL			}
};

static const struct luaL_reg mhlimit_reg[] =
{
  { "__index" 		, mhlimitlua___index	} ,
  { "__newindex"	, mhlimitlua___newindex	} ,
  { NULL		, NULL			}
};

static const struct luaL_reg mslimit_reg[] =
{
  { "__index"		, mslimitlua___index	} ,
  { "__newindex"	, mslimitlua___newindex	} ,
  { NULL		, NULL			}
};

/*************************************************************************/

static int proclua_ignorechild(lua_State *const L __attribute__((unused)))
{
  signal(SIGCHLD,SIG_IGN);
  return 0;
}

/************************************************************************/

static int proclua_getuid(lua_State *const L)
{
  uid_t uid;
  uid_t euid;
  uid_t suid;

  getresuid(&uid,&euid,&suid);
  lua_pushinteger(L,uid);
  lua_pushinteger(L,euid);
  lua_pushinteger(L,suid);
  return 3;
}

/************************************************************************/

static int proclua_getgid(lua_State *const L)
{
  gid_t gid;
  gid_t egid;
  gid_t sgid;
  
  getresgid(&gid,&egid,&sgid);  
  lua_pushinteger(L,gid);
  lua_pushinteger(L,egid);
  lua_pushinteger(L,sgid);
  return 3;
}

/********************************************************************/

static int proclua_setuid(lua_State *const L)
{
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
}

/*************************************************************************/

static int proclua_setgid(lua_State *const L)
{
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
    lua_getglobal(L,"org");
    lua_getfield(L,-1,"conman");
    lua_getfield(L,-1,"process");
    lua_pushinteger(L,getpid());
    lua_setfield(L,-2,"PID");
    lua_pushinteger(L,child);
    return 1;
  }
  else
  {
    int err = errno;
    
    lua_pushnil(L);
    lua_pushinteger(L,err);
    return 2;
  }
}

/***********************************************************************/

static int proclua_wait(lua_State *const L)
{
  pid_t child;
  int   status;
  int   rc;
  
  child = luaL_optinteger(L,1,-1);
  rc    = waitpid(child,&status,0);
  if (rc == -1)
  {
    int err = errno;
    lua_pushnil(L);
    lua_pushinteger(L,err);
    return 2;
  }
  
  lua_createtable(L,0,0);
  lua_pushinteger(L,rc);
  lua_setfield(L,-2,"pid");
  
  if (WIFEXITED(status))
  {
    lua_pushinteger(L,WEXITSTATUS(status));
    lua_setfield(L,-2,"rc");
    lua_pushliteral(L,"normal");
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
  return 1;
}

/*********************************************************************/

static int proclua_waitusage(lua_State *const L)
{
  struct rusage usage;
  pid_t         child;
  int           status;
  int           rc;

  child = luaL_optinteger(L,1,-1);
  rc    = wait4(child,&status,0,&usage);
  if (rc == -1)
  {
    int err = errno;
    lua_pushnil(L);
    lua_pushnil(L);
    lua_pushinteger(L,err);
    return 2;
  }
  
  lua_createtable(L,0,0);
  lua_pushinteger(L,rc);
  lua_setfield(L,-2,"pid");
  
  if (WIFEXITED(status))
  {
    lua_pushinteger(L,WEXITSTATUS(status));
    lua_setfield(L,-2,"rc");
    lua_pushliteral(L,"normal");
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
  
  lua_createtable(L,0,0);
  
  lua_pushnumber(L,usage.ru_utime.tv_sec + ((double)usage.ru_utime.tv_usec / 1000000.0));
  lua_setfield(L,-2,"utime");
  
  lua_pushnumber(L,usage.ru_stime.tv_sec + ((double)usage.ru_stime.tv_usec / 1000000.0));
  lua_setfield(L,-2,"stime");
  
  lua_pushnumber(L,usage.ru_maxrss);
  lua_setfield(L,-2,"maxrss");
  
  lua_pushnumber(L,usage.ru_ixrss);
  lua_setfield(L,-2,"text");
  
  lua_pushnumber(L,usage.ru_idrss);
  lua_setfield(L,-2,"data");
  
  lua_pushnumber(L,usage.ru_isrss);
  lua_setfield(L,-2,"stack");
  
  lua_pushnumber(L,usage.ru_minflt);
  lua_setfield(L,-2,"softfaults");
  
  lua_pushnumber(L,usage.ru_majflt);
  lua_setfield(L,-2,"hardfaults");

  lua_pushnumber(L,usage.ru_nswap);
  lua_setfield(L,-2,"swapped");
  
  lua_pushnumber(L,usage.ru_inblock);
  lua_setfield(L,-2,"inblock");
  
  lua_pushnumber(L,usage.ru_oublock);
  lua_setfield(L,-2,"outblock");
  
  lua_pushnumber(L,usage.ru_msgsnd);
  lua_setfield(L,-2,"ipcsend");
  
  lua_pushnumber(L,usage.ru_msgrcv);
  lua_setfield(L,-2,"ipcreceive");
  
  lua_pushnumber(L,usage.ru_nsignals);
  lua_setfield(L,-2,"signals");
  
  lua_pushnumber(L,usage.ru_nvcsw);
  lua_setfield(L,-2,"coopcs");
  
  lua_pushnumber(L,usage.ru_nivcsw);
  lua_setfield(L,-2,"preemptcs");
  
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
    int err = errno;
    lua_pushnumber(L,param);
    lua_pushinteger(L,err);
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
    int err = errno;
    lua_pushboolean(L,false);
    lua_pushinteger(L,err);
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
  
  argv[0] = basename(binary);
  for (size_t i = 1 ; i < argc - 1 ; i++)
  {
    lua_pushinteger(L,i);
    lua_gettable(L,2);
    argv[i] = (char *)lua_tostring(L,-1);
    lua_pop(L,1);
  }
  argv[argc - 1] = NULL;
  
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
    int err = errno;
    lua_pushnil(L);
    lua_pushinteger(L,err);
    return 2;
  }

  lua_createtable(L,0,4);
  lua_pushinteger(L,tms.tms_utime);
  lua_setfield(L,-2,"utime");
  lua_pushinteger(L,tms.tms_stime);
  lua_setfield(L,-2,"stime");
  lua_pushinteger(L,tms.tms_cutime);
  lua_setfield(L,-2,"cutime"); 
  lua_pushinteger(L,tms.tms_cstime);
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
  else if (strcmp(tag,"as") == 0)
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
  void          *ud;
  const char    *tkey;
  int            key;
  int            rc;
  
  assert(L != NULL);
  
  ud   = luaL_checkudata(L,1,SYS_LIMIT_HARD);
  tkey = luaL_checkstring(L,2);
  
  if (!mlimit_trans(&key,tkey))
    return luaL_error(L,"Illegal limit resource: %s",tkey);
  
  rc = getrlimit(key,&limit);
  if (rc == -1)
  {
    int err = errno;
    lua_pushnil(L);
    lua_pushinteger(L,err);
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
  void          *ud;
  const char    *tkey;
  int            key;
  lua_Integer    ival;

  assert(L != NULL);
  
  ud   = luaL_checkudata(L,1,SYS_LIMIT_HARD);
  tkey = luaL_checkstring(L,2);
  
  if (!mlimit_trans(&key,tkey))
    return luaL_error(L,"Illegal limit resource: %s",tkey);

  if (lua_isstring(L,3))
  {
    const char *tval;
    const char *unit;
    
    tval = lua_tostring(L,3);
    ival = strtoul(tval,(char **)&unit,10);

    if (!mlimit_valid_suffix(&ival,key,unit))
      return luaL_error(L,"Illegal suffix: %c",*unit);
  } 
  else if (lua_isnumber(L,3))
    ival = lua_tointeger(L,3);
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
  void          *ud;
  const char    *tkey;
  int            key;
  int            rc;
  
  assert(L != NULL);
  
  ud   = luaL_checkudata(L,1,SYS_LIMIT_SOFT);
  tkey = luaL_checkstring(L,2);
  
  if (!mlimit_trans(&key,tkey))
    return luaL_error(L,"Illegal limit resource: %s",tkey);
  
  rc = getrlimit(key,&limit);
  if (rc == -1)
  {
    int err = errno;
    lua_pushnil(L);
    lua_pushinteger(L,err);
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
  void          *ud;
  const char    *tkey;
  int            key;
  lua_Integer    ival;
  int            rc;
  
  assert(L != NULL);
  
  ud   = luaL_checkudata(L,1,SYS_LIMIT_SOFT);
  tkey = luaL_checkstring(L,2);
  
  if (!mlimit_trans(&key,tkey))
    return luaL_error(L,"Illegal limit resource: %s",tkey);

  if (lua_isstring(L,3))
  {
    const char *tval;
    const char *unit;
    
    tval = lua_tostring(L,3);
    ival = strtoul(tval,(char **)&unit,10);

    if (!mlimit_valid_suffix(&ival,key,unit))
      return luaL_error(L,"Illegal suffix: %c",*unit);
  } 
  else if (lua_isnumber(L,3))
    ival = lua_tointeger(L,3);
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

int luaopen_org_conman_process(lua_State *const L)
{
  void *udata;
  
  assert(L != NULL);
  
  luaL_newmetatable(L,SYS_LIMIT_HARD);
  luaL_register(L,NULL,mhlimit_reg);
  
  luaL_newmetatable(L,SYS_LIMIT_SOFT);
  luaL_register(L,NULL,mslimit_reg);
  
  luaL_register(L,"org.conman.process",mprocess_reg);
  lua_createtable(L,0,2);
  
  udata = lua_newuserdata(L,sizeof(int));
  luaL_getmetatable(L,SYS_LIMIT_HARD);
  lua_setmetatable(L,-2);
  lua_setfield(L,-2,"hard");
  
  udata = lua_newuserdata(L,sizeof(int));
  luaL_getmetatable(L,SYS_LIMIT_SOFT);
  lua_setmetatable(L,-2);
  lua_setfield(L,-2,"soft");
  
  lua_setfield(L,-2,"limits");
  
  lua_createtable(L,0,0);
  luaL_register(L,NULL,mprocess_meta);
  lua_setmetatable(L,-2);
  
  return 1;
}

/************************************************************************/
