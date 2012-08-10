/*********************************************************************
*
* Copyright 2010 by Sean Conner.  All Rights Reserved.
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
*********************************************************************/

#ifdef __linux
#  define _BSD_SOURCE
#  define _POSIX_SOURCE
#endif

#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>

#include <luaconf.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <libgen.h>
#include <utime.h>

#define FSYS_FD		"fsys:fd"

/*************************************************************************/

typedef struct filedescr
{
  int fh;
} fd__t;

typedef struct fileptr
{
  FILE *fp;
  int   ref;
} fp__t;

/*************************************************************************/

static int fsys_chroot(lua_State *L)
{
  if (chroot(luaL_checkstring(L,1)) < 0)
  {
    int err = errno;
    lua_pushboolean(L,false);
    lua_pushinteger(L,err);
  }
  else
  {
    chdir("/");
    lua_pushboolean(L,true);
    lua_pushinteger(L,0);
  }
  return 2;
}

/*************************************************************************/

static int fsys_chdir(lua_State *L)
{
  if (chdir(luaL_checkstring(L,1)) < 0)
  {
    int err = errno;
    lua_pushboolean(L,false);
    lua_pushinteger(L,err);
  }
  else
  {
    lua_pushboolean(L,true);
    lua_pushinteger(L,0);
  }
  
  return 2;
}

/*************************************************************************/

static int fsys_symlink(lua_State *L)
{
  const char *old = luaL_checkstring(L,1);
  const char *new = luaL_checkstring(L,2);
  
  if (symlink(old,new) < 0)
  {
    int err = errno;
    lua_pushboolean(L,false);
    lua_pushinteger(L,err);
  }
  else
  {
    lua_pushboolean(L,true);
    lua_pushinteger(L,0);
  }
  return 2;
}

/*************************************************************************/

static int fsys_link(lua_State *L)
{
  const char *old = luaL_checkstring(L,1);
  const char *new = luaL_checkstring(L,2);
  
  if (link(old,new) < 0)
  {
    int err = errno;
    lua_pushboolean(L,false);
    lua_pushinteger(L,err);
  }
  else
  {
    lua_pushboolean(L,true);
    lua_pushinteger(L,0);
  }
  return 2;
}

/**********************************************************************/

static int fsys_readlink(lua_State *L)
{
  char buffer[FILENAME_MAX];
  int  size;
  
  size = readlink(luaL_checkstring(L,1),buffer,sizeof(buffer));
  if (size == -1)
  {
    int err = errno;
    lua_pushnil(L);
    lua_pushinteger(L,err);
  }
  else
  {
    lua_pushlstring(L,buffer,size);
    lua_pushinteger(L,0);
  }
  return 2;
}

/*********************************************************************/  

static int fsys_mknod(lua_State *L)
{
  lua_pushboolean(L,false);
  lua_pushinteger(L,ENOSYS);
  return 2;
}

/*********************************************************************/

static int fsys_mkfifo(lua_State *L)
{
  lua_pushboolean(L,false);
  lua_pushinteger(L,ENOSYS);
  return 2;
}

/********************************************************************/

static int fsys_mkdir(lua_State *L)
{
  if (mkdir(luaL_checkstring(L,1),0777) < 0)
  {
    int err = errno;
    lua_pushboolean(L,false);
    lua_pushinteger(L,err);
  }
  else
  {
    lua_pushboolean(L,true);
    lua_pushinteger(L,0);
  }
  return 2;
}

/***********************************************************************/

static int fsys_utime(lua_State *L)
{
  const char     *path;
  struct utimbuf  when;
  
  path         = luaL_checkstring(L,1);
  when.modtime = luaL_checknumber(L,2);
  when.actime  = luaL_optnumber(L,3,when.modtime);
  
  if (utime(path,&when) < 0)
  {
    int err = errno;
    lua_pushboolean(L,false);
    lua_pushinteger(L,err);
  }
  else
  {
    lua_pushboolean(L,true);
    lua_pushinteger(L,0);
  }
  return 2;
}

