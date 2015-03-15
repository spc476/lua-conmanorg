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
#include <assert.h>

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
#include <fnmatch.h>
#include <wordexp.h>

#if !defined(LUA_VERSION_NUM) || LUA_VERSION_NUM < 501
#  error You need to compile against Lua 5.1 or higher
#endif

#define TYPE_DIR	"org.conman.fsys:dir"
#define TYPE_EXPAND	"org.conman.fsys:expand"

/*************************************************************************/

static int fsys_chroot(lua_State *L)
{
  if (chroot(luaL_checkstring(L,1)) < 0)
  {
    lua_pushboolean(L,false);
    lua_pushinteger(L,errno);
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
  errno = 0;
  chdir(luaL_checkstring(L,1));
  lua_pushboolean(L,errno == 0);
  lua_pushinteger(L,errno);
  return 2;
}

/*************************************************************************/

static int fsys_symlink(lua_State *L)
{
  const char *old = luaL_checkstring(L,1);
  const char *new = luaL_checkstring(L,2);
  
  errno = 0;
  symlink(old,new);
  lua_pushboolean(L,errno == 0);
  lua_pushinteger(L,errno);
  return 2;
}

/*************************************************************************/

static int fsys_link(lua_State *L)
{
  const char *old = luaL_checkstring(L,1);
  const char *new = luaL_checkstring(L,2);

  errno = 0;
  link(old,new);
  lua_pushboolean(L,errno == 0);
  lua_pushinteger(L,errno);
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
    lua_pushnil(L);
    lua_pushinteger(L,errno);
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
  const char *fname = luaL_checkstring(L,1);
  const char *value = luaL_checkstring(L,2);
  mode_t      bit   = 0400;
  mode_t      mode  = 0000;
  
  for ( ; *value ; bit >>= 1 , value++)
    if (*value != '-')
      mode |= bit;
  
  errno = 0;
  mkfifo(fname,mode);
  lua_pushboolean(L,errno == 0);
  lua_pushinteger(L,errno);
  return 2;
}

/********************************************************************/

static int fsys_mkdir(lua_State *L)
{
  errno = 0;
  mkdir(luaL_checkstring(L,1),0777);
  lua_pushboolean(L,errno == 0);
  lua_pushinteger(L,errno);
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
  errno        = 0;
  
  utime(path,&when);
  lua_pushboolean(L,errno == 0);
  lua_pushinteger(L,errno);
  return 2;
}

/***********************************************************************/

static int fsys_rmdir(lua_State *L)
{
  errno = 0;
  rmdir(luaL_checkstring(L,1));
  lua_pushboolean(L,errno == 0);
  lua_pushinteger(L,errno);
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
  lua_setfield(L,-2,"type");
  
  impl_setbool(L,"setuid",status->st_mode & S_ISUID);
  impl_setbool(L,"setgid",status->st_mode & S_ISGID);
  impl_setbool(L,"sticky",status->st_mode & S_ISVTX);
  
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
  lua_setfield(L,-2,"perms");
}

/***********************************************************************/

static int fsys_stat(lua_State *L)
{
  struct stat status;
  
  if (lua_isstring(L,1))
  {
    if (stat(lua_tostring(L,1),&status) < 0)
    {
      lua_pushnil(L);
      lua_pushinteger(L,errno);
      return 2;
    }
  }
  else if (lua_isuserdata(L,1))
  {
    FILE **pfp = luaL_checkudata(L,1,LUA_FILEHANDLE);
    if (fstat(fileno(*pfp),&status) < 0)
    {
      lua_pushnil(L);
      lua_pushinteger(L,errno);
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
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    return 2;
  }
  
  impl_dumpstat(L,&status);
  lua_pushinteger(L,0);
  return 2;
}

/*************************************************************************/

static int fsys_chmod(lua_State *L)
{
  const char        *fname = luaL_checkstring(L,1);
  const char        *value = luaL_checkstring(L,2);
  mode_t             bit   = 0400;
  mode_t             mode  = 0;
  
  for ( ; *value ; bit >>= 1 , value++)
    if (*value != '-')
      mode |= bit;

  errno = 0;
  chmod(fname,mode);
  lua_pushboolean(L,errno == 0);
  lua_pushinteger(L,errno);
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
  
  errno = 0;
  access(fname,mode);
  lua_pushboolean(L,errno == 0);
  lua_pushinteger(L,errno);
  return 2;
}

/************************************************************************/

static int fsys_getcwd(lua_State *L)
{
  char cwd[FILENAME_MAX];
  
  errno = 0;
  lua_pushstring(L,getcwd(cwd,sizeof(cwd)));
  lua_pushinteger(L,errno);
  return 2;
}

/************************************************************************/

static int dir_meta___tostring(lua_State *const L)
{
  lua_pushfstring(L,"directory (%p)",lua_touserdata(L,1));
  return 1;
}

/*************************************************************************/

static int dir_meta___gc(lua_State *const L)
{
  closedir(*(DIR **)luaL_checkudata(L,1,TYPE_DIR));
  return 0;
}

/*************************************************************************/

static int dir_meta_rewind(lua_State *const L)
{
  rewinddir(*(DIR **)luaL_checkudata(L,1,TYPE_DIR));
  return 0;
}

/*************************************************************************/

static int dir_meta_next(lua_State *const L)
{
  struct dirent  *entry;
  DIR           **dir;
  
  if (lua_isnil(L,1))
  {
    lua_pushnil(L);
    return 1;
  }
  
  dir = luaL_checkudata(L,1,TYPE_DIR);
  while(true)
  {
    entry = readdir(*dir);
    if (entry == NULL)
    {
      lua_pushnil(L);
      return 1;
    }
    else
    {
      if (
              (strcmp(entry->d_name,".")  != 0)
           && (strcmp(entry->d_name,"..") != 0)
         )
      {
        lua_pushstring(L,entry->d_name);
        return 1;
      }
    }
  }  
}

/*************************************************************************/

static int fsys_opendir(lua_State *L)
{
  const char   *dname;
  DIR          *dir;
  DIR        **pdir;
  
  dname = luaL_optstring(L,1,".");
  
  dir = opendir(dname);
  if (dir == NULL)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    return 2;
  }
  
  pdir  = lua_newuserdata(L,sizeof(DIR *));
  *pdir = dir;
  luaL_getmetatable(L,TYPE_DIR);
  lua_setmetatable(L,-2);
  lua_pushinteger(L,0);
  return 2;
}

/*************************************************************************/

static int fsys_dir(lua_State *L)
{
  fsys_opendir(L);
  lua_pushcfunction(L,dir_meta_next);
  lua_insert(L,-3);
  return 3;
}

/*************************************************************************/

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

static int fsyslib_close(lua_State *L)
{
#if LUA_VERSION_NUM == 501
  FILE **pfp = luaL_checkudata(L,1,LUA_FILEHANDLE);
  fclose(*pfp);
  *pfp = NULL;
  lua_pushboolean(L,1);
  return 1;
#else
  luaL_Stream *pfp = luaL_checkudata(L,1,LUA_FILEHANDLE);
  fclose(pfp->f);
  pfp->closef = NULL;
  lua_pushboolean(L,1);
  return 1;
#endif
}

static int fsys_openfd(lua_State *L)
{
#if LUA_VERSION_NUM == 501
  FILE **pfp;
  
  lua_settop(L,2);
  
  pfp  = lua_newuserdata(L,sizeof(FILE *));
  *pfp = NULL;	/* see comments in fsys_pipe() */
  luaL_getmetatable(L,LUA_FILEHANDLE);
  lua_setmetatable(L,-2);
  lua_createtable(L,0,0);
  lua_pushcfunction(L,fsyslib_close);
  lua_setfield(L,-2,"__close");
  lua_setfenv(L,-2);
  
  *pfp = fdopen(
  		luaL_checkinteger(L,1),
  		luaL_checkstring(L,2)
  	);
  
  if (*pfp == NULL)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    return 2;
  }
  
  lua_pushinteger(L,0);
  return 2;
#else
  luaL_Stream *pfp;
  
  lua_settop(L,2);
  
  pfp         = lua_newuserdata(L,sizeof(luaL_Stream));
  pfp->f      = NULL;
  pfp->closef = NULL;
  
  luaL_getmetatable(L,LUA_FILEHANDLE);
  lua_setmetatable(L,-2);
  
  pfp->f = fdopen(luaL_checkinteger(L,1),luaL_checkstring(L,2));
  if (pfp->f == NULL)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    return 2;
  }
  
  pfp->closef = fsyslib_close;
  lua_pushinteger(L,0);
  return 2;
