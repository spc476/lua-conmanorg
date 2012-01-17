/***************************************************************************
*
* Copyright 2011 by Sean Conner.
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
* Comments, questions and criticisms can be sent to: sean@conman.org
*
*************************************************************************/

#ifndef __GNU__
#  define __attribute__(x)
#endif

#define _BSD_SOURCE
#define _FORTIFY_SOURCE 0

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>

#include <syslog.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/poll.h>
#include <netdb.h>
#include <unistd.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define NET_SOCK	"net:sock"
#define NET_ADDR	"net:addr"

#ifdef __SunOS
#  define AF_LOCAL	AF_UNIX
#  define SUN_LEN(x)	sizeof(struct sockaddr_un)
#endif

/************************************************************************/

typedef union sockaddr_all
{
  struct sockaddr     sa;
  struct sockaddr_in  sin;
  struct sockaddr_in6 sin6;
  struct sockaddr_un  ssun;
} sockaddr_all__t;

typedef struct sock
{
  int fh;
} sock__t;

/************************************************************************/

static int	netlua_socket		(lua_State *const) __attribute__((nonnull));
static int	netlua_socketfd		(lua_State *const) __attribute__((nonnull));
static int	netlua_address		(lua_State *const) __attribute__((nonnull));
static int	socklua___tostring	(lua_State *const) __attribute__((nonnull));
static int      socklua_peer		(lua_State *const) __attribute__((nonnull));
static int	socklua_bind		(lua_State *const) __attribute__((nonnull));
static int	socklua_connect		(lua_State *const) __attribute__((nonnull));
static int	socklua_listen		(lua_State *const) __attribute__((nonnull));
static int	socklua_accept		(lua_State *const) __attribute__((nonnull));
static int	socklua_reuse		(lua_State *const) __attribute__((nonnull));
static int	socklua_read		(lua_State *const) __attribute__((nonnull));
static int	socklua_write		(lua_State *const) __attribute__((nonnull));
static int	socklua_shutdown	(lua_State *const) __attribute__((nonnull));
static int	socklua_close		(lua_State *const) __attribute__((nonnull));
static int	socklua_fd		(lua_State *const) __attribute__((nonnull));
static int	addrlua___index		(lua_State *const) __attribute__((nonnull));
static int	addrlua___tostring	(lua_State *const) __attribute__((nonnull));
static int	addrlua___eq		(lua_State *const) __attribute__((nonnull));
static int	addrlua___lt		(lua_State *const) __attribute__((nonnull));
static int	addrlua___le		(lua_State *const) __attribute__((nonnull));
static int	addrlua___len		(lua_State *const) __attribute__((nonnull));

/*************************************************************************/

static const luaL_reg mnet_reg[] =
{
  { "socket"		, netlua_socket		} ,
  { "socketfd"		, netlua_socketfd	} ,
  { "address"		, netlua_address	} ,
  { "peer"		, socklua_peer		} ,
  { NULL		, NULL			}
};

static const luaL_reg msock_regmeta[] =
{
  { "__tostring"	, socklua___tostring	} ,
  { "__gc"		, socklua_close		} ,
  { "peer"		, socklua_peer		} ,
  { "bind"		, socklua_bind		} ,
  { "connect"		, socklua_connect	} ,
  { "listen"		, socklua_listen	} ,
  { "accept"		, socklua_accept	} ,
  { "reuse"		, socklua_reuse		} ,
  { "read"		, socklua_read		} ,
  { "write"		, socklua_write		} ,
  { "shutdown"		, socklua_shutdown	} ,
  { "close"		, socklua_close		} ,
  { "fd"		, socklua_fd		} ,
  { NULL		, NULL			}
};

static const luaL_reg maddr_regmeta[] =
{
  { "__index"		, addrlua___index	} ,
  { "__tostring"	, addrlua___tostring	} ,
  { "__eq"		, addrlua___eq		} ,
  { "__lt"		, addrlua___lt		} ,
  { "__le"		, addrlua___le		} ,
  { "__len"		, addrlua___len		} ,
  { NULL		, NULL			}
};