/***********************************************************************/

static int fsys_rmdir(lua_State *L)
{
  if (rmdir(luaL_checkstring(L,1)) < 0)
  {
    int err = errno;
    lua_pushboolean(L,false);
    lua_pushinteger(L,err);
  }
  else
  {
    lua_pushboolean(L,true);
    lua_pushinteger(L,0);
  }
  return 2;
}

/************************************************************************/

static void impl_setfield(lua_State *L,const char *name,lua_Number value)
{
  lua_pushstring(L,name);
  lua_pushnumber(L,value);
  lua_settable(L,-3);
}

/*************************************************************************/

static void impl_setbool(lua_State *L,const char *name,int flag)
{
  lua_pushstring(L,name);
  lua_pushboolean(L,flag != 0);
  lua_settable(L,-3);
}

/*************************************************************************/

static void impl_dumpstat(lua_State *L,struct stat *status)
{
  char perms[13];
  
  lua_createtable(L,0,13);
  
  impl_setfield(L,"st_dev"	, status->st_dev);
  impl_setfield(L,"st_ino"	, status->st_ino);
  impl_setfield(L,"st_mode"	, status->st_mode);
  impl_setfield(L,"st_nlink"	, status->st_nlink);
  impl_setfield(L,"st_uid"	, status->st_uid);
  impl_setfield(L,"st_gid"	, status->st_gid);
  impl_setfield(L,"st_rdev"	, status->st_rdev);
  impl_setfield(L,"st_size"	, status->st_size);
  impl_setfield(L,"st_blksize"	, status->st_blksize);
  impl_setfield(L,"st_blocks"	, status->st_blocks);
  impl_setfield(L,"st_atime"	, status->st_atime);
  impl_setfield(L,"st_mtime"	, status->st_mtime);
  impl_setfield(L,"st_ctime"	, status->st_ctime);
  
  lua_pushliteral(L,"type");
  if (S_ISREG(status->st_mode))
    lua_pushliteral(L,"file");
  else if (S_ISDIR(status->st_mode))
    lua_pushliteral(L,"dir");
  else if (S_ISCHR(status->st_mode))
    lua_pushliteral(L,"chardev");
  else if (S_ISBLK(status->st_mode))
    lua_pushliteral(L,"blockdev");
  else if (S_ISFIFO(status->st_mode))
    lua_pushliteral(L,"pipe");
  else if (S_ISLNK(status->st_mode))
    lua_pushliteral(L,"symlink");
  else if (S_ISSOCK(status->st_mode))
    lua_pushliteral(L,"socket");
  else
    lua_pushliteral(L,"?");
  lua_settable(L,-3);
  
  impl_setbool(L,"setuid",status->st_mode & S_ISUID);
  impl_setbool(L,"setgid",status->st_mode & S_ISGID);
  impl_setbool(L,"sticky",status->st_mode & S_ISVTX);
  
  lua_pushliteral(L,"perms");
  perms[ 0] = (status->st_mode & S_ISUID) ? 'u' : '-';
  perms[ 1] = (status->st_mode & S_ISGID) ? 'g' : '-';
  perms[ 2] = (status->st_mode & S_ISVTX) ? 's' : '-';
  perms[ 3] = (status->st_mode & S_IRUSR) ? 'r' : '-';
  perms[ 4] = (status->st_mode & S_IWUSR) ? 'w' : '-';
  perms[ 5] = (status->st_mode & S_IXUSR) ? 'x' : '-';
  perms[ 6] = (status->st_mode & S_IRGRP) ? 'r' : '-';
  perms[ 7] = (status->st_mode & S_IWGRP) ? 'w' : '-';
  perms[ 8] = (status->st_mode & S_IXGRP) ? 'x' : '-';
  perms[ 9] = (status->st_mode & S_IROTH) ? 'r' : '-';
  perms[10] = (status->st_mode & S_IWOTH) ? 'w' : '-';
  perms[11] = (status->st_mode & S_IXOTH) ? 'x' : '-';
  perms[12] = '\0';
  lua_pushstring(L,perms);
  lua_settable(L,-3);
}

