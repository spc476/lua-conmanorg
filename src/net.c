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

#ifndef __GNU__
#  define __attribute__(x)
#endif

#ifdef __linux
#  define _BSD_SOURCE
#  define _POSIX_SOURCE
#  define _FORTIFY_SOURCE 0
#endif

#include <signal.h>
#include <math.h>
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
#include <netinet/tcp.h>
#include <sys/un.h>
#include <sys/poll.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#define TYPE_SOCK	"org.conman.net:sock"
#define TYPE_ADDR	"org.conman.net:addr"
#define TYPE_POLL	"org.conman.net:poll"

#ifdef __linux
#  define IMPL_POLL	"epoll"
#else
#  define IMPL_POLL	"poll"
#endif

#ifdef __SunOS
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

struct strint
{
  const char *const text;
  const int         value;
};

/************************************************************************/

static int	err_meta___index	(lua_State *const) __attribute__((nonnull));

static int	netlua_interfaces	(lua_State *const) __attribute__((nonnull));
static int	netlua_socket		(lua_State *const) __attribute__((nonnull));
static int	netlua_socketfile	(lua_State *const) __attribute__((nonnull));
static int	netlua_address2		(lua_State *const) __attribute__((nonnull));
static int	netlua_address		(lua_State *const) __attribute__((nonnull));
static int	netlua_pollset		(lua_State *const) __attribute__((nonnull));

static int	socklua___tostring	(lua_State *const) __attribute__((nonnull));
static int	socklua___index		(lua_State *const) __attribute__((nonnull));
static int	socklua___newindex	(lua_State *const) __attribute__((nonnull));
static int      socklua_peer		(lua_State *const) __attribute__((nonnull));
static int	socklua_addr		(lua_State *const) __attribute__((nonnull));
static int	socklua_bind		(lua_State *const) __attribute__((nonnull));
static int	socklua_connect		(lua_State *const) __attribute__((nonnull));
static int	socklua_listen		(lua_State *const) __attribute__((nonnull));
static int	socklua_accept		(lua_State *const) __attribute__((nonnull));
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

static int	polllua___tostring	(lua_State *const) __attribute__((nonnull));
static int	polllua___gc		(lua_State *const) __attribute__((nonnull));
static int	polllua_insert		(lua_State *const) __attribute__((nonnull));
static int	polllua_update		(lua_State *const) __attribute__((nonnull));
static int	polllua_remove		(lua_State *const) __attribute__((nonnull));
static int	polllua_events		(lua_State *const) __attribute__((nonnull));

/*************************************************************************/

static const luaL_Reg m_net_reg[] =
{
  { "interfaces"	, netlua_interfaces	} ,
  { "socket"		, netlua_socket		} ,
  { "socketfile"	, netlua_socketfile	} ,
  { "address2"		, netlua_address2	} ,
  { "address"		, netlua_address	} ,
  { "peer"		, socklua_peer		} ,
  { "pollset"		, netlua_pollset	} ,
  { NULL		, NULL			}
};

static const luaL_Reg m_sock_meta[] =
{
  { "__tostring"	, socklua___tostring	} ,
  { "__gc"		, socklua_close		} ,
  { "__index"		, socklua___index	} ,
  { "__newindex"	, socklua___newindex	} ,
  { "peer"		, socklua_peer		} ,
  { "addr"		, socklua_addr		} ,
  { "bind"		, socklua_bind		} ,
  { "connect"		, socklua_connect	} ,
  { "listen"		, socklua_listen	} ,
  { "accept"		, socklua_accept	} ,
  { "read"		, socklua_read		} ,
  { "write"		, socklua_write		} ,
  { "shutdown"		, socklua_shutdown	} ,
  { "close"		, socklua_close		} ,
  { "fd"		, socklua_fd		} ,
  { NULL		, NULL			}
};

static const luaL_Reg m_addr_meta[] =
{
  { "__index"		, addrlua___index	} ,
  { "__tostring"	, addrlua___tostring	} ,
  { "__eq"		, addrlua___eq		} ,
  { "__lt"		, addrlua___lt		} ,
  { "__le"		, addrlua___le		} ,
  { "__len"		, addrlua___len		} ,
  { NULL		, NULL			}
};

static const luaL_Reg m_polllua[] =
{
  { "__tostring"	, polllua___tostring	} ,
  { "__gc"		, polllua___gc		} ,
  { "insert"		, polllua_insert	} ,
  { "update"		, polllua_update	} ,
  { "remove"		, polllua_remove	} ,
  { "events"		, polllua_events	} ,
  { NULL		, NULL			}
};

static const char *const   m_netfamilytext[] = { "ip"    , "ip6"    , "unix" , NULL };
static const int           m_netfamily[]     = { AF_INET , AF_INET6 , AF_UNIX };
static const struct strint m_errors[] =
{
  { "EAI_BADFLAGS"	, EAI_BADFLAGS		} ,
  { "EAI_NONAME"	, EAI_NONAME		} ,
  { "EAI_AGAIN"		, EAI_AGAIN		} ,
  { "EAI_FAIL"		, EAI_FAIL		} ,
#ifdef EAI_NODATA
  { "EAI_NODATA"	, EAI_NODATA		} ,
#endif
  { "EAI_FAMILY"	, EAI_FAMILY		} ,
  { "EAI_SOCKTYPE"	, EAI_SOCKTYPE		} ,
  { "EAI_SERVICE"	, EAI_SERVICE		} ,
#ifdef EAI_ADDRFAMILY
  { "EAI_ADDRFAMILY"	, EAI_ADDRFAMILY	} ,
#endif
  { "EAI_MEMORY"	, EAI_MEMORY		} ,
  { "EAI_SYSTEM"	, EAI_SYSTEM		} ,
  { "EAI_OVERFLOW"	, EAI_OVERFLOW		} ,
  { NULL		, 0			}
};

/************************************************************************/

static inline size_t	  Inet_addrlen	(sockaddr_all__t *const)                               __attribute__((nonnull));
static inline socklen_t   Inet_len    	(sockaddr_all__t *const)                               __attribute__((nonnull));
static inline socklen_t   Inet_lensa	(struct sockaddr *const)                               __attribute__((nonnull));
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

