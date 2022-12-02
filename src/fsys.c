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
#  define _DEFAULT_SOURCE
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
#if !defined(__APPLE__)
#  include <sys/sysmacros.h>
#endif
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <libgen.h>
#include <utime.h>
#include <fnmatch.h>
#include <glob.h>

#if !defined(LUA_VERSION_NUM) || LUA_VERSION_NUM < 501
#  error You need to compile against Lua 5.1 or higher
#endif

#define TYPE_DIR        "org.conman.fsys:dir"
#define TYPE_EXPAND     "org.conman.fsys:expand"

#if LUA_VERSION_NUM == 501
#  define luaL_setfuncs(L,reg,up) luaI_openlib((L),NULL,(reg),(up))
#endif

/*************************************************************************/

static void fsysL_pushperm(lua_State *L,mode_t mode)
{
  char perms[12];
  
  perms[ 0] = (mode & S_ISUID) ? 'u' : '-';
  perms[ 1] = (mode & S_ISGID) ? 'g' : '-';
  perms[ 2] = (mode & S_ISVTX) ? 's' : '-';
  perms[ 3] = (mode & S_IRUSR) ? 'r' : '-';
  perms[ 4] = (mode & S_IWUSR) ? 'w' : '-';
  perms[ 5] = (mode & S_IXUSR) ? 'x' : '-';
  perms[ 6] = (mode & S_IRGRP) ? 'r' : '-';
  perms[ 7] = (mode & S_IWGRP) ? 'w' : '-';
  perms[ 8] = (mode & S_IXGRP) ? 'x' : '-';
  perms[ 9] = (mode & S_IROTH) ? 'r' : '-';
  perms[10] = (mode & S_IWOTH) ? 'w' : '-';
  perms[11] = (mode & S_IXOTH) ? 'x' : '-';
  lua_pushlstring(L,perms,sizeof(perms));
}

/*************************************************************************/

static void fsysL_pushmode(lua_State *L,mode_t mode)
{
  lua_createtable(L,0,5);
  
  fsysL_pushperm(L,mode);            lua_setfield(L,-2,"perms");
  lua_pushboolean(L,mode & S_ISUID); lua_setfield(L,-2,"setuid");
  lua_pushboolean(L,mode & S_ISGID); lua_setfield(L,-2,"getgid");
  lua_pushboolean(L,mode & S_ISVTX); lua_setfield(L,-2,"sticky");
  
  if (S_ISREG(mode))
    lua_pushliteral(L,"file");
  else if (S_ISDIR(mode))
    lua_pushliteral(L,"dir");
  else if (S_ISCHR(mode))
    lua_pushliteral(L,"chardev");
  else if (S_ISBLK(mode))
    lua_pushliteral(L,"blockdev");
  else if (S_ISFIFO(mode))
    lua_pushliteral(L,"pipe");
  else if (S_ISLNK(mode))
    lua_pushliteral(L,"link");
  else if (S_ISSOCK(mode))
    lua_pushliteral(L,"socket");
  else
    lua_pushliteral(L,"?");
    
  lua_setfield(L,-2,"type");
}

/*************************************************************************/

static void fsysL_pushdev(lua_State *L,dev_t dev)
{
  lua_createtable(L,0,2);
  lua_pushinteger(L,major(dev));
  lua_setfield(L,-2,"major");
  lua_pushinteger(L,minor(dev));
  lua_setfield(L,-2,"minor");
}

/*************************************************************************/

static void fsysL_pushstat(lua_State *L,struct stat const *status)
{
  assert(L      != NULL);
  assert(status != NULL);
  
  lua_createtable(L,0,13);
  fsysL_pushdev  (L,status->st_dev);     lua_setfield(L,-2,"dev");
  lua_pushinteger(L,status->st_ino);     lua_setfield(L,-2,"inode");
  fsysL_pushmode (L,status->st_mode);    lua_setfield(L,-2,"mode");
  lua_pushinteger(L,status->st_nlink);   lua_setfield(L,-2,"nlink");
  lua_pushinteger(L,status->st_uid);     lua_setfield(L,-2,"uid");
  lua_pushinteger(L,status->st_gid);     lua_setfield(L,-2,"gid");
  fsysL_pushdev  (L,status->st_rdev);    lua_setfield(L,-2,"rdev");
  lua_pushinteger(L,status->st_size);    lua_setfield(L,-2,"size");
  lua_pushinteger(L,status->st_blksize); lua_setfield(L,-2,"blocksize");
  lua_pushinteger(L,status->st_blocks);  lua_setfield(L,-2,"blocks");
  lua_pushinteger(L,status->st_atime);   lua_setfield(L,-2,"atime");
  lua_pushinteger(L,status->st_mtime);   lua_setfield(L,-2,"mtime");
  lua_pushinteger(L,status->st_ctime);   lua_setfield(L,-2,"ctime");
}