/***********************************************************************/

static int fsys_stat(lua_State *L)
{
  struct stat status;
  int         err;
  
  if (lua_isnumber(L,1))
  {
    if (fstat(lua_tointeger(L,1),&status) < 0)
    {
      err = errno;
      lua_pushnil(L);
      lua_pushinteger(L,err);
      return 2;
    }
  }
  else if (lua_isstring(L,1))
  {
    if (stat(lua_tostring(L,1),&status) < 0)
    {
      err = errno;
      lua_pushnil(L);
      lua_pushinteger(L,err);
      return 2;
    }
  }
  else if (lua_isuserdata(L,1))
  {
    FILE **pfp = luaL_checkudata(L,1,LUA_FILEHANDLE);
    if (fstat(fileno(*pfp),&status) < 0)
    {
      err = errno;
      lua_pushnil(L);
      lua_pushinteger(L,err);
      return 2;
    }
  }
  else
    return luaL_error(L,"invalid file handle");

  impl_dumpstat(L,&status);  
  lua_pushinteger(L,0);
  return 2;
}

/**********************************************************************/

static int fsys_lstat(lua_State *L)
{
  const char  *fname;
  struct stat  status;
  int          rc;
  
  fname = luaL_checkstring(L,1);
  rc    = lstat(fname,&status);
  if (rc == -1)
  {
    int e = errno;
    
    lua_pushnil(L);
    lua_pushinteger(L,e);
    return 2;
  }
  
  impl_dumpstat(L,&status);
  lua_pushinteger(L,0);
  return 2;
}

/*************************************************************************/

static int fsys_chmod(lua_State *L)
{
  const char        *fname;
  const char        *value;
  mode_t             mode;
  mode_t             bit;
  
  fname = luaL_checkstring(L,1);
  value = luaL_checkstring(L,2);
  bit   = 0400;
  mode  = 0;
  
  for ( ; *value ; bit >>= 1 , value++)
    if (*value != '-')
      mode |= bit;

  if (chmod(fname,mode) < 0)
  {
    int err = errno;
    lua_pushboolean(L,false);
    lua_pushinteger(L,err);
  }
  else
  {
    lua_pushboolean(L,true);
    lua_pushinteger(L,0);
  }
  return 2;
}

/***********************************************************************/

static int fsys_umask(lua_State *L)
{
  char        perms[9];
  const char *value;
  mode_t      mask;
  mode_t      bit;
  
  value = luaL_checkstring(L,1);
  bit   = 0400;
  mask  = 0;
  
  for ( ; *value ; bit >>= 1 , value++)
    if (*value != '-')
      mask |= bit;

  mask     = umask(mask);
  perms[0] = (mask & S_IRUSR) ? 'r' : '-';
  perms[1] = (mask & S_IWUSR) ? 'w' : '-';
  perms[2] = (mask & S_IXUSR) ? 'x' : '-';
  perms[3] = (mask & S_IRGRP) ? 'r' : '-';
  perms[4] = (mask & S_IWGRP) ? 'w' : '-';
  perms[5] = (mask & S_IXGRP) ? 'x' : '-';
  perms[6] = (mask & S_IROTH) ? 'r' : '-';
  perms[7] = (mask & S_IWOTH) ? 'w' : '-';
  perms[8] = (mask & S_IXOTH) ? 'x' : '-';

  lua_pushlstring(L,perms,9);
  return 1;
}

/**********************************************************************/