static inline socklen_t Inet_lensa(struct sockaddr *const addr)
{
  assert(addr != NULL);
  switch(addr->sa_family)
  {
    case AF_INET:  return sizeof(struct sockaddr_in);
    case AF_INET6: return sizeof(struct sockaddr_in6);
    case AF_UNIX:  return sizeof(struct sockaddr_un);
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

/**********************************************************************/

static int err_meta___index(lua_State *const L)
{
  int err = luaL_checkinteger(L,2);
  if (err < 0)
    lua_pushstring(L,gai_strerror(err));
  else
    lua_pushstring(L,strerror(err));
  return 1;
}

/**********************************************************************
*
*	list,err = net.interfaces()
*
**********************************************************************/

static int netlua_interfaces(lua_State *const L)
{
  struct ifaddrs  *interfaces;
  struct ifaddrs  *i;
  sockaddr_all__t *addr;
  
  lua_settop(L,0);
  
  if (getifaddrs(&interfaces) < 0)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    return 2;
  }
  
  lua_createtable(L,0,0);
  
  for (i = interfaces ; i != NULL ; i = i->ifa_next)
  {
    if (
            (i->ifa_addr->sa_family != AF_INET)
         && (i->ifa_addr->sa_family != AF_INET6)
       )
      continue;
    
    lua_getfield(L,-1,i->ifa_name);
    if (lua_isnil(L,-1))
    {
      lua_pop(L,1);
      lua_createtable(L,0,0);
      lua_pushvalue(L,-1);
      lua_setfield(L,-3,i->ifa_name);
    }
    
    lua_pushinteger(L,lua_objlen(L,-1) + 1);
    lua_createtable(L,0,0);
    
    addr = lua_newuserdata(L,sizeof(sockaddr_all__t));
    memcpy(&addr->sa,i->ifa_addr,Inet_lensa(i->ifa_addr));
    luaL_getmetatable(L,TYPE_ADDR);
    lua_setmetatable(L,-2);
    lua_setfield(L,-2,"addr");
    
    addr = lua_newuserdata(L,sizeof(sockaddr_all__t));
    memcpy(&addr->sa,i->ifa_netmask,Inet_lensa(i->ifa_netmask));
    luaL_getmetatable(L,TYPE_ADDR);
    lua_setmetatable(L,-2);
    lua_setfield(L,-2,"mask");
    
    if ((i->ifa_flags & IFF_BROADCAST) == IFF_BROADCAST)
    {
      if (i->ifa_ifu.ifu_broadaddr != NULL)
      {
        addr = lua_newuserdata(L,sizeof(sockaddr_all__t));
        memcpy(&addr->sa,i->ifa_ifu.ifu_broadaddr,Inet_lensa(i->ifa_ifu.ifu_broadaddr));
        luaL_getmetatable(L,TYPE_ADDR);
        lua_setmetatable(L,-2);
        lua_setfield(L,-2,"broadcast");
      }
    }
    else if ((i->ifa_flags & IFF_POINTOPOINT) == IFF_POINTOPOINT)
    {
      if (i->ifa_ifu.ifu_dstaddr != NULL)
      {
        addr = lua_newuserdata(L,sizeof(sockaddr_all__t));
        memcpy(&addr->sa,i->ifa_ifu.ifu_dstaddr,Inet_lensa(i->ifa_ifu.ifu_dstaddr));
        luaL_getmetatable(L,TYPE_ADDR);
        lua_setmetatable(L,-2);
        lua_setfield(L,-2,"destaddr");
      }
    }
    
    lua_settable(L,-3);
    lua_pop(L,1);
  }
  
  freeifaddrs(interfaces);
  lua_pushinteger(L,0);
  return 2;
}

/**********************************************************************
*
*	sock,err = net.socket(family,proto)
*
*	family = 'ip'  | 'ip6' | 'unix'
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
  else
    return luaL_error(L,"invalid protocol");
  
  if (proto == IPPROTO_TCP)
    type = SOCK_STREAM;
  else if (proto == IPPROTO_UDP)
    type = SOCK_DGRAM;
  else
    type = SOCK_RAW;

  if (family == AF_UNIX)
    proto = 0;

  sock     = lua_newuserdata(L,sizeof(sock__t));
  sock->fh = socket(family,type,proto);
  if (sock->fh == -1)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
  }
  else
  {
    luaL_getmetatable(L,TYPE_SOCK);
    lua_setmetatable(L,-2);
    lua_pushinteger(L,0);
  }

  return 2;
}
  
/*******************************************************************
*
*	sock,err = net.socketfile(fp)
*
*	fp = io.open(...) or io.stdin or io.stdout or io.stderr
*
********************************************************************/

static int netlua_socketfile(lua_State *const L)
{
  FILE    **pfp  = luaL_checkudata(L,1,LUA_FILEHANDLE);
  sock__t  *sock = lua_newuserdata(L,sizeof(sock__t));
  
  sock->fh = fileno(*pfp);  
  luaL_getmetatable(L,TYPE_SOCK);
  lua_setmetatable(L,-2);
  lua_pushinteger(L,0);

  return 2;
}

/***********************************************************************
* 
* XXX Experimental
*
***********************************************************************/

static int netlua_address2(lua_State *const L)
{
  struct addrinfo  hints;
  struct addrinfo *results;
  const char      *hostname;
  const char      *family;
  int              protocol;
  const char      *port;
  int              rc;
  
  lua_settop(L,4);
  memset(&hints,0,sizeof(hints));
  results = NULL;
  
  hostname = luaL_checkstring(L,1);
  
  /*------------------------------------------------
  ; set the address family, this can be
  ;
  ;	ip
  ;	ip6
  ;	any
  ;------------------------------------------------*/
  
  family = luaL_optstring(L,2,"any");

  if (strcmp(family,"any") == 0)
    hints.ai_family = AF_UNSPEC;
  else if (strcmp(family,"ip") == 0)
    hints.ai_family = AF_INET;
  else if (strcmp(family,"ip6") == 0)
    hints.ai_family = AF_INET6;
  else
  {
    lua_pushnil(L);
    lua_pushinteger(L,EPROTONOSUPPORT);
    return 2;
  }
  
  /*--------------------------------------------
  ; set the protocol type, samples:
  ;
  ;	udp
  ;	tcp
  ;---------------------------------------------*/
  
  if (lua_isnumber(L,3))
    protocol = lua_tointeger(L,3);
  else if (lua_isstring(L,3))
  {
    struct protoent *e = getprotobyname(lua_tostring(L,3));
    if (e == NULL)
    {
      lua_pushnil(L);
      lua_pushinteger(L,ENOPROTOOPT);
      return 2;
    }
    protocol = e->p_proto;
  }
  else
    protocol = 0;
  
  if (protocol == IPPROTO_TCP)
    hints.ai_socktype = SOCK_STREAM;
  else if (protocol == IPPROTO_UDP)
    hints.ai_socktype = SOCK_DGRAM;
  else if (protocol != 0)
    hints.ai_socktype = SOCK_RAW;
    
  /*-------------------------------------------------
  ; now set the port (or service), examples:
  ;
  ;	finger
  ;	www
  ;	smtp
  ;-------------------------------------------------*/
  
  port  = lua_tostring(L,4);
  errno = 0;
  rc    = getaddrinfo(hostname,port,&hints,&results);
  
  if (rc != 0)
  {
    lua_pushnil(L);
    lua_pushinteger(L,(rc == EAI_SYSTEM) ? errno : rc);
    freeaddrinfo(results);
    return 2;
  }
  
  lua_createtable(L,0,0);
  
  if (results == NULL)
  {
    lua_pushinteger(L,0);
    return 2;
  }
  
  /*-----------------------------------------------------------------
  ; getaddrinfo() returns addresses that can use used by bind(), connect(),
  ; etc.  If a protocol isn't specified, it will return multiple addresses
  ; (say, one for TCP, one for UDP, one just an address) when really we only
  ; need one.  To determine which ne need, if port is not NULL but protocol
  ; == 0, then set protocol = IPROTO_TCP.  This will filter out the results
  ;-----------------------------------------------------------------------*/
  
  if ((port != NULL) && (protocol == 0))
    protocol = IPPROTO_TCP;
  
  for (int i = 1 ; results != NULL ; results = results->ai_next)
  {
    if (results->ai_protocol == protocol)
    {
      lua_pushinteger(L,i);
      sockaddr_all__t *addr = lua_newuserdata(L,sizeof(sockaddr_all__t));
      luaL_getmetatable(L,TYPE_ADDR);
      lua_setmetatable(L,-2);
      memcpy(&addr->sa,results->ai_addr,sizeof(struct sockaddr));
      lua_settable(L,-3);
      i++;
    }
  }
  
  freeaddrinfo(results);
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
    luaL_getmetatable(L,TYPE_ADDR);
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
        lua_pushnil(L);
        lua_pushinteger(L,errno);
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
      luaL_getmetatable(L,TYPE_ADDR);
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
      lua_pushnil(L);
      lua_pushinteger(L,errno);
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
  
  luaL_getmetatable(L,TYPE_ADDR);
  lua_setmetatable(L,-2);
  lua_pushinteger(L,0);
  return 2;
}

/***********************************************************************/

static int socklua___tostring(lua_State *const L)
{
  sock__t *sock;
  
  sock = luaL_checkudata(L,1,TYPE_SOCK);
  lua_pushfstring(L,"SOCK:%d",sock->fh);
  return 1;
}

/***********************************************************************/

typedef enum sopt
{
  SOPT_FLAG,
  SOPT_INT,
  SOPT_LINGER,
  SOPT_TIMEVAL,
  SOPT_FCNTL,
} sopt__t;

struct sockoptions
{
  const char *const name;
  const int         level;
  const int         setlevel;
  const int         option;
  const sopt__t     type;
  const bool	    get;
  const bool        set;
};

static const struct sockoptions m_sockoptions[] = 
{
  { "broadcast"		, SOL_SOCKET 	, 0		, SO_BROADCAST 		, SOPT_FLAG 	, true , true  } ,
#ifdef FD_CLOEXEC
  { "closeexec"		, F_GETFD	, F_SETFD	, FD_CLOEXEC		, SOPT_FCNTL	, true , true  } ,
#endif
  { "debug"		, SOL_SOCKET 	, 0		, SO_DEBUG		, SOPT_FLAG 	, true , true  } ,
  { "dontroute"		, SOL_SOCKET 	, 0		, SO_DONTROUTE		, SOPT_FLAG 	, true , true  } ,
  { "error"		, SOL_SOCKET 	, 0		, SO_ERROR		, SOPT_INT  	, true , false } ,
  { "keepalive"		, SOL_SOCKET 	, 0		, SO_KEEPALIVE		, SOPT_FLAG 	, true , true  } ,
  { "linger"		, SOL_SOCKET 	, 0		, SO_LINGER		, SOPT_LINGER	, true , true  } ,
  { "maxsegment"	, IPPROTO_TCP	, 0		, TCP_MAXSEG		, SOPT_INT	, true , true  } ,
  { "nodelay"		, IPPROTO_TCP	, 0		, TCP_NODELAY		, SOPT_FLAG	, true , true  } ,
  { "nonblock"		, F_GETFL	, F_SETFL	, O_NONBLOCK		, SOPT_FCNTL	, true , true  } ,
#ifdef SO_NOSIGPIPE
  { "nosigpipe"		, SOL_SOCKET	, 0		, SO_NOSIGPIPE		, SOPT_FLAG	, true , true  } ,
#endif
  { "oobinline"		, SOL_SOCKET 	, 0		, SO_OOBINLINE		, SOPT_FLAG	, true , true  } ,
  { "recvbuffer"	, SOL_SOCKET 	, 0		, SO_RCVBUF		, SOPT_INT	, true , true  } ,
  { "recvlow"		, SOL_SOCKET 	, 0		, SO_RCVLOWAT		, SOPT_INT	, true , true  } ,
  { "recvtimeout"	, SOL_SOCKET 	, 0		, SO_RCVTIMEO		, SOPT_INT	, true , true  } ,
  { "reuseaddr"		, SOL_SOCKET 	, 0		, SO_REUSEADDR		, SOPT_FLAG	, true , true  } ,
#ifdef SO_REUSEPORT  
  { "reuseport"		, SOL_SOCKET 	, 0		, SO_REUSEPORT		, SOPT_FLAG	, true , true  } ,
#endif
  { "sendbuffer"	, SOL_SOCKET 	, 0		, SO_SNDBUF		, SOPT_INT	, true , true  } ,
  { "sendlow"		, SOL_SOCKET 	, 0		, SO_SNDLOWAT		, SOPT_INT	, true , true  } ,
  { "sendtimeout"	, SOL_SOCKET 	, 0		, SO_SNDTIMEO		, SOPT_INT	, true , true  } ,
  { "type"		, SOL_SOCKET 	, 0		, SO_TYPE		, SOPT_INT	, true , false } ,
#ifdef SO_USELOOPBACK
  { "useloopback"	, SOL_SOCKET 	, 0		, SO_USELOOPBACK	, SOPT_FLAG	, true , true  } ,
#endif
};

#define MAX_SOPTS	(sizeof(m_sockoptions) / sizeof(struct sockoptions))

/*********************************************************************/

static int sopt_compare(const void *needle,const void *haystack)
{
  const char               *const key   = needle;
  const struct sockoptions *const value = haystack;
  
  return strcmp(key,value->name);
}

/*********************************************************************/

static int socklua___index(lua_State *const L)
{
  sock__t                  *sock;
  const char               *tkey;
  const struct sockoptions *value;
  int                       ivalue;
  struct timeval            tvalue;
  struct linger             lvalue;
  double                    dvalue;
  socklen_t                 len;
  
  sock  = luaL_checkudata(L,1,TYPE_SOCK);
  tkey  = luaL_checkstring(L,2);
  value = bsearch(tkey,m_sockoptions,MAX_SOPTS,sizeof(struct sockoptions),sopt_compare);

  if (value == NULL)
  {
    lua_getmetatable(L,1);
    lua_pushvalue(L,2);
    lua_gettable(L,-2);
    return 1;
  }
  
  if (!value->get)
  {
    lua_pushnil(L);
    return 1;
  }
  
  switch(value->type)
  {
    case SOPT_FLAG:
         len = sizeof(ivalue);
         if (getsockopt(sock->fh,value->level,value->option,&ivalue,&len) < 0)
           lua_pushboolean(L,false);
         else
           lua_pushboolean(L,ivalue);
         break;
         
    case SOPT_INT:
         len = sizeof(ivalue);
         if (getsockopt(sock->fh,value->level,value->option,&ivalue,&len) < 0)
           lua_pushinteger(L,-1);
         else
           lua_pushinteger(L,ivalue);
         break;
    
    case SOPT_LINGER:
         len = sizeof(lvalue);
         if (getsockopt(sock->fh,value->level,value->option,&lvalue,&len) < 0)
         {
           lua_pushnil(L);
           return 1;
         }
         
         lua_createtable(L,2,0);
         lua_pushboolean(L,lvalue.l_onoff);
         lua_setfield(L,-2,"on");
         lua_pushinteger(L,lvalue.l_linger);
         lua_setfield(L,-2,"linger");
         break;

    case SOPT_TIMEVAL:
         len = sizeof(tvalue);
         if (getsockopt(sock->fh,value->level,value->option,&tvalue,&len) < 0)
           lua_pushnumber(L,-1);
         else
         {
           dvalue = (double)tvalue.tv_sec
                  + ((double)tvalue.tv_usec / 1000000.0);
           lua_pushnumber(L,dvalue);
         }
         break;

    case SOPT_FCNTL:
         ivalue = fcntl(sock->fh,value->level,0);
         if (ivalue == -1)
           lua_pushboolean(L,false);
         else
           lua_pushboolean(L,(ivalue & value->option) == value->option);
         break;
         
    default:
         assert(0);
         return luaL_error(L,"internal error");
  }
  return 1;
}

/***********************************************************************/

static int socklua___newindex(lua_State *const L)
{
  sock__t                  *sock;
  const char               *tkey;
  const struct sockoptions *value;
  int                       ivalue;
  struct timeval            tvalue;
  struct linger             lvalue;
  double                    dvalue;
  double                    seconds;
  double                    fract;
  
  sock  = luaL_checkudata(L,1,TYPE_SOCK);
  tkey  = luaL_checkstring(L,2);
  value = bsearch(tkey,m_sockoptions,MAX_SOPTS,sizeof(struct sockoptions),sopt_compare);
  
  if (value == NULL)
    return 0;
  
  if (!value->set)
    return 0;
  
  switch(value->type)
  {
    case SOPT_FLAG:
         ivalue = lua_toboolean(L,3);
         if (setsockopt(sock->fh,value->level,value->option,&ivalue,sizeof(ivalue)) < 0)
           syslog(LOG_ERR,"setsockopt() = %s",strerror(errno));
         break;
         
    case SOPT_INT:
         ivalue = lua_tointeger(L,3);
         if (setsockopt(sock->fh,value->level,value->option,&ivalue,sizeof(ivalue)) < 0)
           syslog(LOG_ERR,"setsockopt() = %s",strerror(errno));
         break;
         
    case SOPT_LINGER:
         luaL_checktype(L,3,LUA_TTABLE);
         lua_getfield(L,3,"on");
         lua_getfield(L,3,"linger");
         
         lvalue.l_onoff = lua_toboolean(L,-2);
         lvalue.l_linger = lua_tointeger(L,-1);
         if (setsockopt(sock->fh,value->level,value->option,&lvalue,sizeof(lvalue)) < 0)
           syslog(LOG_ERR,"setsockopt() = %s",strerror(errno));
         break;
    
    case SOPT_TIMEVAL:
         dvalue         = lua_tonumber(L,3);
         fract          = modf(dvalue,&seconds);
         tvalue.tv_sec  = (time_t)seconds;
         tvalue.tv_usec = (long)(fract * 1000000.0);
         if (setsockopt(sock->fh,value->level,value->option,&tvalue,sizeof(tvalue)) < 0)
           syslog(LOG_ERR,"setsockopt() = %s",strerror(errno));
         break;
    
    case SOPT_FCNTL:
         ivalue = fcntl(sock->fh,value->level,0);
         if (ivalue > 0)
         {
           if (lua_toboolean(L,3))
             ivalue |= value->option;
           else
             ivalue &= ~value->option;
             
           if (fcntl(sock->fh,value->setlevel,ivalue) < 0)
             syslog(LOG_ERR,"fcntl() = %s",strerror(errno));
         }
         else
           syslog(LOG_ERR,"fcntl() = %s",strerror(errno));
         break;
         
    default:
         assert(0);
         return luaL_error(L,"internal error");
  }
  
  return 0;
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
    sock__t *sock = luaL_checkudata(L,1,TYPE_SOCK);
    s = sock->fh;
  }
  else
    s = lua_tointeger(L,1);

  len  = sizeof(sockaddr_all__t);
  addr = lua_newuserdata(L,sizeof(sockaddr_all__t));
  
  if (getpeername(s,&addr->sa,&len) < 0)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    return 2;
  }
  
  luaL_getmetatable(L,TYPE_ADDR);
  lua_setmetatable(L,-2);
  lua_pushinteger(L,0);
  return 2;
}