#endif
}

/***********************************************************************/

static int fsys_pipe(lua_State *L)
{
#if LUA_VERSION_NUM == 501
  FILE **pfpread;
  FILE **pfpwrite;
  FILE  *fpr;
  FILE  *fpw;
  int    fh[2];
  char  *rm;
  char  *wm;

  /*------------------------------------------------------------------------
  ; This is done first because there may not be a paramter, and if we create
  ; the return data, we might get behavior we weren't expecting.
  ;-------------------------------------------------------------------------*/
  
  if (lua_isboolean(L,1))
  {
    rm = "rb";
    wm = "wb";
  }
  else
  {
    rm = "r";
    wm = "w";
  }
  
  /*---------------------------------------------------------------------
  ; Create our return table.  We initialize the FILE pointers to NULL to
  ; signify a "closed" file.  We do this all up front first before anything
  ; else in case things go wrong to the calls in Lua.  By the time we get
  ; through this code, we should be okay to continue without worry of losing
  ; an open file.
  ;-------------------------------------------------------------------------*/
  
  lua_createtable(L,0,2);
  
  pfpread  = lua_newuserdata(L,sizeof(FILE *));
  *pfpread = NULL;
  luaL_getmetatable(L,LUA_FILEHANDLE);
  lua_setmetatable(L,-2);
  lua_createtable(L,0,0);
  lua_pushcfunction(L,fsyslib_close);
  lua_setfield(L,-2,"__close");
  lua_setfenv(L,-2);
  lua_setfield(L,-2,"read");  
  
  pfpwrite  = lua_newuserdata(L,sizeof(FILE *));
  *pfpwrite = NULL;
  luaL_getmetatable(L,LUA_FILEHANDLE);
  lua_setmetatable(L,-2);
  lua_createtable(L,0,0);
  lua_pushcfunction(L,fsyslib_close);
  lua_setfield(L,-2,"__close");
  lua_setfenv(L,-2);
  lua_setfield(L,-2,"write");
  
  /*---------------------------------------------------------------------
  ; now we can create our pipe and reopen them as FILE*s.  If these fail,
  ; the table and userdata structures we created will be reclaimed properly
  ; in time (since to Lua, they're closed files).
  ;-----------------------------------------------------------------------*/
  
  if (pipe(fh) < 0)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    return 2;
  }
  
  fpr = fdopen(fh[0],rm);
  if (fpr == NULL)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    close(fh[1]);
    close(fh[0]);
    return 2;
  }
  
  fpw = fdopen(fh[1],wm);
  if (fpw == NULL)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    close(fh[1]);
    fclose(fpr);
    return 2;
  }
  
  /*-------------------------------------------------------------------
  ; everything has gone as planned.  Set the open files and return it
  ; back to our Lua script.
  ;-------------------------------------------------------------------*/
  
  *pfpread  = fpr;
  *pfpwrite = fpw;
  
  lua_pushinteger(L,0);
  return 2;
