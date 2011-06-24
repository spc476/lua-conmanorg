/*********************************************************************
*
* Copyright 2010 by Sean Conner.  All Rights Reserved.
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
* Comments, questions and criticisms can be sent to: sean@conman.org
*
*********************************************************************/

#define _BSD_SOURCE
#define _POSIX_SOURCE

#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <libgen.h>

/*************************************************************************/

static int fsys_chdir(lua_State *L)
{
  const char *dir;
  int         rc;
  
  dir = luaL_checkstring(L,1);
  rc  = chdir(dir);
  if (rc == -1)
  {
    int err = errno;
    lua_pushboolean(L,false);
    lua_pushinteger(L,err);
    return 2;
  }
  
  lua_pushboolean(L,true);
  return 1;
}

/*************************************************************************/

static int fsys_symlink(lua_State *L)
{
  const char *old;
  const char *new;
  int         rc;
  
  old = luaL_checkstring(L,1);
  new = luaL_checkstring(L,2);
  lua_pop(L,2);
  
  rc = symlink(old,new);
  if (rc == -1)
  {
    int e = errno;
    lua_pushboolean(L,false);
    lua_pushinteger(L,e);
    return 2;
  }
  
  lua_pushboolean(L,true);
  return 1;
}

/*************************************************************************/

static int fsys_link(lua_State *L)
{
  const char *old;
  const char *new;
  int         rc;
  
  old = luaL_checkstring(L,1);
  new = luaL_checkstring(L,2);
  lua_pop(L,2);
  
  rc = link(old,new);
  if (rc == -1)
  {
    int e = errno;
    lua_pushboolean(L,false);
    lua_pushinteger(L,e);
    return 2;
  }
  
  lua_pushboolean(L,true);
  return 1;
}

/**********************************************************************/

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
  const char *dname;
  int         rc;
  
  dname = luaL_checkstring(L,1);
  lua_pop(L,1);
  errno = 0;
  rc = mkdir(dname,0777);
  if (rc == -1)
  {
    int e = errno;
    lua_pushboolean(L,false);
    lua_pushinteger(L,e);
    return 2;
  }
  
  lua_pushboolean(L,true);
  return 1;
}

/***********************************************************************/

static int fsys_rmdir(lua_State *L)
{
  const char *dname;
  int         rc;
  
  dname = luaL_checkstring(L,1);
  lua_pop(L,1);
  errno = 0;
  rc = rmdir(dname);
  if (rc == -1)
  {
    int e = errno;
    lua_pushboolean(L,false);
    lua_pushinteger(L,e);
    return 2;
  }
  
  lua_pushboolean(L,true);
  return 1;
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

  impl_dumpstat(L,&status);  
  return 1;
}

/**********************************************************************/

static int fsys_lstat(lua_State *L)
{
  const char  *fname;
  struct stat  status;
  int          rc;
  
  fname = luaL_checkstring(L,1);
  errno = 0;
  rc    = lstat(fname,&status);
  if (rc == -1)
  {
    int e = errno;
    
    lua_pushnil(L);
    lua_pushinteger(L,e);
    return 2;
  }
  
  impl_dumpstat(L,&status);
  return 1;
}

/*************************************************************************/

static int fsys_chmod(lua_State *L)
{
  const char        *fname;
  const char        *tmode;
  mode_t             mode;
  
  fname = luaL_checkstring(L,1);
  tmode = luaL_checkstring(L,2);
  mode  = strtoul(tmode,NULL,8);
  
  if (chmod(fname,mode) < 0)
  {
    int err = errno;
    
    lua_pushboolean(L,false);
    lua_pushinteger(L,err);
    return 2;
  }
  
  lua_pushboolean(L,true);
  return 1;
}

/***********************************************************************/

static int fsys_access(lua_State *L)
{
  const char *fname  = luaL_checkstring(L,1);
  const char *tmode  = luaL_checkstring(L,2);
  int         mode   = 0;
  int         rc;
  int         e;
  
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
  rc    = access(fname,mode);
  e     = errno;
  
  lua_pushboolean(L,rc != -1);
  lua_pushinteger(L,e);

  return 2;
}

/************************************************************************/

static int fsys_getcwd(lua_State *L)
{
  char cwd[FILENAME_MAX];
  char *p;
  
  p = getcwd(cwd,sizeof(cwd));
  if (p == NULL)
  {
    int e = errno;
    
    lua_pushnil(L);
    lua_pushinteger(L,e);
    return 2;
  }

  lua_pushstring(L,cwd);
  return 1;
}

/************************************************************************/

static int fsys_opendir(lua_State *L)
{
  const char *dname;
  DIR        *dir;
  
  dname = luaL_checkstring(L,1);
  lua_pop(L,1);
  
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
  return 1;  
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
  return 1;
}

/***********************************************************************/

static const struct luaL_reg reg_fsys[] = 
{
  { "symlink"	, fsys_symlink 	} ,
  { "link"	, fsys_link	} ,
  { "mknod"	, fsys_mknod	} ,
  { "mkfifo"	, fsys_mkfifo	} ,
  { "mkdir"	, fsys_mkdir	} ,
  { "rmdir"	, fsys_rmdir	} ,
  { "stat"	, fsys_stat	} ,
  { "lstat"	, fsys_lstat	} ,
  { "chmod"	, fsys_chmod	} ,
  { "access"	, fsys_access	} ,
  { "opendir"	, fsys_opendir	} ,
  { "rewinddir"	, fsys_rewinddir} ,
  { "readdir"	, fsys_readdir	} ,
  { "closedir"	, fsys_closedir	} ,
  { "chdir"     , fsys_chdir	} ,
  { "getcwd"	, fsys_getcwd   } ,
  { "dir"	, fsys_dir	} ,
  { "_safename"	, fsys__safename} ,
  { "basename"	, fsys_basename	} ,
  { "dirname"	, fsys_dirname	} ,
  { NULL	, NULL		}
};

int luaopen_org_conman_fsys(lua_State *L)
{
  luaL_register(L,"org.conman.fsys",reg_fsys);
  
  lua_pushliteral(L,"Copyright 2010 by Sean Conner.  All Rights Reserved.");
  lua_setfield(L,-2,"_COPYRIGHT");

  lua_pushliteral(L,"GNU-GPL 3");
  lua_setfield(L,-2,"_LICENSE");
  
  lua_pushliteral(L,"Useful file manipulation functions available under Unix.");
  lua_setfield(L,-2,"_DESCRIPTION");
  
  lua_pushliteral(L,"0.4.0");
  lua_setfield(L,-2,"_VERSION");

  return 1;
}

/*******************************************************************/