/*********************************************************************
*
*	addr,err = sock:addr(sock | integer)
*
*	sock = net.socket(...)
********************************************************************/

static int socklua_addr(lua_State *const L)
{
  sockaddr_all__t *addr;
  socklen_t        len;
  int              s;
  
  if (lua_isuserdata(L,1))
  {
    sock__t *sock = luaL_checkudata(L,1,TYPE_SOCK);
    s = sock->fh;
  }
  else
    s = lua_tointeger(L,1);
    
  len  = sizeof(sockaddr_all__t);
  addr = lua_newuserdata(L,sizeof(sockaddr_all__t));
  
  if (getsockname(s,&addr->sa,&len) < 0)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    return 2;
  }
  
  luaL_getmetatable(L,TYPE_ADDR);
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
  
  sock = luaL_checkudata(L,1,TYPE_SOCK);
  addr = luaL_checkudata(L,2,TYPE_ADDR);
  
  if (bind(sock->fh,&addr->sa,Inet_len(addr)) < 0)
    lua_pushinteger(L,errno);
  else
    lua_pushinteger(L,0);
  
  if (addr->sa.sa_family == AF_INET)
  {
    if (IN_MULTICAST(ntohl(addr->sin.sin_addr.s_addr)))
    {
      unsigned char  on = 0;
      struct ip_mreq mreq;
      
      if (setsockopt(sock->fh,IPPROTO_IP,IP_MULTICAST_LOOP,&on,1) < 0)
      {
        lua_pushinteger(L,errno);
        return 1;
      }
      
      mreq.imr_multiaddr        = addr->sin.sin_addr;
      mreq.imr_interface.s_addr = INADDR_ANY;
      if (setsockopt(sock->fh,IPPROTO_IP,IP_ADD_MEMBERSHIP,&mreq,sizeof(mreq)) < 0)
      {
        lua_pushinteger(L,errno);
        return 1;
      }
    }   
  }
  else if (addr->sa.sa_family == AF_INET6)
  {
    if (IN6_IS_ADDR_MULTICAST(&addr->sin6.sin6_addr))
    {
      unsigned int     on = 0;
      struct ipv6_mreq mreq6;
      
      if (setsockopt(sock->fh,IPPROTO_IPV6,IPV6_MULTICAST_LOOP,&on,sizeof(on)) < 0)
      {
        lua_pushinteger(L,errno);
        return 1;
      }
      
      mreq6.ipv6mr_multiaddr = addr->sin6.sin6_addr;
      mreq6.ipv6mr_interface = 0;
      if (setsockopt(sock->fh,IPPROTO_IPV6,IPV6_ADD_MEMBERSHIP,&mreq6,sizeof(mreq6)) < 0)
      {
        lua_pushinteger(L,errno);
        return 1;
      }      
    }
  }
  
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
  sock__t         *sock = luaL_checkudata(L,1,TYPE_ADDR);
  sockaddr_all__t *addr = luaL_checkudata(L,2,TYPE_ADDR);  

  errno = 0;
  connect(sock->fh,&addr->sa,Inet_len(addr));
  lua_pushinteger(L,errno);
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
  sock__t *sock = luaL_checkudata(L,1,TYPE_SOCK);
  
  errno = 0;
  listen(sock->fh,luaL_optint(L,2,5));
  lua_pushinteger(L,errno);
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
  
  sock = luaL_checkudata(L,1,TYPE_SOCK);
  
  newsock = lua_newuserdata(L,sizeof(sock__t));
  luaL_getmetatable(L,TYPE_SOCK);
  lua_setmetatable(L,-2);
  
  remsize = sizeof(sockaddr_all__t);
  remote  = lua_newuserdata(L,sizeof(sockaddr_all__t));
  luaL_getmetatable(L,TYPE_ADDR);
  lua_setmetatable(L,-2);
  
  newsock->fh = accept(sock->fh,&remote->sa,&remsize);
  if (newsock->fh == -1)
  {
    lua_pushnil(L);
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    return 3;
  }
  
  lua_pushinteger(L,0);
  return 3;
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
  
  sock          = luaL_checkudata(L,1,TYPE_SOCK);
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
  luaL_getmetatable(L,TYPE_ADDR);
  lua_setmetatable(L,-2);
  
  bytes = recvfrom(fdlist.fd,buffer,sizeof(buffer),0,&remaddr->sa,&remsize);
  if (bytes < 0)
  {
    lua_pushnil(L);
    lua_pushnil(L);
    lua_pushinteger(L,errno);
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
  struct sockaddr *remaddr;
  socklen_t        remsize;
  sock__t         *sock;
  const char      *buffer;
  size_t           bufsiz;
  ssize_t          bytes;
  
  sock   = luaL_checkudata(L,1,TYPE_SOCK);
  buffer = luaL_checklstring(L,3,&bufsiz);  
  
  /*--------------------------------------------------------------------
  ; sometimes, a connected socket (in my experience, a UNIX domain TCP
  ; socket) will return EISCONN if remote isn't NULL.  So, we *may* need
  ; to, in some cases, specify a nil parameter here.  
  ;---------------------------------------------------------------------*/
  
  if (lua_isnil(L,2))
  {
    remaddr = NULL;
    remsize = 0;
  }
  else
  {
    remote  = luaL_checkudata(L,2,TYPE_ADDR);
    remaddr = &remote->sa;
    remsize = Inet_len(remote);
  }
  
  bytes  = sendto(sock->fh,buffer,bufsiz,0,remaddr,remsize);
  if (bytes < 0)
  {
    lua_pushinteger(L,-1);
    lua_pushinteger(L,errno);
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
  sock__t *sock = luaL_checkudata(L,1,TYPE_SOCK);
  
  errno = 0;
  shutdown(sock->fh,luaL_checkoption(L,2,"rw",opts));
  lua_pushinteger(L,errno);
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
  sock__t *sock = luaL_checkudata(L,1,TYPE_SOCK);

  if (sock->fh != -1)
  {
    errno = 0;
    close(sock->fh);
    lua_pushinteger(L,errno);
    sock->fh = -1;
  }
  else
    lua_pushinteger(L,0);

  return 1;
}

/*******************************************************************/

static int socklua_fd(lua_State *const L)
{
  sock__t *sock = luaL_checkudata(L,1,TYPE_SOCK);
  lua_pushinteger(L,sock->fh);
  return 1;
}

/***********************************************************************/

static int addrlua___index(lua_State *const L)
{
  sockaddr_all__t *addr;
  const char      *sidx;
  
  addr = luaL_checkudata(L,1,TYPE_ADDR);
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
  
  addr = luaL_checkudata(L,1,TYPE_ADDR);
  switch(addr->sa.sa_family)
  {
    case AF_INET:
         lua_pushfstring(L,"ip:%s:%d",Inet_addr(addr,taddr),Inet_port(addr));
         break;
    case AF_INET6:
         lua_pushfstring(L,"ip6:[%s]:%d",Inet_addr(addr,taddr),Inet_port(addr));
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
  
  a = luaL_checkudata(L,1,TYPE_ADDR);
  b = luaL_checkudata(L,2,TYPE_ADDR);
  
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
  
  a = luaL_checkudata(L,1,TYPE_ADDR);
  b = luaL_checkudata(L,2,TYPE_ADDR);
  
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
  
  a = luaL_checkudata(L,1,TYPE_ADDR);
  b = luaL_checkudata(L,2,TYPE_ADDR);
  
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
  lua_pushinteger(L,Inet_addrlen(luaL_checkudata(L,1,TYPE_ADDR)));
  return 1;
}

/*********************************************************************/

int luaopen_org_conman_net(lua_State *const L)
{
  /*----------------------------------------------------------------------
  ; I unilaterally block SIGPIPE.  Why?  Because it's a bad idea.  Sure,
  ; it's fine if you only one have file descriptor where the other end can
  ; disconnect, but when you have more than one?  Can you say "Charlie
  ; Foxtrox"?
  ;
  ; By ignoring SIGPIPE, we ignore this whole mess.
  ;------------------------------------------------------------------------*/
  
  signal(SIGPIPE,SIG_IGN);
  
  luaL_newmetatable(L,TYPE_SOCK);
  luaL_register(L,NULL,m_sock_meta);

  luaL_newmetatable(L,TYPE_ADDR);
  luaL_register(L,NULL,m_addr_meta);
  
  luaL_newmetatable(L,TYPE_POLL);
  luaL_register(L,NULL,m_polllua);
  lua_pushvalue(L,-1);
  lua_setfield(L,-1,"__index");
  
  luaL_register(L,"org.conman.net",m_net_reg);
  lua_pushliteral(L,IMPL_POLL);
  lua_setfield(L,-2,"_POLL");
  
  lua_createtable(L,0,0);
  for (size_t i = 0 ; m_errors[i].text != NULL ; i++)
  {
    lua_pushinteger(L,m_errors[i].value);
    lua_setfield(L,-2,m_errors[i].text);
  }
  lua_createtable(L,0,1);
  lua_pushcfunction(L,err_meta___index);
  lua_setfield(L,-2,"__index");
  lua_setmetatable(L,-2);
  lua_setfield(L,-2,"errno");
  
  return 1;
}

/************************************************************************
*
* The various implementations of pollsets.  The reason they're here and not
* in org.conman.fsys is because they're mostly used for network based
* connections, and while you can use select()/poll()/epoll() for serial
* ports and ptys they aren't used that much any more, and more importantly,
* for my own usage patterns, I use more network connections.
*
* So they're here in this module.
*
* Also, each of the three implementations present the same API, but some
* like epoll() might have more functionality than others, like select() that
* can be used.  The org.conman.net module indicates which implementation is
* in use, but as long as you stick to select() type operations, you should
* be fine.
*
* LINUX implementation of pollset, using epoll()
*
*************************************************************************/

#ifdef __linux

#include <sys/epoll.h>

typedef struct
{
  int    efh;
  int    ref;
  size_t idx;
} pollset__t;

/**********************************************************************/

static int pollset_toevents(lua_State *const L,int idx)
{
  int events = 0;
  
  if (lua_istable(L,idx))
  {
    lua_getfield(L,idx,"read");
    events |= lua_isboolean(L,-1) ? EPOLLIN : 0;
    lua_getfield(L,idx,"write");
    events |= lua_isboolean(L,-1) ? EPOLLOUT : 0;
    lua_getfield(L,idx,"priority");
    events |= lua_isboolean(L,-1) ? EPOLLPRI : 0;
    lua_getfield(L,idx,"error");
    events |= lua_isboolean(L,-1) ? EPOLLERR : 0;
    lua_getfield(L,idx,"hangup");
    events |= lua_isboolean(L,-1) ? EPOLLHUP : 0;
    lua_getfield(L,idx,"trigger");
    events |= lua_isboolean(L,-1) ? EPOLLET : 0;
    lua_getfield(L,idx,"oneshot");
    events |= lua_isboolean(L,-1) ? EPOLLONESHOT : 0;
    lua_pop(L,7);
  }
  else if (lua_isstring(L,idx))
  {
    const char *flags = lua_tostring(L,idx);
    for ( ; *flags ; flags++)
    {
      switch(*flags)
      {
        case 'r': events |= EPOLLIN;      break;
        case 'w': events |= EPOLLOUT;     break;
        case 'p': events |= EPOLLPRI;     break;
        case 'e': events |= EPOLLERR;     break;
        case 'h': events |= EPOLLHUP;     break;
        case 't': events |= EPOLLET;      break;
        case '1': events |= EPOLLONESHOT; break;
        default:  break;
      }
    }
  }
  else if (lua_isnil(L,idx))
    events |= EPOLLIN;
  else
    return luaL_error(L,"expected table or string");
  
  return events;
}

/**********************************************************************/

static void pollset_pushevents(lua_State *const L,int events)
{
  lua_createtable(L,0,8);
  lua_pushboolean(L,(events & EPOLLIN)      != 0);
  lua_setfield(L,-2,"read");
  lua_pushboolean(L,(events & EPOLLOUT)     != 0);
  lua_setfield(L,-2,"write");
  lua_pushboolean(L,(events & EPOLLPRI)     != 0);
  lua_setfield(L,-2,"priority");
  lua_pushboolean(L,(events & EPOLLERR)     != 0);
  lua_setfield(L,-2,"error");
  lua_pushboolean(L,(events & EPOLLHUP)     != 0);
  lua_setfield(L,-2,"hangup");
  lua_pushboolean(L,(events & EPOLLET)      != 0);
  lua_setfield(L,-2,"trigger");
  lua_pushboolean(L,(events & EPOLLONESHOT) != 0);
  lua_setfield(L,-2,"oneshot");
}

/**********************************************************************/

static int netlua_pollset(lua_State *const L)
{
  pollset__t *set;
  
  set      = lua_newuserdata(L,sizeof(pollset__t));
  set->idx = 0;
  set->efh = epoll_create(10);
  
  if (set->efh == -1)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    return 2;
  }
  
  lua_createtable(L,0,0);
  set->ref = luaL_ref(L,LUA_REGISTRYINDEX);
  
  luaL_getmetatable(L,TYPE_POLL);
  lua_setmetatable(L,-2);
  return 1;
}

/**********************************************************************/

static int polllua___tostring(lua_State *const L)
{
  pollset__t *set = luaL_checkudata(L,1,TYPE_POLL);
  lua_pushfstring(L,"SET:%d:%p",set->idx,(void *)set);
  return 1;
}

/**********************************************************************/

static int polllua___gc(lua_State *const L)
{
  pollset__t *set = luaL_checkudata(L,1,TYPE_POLL);
  luaL_unref(L,LUA_REGISTRYINDEX,set->ref);
  close(set->efh);
  return 0;
}

/**********************************************************************/

static int polllua_insert(lua_State *const L)
{
  pollset__t         *set  = luaL_checkudata(L,1,TYPE_POLL);
  sock__t            *sock = luaL_checkudata(L,2,TYPE_SOCK);
  struct epoll_event  event;
  
  lua_settop(L,4);
  
  event.events  = pollset_toevents(L,3);
  event.data.fd = sock->fh;
  
  if (epoll_ctl(set->efh,EPOLL_CTL_ADD,sock->fh,&event) < 0)
  {
    lua_pushinteger(L,errno);
    return 1;
  }
  
  lua_pushinteger(L,set->ref);
  lua_gettable(L,LUA_REGISTRYINDEX);
  lua_pushinteger(L,sock->fh);
  
  if (lua_isnil(L,4))
    lua_pushvalue(L,2);
  else
    lua_pushvalue(L,4);
  
  lua_settable(L,-3);
  
  set->idx++;
  lua_pushinteger(L,0);
  return 1;
}

/**********************************************************************/

static int polllua_update(lua_State *const L)
{
  pollset__t         *set  = luaL_checkudata(L,1,TYPE_POLL);
  sock__t            *sock = luaL_checkudata(L,2,TYPE_SOCK);
  struct epoll_event  event;
  
  lua_settop(L,3);
  
  event.events  = pollset_toevents(L,3);
  event.data.fd = sock->fh;
  
  if (epoll_ctl(set->efh,EPOLL_CTL_MOD,sock->fh,&event) < 0)
  {
    lua_pushinteger(L,errno);
    return 1;
  }
  
  lua_pushinteger(L,0);
  return 1;
}

/**********************************************************************/

static int polllua_remove(lua_State *const L)
{
  pollset__t         *set  = luaL_checkudata(L,1,TYPE_POLL);
  sock__t            *sock = luaL_checkudata(L,2,TYPE_SOCK);
  struct epoll_event  event;
  
  if (epoll_ctl(set->efh,EPOLL_CTL_DEL,sock->fh,&event) < 0)
  {
    lua_pushinteger(L,errno);
    return 1;
  }
  
  lua_pushinteger(L,set->ref);
  lua_gettable(L,LUA_REGISTRYINDEX);
  lua_pushinteger(L,sock->fh);
  lua_pushnil(L);
  lua_settable(L,-3);
  
  set->idx--;
  lua_pushinteger(L,0);
  return 1;
}

/**********************************************************************/

static int polllua_events(lua_State *const L)
{
  pollset__t *set     = luaL_checkudata(L,1,TYPE_POLL);
  int         timeout = luaL_optinteger(L,2,-1);
  int         count;
  size_t      idx;
  int         i;
  
  if (timeout > 0)
    timeout *= 1000;
  
  struct epoll_event events[set->idx];
  
  count = epoll_wait(set->efh,events,set->idx,timeout);
  
  if (count < 0)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    return 2;
  }
  
  lua_pushinteger(L,set->ref);
  lua_gettable(L,LUA_REGISTRYINDEX);
  
  lua_createtable(L,set->idx,0);
  for (idx = 1 , i = 0 ; i < count ; i++)
  {
    lua_pushnumber(L,idx++);
    pollset_pushevents(L,events[i].events);
    lua_pushinteger(L,events[i].data.fd);
    lua_gettable(L,-5);
    lua_setfield(L,-2,"obj");
    lua_settable(L,-3);    
  }
  
  lua_pushinteger(L,0);
  return 2;
}