/*************************************************************************/

static mode_t fsysL_checkmode(lua_State *L,int idx,mode_t def)
{
  size_t      len;
  char const *txt;
  mode_t      mode;
  
  if (lua_isnoneornil(L,idx))
    return def;
  
  mode = 0;
  txt  = luaL_checklstring(L,idx,&len);
  
  if (len == 3)
  {
    if (*txt++ == 'r')
      mode |= S_IRUSR | S_IRGRP | S_IROTH;
    if (*txt++ == 'w')
      mode |= S_IWUSR | S_IWGRP | S_IWOTH;
    if (*txt++ == 'x')
      mode |= S_IXUSR | S_IXGRP | S_IXOTH;
      
    return mode;
  }
  
  if (len == 12)
  {
    if (*txt++ == 'u')
      mode |= S_ISUID;
    if (*txt++ == 'g')
      mode |= S_ISGID;
    if (*txt++ == 's')
      mode |= S_ISVTX;
    len -= 3;
  }
  
  if (len >= 9)
  {
    mode |= (*txt++ == 'r') ? S_IRUSR : 0;
    mode |= (*txt++ == 'w') ? S_IWUSR : 0;
    mode |= (*txt++ == 'x') ? S_IXUSR : 0;
    mode |= (*txt++ == 'r') ? S_IRGRP : 0;
    mode |= (*txt++ == 'w') ? S_IWGRP : 0;
    mode |= (*txt++ == 'x') ? S_IXGRP : 0;
    mode |= (*txt++ == 'r') ? S_IROTH : 0;
    mode |= (*txt++ == 'w') ? S_IWOTH : 0;
    mode |= (*txt++ == 'x') ? S_IXOTH : 0;
  }
  
  return mode;
}

/*************************************************************************/

static int fsys_stat(lua_State *L)
{
  struct stat status;
  
  if (lua_isstring(L,1))
  {
    if (stat(lua_tostring(L,1),&status) < 0)
      goto fsys_stat_err;
  }
  else if (lua_isuserdata(L,1))
  {
    if (!luaL_callmeta(L,1,"_tofd"))
      goto fsys_stat_err;
    if (fstat(luaL_checkinteger(L,-1),&status) < 0)
      goto fsys_stat_err;
  }
  
  fsysL_pushstat(L,&status);
  lua_pushinteger(L,0);
  return 2;
  
fsys_stat_err:
  lua_pushnil(L);
  lua_pushinteger(L,errno);
  return 2;
}

/*************************************************************************/

static int fsys_lstat(lua_State *L)
{
  struct stat status;
  
  if (lstat(luaL_checkstring(L,1),&status) < 0)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
  }
  else
  {
    fsysL_pushstat(L,&status);
    lua_pushinteger(L,0);
  }
  
  return 2;
}

/*************************************************************************/

static int fsys_chmod(lua_State *L)
{
  errno = 0;

  if (lua_isstring(L,1))
    chmod(luaL_checkstring(L,1),fsysL_checkmode(L,2,0666));
  else if (luaL_callmeta(L,1,"_tofd"))
    fchmod(luaL_checkinteger(L,-1),fsysL_checkmode(L,2,0664));
  else
    errno = EINVAL;
  
  lua_pushboolean(L,errno == 0);
  lua_pushinteger(L,errno);
  return 2;
}

/*************************************************************************/

static int fsys_umask(lua_State *L)
{
  fsysL_pushperm(L,~umask(~fsysL_checkmode(L,1,0777)) & 0777);
  return 1;
}

/*************************************************************************/

static int fsys_mkfifo(lua_State *L)
{
  errno = 0;
  mkfifo(luaL_checkstring(L,1),fsysL_checkmode(L,2,0666));
  lua_pushboolean(L,errno == 0);
  lua_pushinteger(L,errno);
  return 2;
}

/*************************************************************************/