static int fsys_access(lua_State *L)
{
  const char *fname  = luaL_checkstring(L,1);
  const char *tmode  = luaL_checkstring(L,2);
  int         mode   = 0;
  
  for (int i = 0 ; tmode[i] != '\0'; i++)
  {
    switch(toupper(tmode[i]))
    {
      case 'R': mode |= R_OK; break;
      case 'W': mode |= W_OK; break;
      case 'X': mode |= X_OK; break;
      case 'F': mode |= F_OK; break;
      default: break;
    }
  }
  
  if (access(fname,mode) < 0)
  {
    int err = errno;
    lua_pushboolean(L,false);
    lua_pushinteger(L,err);
  }
  else
  {
    lua_pushboolean(L,true);
    lua_pushinteger(L,0);
  }
  return 2;
}

/************************************************************************/

static int fsys_getcwd(lua_State *L)
{
  char cwd[FILENAME_MAX];
  
  if (getcwd(cwd,sizeof(cwd)) == NULL)
  {
    int e = errno;    
    lua_pushnil(L);
    lua_pushinteger(L,e);
  }
  else
  {
    lua_pushstring(L,cwd);
    lua_pushinteger(L,0);
  }
  
  return 2;
}

/************************************************************************/

static int fsys_opendir(lua_State *L)
{
  const char *dname;
  DIR        *dir;
  
  dname = luaL_optstring(L,1,".");
  
  dir = opendir(dname);
  if (dir == NULL)
  {
    int e = errno;
    lua_pushnil(L);
    lua_pushinteger(L,e);
    return 2;
  }
  
  lua_pushlightuserdata(L,dir);
  return 1;
}

static int fsys_rewinddir(lua_State *L)
{
  DIR *dir;
  
  if (!lua_islightuserdata(L,1))
  {
    lua_pop(L,1);
    return luaL_error(L,"incorrect type");
  }
  
  dir = lua_touserdata(L,1);
  lua_pop(L,1);
  
  rewinddir(dir);
  return 0;
}

static int fsys_readdir(lua_State *L)
{
  DIR           *dir;
  struct dirent *entry;
  
  if (!lua_islightuserdata(L,1))
  {
    lua_pop(L,1);
    return luaL_error(L,"incorrect type");
  }
  
  dir = lua_touserdata(L,1);
  lua_pop(L,1);
  
  while(true)
  {
    errno = 0;
    entry = readdir(dir);
    
    if (entry == NULL)
    {
      int e = errno;
      lua_pushnil(L);
      if (e == 0)
      {
        lua_pushboolean(L,1);
        return 2;
      }
      else
      {
        lua_pushboolean(L,0);
        lua_pushinteger(L,e);
        return 3;
      }
    }
    
    if (
            (strcmp(entry->d_name,".")  != 0) 
         && (strcmp(entry->d_name,"..") != 0)
       )
      break;
  }
  
  lua_pushstring(L,entry->d_name);
  return 1;
}

static int fsys_closedir(lua_State *L)
{
  DIR *dir;

  if (!lua_islightuserdata(L,1))
  {
    lua_pop(L,1);
    return luaL_error(L,"incorrect type");
  }
  
  dir = lua_touserdata(L,1);
  lua_pop(L,1);
  closedir(dir);
  return 0;
}

static int impl_dir(lua_State *L)
{
  int rc;
  
  rc = fsys_readdir(L);
  if (rc == 1)		/* normal return */
    return 1;
  else if (rc == 2)	/* no more entries, so close out dir */
  {
    lua_pushvalue(L,1);
    fsys_closedir(L);
    lua_pushnil(L);
    return 1;
  }
  else			/* error */
    return 3;
}
    
static int fsys_dir(lua_State *L)
{
  int rc;
  
  rc = fsys_opendir(L);
  if (rc == 2) return rc;
  lua_pushcfunction(L,impl_dir);
  lua_pushvalue(L,-2);
  lua_pushnil(L);
  return 3;
}