#endif

/*********************************************************************
*
* poll() based version, used for Unix systems other than Linux
*
* This implementation uses the memory allocation functions for the
* given Lua state.  I felt this was a Good Idea(TM), since if a Lua
* instance is using a particular memory allocation scheme, there is
* probably a good reason for it.  
*********************************************************************/

#ifndef __linux

#include <poll.h>

typedef struct
{
  struct pollfd *set;
  size_t         idx;
  size_t         max;
  int            ref;
} pollset__t;

/**********************************************************************/

static int pollset_toevents(lua_State *const L,int idx)
{
  int events = 0;
  
  if (lua_istable(L,idx))
  {
    lua_getfield(L,idx,"read");
    events |= lua_isboolean(L,-1) ? POLLIN : 0;
    lua_getfield(L,idx,"write");
    events |= lua_isboolean(L,-1) ? POLLOUT : 0;
    lua_getfield(L,idx,"priority");
    events |= lua_isboolean(L,-1) ? POLLPRI : 0;
    lua_getfield(L,idx,"error");
    lua_pop(L,3);
  }
  else if (lua_isstring(L,idx))
  {
    const char *flags = lua_tostring(L,idx);
    for ( ; *flags ; flags++)
    {
      switch(*flags)
      {
        case 'r': events |= POLLIN;  break;
        case 'w': events |= POLLOUT; break;
        case 'p': events |= POLLPRI; break;
        default:  break;
      }
    }
  }
  else if (lua_isnil(L,idx))
    events |= POLLIN;
  else
    return luaL_error(L,"expected table or string");
  
  return events;
}