static const char *const m_netfamilytext[] = { "ip"    , "ip6"    , "unix" };
static const int         m_netfamily[]     = { AF_INET , AF_INET6 , AF_UNIX };

/************************************************************************/

static inline size_t	  Inet_addrlen	(sockaddr_all__t *const)                               __attribute__((nonnull));
static inline socklen_t   Inet_len    	(sockaddr_all__t *const)                               __attribute__((nonnull));
static inline int         Inet_port   	(sockaddr_all__t *const)                               __attribute__((nonnull));
static inline void        Inet_setport	(sockaddr_all__t *const,const int)                     __attribute__((nonnull(1)));
static inline void	  Inet_setportn	(sockaddr_all__t *const,const int)                     __attribute__((nonnull(1)));
static inline const char *Inet_addr   	(sockaddr_all__t *const restrict,char *const restrict) __attribute__((nonnull));
static inline void	 *Inet_address	(sockaddr_all__t *const)                               __attribute__((nonnull));

/*-----------------------------------------------------------------------*/

static inline size_t Inet_addrlen(sockaddr_all__t *const addr)
{
  assert(addr != NULL);
  switch(addr->sa.sa_family)
  {
    case AF_INET:  return sizeof(addr->sin.sin_addr.s_addr);
    case AF_INET6: return sizeof(addr->sin6.sin6_addr.s6_addr);
    case AF_UNIX:  return strlen(addr->ssun.sun_path);
    default:       assert(0); return 0;
  }
}

/*-----------------------------------------------------------------------*/

static inline socklen_t Inet_len(sockaddr_all__t *const addr)
{
  assert(addr != NULL);
  switch(addr->sa.sa_family)
  {
    case AF_INET:  return sizeof(addr->sin);
    case AF_INET6: return sizeof(addr->sin6);
    case AF_UNIX:  return SUN_LEN(&addr->ssun);
    default:       assert(0); return 0;
  }
}

/*----------------------------------------------------------------------*/

static inline int Inet_port(sockaddr_all__t *const addr)
{
  assert(addr != NULL);
  switch(addr->sa.sa_family)
  {
    case AF_INET:  return ntohs(addr->sin.sin_port);
    case AF_INET6: return ntohs(addr->sin6.sin6_port);
    case AF_UNIX:  return 0;
    default:       assert(0); return 0;
  }
}

/*-----------------------------------------------------------------------*/

static inline void Inet_setport(sockaddr_all__t *const addr,const int port)
{
  assert(addr != NULL);
  assert((addr->sa.sa_family == AF_INET) || (addr->sa.sa_family == AF_INET6));
  assert(port >= 0);
  assert(port <= 65535);
  
  switch(addr->sa.sa_family)
  {
    case AF_INET:  addr->sin.sin_port   = htons(port); return;
    case AF_INET6: addr->sin6.sin6_port = htons(port); return;
    default:       assert(0);                          return;
  }
}

/*----------------------------------------------------------------------*/

static inline void Inet_setportn(sockaddr_all__t *const addr,const int port)
{
  assert(addr != NULL);
  assert((addr->sa.sa_family == AF_INET) || (addr->sa.sa_family == AF_INET6));
  
  switch(addr->sa.sa_family)
  {
    case AF_INET:  addr->sin.sin_port   = port; return;
    case AF_INET6: addr->sin6.sin6_port = port; return;
    default:       assert(0);                   return;
  }
}

/*------------------------------------------------------------------------*/

static inline const char *Inet_addr(
	sockaddr_all__t *const restrict addr,
	char            *const restrict dest
)
{
  assert(addr != NULL);
  assert(dest != NULL);
  switch(addr->sa.sa_family)
  {
    case AF_INET:  return inet_ntop(AF_INET, &addr->sin.sin_addr.s_addr,   dest,INET6_ADDRSTRLEN);
    case AF_INET6: return inet_ntop(AF_INET6,&addr->sin6.sin6_addr.s6_addr,dest,INET6_ADDRSTRLEN);
    case AF_UNIX:  return addr->ssun.sun_path;
    default:       assert(0); return NULL;
  }
}