static int fsys__safename(lua_State *L)
{
  const char *old;
  size_t      len;
 
  old = luaL_checklstring(L,1,&len);
  char buffer[len + 1];
  
  for (size_t i = 0 ; i < len ; i++)
  {
    int c;
    
    c = old[i];
    if (c == '.')   { buffer[i] = c; continue; }
    if (c == '_')   { buffer[i] = c; continue; }
    if (isdigit(c)) { buffer[i] = c; continue; }
    if (isalpha(c)) { buffer[i] = c; continue; }
    buffer[i] = '_';
  }
  
  lua_pushlstring(L,buffer,len);
  return 1;
}
 
/**********************************************************************/

static int fsys_basename(lua_State *L)
{
  char        name[FILENAME_MAX];
  const char *path;
  size_t      size;
  
  path = luaL_checklstring(L,1,&size);
  if (size >= FILENAME_MAX - 1)
  {
    lua_pushnil(L);
    lua_pushinteger(L,ENAMETOOLONG);
    return 2;
  }
  
  /*---------------------------------------------------------------------
  ; POSIX states that basename() modifies its arguemnts, so pass in a copy
  ;-----------------------------------------------------------------------*/
  
  memcpy(name,path,size + 1);
  lua_pushstring(L,basename(name));
  lua_pushinteger(L,0);
  return 2;
}

/**********************************************************************/

static int fsys_dirname(lua_State *L)
{
  char        name[FILENAME_MAX];
  const char *path;
  size_t      size;
  
  path = luaL_checklstring(L,1,&size);
  if (size >= FILENAME_MAX - 1)
  {
    lua_pushnil(L);
    lua_pushinteger(L,ENAMETOOLONG);
    return 2;
  }
  
  /*----------------------------------------------------------------------
  ; POSIX states that dirname() modifies its arguemnt, so pass in a copy
  ;-----------------------------------------------------------------------*/
  
  memcpy(name,path,size + 1);
  lua_pushstring(L,dirname(name));
  lua_pushinteger(L,0);
  return 2;
}

/***********************************************************************/

static int fsys_open(lua_State *L)
{ 
  fd__t      *fd;
  const char *fname;
  const char *flags;
  int         oflags;
  
  fname = luaL_checkstring(L,1);
  flags = luaL_checkstring(L,2);
  oflags = 0;
  
  if (flags[0] == 'r')
    oflags = (flags[1] == '+')
    	? O_RDWR
    	: O_RDONLY;
  else if (flags[0] == 'w')
    oflags = (flags[1] == '+')
    	? O_CREAT | O_RDWR
    	: O_CREAT | O_WRONLY | O_TRUNC;
  else if (flags[0] == 'a')
    oflags = (flags[1] == '+')
    	? O_RDWR   | O_APPEND
    	: O_WRONLY | O_APPEND;
  else
    luaL_error(L,"illegal flag: %s",flags);
  
  fd = lua_newuserdata(L,sizeof(fd__t));
  luaL_getmetatable(L,FSYS_FD);
  lua_setmetatable(L,-2);
  
  fd->fh = open(fname,oflags,0666);
  if (fd->fh == -1)
  {
    int err = errno;
    lua_pushnil(L);
    lua_pushinteger(L,err);
  }
  else
    lua_pushinteger(L,0);
  
  return 2;
}

/***********************************************************************/

static int fsys_openfd(lua_State *L)
{
  int    fh;
  fd__t *fd;
  
  fh = luaL_checkint(L,1);
  fd = lua_newuserdata(L,sizeof(fd__t));
  luaL_getmetatable(L,FSYS_FD);
  lua_setmetatable(L,-2);
  fd->fh = fh;
  return 1;
}

/***********************************************************************/