/**********************************************************************/

static void pollset_pushevents(lua_State *const L,int events)
{
  lua_createtable(L,0,7);
  lua_pushboolean(L,(events & POLLIN)   != 0);
  lua_setfield(L,-2,"read");
  lua_pushboolean(L,(events & POLLOUT)  != 0);
  lua_setfield(L,-2,"write");
  lua_pushboolean(L,(events & POLLPRI)  != 0);
  lua_setfield(L,-2,"priority");
  lua_pushboolean(L,(events & POLLERR)  != 0);
  lua_setfield(L,-2,"error");
  lua_pushboolean(L,(events & POLLHUP)  != 0);
  lua_setfield(L,-2,"hangup");
  lua_pushboolean(L,(events & POLLNVAL) != 0);
  lua_setfield(L,-2,"invalid");
}

/**********************************************************************/

static int netlua_pollset(lua_State *const L)
{
  pollset__t *set;
  
  set = lua_newuserdata(L,sizeof(pollset__t));
  set->set = NULL;
  set->idx = 0;
  set->max = 0;
  
  lua_createtable(L,0,0);
  set->ref = luaL_ref(L,LUA_REGISTRYINDEX);
  
  luaL_getmetatable(L,TYPE_POLL);
  lua_setmetatable(L,-2);
  return 1;
}

