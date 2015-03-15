
#ifdef __GNUC__
#  define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>


#include <lua.h>
#include <lauxlib.h>

#define TYPE_TERMIOS	"org.conman.pts:termios"
#define INT(x)	{ #x , x }

/*************************************************************************/

typedef struct xlua_Const
{
  const char *const name;
  const int         value;
} xlua_Const;

/*************************************************************************/

#if LUA_VERSION_NUM < 502
int lua_absindex(lua_State *L,int idx)
{
  return (idx > 0) || (idx <= LUA_REGISTRYINDEX)
         ? idx
         : lua_gettop(L) + idx + 1
         ;
}
#endif

/**************************************************************************/

static void xlua_createconst(
        lua_State        *L,
        int               idx,
        const char       *name,
        const xlua_Const *list
)
{
  idx = lua_absindex(L,idx);
  lua_createtable(L,0,0);
  for (size_t i = 0 ; list[i].name != NULL ; i++)  
  {
    lua_pushinteger(L,list[i].value);
    lua_setfield(L,-2,list[i].name);
  }
  lua_setfield(L,idx,name);
}

/*************************************************************************/

static int pts_getpt(lua_State *L)
{
  errno = 0;
  lua_pushinteger(L,getpt());
  lua_pushinteger(L,errno);
  return 2;
}

/*************************************************************************/

static int pts_grantpt(lua_State *L)
{
  /*--------------------------------------------------------
  ; SIGCHLD must not be caught, least this hang this call
  ;---------------------------------------------------------*/
  
  errno = 0;
  grantpt(luaL_checkinteger(L,1));
  lua_pushinteger(L,errno);
  return 1;
}

/*************************************************************************/

static int pts_unlockpt(lua_State *L)
{
  errno = 0;
  unlockpt(luaL_checkinteger(L,1));
  lua_pushinteger(L,errno);
  return 1;
}

/*************************************************************************/

static int pts_ptsname(lua_State *L)
{
  lua_pushstring(L,ptsname(luaL_checkinteger(L,1)));
  return 1;
}

/*************************************************************************/

static int pts_getsize(lua_State *L)
{
  struct winsize ws;
  
  ws.ws_col = 0;
  ws.ws_row = 0;
  errno     = 0;
  
  ioctl(luaL_checkinteger(L,1),TIOCGWINSZ,&ws);
  lua_pushinteger(L,ws.ws_col);
  lua_pushinteger(L,ws.ws_row);
  lua_pushinteger(L,errno);
  return 3;
}
  
/*************************************************************************/

static int pts_setsize(lua_State *L)
{
  errno = 0;
  ioctl(
         luaL_checkinteger(L,1),
         TIOCSWINSZ,
         &(struct winsize) {
           .ws_col    = luaL_checkinteger(L,2),
           .ws_row    = luaL_checkinteger(L,3),
           .ws_xpixel = 0,
           .ws_ypixel = 0,
        }
    );
  lua_pushinteger(L,errno);
  return 1;
}

/*************************************************************************/

static int pts_session(lua_State *L)
{
  setsid();
  ioctl(luaL_checkinteger(L,1),TIOCSCTTY,1);
  return 0;
}

/*************************************************************************/

static int pts_getattr(lua_State *L)
{
  struct termios  attr;
  struct termios *tio;
  
  if (tcgetattr(luaL_checkinteger(L,1),&attr) < 0)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    return 2;
  }
  
  tio = lua_newuserdata(L,sizeof(struct termios));
  *tio = attr;
  luaL_getmetatable(L,TYPE_TERMIOS);
  lua_setmetatable(L,-2);
  lua_pushinteger(L,0);
  return 2;
}

/*************************************************************************/

static int pts_rawattr(lua_State *L)
{
  struct termios *orig;
  struct termios *raw;
  
  orig = luaL_checkudata(L,1,TYPE_TERMIOS);
  raw  = lua_newuserdata(L,sizeof(struct termios));
  *raw = *orig;
  cfmakeraw(raw);
  luaL_getmetatable(L,TYPE_TERMIOS);
  lua_setmetatable(L,-2);
  return 1;
}

/*************************************************************************/

static int pts_setattr(lua_State *L)
{
  errno = 0;
  tcsetattr(
             luaL_checkinteger(L,1),
             luaL_checkinteger(L,2),
             luaL_checkudata(L,3,TYPE_TERMIOS)
           );
  lua_pushinteger(L,errno);
  return 1;
}

/*************************************************************************/

static int pts_drain(lua_State *L)
{
  errno = 0;
  tcdrain(luaL_checkinteger(L,1));
  lua_pushinteger(L,errno);
  return 1;
}

/*************************************************************************/

static int pts_flush(lua_State *L)
{
  errno = 0;
  tcflush(luaL_checkinteger(L,1),luaL_checkinteger(L,2));
  lua_pushinteger(L,errno);
  return 1;
}

/*************************************************************************/

static int pts_flow(lua_State *L)
{
  errno = 0;
  tcflow(luaL_checkinteger(L,1),luaL_checkinteger(L,2));
  lua_pushinteger(L,errno);
  return 1;
}

/*************************************************************************/  

static int pts_read(lua_State *L)
{
  char buffer[64];
  ssize_t bytes;
  
  bytes = read(luaL_checkinteger(L,1),buffer,sizeof(buffer));
  if (bytes < 0)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
  }
  else
  {
    lua_pushlstring(L,buffer,bytes);
    lua_pushinteger(L,0);
  }
  return 2;
}