static int fsys_pipe(lua_State *L)
{
  fd__t *read;
  fd__t *write;
  int    fh[2];
  
  if (pipe(fh) < 0)
  {
    int err = errno;
    lua_pushnil(L);
    lua_pushinteger(L,err);
    return 2;
  }
  
  lua_createtable(L,0,2);
  
  read = lua_newuserdata(L,sizeof(fd__t));
  read->fh = fh[0];
  luaL_getmetatable(L,FSYS_FD);
  lua_setmetatable(L,-2);
  lua_setfield(L,-2,"read");
  
  write = lua_newuserdata(L,sizeof(fd__t));
  write->fh = fh[1];
  luaL_getmetatable(L,FSYS_FD);
  lua_setmetatable(L,-2);
  lua_setfield(L,-2,"write");
  
  lua_pushinteger(L,0);
  return 2;
}

/***********************************************************************/

static void *ifsys_checkudata(lua_State *L,int idx,const char *tname)
{
  /*------------------------------------------------------------------------
  ; XXX---gross hack to check the userdata type without erroring out.  Uses
  ; internals of Lua implementation.
  ;-----------------------------------------------------------------------*/
  
  void *p = lua_touserdata(L,idx);
  
  if (p != NULL)
  {
    if (lua_getmetatable(L,idx))
    {
      lua_getfield(L,LUA_REGISTRYINDEX,tname);
      if (lua_rawequal(L,-1,-2))
      {
        lua_pop(L,2);
        return p;
      }
    }
  }
  return NULL;
}

/********************************************************************/

static int fsys_dup(lua_State *L)
{
  int orig;
  int copy;
  
  if (lua_isnumber(L,1))
    orig = lua_tointeger(L,1);
  else
  {
    FILE **pfp = ifsys_checkudata(L,1,LUA_FILEHANDLE);
    if (pfp)
      orig = fileno(*pfp);
    else
    {
      lua_getfield(L,1,"fd");
      lua_pushvalue(L,1);
      lua_call(L,1,1);
      orig = luaL_checkint(L,-1);
    }
  }
  
  if (lua_isnumber(L,2))
    copy = lua_tointeger(L,2);
  else
  {
    FILE **pfp = ifsys_checkudata(L,1,LUA_FILEHANDLE);
    if (pfp)
      copy = fileno(*pfp);
    else
    {
      lua_getfield(L,2,"fd"); 
      lua_pushvalue(L,1);
      lua_call(L,1,1);
      copy = luaL_checkint(L,-1);
    }
  }
  
  if (dup2(orig,copy) < 0)
  {
    int err = errno;
    lua_pushboolean(L,false);
    lua_pushinteger(L,err);
  }
  else
  {
    lua_pushboolean(L,true);
    lua_pushinteger(L,0);
  }
  
  return 2;
}

/**************************************************************************/  

static int fsys_fdopen(lua_State *L)
{
  fp__t *fp;
  int    fh;
  int    ref;
  
  if (lua_isnumber(L,1))
  {
    fh = lua_tointeger(L,1);
    ref = LUA_NOREF;
  }
  else
  {
    lua_getfield(L,1,"fd");
    lua_pushvalue(L,1);
    lua_call(L,1,1);
    fh  = luaL_checkint(L,-1);
    lua_pushvalue(L,1);
    ref = luaL_ref(L,LUA_REGISTRYINDEX);
  }
  
  fp      = lua_newuserdata(L,sizeof(fp__t));
  fp->fp  = NULL;
  fp->ref = LUA_NOREF;
  
  luaL_getmetatable(L,LUA_FILEHANDLE);
  lua_setmetatable(L,-2);
  
  fp->ref = ref;
  fp->fp  = fdopen(fh,luaL_checkstring(L,2));
  if (fp->fp == NULL)
  {
    int err = errno;
    lua_pushnil(L);
    lua_pushinteger(L,err);
    return 2;
  }
  
  return 1;
}

/************************************************************************/  