/*------------------------------------------------------------------------*/

static inline void *Inet_address(sockaddr_all__t *const addr)
{
  assert(addr != NULL);
  switch(addr->sa.sa_family)
  {
    case AF_INET:  return &addr->sin.sin_addr.s_addr;
    case AF_INET6: return &addr->sin6.sin6_addr.s6_addr;
    case AF_UNIX:  return &addr->ssun.sun_path;
    default:       assert(0); return NULL;
  }
}

/**********************************************************************
*
*	sock,err = net.socket(family,proto)
*
*	family = 'ip'  | 'ipv6' | 'unix'
*	proto  = string | number
*
**********************************************************************/

static int netlua_socket(lua_State *const L)
{
  int      family;
  int      proto;
  int      type;
  sock__t *sock;

  family = m_netfamily[luaL_checkoption(L,1,NULL,m_netfamilytext)];
  
  if (lua_isnumber(L,2))
    proto = lua_tointeger(L,2);
  else if (lua_isstring(L,2))
  {
    struct protoent *e = getprotobyname(lua_tostring(L,2));
    if (e == NULL)
    {
      lua_pushnil(L);
      lua_pushinteger(L,ENOPROTOOPT);
      return 2;
    }
    proto = e->p_proto;
  }
  
  if (proto == IPPROTO_TCP)
    type = SOCK_STREAM;
  else if (proto == IPPROTO_UDP)
    type = SOCK_DGRAM;
  else
    type = SOCK_RAW;

  sock     = lua_newuserdata(L,sizeof(sock__t));
  sock->fh = socket(family,type,proto);
  if (sock->fh == -1)
  {
    int err = errno;
    lua_pushnil(L);
    lua_pushinteger(L,err);
  }
  else
  {
    luaL_getmetatable(L,NET_SOCK);
    lua_setmetatable(L,-2);
    lua_pushinteger(L,0);
  }

  return 2;
}
  
/*******************************************************************
*
*	sock,err = net.socketfd(fd)
*
*	fd = integer
*
********************************************************************/

static int netlua_socketfd(lua_State *const L)
{
  sock__t *sock;
  
  sock     = lua_newuserdata(L,sizeof(sock__t));
  sock->fh = luaL_checkinteger(L,1);
  luaL_getmetatable(L,NET_SOCK);
  lua_setmetatable(L,-2);
  lua_pushinteger(L,0);
  return 2;
}

/***********************************************************************
*
*	addr,err = net.address(address,port[,type = 'tcp'])
*
*	address = ip (192.168.1.1) | ip6 (fc00::1) | unix (/dev/log)
*	port    = number | string (not used for unix addresses)
*	type    = 'tcp'  | 'udp' | 'raw'
*
***********************************************************************/