/*************************************************************************/

static const luaL_Reg m_pts_reg[] =
{
  { "getpt"	, pts_getpt	} ,
  { "grantpt"	, pts_grantpt	} ,
  { "unlockpt"	, pts_unlockpt	} ,
  { "ptsname"	, pts_ptsname	} ,
  { "getsize"	, pts_getsize	} ,
  { "setsize"	, pts_setsize	} ,
  { "session"	, pts_session	} ,
  { "getattr"	, pts_getattr	} ,
  { "setattr"	, pts_setattr	} ,
  { "rawattr"	, pts_rawattr	} ,
  { "drain"	, pts_drain	} ,
  { "flush"	, pts_flush	} ,
  { "flow"	, pts_flow	} ,
  { "read"	, pts_read	} ,
  { NULL	, NULL		}
};

static const xlua_Const m_iflags[] =
{
  INT(IGNBRK),
  INT(BRKINT),
  INT(IGNPAR),
  INT(PARMRK),
  INT(INPCK),
  INT(ISTRIP),
  INT(INLCR),
  INT(IGNCR),
  INT(ICRNL),
  INT(IUCLC),
  INT(IXON),
  INT(IXANY),
  INT(IXOFF),
  INT(IMAXBEL),
  { NULL , 0 } 
};

static const xlua_Const m_oflags[] =
{
  INT(OPOST),
  INT(OLCUC),
  INT(ONLCR),
  INT(ONOCR),
  INT(ONLRET),
  INT(OFILL),
  INT(OFDEL),
  INT(NLDLY),
  INT(CRDLY),
  INT(TABDLY),
  INT(BSDLY),
  INT(VTDLY),
  INT(FFDLY),
  { NULL , 0 }
};

static const xlua_Const m_cflags[] =
{
  INT(CBAUD),
  INT(CBAUDEX),
  INT(CSIZE),
    INT(CS5),
    INT(CS6),
    INT(CS7),
    INT(CS8),
  INT(CSTOPB),
  INT(CREAD),
  INT(PARENB),
  INT(PARODD),
  INT(HUPCL),
  INT(CLOCAL),
#ifdef LOBLK
  INT(LOBLK),
#endif
  INT(CIBAUD),
  INT(CRTSCTS),
  { NULL , 0 }
};

static const xlua_Const m_lflags[] =
{
  INT(ISIG),
  INT(ICANON),
#if 0
    INT(EOF),
    INT(EOL),
    INT(EOL2),
    INT(ERASE),
    INT(KILL),
    INT(LNEXT),
    INT(REPRINT),
    INT(STATUS),
    INT(WERASE),
#endif
  INT(XCASE),
  INT(ECHO),
  INT(ECHOE),
  INT(ECHOK),
  INT(ECHONL),
  INT(ECHOCTL),
#ifdef ECHOPTR
  INT(ECHOPTR),
#endif
  INT(ECHOKE),
#ifdef DEFECHO
  INT(DEFECHO),
#endif
  INT(FLUSHO),
  INT(NOFLSH),
  INT(TOSTOP),
  INT(PENDIN),
  INT(IEXTEN),
  { NULL , 0 }
};

static const xlua_Const m_cc[] =
{
  INT(VINTR),
  INT(VQUIT),
  INT(VERASE),
  INT(VKILL),
  INT(VEOF),
  INT(VMIN),
  INT(VEOL),
  INT(VTIME),
  INT(VEOL2),
#ifdef VSWTCH
  INT(VSWTCH),
#endif
  INT(VSTART),
  INT(VSTOP),
  INT(VSUSP),
#ifdef VDSUSP
  INT(VDSUSP),
#endif
  INT(VLNEXT),
  INT(VWERASE),
  INT(VREPRINT),
  INT(VDISCARD),
#ifdef VSTATUS
  INT(VSTATUS),
#endif
  { NULL , 0 }
};

static const xlua_Const m_setattr[] =
{
  INT(TCSANOW),
  INT(TCSADRAIN),
  INT(TCSAFLUSH),
  { NULL , 0 }
};

static const xlua_Const m_tcflush[] =
{
  INT(TCIFLUSH),
  INT(TCOFLUSH),
  INT(TCIOFLUSH),
  { NULL , 0 }
};

static const xlua_Const m_tcflow[] =
{
  INT(TCOOFF),
  INT(TCIOFF),
  INT(TCION),
  { NULL , 0 }
};

int luaopen_org_conman_ptscore(lua_State *L)
{
  luaL_newmetatable(L,TYPE_TERMIOS);
  
  luaL_register(L,"org.conman.ptscore",m_pts_reg);
  xlua_createconst(L,-1,"_iflags", m_iflags);
  xlua_createconst(L,-1,"_oflags", m_oflags);
  xlua_createconst(L,-1,"_cflags", m_cflags);
  xlua_createconst(L,-1,"_lflags", m_lflags);
  xlua_createconst(L,-1,"_cc",     m_cc);
  xlua_createconst(L,-1,"_setattr",m_setattr);
  xlua_createconst(L,-1,"_tcflush",m_tcflush);
  xlua_createconst(L,-1,"_tcflow", m_tcflow);
  return 1;
}

/*************************************************************************/