/**********************************************************************/

static int polllua___tostring(lua_State *const L)
{
  pollset__t *set = luaL_checkudata(L,1,TYPE_POLL);
  lua_pushfstring(L,"SET:%d:%p",set->idx,(void *)set);
  return 1;
}

/**********************************************************************/

static int polllua___gc(lua_State *const L)
{
  lua_Alloc  allocf;
  void      *ud;
  
  pollset__t *set = luaL_checkudata(L,1,TYPE_POLL);
  luaL_unref(L,LUA_REGISTRYINDEX,set->ref);
  allocf = lua_getallocf(L,&ud);
  (*allocf)(ud,set->set,set->max * sizeof(struct pollfd),0);  
  return 0;
}

/**********************************************************************/

static int polllua_insert(lua_State *const L)
{
  pollset__t *set  = luaL_checkudata(L,1,TYPE_POLL);
  sock__t    *sock = luaL_checkudata(L,2,TYPE_SOCK);
  
  lua_settop(L,4);
  
  if (set->idx == set->max)
  {
    struct pollfd *new;
    size_t         newmax;
    lua_Alloc      allocf;
    void          *ud;
    
    allocf = lua_getallocf(L,&ud);
    newmax = set->max + 10;
    new    = (*allocf)(
    		ud,
    		set->set,
    		set->max * sizeof(struct pollfd),
    		newmax   * sizeof(struct pollfd)
    	);
    
    if (new == NULL)
    {
      lua_pushinteger(L,ENOMEM);
      return 1;
    }
    
    set->set = new;
    set->max = newmax;
  }
  
  set->set[set->idx].events = pollset_toevents(L,3);
  set->set[set->idx].fd     = sock->fh;
  
  lua_pushinteger(L,set->ref);
  lua_gettable(L,LUA_REGISTRYINDEX);
  lua_pushinteger(L,sock->fh);
  
  if (lua_isnil(L,4))
    lua_pushvalue(L,2);
  else
    lua_pushvalue(L,4);
  
  lua_settable(L,-3);

  set->idx++;
  lua_pushinteger(L,0);
  return 1;
}