static int netlua_address(lua_State *const L)
{
  sockaddr_all__t *addr;
  const char      *host;
  size_t           hsize;
  int              top;
  
  top  = lua_gettop(L);
  addr = lua_newuserdata(L,sizeof(sockaddr_all__t));
  host = luaL_checklstring(L,1,&hsize);
  
  if (inet_pton(AF_INET,host,&addr->sin.sin_addr.s_addr))
    addr->sin.sin_family = AF_INET;
  else if (inet_pton(AF_INET6,host,&addr->sin6.sin6_addr.s6_addr))
    addr->sin6.sin6_family = AF_INET6;
  else
  {
    if (hsize > sizeof(addr->ssun.sun_path) - 1)
    {
      lua_pushnil(L);
      lua_pushinteger(L,EINVAL);
      return 2;
    }
    
    addr->ssun.sun_family = AF_UNIX;
    memcpy(addr->ssun.sun_path,host,hsize + 1);
    luaL_getmetatable(L,NET_ADDR);
    lua_setmetatable(L,-2);
    lua_pushinteger(L,0);
    return 2;
  }
  
  if (lua_isnumber(L,2))
  {
    int port;
    
    port = lua_tointeger(L,2);
    if ((port < 0) || (port > 65535))
      return luaL_error(L,"invalid port number");
    Inet_setport(addr,port);
  }
  else if (lua_isstring(L,2))
  {
    const char *serv;
    const char *type;
    
    serv = lua_tostring(L,2);
    type = (top == 3) ? lua_tostring(L,3) : "tcp";
    
    if (strcmp(type,"raw") == 0)
    {
      struct protoent  result;
      struct protoent *presult;
      char             tmp[BUFSIZ];

#ifdef __SunOS
      presult = getprotobyname_r(serv,&result,tmp,sizeof(tmp));
      if (presult == NULL)
      {
        int err = errno;
        lua_pushnil(L);
        lua_pushinteger(L,err);
        return 2;
      }
#else           
      int rc = getprotobyname_r(serv,&result,tmp,sizeof(tmp),&presult);
      if (rc != 0)
      {
        lua_pushnil(L);
        lua_pushinteger(L,rc);
        return 2;
      }
#endif
       
      Inet_setport(addr,result.p_proto);
      luaL_getmetatable(L,NET_ADDR);
      lua_setmetatable(L,-2);
      lua_pushinteger(L,0);
      return 2;
    }
    
    if ((strcmp(type,"tcp") != 0) && (strcmp(type,"udp") != 0))
    {
      lua_pushnil(L);
      lua_pushinteger(L,EPROTOTYPE);
      return 2;
    }
    
    struct servent  result;
    struct servent *presult;
    char            tmp[BUFSIZ];

#ifdef __SunOS
    presult = getservbyname_r(serv,type,&result,tmp,sizeof(tmp));
    if (presult == NULL)
    {
      int err = errno;
      lua_pushnil(L);
      lua_pushinteger(L,err);
      return 2;
    }
#else    
    int rc = getservbyname_r(serv,type,&result,tmp,sizeof(tmp),&presult);
    if (rc != 0)
    {
      lua_pushnil(L);
      lua_pushinteger(L,rc);
      return 2;
    }
#endif

    Inet_setportn(addr,result.s_port);
  }
  
  luaL_getmetatable(L,NET_ADDR);
  lua_setmetatable(L,-2);
  lua_pushinteger(L,0);
  return 2;
}

/***********************************************************************/

static int socklua___tostring(lua_State *const L)
{
  sock__t *sock;
  
  sock = luaL_checkudata(L,1,NET_SOCK);
  lua_pushfstring(L,"SOCK:%d",sock->fh);
  return 1;
}

/*********************************************************************
*
*	addr,err = sock:peer(sock | integer)
*
*	sock = net.socket(...)
********************************************************************/

static int socklua_peer(lua_State *const L)
{
  sockaddr_all__t *addr;
  socklen_t        len;
  int              s;
  
  if (lua_isuserdata(L,1))
  {
    sock__t *sock = luaL_checkudata(L,1,NET_SOCK);
    s = sock->fh;
  }
  else
    s = lua_tointeger(L,1);

  len  = sizeof(sockaddr_all__t);
  addr = lua_newuserdata(L,sizeof(sockaddr_all__t));
  
  if (getpeername(s,&addr->sa,&len) < 0)
  {
    int err = errno;
    lua_pushnil(L);
    lua_pushinteger(L,err);
    return 2;
  }
  
  luaL_getmetatable(L,NET_ADDR);
  lua_setmetatable(L,-2);
  lua_pushinteger(L,0);
  return 2;
}

/***********************************************************************
*
*	err = sock:bind(addr)
*
*	sock = net.socket(...)
*	addr = net.address(...)
*
***********************************************************************/