static int fsys_close(lua_State *L)
{
  if (lua_isnumber(L,1))
  {
    if (close(lua_tointeger(L,1)) < 0)
    {
      int err = errno;
      lua_pushboolean(L,false);
      lua_pushinteger(L,err);
      return 2;
    }
    else
    {
      lua_pushboolean(L,true);
      lua_pushinteger(L,0);
      return 2;
    }
  }
  else
  {
    lua_getfield(L,1,"close");
    lua_pushvalue(L,1);
    lua_call(L,1,1);
    return 2;
  }
}

/**********************************************************************/
    
static int fiolua___close(lua_State *L)
{
  fp__t *fp;
  
  fp = luaL_checkudata(L,1,LUA_FILEHANDLE);
  fclose(fp->fp);
  fp->fp = NULL;
  luaL_unref(L,LUA_REGISTRYINDEX,fp->ref);
  lua_pushboolean(L,true);
  return 1;
}

/************************************************************************/

static int fiolua___tostring(lua_State *L)
{
  fd__t *fd;
  
  fd = luaL_checkudata(L,1,FSYS_FD);
  lua_pushfstring(L,"FILE:%d",fd->fh);
  return 1;
}

/************************************************************************/

static int fiolua_read(lua_State *L)
{
  char    buffer[LUAL_BUFFERSIZE];
  ssize_t bytes;
  int     fh;
  
  if (lua_isnumber(L,1))
    fh = lua_tointeger(L,1);
  else
  {
    lua_getfield(L,1,"fd");
    lua_pushvalue(L,1);
    lua_call(L,1,1);
    fh = luaL_checkint(L,-1);
  }
  
  bytes = read(fh,buffer,sizeof(buffer));
  if (bytes < 0)
  {
    int err = errno;
    lua_pushnil(L);
    lua_pushinteger(L,err);
  }
  else
  {
    lua_pushlstring(L,buffer,bytes);
    lua_pushinteger(L,0);
  }
  
  return 2;
}

/************************************************************************/

static int fiolua_write(lua_State *L)
{
  int         fh;
  const char *data;
  size_t      size;
  ssize_t     bytes;
  
  if (lua_isnumber(L,1))
    fh = lua_tonumber(L,1);
  else
  {
    lua_getfield(L,1,"fd");
    lua_pushvalue(L,1);
    lua_call(L,1,1);
    fh = luaL_checkint(L,1);
  }
  
  data  = luaL_checklstring(L,2,&size);
  errno = 0;
  bytes = write(fh,data,size);
  lua_pushinteger(L,bytes);
  lua_pushinteger(L,errno);
  return 2;
}

/************************************************************************/

static int fiolua_isfile(lua_State *L)
{
  int fh;
  
  if (lua_isnumber(L,1))
    fh = lua_tonumber(L,1);
  else
  {
    lua_getfield(L,1,"fd");
    lua_pushvalue(L,1);
    lua_call(L,1,1);
    fh = luaL_checkint(L,-1);
  }
  
  if (isatty(fh) < 0)
  {
    int err = errno;
    lua_pushboolean(L,false);
    lua_pushinteger(L,err);
  }
  else
  {
    lua_pushboolean(L,true);
    lua_pushinteger(L,0);
  }
  return 2;
}

/************************************************************************/

static int fiolua_close(lua_State *L)
{
  fd__t *fd;
  
  fd = luaL_checkudata(L,1,FSYS_FD);

  if (fd->fh >= 0)
  {
    if (close(fd->fh) < 0)
    {
      int err = errno;
      lua_pushboolean(L,false);
      lua_pushinteger(L,err);
      return 2;
    }
  }
  
  lua_pushboolean(L,true);
  lua_pushinteger(L,0);
  return 2;
}

/************************************************************************/

static int fiolua_fd(lua_State *L)
{
  fd__t *fd;
  
  fd = luaL_checkudata(L,1,FSYS_FD);
  lua_pushinteger(L,fd->fh);
  return 1;
}

/************************************************************************/