static int fsys_mkdir(lua_State *L)
{
  errno = 0;
  mkdir(luaL_checkstring(L,1),fsysL_checkmode(L,2,0777));
  lua_pushboolean(L,errno == 0);
  lua_pushinteger(L,errno);
  return 2;
}

/*************************************************************************/

static int fsys_rmdir(lua_State *L)
{
  errno = 0;
  rmdir(luaL_checkstring(L,1));
  lua_pushboolean(L,errno == 0);
  lua_pushinteger(L,errno);
  return 2;
}

/*************************************************************************/

static int fsys_redirect(lua_State *L)
{
  if (luaL_callmeta(L,1,"_tofd") && luaL_callmeta(L,2,"_tofd"))
  {
    int orig = luaL_checkinteger(L,-2);
    int copy = luaL_checkinteger(L,-1);
    
    errno = 0;
    dup2(orig,copy);
  }
  else
    errno = EINVAL;
  
  lua_pushboolean(L,errno == 0);
  lua_pushinteger(L,errno);
  return 2;
}

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
  char const *old = luaL_checkstring(L,1);
  char const *new = luaL_checkstring(L,2);
  
  errno = 0;
  symlink(old,new);
  lua_pushboolean(L,errno == 0);
  lua_pushinteger(L,errno);
  return 2;
}

/*************************************************************************/