static int socklua_bind(lua_State *const L)
{
  sockaddr_all__t *addr;
  sock__t         *sock;
  
  sock = luaL_checkudata(L,1,NET_SOCK);
  addr = luaL_checkudata(L,2,NET_ADDR);
  
  if (bind(sock->fh,&addr->sa,Inet_len(addr)) < 0)
    lua_pushinteger(L,errno);
  else
    lua_pushinteger(L,0);
  return 1;
}

/**********************************************************************
*
*	err = sock:connect(addr)
*
*	sock = net.socket(...)
*	addr = net.address(...)
*
**********************************************************************/

static int socklua_connect(lua_State *const L)
{
  sockaddr_all__t *addr;
  sock__t         *sock;
  
  sock = luaL_checkudata(L,1,NET_SOCK);
  addr = luaL_checkudata(L,2,NET_ADDR);
  
  if (connect(sock->fh,&addr->sa,Inet_len(addr)) < 0)
    lua_pushinteger(L,errno);
  else
    lua_pushinteger(L,0);
  return 1;
}

/**********************************************************************
*
*	err = sock:listen([backlog = 5])
*
*	sock = net.socket(...)
*
**********************************************************************/

static int socklua_listen(lua_State *const L)
{
  sock__t *sock;
  int      backlog;
  
  sock    = luaL_checkudata(L,1,NET_SOCK);
  backlog = luaL_optint(L,2,5);
  
  if (listen(sock->fh,backlog) < 0)
    lua_pushinteger(L,errno);
  else
    lua_pushinteger(L,0);
  return 1;
}

/**********************************************************************
*
*	newsock,addr,err = sock:accept()
*	
*	sock = net.socket(...)
*
***********************************************************************/

static int socklua_accept(lua_State *const L)
{
  sockaddr_all__t *remote;
  socklen_t        remsize;
  sock__t         *sock;
  sock__t         *newsock;
  
  sock = luaL_checkudata(L,1,NET_SOCK);
  
  newsock = lua_newuserdata(L,sizeof(sock__t));
  luaL_getmetatable(L,NET_SOCK);
  lua_setmetatable(L,-2);
  
  remsize = sizeof(sockaddr_all__t);
  remote  = lua_newuserdata(L,sizeof(sockaddr_all__t));
  luaL_getmetatable(L,NET_ADDR);
  lua_setmetatable(L,-2);
  
  newsock->fh = accept(sock->fh,&remote->sa,&remsize);
  if (newsock->fh == -1)
  {
    int err = errno;
    lua_pushnil(L);
    lua_pushnil(L);
    lua_pushinteger(L,err);
    return 3;
  }
  
  lua_pushinteger(L,0);
  return 3;
}

/**********************************************************************
*
*	bool,err = sock:reuse()
*
*	sock = net.socket(...)
*
**********************************************************************/