#else
  luaL_Stream *readp;
  luaL_Stream *writep;
  FILE        *fpr;
  FILE        *fpw;
  int          fh[2];
  char        *rm;
  char        *wm;
  
  if (lua_isboolean(L,1))
  {
    rm = "rb";
    wm = "wb";
  }
  else
  {
    rm = "r";
    wm = "w";
  }
  
  lua_createtable(L,0,2);
  
  readp         = lua_newuserdata(L,sizeof(luaL_Stream));
  readp->f      = NULL;
  readp->closef = NULL;
  luaL_getmetatable(L,LUA_FILEHANDLE);
  lua_setmetatable(L,-2);
  lua_setfield(L,-2,"read");
  
  writep         = lua_newuserdata(L,sizeof(luaL_Stream));
  writep->f      = NULL;
  writep->closef = NULL;
  luaL_getmetatable(L,LUA_FILEHANDLE);
  lua_setmetatable(L,-2);
  lua_setfield(L,-2,"write");
  
  if (pipe(fh) < 0)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    return 2;
  }
  
  fpr = fdopen(fh[0],rm);
  if (fpr == NULL)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    close(fh[1]);
    close(fh[0]);
    return 2;
  }
  
  fpw = fdopen(fh[1],wm);
  if (fpw == NULL)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    close(fh[1]);
    fclose(fpr);
    return 2;
  }
  
  readp->f  = fpr;
  writep->f = fpw;
  
  lua_pushinteger(L,0);
  return 2;