/**********************************************************************/

static int polllua_update(lua_State *const L)
{
  pollset__t *set  = luaL_checkudata(L,1,TYPE_POLL);
  sock__t    *sock = luaL_checkudata(L,2,TYPE_SOCK);
  
  lua_settop(L,3);
  
  for (size_t i = 0 ; i < set->idx ; i++)
  {
    if (set->set[i].fd == sock->fh)
    {
      set->set[i].events = pollset_toevents(L,3);
      lua_pushinteger(L,0);
      return 1;
    }
  }
  
  lua_pushinteger(L,EINVAL);
  return 1;
}

/**********************************************************************/

static int polllua_remove(lua_State *const L)
{
  pollset__t *set  = luaL_checkudata(L,1,TYPE_POLL);
  sock__t    *sock = luaL_checkudata(L,2,TYPE_SOCK);
  
  for (size_t i = 0 ; i < set->idx ; i++)
  {
    if (set->set[i].fd == sock->fh)
    {
      lua_pushinteger(L,set->ref);
      lua_gettable(L,LUA_REGISTRYINDEX);
      lua_pushinteger(L,sock->fh);
      lua_pushnil(L);
      lua_settable(L,-3);
    
      memmove(
               &set->set[i],
               &set->set[i+1],
               (set->idx - i - 1) * sizeof(struct pollfd)
            );
            
      set->idx--;
      lua_pushinteger(L,0);
      return 1;
    }
  }
  
  lua_pushinteger(L,EINVAL);
  return 1;
}

/**********************************************************************/

static int polllua_events(lua_State *const L)
{
  pollset__t *set     = luaL_checkudata(L,1,TYPE_POLL);
  int         timeout = luaL_optinteger(L,2,-1);
  
  if (timeout > 0)
    timeout *= 1000;
  
  if (poll(set->set,set->idx,timeout) < 0)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    return 2;
  }
  
  lua_pushinteger(L,set->ref);
  lua_gettable(L,LUA_REGISTRYINDEX);
  
  lua_createtable(L,set->idx,0);
  for (size_t idx = 1 , i = 0 ; i < set->idx ; i++)
  {
    if (set->set[i].revents != 0)
    {
      lua_pushnumber(L,idx++);
      pollset_pushevents(L,set->set[i].events);
      lua_pushinteger(L,set->set[i].fd);
      lua_gettable(L,-5);
      lua_setfield(L,-2,"obj");
      lua_settable(L,-3);      
    }
  }
  
  lua_pushinteger(L,0);
  return 2;
}