static int socklua_reuse(lua_State *const L)
{
  sock__t *sock;
  int      reuse = 1;
  
  sock = luaL_checkudata(L,1,NET_SOCK);
  if (setsockopt(sock->fh,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(int)) < 0)
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

/***********************************************************************
*
*	remaddr,data,err = sock:read([timeout = inf])
*
*	sock    = net.socket(...)
*	timeout = number (in seconds, -1 = inf)
*	err     = number
*
**********************************************************************/

static int socklua_read(lua_State *const L)
{
  sockaddr_all__t *remaddr;
  socklen_t        remsize;
  sock__t         *sock;
  struct pollfd    fdlist;
  char             buffer[65535uL];
  ssize_t          bytes;
  int              timeout;
  int              rc;
  
  sock          = luaL_checkudata(L,1,NET_SOCK);
  fdlist.events = POLLIN;
  fdlist.fd     = sock->fh;
  
  if (lua_isnoneornil(L,2))
    timeout = -1;
  else
    timeout = (int)(lua_tonumber(L,2) * 1000.0);
  
  rc = poll(&fdlist,1,timeout);
  if (rc < 1)
  {
    int err = (rc == 0) ? ETIMEDOUT : errno;
    lua_pushnil(L);
    lua_pushnil(L);
    lua_pushinteger(L,err);
    return 3;
  }
  
  remaddr = lua_newuserdata(L,sizeof(sockaddr_all__t));
  remsize = sizeof(sockaddr_all__t);
  luaL_getmetatable(L,NET_ADDR);
  lua_setmetatable(L,-2);
  
  bytes = recvfrom(fdlist.fd,buffer,sizeof(buffer),0,&remaddr->sa,&remsize);
  if (bytes < 0)
  {
    int err = errno;
    lua_pushnil(L);
    lua_pushnil(L);
    lua_pushinteger(L,err);
    return 3;
  }
  
  lua_pushlstring(L,buffer,bytes);
  lua_pushinteger(L,0);
  return 3;
}

/*************************************************************************
*
*	numbytes,err = sock:write(addr,data)
*
*	sock = net.socket(...)
*	addr = net.address(...)
*	data = string
*
***********************************************************************/

static int socklua_write(lua_State *const L)
{
  sockaddr_all__t *remote;
  sock__t         *sock;
  const char      *buffer;
  size_t           bufsiz;
  ssize_t          bytes;
  
  sock   = luaL_checkudata(L,1,NET_SOCK);
  remote = luaL_checkudata(L,2,NET_ADDR);
  buffer = luaL_checklstring(L,3,&bufsiz);  
  bytes  = sendto(sock->fh,buffer,bufsiz,0,&remote->sa,Inet_len(remote));
  if (bytes < 0)
  {
    int err = errno;
    lua_pushinteger(L,-1);
    lua_pushinteger(L,err);
    return 2;
  }
  
  lua_pushinteger(L,bytes);
  lua_pushinteger(L,0);
  return 2;
}
  
/**********************************************************************
*
*	err = sock:shutdown([how = "rw"])
*
*	sock = net.socket(...)
*	how  = "r" (close read) | "w" (close write) | "rw" (close both)
*
**********************************************************************/

static int socklua_shutdown(lua_State *const L)
{
  static const char *const opts[] = { "r" , "w" , "rw" };
  sock__t *sock;
  
  sock = luaL_checkudata(L,1,NET_SOCK);
  if (shutdown(sock->fh,luaL_checkoption(L,2,"rw",opts)) < 0)
    lua_pushinteger(L,errno);
  else
    lua_pushinteger(L,0);
  return 1;
}

/*******************************************************************
*
*	err = sock:close()
*
*	sock = net.socket(...)
*
*******************************************************************/

static int socklua_close(lua_State *const L)
{
  sock__t *sock = luaL_checkudata(L,1,NET_SOCK);

  if (sock->fh != -1)
  {
    if (close(sock->fh) < 0)
      lua_pushinteger(L,errno);
    else
      lua_pushinteger(L,0);
    sock->fh = -1;
  }
  else
    lua_pushinteger(L,0);

  return 1;
}

/*******************************************************************/

static int socklua_fd(lua_State *const L)
{
  sock__t *sock;
  
  sock = luaL_checkudata(L,1,NET_SOCK);
  lua_pushinteger(L,sock->fh);
  return 1;
}

/***********************************************************************/

static int addrlua___index(lua_State *const L)
{
  sockaddr_all__t *addr;
  const char      *sidx;
  
  addr = luaL_checkudata(L,1,NET_ADDR);
  if (!lua_isstring(L,2))
  {
    lua_pushnil(L);
    return 1;
  }
  
  if (!lua_isstring(L,2))
  {
    lua_pushnil(L);
    return 1;
  }
  
  sidx = lua_tostring(L,2);
  if (strcmp(sidx,"addr") == 0)
  {
    const char *p;
    char        taddr[INET6_ADDRSTRLEN];
    
    p = Inet_addr(addr,taddr);
    if (p == NULL)
      lua_pushnil(L);
    else
      lua_pushstring(L,p);
  }
  else if (strcmp(sidx,"port") == 0)
    lua_pushinteger(L,Inet_port(addr));
  else if (strcmp(sidx,"family") == 0)
  {
    switch(addr->sa.sa_family)
    {
      case AF_INET:  lua_pushliteral(L,"ip");   break;
      case AF_INET6: lua_pushliteral(L,"ip6");  break;
      case AF_UNIX:  lua_pushliteral(L,"unix"); break;
      default: assert(0); lua_pushnil(L); break;
    }   
  }
  else
    lua_pushnil(L);
  
  return 1;
}

/***********************************************************************/

static int addrlua___tostring(lua_State *const L)
{
  sockaddr_all__t *addr;
  char             taddr[INET6_ADDRSTRLEN];
  
  addr = luaL_checkudata(L,1,NET_ADDR);
  switch(addr->sa.sa_family)
  {
    case AF_INET:
         lua_pushfstring(L,"ip:%s:%d",Inet_addr(addr,taddr),Inet_port(addr));
         break;
    case AF_INET6:
         lua_pushfstring(L,"ip6:%s:%d",Inet_addr(addr,taddr),Inet_port(addr));
         break;
    case AF_UNIX:
         lua_pushfstring(L,"unix:%s",Inet_addr(addr,taddr));
         break;
    default:
         assert(0);
         lua_pushstring(L,"unknown:");
  }
  return 1;
}

/**********************************************************************/

static int addrlua___eq(lua_State *const L)
{
  sockaddr_all__t *a;
  sockaddr_all__t *b;
  
  a = luaL_checkudata(L,1,NET_ADDR);
  b = luaL_checkudata(L,2,NET_ADDR);
  
  if (a->sa.sa_family != b->sa.sa_family)
  {
    lua_pushboolean(L,false);
    return 1;
  }
  
  if (memcmp(Inet_address(a),Inet_address(b),Inet_addrlen(a)) != 0)
  {
    lua_pushboolean(L,false);
    return 1;
  }
  
  lua_pushboolean(L,Inet_port(a) == Inet_port(b));
  return 1;
}

/**********************************************************************/

static int addrlua___lt(lua_State *const L)
{
  sockaddr_all__t *a;
  sockaddr_all__t *b;
  
  a = luaL_checkudata(L,1,NET_ADDR);
  b = luaL_checkudata(L,2,NET_ADDR);
  
  if (a->sa.sa_family < b->sa.sa_family)
  {
    lua_pushboolean(L,true);
    return 1;
  }
  
  if (memcmp(Inet_address(a),Inet_address(b),Inet_addrlen(a)) < 0)
  {
    lua_pushboolean(L,true);
    return 1;
  }
  
  lua_pushboolean(L,Inet_port(a) < Inet_port(b));
  return 1;
}

/**********************************************************************/

static int addrlua___le(lua_State *const L)
{
  sockaddr_all__t *a;
  sockaddr_all__t *b;
  
  a = luaL_checkudata(L,1,NET_ADDR);
  b = luaL_checkudata(L,2,NET_ADDR);
  
  if (a->sa.sa_family <= b->sa.sa_family)
  {
    lua_pushboolean(L,true);
    return 1;
  }
  
  if (memcmp(Inet_address(a),Inet_address(b),Inet_addrlen(a)) <= 0)
  {
    lua_pushboolean(L,true);
    return 1;
  }
  
  lua_pushboolean(L,Inet_port(a) <= Inet_port(b));
  return 1;
}
    
/**********************************************************************/

static int addrlua___len(lua_State *const L)
{
  lua_pushinteger(L,Inet_addrlen(luaL_checkudata(L,1,NET_ADDR)));
  return 1;
}

/*********************************************************************/
  
int luaopen_org_conman_net(lua_State *const L)
{
  luaL_newmetatable(L,NET_SOCK);
  luaL_register(L,NULL,msock_regmeta);
  lua_pushvalue(L,-1);
  lua_setfield(L,-2,"__index");
  
  luaL_newmetatable(L,NET_ADDR);
  luaL_register(L,NULL,maddr_regmeta);
  
  luaL_register(L,"org.conman.net",mnet_reg);
  return 1;
}

/************************************************************************/