static const struct luaL_reg reg_fsys[] = 
{
  { "symlink"	, fsys_symlink 	} ,
  { "link"	, fsys_link	} ,
  { "readlink"	, fsys_readlink	} ,
  { "mknod"	, fsys_mknod	} ,
  { "mkfifo"	, fsys_mkfifo	} ,
  { "mkdir"	, fsys_mkdir	} ,
  { "rmdir"	, fsys_rmdir	} ,
  { "utime"	, fsys_utime	} ,
  { "stat"	, fsys_stat	} ,
  { "lstat"	, fsys_lstat	} ,
  { "umask"	, fsys_umask	} ,
  { "chmod"	, fsys_chmod	} ,
  { "access"	, fsys_access	} ,
  { "opendir"	, fsys_opendir	} ,
  { "rewinddir"	, fsys_rewinddir} ,
  { "readdir"	, fsys_readdir	} ,
  { "closedir"	, fsys_closedir	} ,
  { "chroot"	, fsys_chroot	} ,
  { "chdir"     , fsys_chdir	} ,
  { "getcwd"	, fsys_getcwd   } ,
  { "dir"	, fsys_dir	} ,
  { "_safename"	, fsys__safename} ,
  { "basename"	, fsys_basename	} ,
  { "dirname"	, fsys_dirname	} ,
  { "open"	, fsys_open	} ,
  { "close"	, fsys_close	} ,
  { "openfd"	, fsys_openfd	} ,
  { "pipe"	, fsys_pipe	} ,
  { "dup"	, fsys_dup	} ,
  { "fdopen"	, fsys_fdopen	} ,
  { NULL	, NULL		}
};

static const luaL_reg mfio_regmeta[] =
{
  { "__tostring", fiolua___tostring	} ,
  { "__gc"	, fiolua_close		} ,
  { "read"	, fiolua_read		} ,
  { "write"	, fiolua_write		} ,
  { "isfile"	, fiolua_isfile		} ,
  { "close"	, fiolua_close		} ,
  { "fd"	, fiolua_fd		} ,
  { NULL	, NULL			}
};

int luaopen_org_conman_fsys(lua_State *L)
{
  luaL_newmetatable(L,FSYS_FD);
  luaL_register(L,NULL,mfio_regmeta);
  lua_pushvalue(L,-1);
  lua_setfield(L,-2,"__index");
  
  luaL_register(L,"org.conman.fsys",reg_fsys);
  
  /*----------------------------------------------------------------------
  ; there's no API to manipulate LUA_FILEHANDLEs safely, so we kind of have
  ; to hack our way in there.  We need to define a "__close()" function
  ; associated with the function used to create the LUA_FILEHANDLE.  This is
  ; a gross hack and deals with Lua internals that aren't really well
  ; documented.  We may have to reinvestigate this for Lua 5.2.
  ;------------------------------------------------------------------------*/
  
  lua_getfield(L,-1,"fdopen");
  lua_createtable(L,0,1);
  lua_pushcfunction(L,fiolua___close);
  lua_setfield(L,-2,"__close");
  lua_setfenv(L,-2);
  lua_pop(L,1);
  
  lua_pushinteger(L,STDIN_FILENO);
  lua_setfield(L,-2,"_STDIN");
  lua_pushinteger(L,STDOUT_FILENO);
  lua_setfield(L,-2,"_STDOUT");
  lua_pushinteger(L,STDERR_FILENO);
  lua_setfield(L,-2,"_STDERR");
  
  lua_pushliteral(L,"Copyright 2010 by Sean Conner.  All Rights Reserved.");
  lua_setfield(L,-2,"_COPYRIGHT");

  lua_pushliteral(L,"GNU-GPL 3");
  lua_setfield(L,-2,"_LICENSE");
  
  lua_pushliteral(L,"Useful file manipulation functions available under Unix.");
  lua_setfield(L,-2,"_DESCRIPTION");
  
  lua_pushliteral(L,"0.7.2");
  lua_setfield(L,-2,"_VERSION");

  return 1;
}

/*******************************************************************/