#endif
}

/***********************************************************************/

static void *checkutype(lua_State *const L,int idx,const char *tname)
{
  void *p = lua_touserdata(L,idx);
  
  assert(p != NULL);
  
  if (lua_getmetatable(L,idx))
  {
    lua_getfield(L,LUA_REGISTRYINDEX,tname);
    if (lua_rawequal(L,-1,-2))
    {
      lua_pop(L,2);
      return p;
    }
  }
  return NULL;
}
  
/***********************************************************************/

static int getfh(lua_State *const L,int idx)
{
  void *p;
  
  p = checkutype(L,idx,LUA_FILEHANDLE);
  if (p != NULL)
    return fileno(*(FILE **)p);
  
  p = checkutype(L,idx,"org.conman.net:sock");
  if (p != NULL)
    return *(int *)p;
  
  return luaL_error(L,"not a file nor a socket");
}

/***********************************************************************/

static int fsys_dup(lua_State *L)
{
  int orig;
  int copy;
  
  if (lua_isuserdata(L,1))
    orig = getfh(L,1);
  else
    orig = luaL_checkinteger(L,1);
  
  if (lua_isuserdata(L,2))
    copy = getfh(L,2);
  else
    copy = luaL_checkinteger(L,2);
  
  errno = 0;
  dup2(orig,copy);
  lua_pushboolean(L,errno == 0);
  lua_pushinteger(L,errno);
  
  return 2;
}

/**************************************************************************/  

static int fsys_isfile(lua_State *L)
{
  lua_pushboolean(L,!isatty(fileno(*(FILE **)luaL_checkudata(L,1,LUA_FILEHANDLE))));
  return 1;
}

/************************************************************************/

static int fsys_fnmatch(lua_State *L)
{
  const char *pattern  = luaL_checkstring(L,1);
  const char *filename = luaL_checkstring(L,2);
  const char *tflags   = luaL_optstring(L,3,"");
  int         flags    = 0;
  
  for ( ; *tflags ; tflags++)
  {
    switch(*tflags)
    {
      case '\\': flags |= FNM_NOESCAPE; break;
      case '/' : flags |= FNM_PATHNAME; break;
      case '.' : flags |= FNM_PERIOD;   break;
    }
  }
  
  lua_pushboolean(L,fnmatch(pattern,filename,flags) == 0);
  return 1;
}

/************************************************************************/