static int fsys_link(lua_State *L)
{
  char const *old = luaL_checkstring(L,1);
  char const *new = luaL_checkstring(L,2);
  
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

static int fsys_utime(lua_State *L)
{
  char const     *path;
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

/**********************************************************************/

static int fsys_access(lua_State *L)
{
  char const *fname  = luaL_checkstring(L,1);
  char const *tmode  = luaL_checkstring(L,2);
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

static int dir_meta___tostring(lua_State *L)
{
  lua_pushfstring(L,"directory (%p)",lua_touserdata(L,1));
  return 1;
}

/*************************************************************************/

static int dir_meta___gc(lua_State *L)
{
  DIR **dir = luaL_checkudata(L,1,TYPE_DIR);
  
  if (*dir != NULL)
  {
    closedir(*dir);
    *dir = NULL;
  }
  
  return 0;
}

/*************************************************************************/

static int dir_meta__tofd(lua_State *L)
{
#if defined(__SunOS_5_10)
  lua_pushinteger(L,-1);
#else
  lua_pushinteger(L,dirfd(*(DIR **)luaL_checkudata(L,1,TYPE_DIR)));
#endif
  return 1;
}

/*************************************************************************/

static int dir_meta_rewind(lua_State *L)
{
  rewinddir(*(DIR **)luaL_checkudata(L,1,TYPE_DIR));
  return 0;
}

/*************************************************************************
*
* Usage:        entry = dir:read()
*
* Desc:         Return the next filename in the directory
*
* Return:       entry (string) filename, nil when at end of directory
*
* Note:         This function will return "." (current directory) and
*               ".." (parent) directory.  Also, this function, upon end-
*               of-directory, will *NOT* close the directory.
*
**************************************************************************/

static int dir_meta_read(lua_State *L)
{
  struct dirent *entry = readdir(*(DIR **)luaL_checkudata(L,1,TYPE_DIR));
  
  if (entry)
    lua_pushstring(L,entry->d_name);
  else
    lua_pushnil(L);
  return 1;
}

/*************************************************************************
*
* Usage:        entry = dir:next()
*
* Desc:         Return the next filename in the directory.
*
* Return:       entry (string) filename, nil when at end of directory
*
* Note:         This function will *NOT* return "." (current directory)
*               or ".." (parent directory).  Also, this function *WILL*
*               close the directory when it reaches the end.
*
*               The intent of this function is to be used as an iterator,
*               where getting "." or ".." is most likely *not* something
*               that is wanted.
*
**************************************************************************/

static int dir_meta_nil(lua_State *L)
{
  /*--------------------------------------------------------------------
  ; This function is used for our iterator function when we can't open a
  ; directory for reading.
  ;---------------------------------------------------------------------*/
  
  (void)L;
  return 0;
}

static int dir_meta_next(lua_State *L)
{
  struct dirent  *entry;
  DIR           **dir = luaL_checkudata(L,1,TYPE_DIR);
  
  while(true)
  {
    entry = readdir(*dir);
    if (entry == NULL)
    {
      dir_meta___gc(L);
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
  char const   *dname;
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
  lua_pop(L,1); /* remove errno from stack */
  
  if (lua_isnil(L,-1))
    lua_pushcfunction(L,dir_meta_nil);
  else
    lua_pushcfunction(L,dir_meta_next);
  
  lua_insert(L,-2);
  lua_pushnil(L);      /* no initial value */
  lua_pushvalue(L,-2); /* Lua 5.4 toclose variable, safe to set pre 5.4 */
  return 4;
}

/*************************************************************************/

static int fsys__safename(lua_State *L)
{
  char const  *old = luaL_checkstring(L,1);
  luaL_Buffer  buf;
  
  luaL_buffinit(L,&buf);
  
  while(*old)
  {
    switch(*old)
    {
      case '/': luaL_addstring(&buf,"%2F"); break;
      case '%': luaL_addstring(&buf,"%25"); break;
      default:  luaL_addchar  (&buf,*old);  break;
    }
    old++;
  }
  
  luaL_pushresult(&buf);
  return 1;
}

/**********************************************************************
* Usage:        base = fsys.basename(path)
* Desc:         Return the last segment from a pathname.
* Input:        path (string)
* Return:       base (string)
*
* Note:         The original pathname can be reconstructed by the following
*               code:
*
*                       path = "/home/spc/spc"
*                       base = fsys.basename(path)
*                       dir  = fsys.dirname(path)
*                       full = dir .. "/" .. base
*                       assert(full == path)
*
**********************************************************************/

static int fsys_basename(lua_State *L)
{
  char const *name  = luaL_checkstring(L,1);
  char const *slash = strrchr(name,*LUA_DIRSEP);
  
  if (slash)
    lua_pushstring(L,slash + 1);
  else
    lua_pushvalue(L,1);
    
  return 1;
}

/**********************************************************************
* Usage:        dir = fsys.dirname(path)
* Desc:         Return the path of a filename (path minus the last segment)
* Input:        path (string)
* Return:       dir (string)
*
* Note:         See note for fsys.basename().
**********************************************************************/

static int fsys_dirname(lua_State *L)
{
  char const *name  = luaL_checkstring(L,1);
  char const *slash = strrchr(name,*LUA_DIRSEP);
  
  if (slash)
    lua_pushlstring(L,name,(size_t)(slash - name));
  else
    lua_pushstring(L,".");
    
  return 1;
}

/***********************************************************************
* Usage:	ext = fsys.extension(path)
* Desc:		Return the extension (if any) of a filename
* Input:	path (string)
* Return:	ext (string)
*
* Note:		Returns an empty string if no extension exists.
**********************************************************************/

static int fsys_extension(lua_State *L)
{
  char const *name  = luaL_checkstring(L,1);
  char const *slash = strrchr(name,*LUA_DIRSEP);
  char const *dot   = strrchr(name,'.');
  
  if (dot && (dot > slash))
    lua_pushstring(L,dot + 1);
  else
    lua_pushliteral(L,"");
  
  return 1;
}

/************************************************************************/

static int fsys_filename(lua_State *L)
{
  char const *name  = luaL_checkstring(L,1);
  char const *slash = strrchr(name,*LUA_DIRSEP);
  char const *dot   = strrchr(name,'.');
  
  if (dot && (dot > slash))
    lua_pushlstring(L,name,(size_t)(dot - name));
  else
    lua_pushvalue(L,1);
    
  return 1;
}

/************************************************************************/

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

/***********************************************************************
* Usage:        pipe = fsys.pipe([binary])
* Desc:         Create a Unix pipe
* Input:        binary (boolean) true if creating a binary stream
* Return:       pipe (table)
*                       * read = read stream
*                       * wrote = write stream
************************************************************************/

static int fsys_pipe(lua_State *L)
{
#if LUA_VERSION_NUM == 501
  FILE       **pfpread;
  FILE       **pfpwrite;
  FILE        *fpr;
  FILE        *fpw;
  int          fh[2];
  char const  *rm;
  char const  *wm;
  
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
  char const  *rm;
  char const  *wm;
  
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
  
  readp->f       = fpr;
  readp->closef  = fsyslib_close;
  writep->f      = fpw;
  writep->closef = fsyslib_close;
  
  lua_pushinteger(L,0);
  return 2;
#endif
}

/***********************************************************************/

static int fsys_isatty(lua_State *L)
{
  if (!luaL_callmeta(L,1,"_tofd"))
    lua_pushboolean(L,false);
  else
    lua_pushboolean(L,isatty(luaL_checkinteger(L,-1)));
  return 1;
}

/************************************************************************/

static int fsys_fnmatch(lua_State *L)
{
  char const *pattern  = luaL_checkstring(L,1);
  char const *filename = luaL_checkstring(L,2);
  char const *tflags   = luaL_optstring(L,3,"");
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
  char const *pattern = luaL_checkstring(L,1);
  glob_t      gbuf;
  int         rc;
  
  memset(&gbuf,0,sizeof(gbuf));
  rc = glob(pattern,0,NULL,&gbuf);
  
  if (rc != 0)
  {
    lua_pushnil(L);
    switch(rc)
    {
      case GLOB_NOSPACE: rc = ENOMEM; break;
      case GLOB_ABORTED: rc = EIO;    break;
      case GLOB_NOMATCH: rc = ENOENT; break;
      default:           rc = EINVAL; break;
    }
    
    lua_pushinteger(L,rc);
    return 2;
  }
  
  lua_createtable(L,gbuf.gl_pathc,0);
  for (size_t i = 0 ; i < gbuf.gl_pathc ; i++)
  {
    lua_pushinteger(L,i+1);
    lua_pushstring(L,gbuf.gl_pathv[i]);
    lua_settable(L,-3);
  }
  
  globfree(&gbuf);
  lua_pushinteger(L,0);
  return 2;
}

/************************************************************************/

struct myexpand
{
  glob_t gbuf;
  size_t idx;
  bool   gc;
};

static int expand_meta___gc(lua_State *L)
{
  struct myexpand *data = lua_touserdata(L,1);
  if (!data->gc)
  {
    globfree(&data->gbuf);
    data->gc = true;
  }
  return 0;
}

/************************************************************************/

static int expand_meta_next(lua_State *L)
{
  struct myexpand *data = lua_touserdata(L,1);
  
  if (data->idx < data->gbuf.gl_pathc)
    lua_pushstring(L,data->gbuf.gl_pathv[data->idx++]);
  else
  {
    globfree(&data->gbuf);
    data->gc = true;
    lua_pushnil(L);
  }
  
  return 1;
}

/************************************************************************/

static int fsys_gexpand(lua_State *L)
{
  char const      *pattern = luaL_checkstring(L,1);
  struct myexpand *data;
  
  lua_pushcfunction(L,expand_meta_next);
  
  data = lua_newuserdata(L,sizeof(struct myexpand));
  luaL_getmetatable(L,TYPE_EXPAND);
  lua_setmetatable(L,-2);
  
  memset(data,0,sizeof(struct myexpand));
  
  if (glob(pattern,0,NULL,&data->gbuf) != 0)
    data->gc = true;
  
  lua_pushnil(L);
  lua_pushvalue(L,-2);
  return 4;
}

/************************************************************************
*
* Usage:        value,err = fsys.pathconf(filespec,var)
*
* Desc:         Return data about a specific value for an open file
*               descriptor.
*
* Input:        filespec (string userdata(FILE)) name of a file
*                       | or an open file
*               var (string/enum)
*                       * link     maximum number of links to the file
*                       * canon    maximum size of a formatted input line
*                       * input    maximum size of an input line
*                       * name     size of filename in current directory
*                       * path     size of relative path
*                       * pipe     size of pipe buffer
*                       * chown    <> 0 if chown cannot be used on this file
*                       * trunc    <> 0 if filenames longer than 'name'
*                                       | generates an error
*                       * vdisable <> 0 if special character processing can
*                                       | be disabled
*
* Return:       value (number) value for specific value (only if err is 0)
*               err (integer) system error, 0 if successful
*
************************************************************************/

static int fsys_pathconf(lua_State *L)
{
  static char const *const m_pathconfopts[] =
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

  static int const m_pathconfmap[] =
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
    char const *path = lua_tostring(L,1);
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

/************************************************************************
* Usage:	okay,err = fsys._close(fh)
* Desc:		Low level OS close() system call (when you need it, you need it)
* Input:	fh (integer) file descriptor to close
* Return:	okay (boolean) true if okay, false if error
*		err (integer) system error
*************************************************************************/

static int fsys__close(lua_State *L)
{
  lua_pushboolean(L,close(luaL_checkinteger(L,1)) == 0);
  lua_pushinteger(L,errno);
  return 2;
}

/************************************************************************
* Usage:	okay,err = fsys.fsync(file)
* Desc:		Syncronize file's in-core state with disk
* Input:	file (userdata) file
* Return:	okay (boolean) true if okay, false if error
*		err (integer) system error
*************************************************************************/

static int fsys_fsync(lua_State *L)
{
  if (!luaL_callmeta(L,1,"_tofd"))
  {
    lua_pushboolean(L,false);
    lua_pushinteger(L,EBADF);
  }
  else
  {
    errno = 0;
    isatty(luaL_checkinteger(L,-1));
    lua_pushboolean(L,errno == 0);
    lua_pushinteger(L,errno);
  }
  return 2;
}

/************************************************************************
* Usage:        okay,err = fsys._lock(file[,mode][,nonblock])
* Desc:         Lock or unlock a file
* Input:        file (userdata) file
*		mode (string/enum/optional)
*			* read (default)
*			* write
*			* release
*		nonblock (boolean/optional) Use nonblocking call
* Return:       okay (boolean) true if okay, false if error
*               err (integer) system error
*
* Note:		This function uses fcntl() to do the locking, and will
*		lock the entire contents of the file.
*************************************************************************/

static int fsys__lock(lua_State *L)
{
  static char const *const modename[] = { "read"  , "write" , "release" };
  static int const         mode[]     = { F_RDLCK , F_WRLCK , F_UNLCK   };
#if LUA_VERSION_NUM == 501
  FILE **pfp = luaL_checkudata(L,1,LUA_FILEHANDLE);
  int    fh  = fileno(*pfp);
#else
  luaL_Stream *pfp = luaL_checkudata(L,1,LUA_FILEHANDLE);
  int          fh  = fileno(pfp->f);
#endif
  int type = luaL_checkoption(L,2,"read",modename);
  int cmd  = lua_toboolean(L,3) ? F_SETLK : F_SETLKW;
  
  fcntl(
         fh,
         cmd,
         &(struct flock) {
           .l_type   = mode[type],
           .l_start  = 0,
           .l_whence = SEEK_SET,
           .l_len    = 0,
         }
       );
  lua_pushboolean(L,errno == 0);
  lua_pushinteger(L,errno);
  return 2;
}

/************************************************************************
*                 MONKEY PATCH! MONKEY PATCH! MONKEY PATCH!
*
* We add some functions to the io module
*
* Usage:        f = io._fromfd(fh,mode)
* Desc:         Create a file from a file handle
* Input:        fh (integer) a Unix file handle
*               mode (string) same as from io.open()
* Return:       f (userdata/FILE*) a file
*************************************************************************/

static int monkeypatch_mod__fromfd(lua_State *L)
{
  lua_settop(L,2);
  
  FILE *fp = fdopen(luaL_checkinteger(L,1),luaL_checkstring(L,2));
  
  if (fp == NULL)
  {
    lua_pushnil(L);
    lua_pushstring(L,strerror(errno));
    lua_pushinteger(L,errno);
    return 3;
  }
  
#if LUA_VERSION_NUM == 501

  FILE **pfp = lua_newuserdata(L,sizeof(FILE *));
  luaL_getmetatable(L,LUA_FILEHANDLE);
  lua_setmetatable(L,-2);
  lua_createtable(L,0,0);
  lua_pushcfunction(L,fsyslib_close);
  lua_setfield(L,-2,"__close");
  lua_setfenv(L,-2);
  *pfp = fp;
  
#else

  luaL_Stream *ps = lua_newuserdata(L,sizeof(luaL_Stream));
  ps->f           = fp;
  ps->closef      = fsyslib_close;
  luaL_getmetatable(L,LUA_FILEHANDLE);
  lua_setmetatable(L,-2);
  
#endif

  return 1;
}

/************************************************************************
* Usage:        fh = file:_tofd()
* Desc:         Return the file handle from a file
* Input:        file (userdata/FILE*) file
* Return:       fh (integer) file handle
*************************************************************************/

static int monkeypatch_meta__tofd(lua_State *L)
{
  lua_pushinteger(L,fileno(*(FILE **)lua_touserdata(L,1)));
  return 1;
}

/************************************************************************
* Usage:        f = file:_dup(mode)
* Desc:         Duplicate a file to a new file object
* Input:        file (userdata/FILE*) file
*               mode (string) same as used in io.open()
* Return:       f (userdata/FILE*) new file
*************************************************************************/

static int monkeypatch_meta__dup(lua_State *L)
{
  FILE **pfp = luaL_checkudata(L,1,LUA_FILEHANDLE);
  int    fh  = dup(fileno(*pfp));
  int    rc;
  
  if (fh == -1)
  {
    lua_pushnil(L);
    lua_pushstring(L,strerror(errno));
    lua_pushinteger(L,errno);
    return 3;
  }
  
  lua_pushinteger(L,fh);
  lua_pushvalue(L,2);
  rc = monkeypatch_mod__fromfd(L);
  if (rc == 3)
    close(fh);
  return rc;
}

/************************************************************************/

int luaopen_org_conman_fsys(lua_State *L)
{
  static struct luaL_Reg const reg_fsys[] =
  {
    { "symlink"   , fsys_symlink   } ,
    { "link"      , fsys_link      } ,
    { "readlink"  , fsys_readlink  } ,
    { "mkfifo"    , fsys_mkfifo    } ,
    { "mkdir"     , fsys_mkdir     } ,
    { "rmdir"     , fsys_rmdir     } ,
    { "utime"     , fsys_utime     } ,
    { "stat"      , fsys_stat      } ,
    { "lstat"     , fsys_lstat     } ,
    { "umask"     , fsys_umask     } ,
    { "chmod"     , fsys_chmod     } ,
    { "access"    , fsys_access    } ,
    { "opendir"   , fsys_opendir   } ,
    { "chroot"    , fsys_chroot    } ,
    { "chdir"     , fsys_chdir     } ,
    { "getcwd"    , fsys_getcwd    } ,
    { "dir"       , fsys_dir       } ,
    { "_safename" , fsys__safename } ,
    { "basename"  , fsys_basename  } ,
    { "dirname"   , fsys_dirname   } ,
    { "extension" , fsys_extension } ,
    { "filename"  , fsys_filename  } ,
    { "pipe"      , fsys_pipe      } ,
    { "redirect"  , fsys_redirect  } ,
    { "isatty"    , fsys_isatty    } ,
    { "fnmatch"   , fsys_fnmatch   } ,
    { "expand"    , fsys_expand    } ,
    { "gexpand"   , fsys_gexpand   } ,
    { "pathconf"  , fsys_pathconf  } ,
    { "_close"    , fsys__close    } ,
    { "fsync"     , fsys_fsync     } ,
    { "_lock"     , fsys__lock     } ,
    { NULL        , NULL           }
  };
  
  static luaL_Reg const m_dir_meta[] =
  {
    { "__tostring"        , dir_meta___tostring   } ,
    { "__gc"              , dir_meta___gc         } ,
  #if LUA_VERSION_NUM >= 504
    { "__close"           , dir_meta___gc         } ,
  #endif
    { "_tofd"             , dir_meta__tofd        } ,
    { "rewind"            , dir_meta_rewind       } ,
    { "read"              , dir_meta_read         } ,
    { "next"              , dir_meta_next         } ,
    { NULL                , NULL                  }
  };
  
  static luaL_Reg const m_expand_meta[] =
  {
    { "__gc"              , expand_meta___gc      } ,
  #if LUA_VERSION_NUM >= 504
    { "__close"           , expand_meta___gc      } ,
  #endif
    { NULL                , NULL                  }
  };
  
  luaL_getmetatable(L,LUA_FILEHANDLE);
  lua_pushcfunction(L,monkeypatch_meta__tofd);
  lua_setfield(L,-2,"_tofd");
  lua_pushcfunction(L,monkeypatch_meta__dup);
  lua_setfield(L,-2,"_dup");
  
  lua_getfield(L,LUA_REGISTRYINDEX,"_LOADED");
  lua_getfield(L,-1,"io");
  lua_pushcfunction(L,monkeypatch_mod__fromfd);
  lua_setfield(L,-2,"_fromfd");
  
  luaL_newmetatable(L,TYPE_DIR);
  luaL_setfuncs(L,m_dir_meta,0);

  lua_pushvalue(L,-1);
  lua_setfield(L,-2,"__index");
  
  luaL_newmetatable(L,TYPE_EXPAND);
  luaL_setfuncs(L,m_expand_meta,0);

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

  return 1;
}

/*******************************************************************/