#endif

/**********************************************************************
*
* And finally the version based on select(), for those really old Unix
* systems that just can't handle poll().
*
***********************************************************************/

#ifdef X_O_REALLY

#include <sys/select.h>

typedef struct
{
  fd_set read;
  fd_set write;
  fd_set except;
  int    min;
  int    max;
  size_t idx;
  int    ref;
} pollset__t;

/**********************************************************************/

static void pollset_toevents(lua_State *const L,int idx,pollset__t *set,int fd)
{
  if (lua_istable(L,idx))
  {
    lua_getfield(L,idx,"read");
    if (lua_isboolean(L,-1)) FD_SET(fd,&set->read);
    lua_getfield(L,idx,"write");
    if (lua_isboolean(L,-1)) FD_SET(fd,&set->write);
    lua_getfield(L,idx,"except");
    if (lua_isboolean(L,-1)) FD_SET(fd,&set->except);
    lua_pop(L,3);
  }
  else if (lua_isstring(L,idx))
  {
    const char *flags = lua_tostring(L,idx);
    for ( ; *flags ; flags++)
    {
      switch(*flags)
      {
        case 'r': FD_SET(fd,&set->read);   break;
        case 'w': FD_SET(fd,&set->write);  break;
        case 'p': FD_SET(fd,&set->except); break;
        case 'e': FD_SET(fd,&set->except); break;
        default:  break;
      }
    }
  }
  else if (lua_isnil(L,idx))
    FD_SET(fd,&set->read);
  else
    luaL_error(L,"expected table or string");
}

/**********************************************************************/

static void pollset_pushevents(lua_State *const L,pollset__t *set,int fd)
{
  lua_createtable(L,0,4);
  lua_pushboolean(L,FD_ISSET(fd,&set->read));
  lua_setfield(L,-2,"read");
  lua_pushboolean(L,FD_ISSET(fd,&set->write));
  lua_setfield(L,-2,"write");
  lua_pushboolean(L,FD_ISSET(fd,&set->except));
  lua_setfield(L,-2,"except");
}

/**********************************************************************/

static int netlua_pollset(lua_State *const L)
{
  pollset__t *set;
  
  set = lua_newuserdata(L,sizeof(pollset__t));
  FD_ZERO(&set->read);
  FD_ZERO(&set->write);
  FD_ZERO(&set->except);
  set->idx = 0;
  set->min = INT_MAX;
  set->max = 0;
  
  lua_createtable(L,0,0);
  set->ref = luaL_ref(L,LUA_REGISTRYINDEX);
  
  luaL_getmetatable(L,TYPE_POLL);
  lua_setmetatable(L,-2);
  return 1;
}

/**********************************************************************/

static int polllua___tostring(lua_State *const L)
{
  pollset__t *set = luaL_checkudata(L,1,TYPE_POLL);
  lua_pushfstring(L,"SET:%d:%p",set->idx,(void *)set);
  return 1;
}

/**********************************************************************/

static int polllua___gc(lua_State *const L)
{
  pollset__t *set = luaL_checkudata(L,1,TYPE_POLL);
  luaL_unref(L,LUA_REGISTRYINDEX,set->ref);
  return 0;
}

/**********************************************************************/

static int polllua_insert(lua_State *const L)
{
  pollset__t *set  = luaL_checkudata(L,1,TYPE_POLL);
  sock__t    *sock = luaL_checkudata(L,2,TYPE_SOCK);
  
  lua_settop(L,4);
  
  if (sock->fh > FD_SETSIZE)
  {
    lua_pushinteger(L,EINVAL);
    return 1;
  }
  
  if (sock->fh < set->min) set->min = sock->fh;
  if (sock->fh > set->max) set->max = sock->fh;
  
  pollset_toevents(L,3,set,sock->fh);
  
  lua_pushinteger(L,set->ref);
  lua_gettable(L,LUA_REGISTRYINDEX);
  lua_pushinteger(L,sock->fh);
  
  if (lua_isnil(L,4))
    lua_pushvalue(L,2);
  else
    lua_pushvalue(L,4);
  
  lua_settable(L,-3);
  
  set->idx++;
  lua_pushinteger(L,0);
  return 1;
}

/**********************************************************************/

static int polllua_update(lua_State *const L)
{
  pollset__t *set  = luaL_checkudata(L,1,TYPE_POLL);
  sock__t    *sock = luaL_checkudata(L,2,TYPE_SOCK);
  
  FD_CLR(sock->fh,&set->read);
  FD_CLR(sock->fh,&set->write);
  FD_CLR(sock->fh,&set->except);
  
  pollset_toevents(L,3,set,sock->fh);
  lua_pushinteger(L,0);
  return 1;
}

/**********************************************************************/

static int polllua_remove(lua_State *const L)
{
  pollset__t *set  = luaL_checkudata(L,1,TYPE_POLL);
  sock__t    *sock = luaL_checkudata(L,2,TYPE_SOCK);
  
  FD_CLR(sock->fh,&set->read);
  FD_CLR(sock->fh,&set->write);
  FD_CLR(sock->fh,&set->except);
  
  lua_pushinteger(L,set->ref);
  lua_gettable(L,LUA_REGISTRYINDEX);
  lua_pushinteger(L,sock->fh);
  lua_pushnil(L);
  lua_settable(L,-3);
  
  set->idx--;
  lua_pushinteger(L,0);
  return 1;
}

/**********************************************************************/

static int polllua_events(lua_State *const L)
{
  pollset__t     *set     = luaL_checkudata(L,1,TYPE_POLL);
  double          timeout = luaL_optnumber(L,2,-1.0);
  struct timeval  tout;
  struct timeval *ptout;
  fd_set          read;
  fd_set          write;
  fd_set          except;
  int             rc;
  int             i;
  size_t          idx;
  
  if (timeout < 0)
    ptout = NULL;
  else
  {
    double seconds;
    double fract;
    
    fract        = modf(timeout,&seconds);
    tout.tv_sec  = (long)seconds;
    tout.tv_usec = (long)(fract * 1000000.0);
    ptout        = &tout;
  }
  
  read   = set->read;
  write  = set->write;
  except = set->except;
  
  rc = select(FD_SETSIZE,&read,&write,&except,ptout);
  if (rc < 0)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    return 2;
  }
  
  lua_pushinteger(L,set->ref);
  lua_gettable(L,LUA_REGISTRYINDEX);
  
  lua_createtable(L,set->idx,0);  
  for (idx = 1 , i = set->min ; i <= set->max ; i++)
  {
    if (FD_ISSET(i,&read) || FD_ISSET(i,&write) || FD_ISSET(i,&except))
    {
      lua_pushnumber(L,idx++);
      pollset_pushevents(L,set,i);
      lua_pushinteger(L,i);
      lua_gettable(L,-5);
      lua_setfield(L,-2,"obj");
      lua_settable(L,-3);
    }
  }
  
  lua_pushinteger(L,0);
  return 2;
}

#endif

/**********************************************************************/