int fsys_expand(lua_State *L)
{
  const char *pattern = luaL_checkstring(L,1);
  int         undef   = lua_toboolean(L,2) 
                      ? WRDE_NOCMD | WRDE_UNDEF 
                      : WRDE_NOCMD
                      ;
  wordexp_t exp;
  int       rc;
  
  rc = wordexp(pattern,&exp,undef);
  
  if (rc != 0)
  {
    lua_pushnil(L);
    switch(rc)
    {
      case WRDE_BADCHAR: rc = EILSEQ; break;
      case WRDE_BADVAL:  rc = ENOENT; break;
      case WRDE_NOSPACE: rc = ENOMEM; break;
      case WRDE_SYNTAX:  rc = EINVAL; break;
      default:           rc = EDOM;   break;
    }
    
    lua_pushinteger(L,rc);
    return 2;
  }
  
  lua_createtable(L,exp.we_wordc,0);
  for (size_t i = 0 ; i < exp.we_wordc ; i++)
  {
    lua_pushinteger(L,i+1);
    lua_pushstring(L,exp.we_wordv[i]);
    lua_settable(L,-3);
  }
  
  wordfree(&exp);
  lua_pushinteger(L,0);
  return 2;
}

/************************************************************************/

struct myexpand
{
  wordexp_t exp;
  size_t    idx;
};

static int expand_meta___gc(lua_State *L)
{
  struct myexpand *data = lua_touserdata(L,1);
  wordfree(&data->exp);
  return 0;
}

/************************************************************************/

static int expand_meta_next(lua_State *L)
{
  struct myexpand *data = lua_touserdata(L,1);
  
  if (data->idx < data->exp.we_wordc)
    lua_pushstring(L,data->exp.we_wordv[data->idx++]);
  else
    lua_pushnil(L);

  return 1;
}

/************************************************************************/

static int fsys_gexpand(lua_State *L)
{
  const char *pattern = luaL_checkstring(L,1);
  int         undef   = lua_toboolean(L,2)
                      ? WRDE_NOCMD | WRDE_UNDEF
                      : WRDE_NOCMD
                      ;
  struct myexpand *data;
  
  lua_pushcfunction(L,expand_meta_next);
  
  data = lua_newuserdata(L,sizeof(struct myexpand));  
  luaL_getmetatable(L,TYPE_EXPAND);
  lua_setmetatable(L,-2);
  
  if (wordexp(pattern,&data->exp,undef) < 0)
  {
    /*-------------------------------------------------------------
    ; we can do this since data has a __gc method attached to it.  
    ;-------------------------------------------------------------*/
    
    lua_pop(L,1);
    lua_pushnil(L);
  }
  else
    data->idx = 0;

  return 2;
}

/************************************************************************/

static int fsys_fileno(lua_State *L)
{
  lua_pushinteger(
    L,
    fileno(*(FILE **)luaL_checkudata(L,1,LUA_FILEHANDLE))
  );
  return 1;
}

/************************************************************************
*
* Usage:	value,err = fsys.pathconf(filespec,var)
*
* Desc:		Return data about a specific value for an open file 
*		descriptor.
*
* Input:	filespec (string userdata(FILE)) name of a file
*			| or an open file
*		var (string/enum)
*			* link     maximum number of links to the file
*			* canon    maximum size of a formatted input line
*			* input    maximum size of an input line
*			* name     size of filename in current directory
*			* path     size of relative path
*			* pipe     size of pipe buffer
*			* chown    <> 0 if chown cannot be used on this file
*			* trunc    <> 0 if filenames longer than 'name' 
*					| generates an error
*			* vdisable <> 0 if special character processing can 
*					| be disabled
*
* Return:	value (number) value for specific value (only if err is 0)
*		err (integer) system error, 0 if successful
*
************************************************************************/

static const char *const m_pathconfopts[] =
{
  "link",
  "canon",
  "input",
  "name",
  "path",
  "pipe",
  "chown",
  "trunc",
  "vdisable",
  NULL
};

static const int m_pathconfmap[] =
{
  _PC_LINK_MAX,
  _PC_MAX_CANON,
  _PC_MAX_INPUT,
  _PC_NAME_MAX,
  _PC_PATH_MAX,
  _PC_PIPE_BUF,
  _PC_CHOWN_RESTRICTED,
  _PC_NO_TRUNC,
  _PC_VDISABLE
};

static int fsys_pathconf(lua_State *L)
{
  long res;
  
  if (lua_type(L,1) == LUA_TUSERDATA)
  {
    int fh   = fileno(*(FILE **)luaL_checkudata(L,1,LUA_FILEHANDLE));
    int name = m_pathconfmap[luaL_checkoption(L,2,NULL,m_pathconfopts)];
    
    errno = 0;
    res   = fpathconf(fh,name);
  }
  else if (lua_type(L,1) == LUA_TSTRING)
  {
    const char *path = lua_tostring(L,1);
    int         name = m_pathconfmap[luaL_checkoption(L,2,NULL,m_pathconfopts)];
    
    errno = 0;
    res   = pathconf(path,name);
  }
  else
  {
    errno = EINVAL;
    res   = -1;
  }
  
  lua_pushnumber(L,res);
  lua_pushinteger(L,errno);
  return 2;
}

/************************************************************************/

static const struct luaL_Reg reg_fsys[] = 
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
  { "chroot"	, fsys_chroot	} ,
  { "chdir"     , fsys_chdir	} ,
  { "getcwd"	, fsys_getcwd   } ,
  { "dir"	, fsys_dir	} ,
  { "_safename"	, fsys__safename} ,
  { "basename"	, fsys_basename	} ,
  { "dirname"	, fsys_dirname	} ,
  { "openfd"	, fsys_openfd	} ,
  { "pipe"	, fsys_pipe	} ,
  { "dup"	, fsys_dup	} ,
  { "isfile"	, fsys_isfile	} ,
  { "fnmatch"	, fsys_fnmatch	} ,
  { "expand"	, fsys_expand	} ,
  { "gexpand"	, fsys_gexpand	} ,
  { "fileno"	, fsys_fileno	} ,
  { "pathconf"	, fsys_pathconf	} ,
  { NULL	, NULL		}
};

static const luaL_Reg m_dir_meta[] =
{
  { "__tostring"	, dir_meta___tostring 	} ,
  { "__gc"		, dir_meta___gc		} ,
  { "rewind"		, dir_meta_rewind	} ,
  { "next"		, dir_meta_next		} ,
  { NULL		, NULL			}
};

static const luaL_Reg m_expand_meta[] =
{
  { "__gc"		, expand_meta___gc	} ,
  { NULL		, NULL			}
};

#if LUA_VERSION_NUM == 501
#  define luaL_newlib(lua,reg) luaL_register(lua,NULL,reg)
#endif

int luaopen_org_conman_fsys(lua_State *L)
{
  luaL_newmetatable(L,TYPE_DIR);
#if LUA_VERSION_NUM == 501
  luaL_register(L,NULL,m_dir_meta);
#else
  luaL_setfuncs(L,m_dir_meta,0);
#endif

  lua_pushvalue(L,-1);
  lua_setfield(L,-2,"__index");
  
  luaL_newmetatable(L,TYPE_EXPAND);
#if LUA_VERSION_NUM == 501
  luaL_register(L,NULL,m_expand_meta);
#else
  luaL_setfuncs(L,m_expand_meta,0);
#endif

#if LUA_VERSION_NUM == 501  
  /*------------------------------------------------------------------------
  ; the Lua io module requires a unique environment.  Let's crib it (we grab
  ; it from io.open()) and use it for ourselves.  This way, when we attempt
  ; to close pipes, Lua won't crash.
  ;-------------------------------------------------------------------------*/
  
  lua_getglobal(L,"io");
  lua_getfield(L,-1,"open");
  lua_getfenv(L,-1);
  lua_replace(L, LUA_ENVIRONINDEX);
  luaL_register(L,"org.conman.fsys",reg_fsys);
#else
  luaL_newlib(L,reg_fsys);
#endif

  lua_pushinteger(L,STDIN_FILENO);
  lua_setfield(L,-2,"STDIN");
  lua_pushinteger(L,STDOUT_FILENO);
  lua_setfield(L,-2,"STDOUT");
  lua_pushinteger(L,STDERR_FILENO);
  lua_setfield(L,-2,"STDERR");
  return 1;
}

/*******************************************************************/

