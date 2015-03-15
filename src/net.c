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

#ifndef __GNUC__
#  define __attribute__(x)
#endif

#ifdef __linux
#  define _BSD_SOURCE
#  define _POSIX_SOURCE
#  include <sys/ioctl.h>
#  include <linux/sockios.h>
#endif

#include <signal.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
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
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef __linux__
#  include <ifaddrs.h>
#endif

#ifdef __APPLE__
#  include <sys/ioctl.h>
#endif

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#if !defined(LUA_VERSION_NUM) || LUA_VERSION_NUM < 501
#  error You need to compile against Lua 5.1 or higher
#endif

#define TYPE_SOCK	"org.conman.net:sock"
#define TYPE_ADDR	"org.conman.net:addr"

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

typedef union
{
  char     c[sizeof(uint16_t)];
  uint16_t i;
} hns__u;

typedef union
{
  char     c[sizeof(uint32_t)];
  uint32_t i;
} hnl__u;

/************************************************************************/

static int	err_meta___index	(lua_State *const) __attribute__((nonnull));

#ifdef __linux__
static int	netlua_interfaces	(lua_State *const) __attribute__((nonnull));
#endif
static int	netlua_socket		(lua_State *const) __attribute__((nonnull));
static int	netlua_socketfile	(lua_State *const) __attribute__((nonnull));
static int	netlua_socketfd		(lua_State *const) __attribute__((nonnull));
static int	netlua_socketpair	(lua_State *const) __attribute__((nonnull));
static int	netlua_address2		(lua_State *const) __attribute__((nonnull));
static int	netlua_address		(lua_State *const) __attribute__((nonnull));
static int      netlua_addressraw	(lua_State *const) __attribute__((nonnull));
static int	netlua_htons		(lua_State *const) __attribute__((nonnull));
static int	netlua_htonl		(lua_State *const) __attribute__((nonnull));
static int	netlua_ntohs		(lua_State *const) __attribute__((nonnull));
static int	netlua_ntohl		(lua_State *const) __attribute__((nonnull));

static int	socklua___tostring	(lua_State *const) __attribute__((nonnull));
static int	socklua___index		(lua_State *const) __attribute__((nonnull));
static int	socklua___newindex	(lua_State *const) __attribute__((nonnull));
static int      socklua_peer		(lua_State *const) __attribute__((nonnull));
static int	socklua_addr		(lua_State *const) __attribute__((nonnull));
static int	socklua_bind		(lua_State *const) __attribute__((nonnull));
static int	socklua_connect		(lua_State *const) __attribute__((nonnull));
#ifdef TCP_FASTOPEN
static int	socklua_fastconnect	(lua_State *const) __attribute__((nonnull));
#endif
static int	socklua_listen		(lua_State *const) __attribute__((nonnull));
static int	socklua_accept		(lua_State *const) __attribute__((nonnull));
static int	socklua_recv		(lua_State *const) __attribute__((nonnull));
static int	socklua_send		(lua_State *const) __attribute__((nonnull));
static int	socklua_shutdown	(lua_State *const) __attribute__((nonnull));
static int	socklua_close		(lua_State *const) __attribute__((nonnull));
static int	socklua_fd		(lua_State *const) __attribute__((nonnull));

static int	addrlua___index		(lua_State *const) __attribute__((nonnull));
static int	addrlua___tostring	(lua_State *const) __attribute__((nonnull));
static int	addrlua___eq		(lua_State *const) __attribute__((nonnull));
static int	addrlua___lt		(lua_State *const) __attribute__((nonnull));
static int	addrlua___le		(lua_State *const) __attribute__((nonnull));
static int	addrlua___len		(lua_State *const) __attribute__((nonnull));

static int	net_toproto		(lua_State *const,const int)           __attribute__((nonnull));
static int	net_toport		(lua_State *const,const int,const int) __attribute__((nonnull));

/*************************************************************************/

static const luaL_Reg m_net_reg[] =
{
#ifdef __linux__
  { "interfaces"	, netlua_interfaces	} ,
#endif
  { "socket"		, netlua_socket		} ,
  { "socketfile"	, netlua_socketfile	} ,
  { "socketfd"		, netlua_socketfd	} ,
  { "socketpair"	, netlua_socketpair	} ,
  { "address2"		, netlua_address2	} ,
  { "address"		, netlua_address	} ,
  { "addressraw"	, netlua_addressraw	} ,
  { "htons"		, netlua_htons		} ,
  { "htonl"		, netlua_htonl		} ,
  { "ntohs"		, netlua_ntohs		} ,
  { "ntohl"		, netlua_ntohl		} ,
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
#ifdef TCP_FASTOPEN
  { "fastconnect"	, socklua_fastconnect	} ,
#endif
  { "listen"		, socklua_listen	} ,
  { "accept"		, socklua_accept	} ,
  { "recv"		, socklua_recv		} ,
  { "send"		, socklua_send		} ,
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

static inline socklen_t Inet_lensa(struct sockaddr *) __attribute__((unused));
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

static inline void Inet_setportn(sockaddr_all__t *,const int) __attribute__((unused));
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
* Usage:	list,err = net.interfaces()
*
* Desc:		Return a list of network interfaces
*
* Return:	list (table) A list of interfaces (see Note), nil on error
*		err (integer) system error, 0 on success
*
* Note:		This is a Linux-only routine.
*
*		The list is a table, where the keys are the names of the
*		interfaces, and the values are an array of address/mask/-
*		broadcast values:
*
*			{
*			  addr      = <addr userdata>,
*			  mask      = <addr userdata>,
*			  broadcast = <addr usedata> -- only for IPv4
*			}
*
*		An example;
*
*			{ 
*			  lo = 
*			  {
*			    {
*			      addr = <ip:127.0.0.1:0>,
*			      mask = <ip:255.0.0.0:0>
*			    },
*			    {
*			      addr = <ip6:[::1]:0>,
*			      mask = <ip6:[ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff]:0>
*			    }
*			  },
*			  eth0 = 
*			  { 
*			    {
*			      addr      = <ip:192.168.1.10:0>,
*			      mask      = <ip:255.255.255.0:0>,
*			      broadcast = <192.168.1.255:0>
*			    }
*			  }
*			}
*
**********************************************************************/

#ifdef __linux__
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

#if LUA_VERSION_NUM == 501    
      lua_pushinteger(L,lua_objlen(L,-1) + 1);
#else
      lua_pushinteger(L,lua_rawlen(L,-1) + 1);
#endif
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
#endif

/**********************************************************************
*
* Usage:	sock,err = net.socket(family,proto)
*
* Desc:		Creates a socket to support the given family and protocol.
*
* Input:	family (string) 	'ip' | 'ipv6' | 'unix'
*		proto (string number)	name or value of protocol
*
* Return:	sock (userdata)	socket, nil on error
*		err (integer)	system error, 0 on success
*
**********************************************************************/

static int netlua_socket(lua_State *const L)
{
  int      family;
  int      proto;
  int      type;
  sock__t *sock;

  family = m_netfamily[luaL_checkoption(L,1,NULL,m_netfamilytext)];  
  proto  = net_toproto(L,2);
  
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

/*******************************************************************
*
*	sock,err = net.socketfd(fh)
*
*	fh = <integer>
*
********************************************************************/

static int netlua_socketfd(lua_State *const L)
{
  int      fh   = luaL_checkinteger(L,1);
  sock__t *sock = lua_newuserdata(L,sizeof(sock__t));
  
  sock->fh = fh;
  luaL_getmetatable(L,TYPE_SOCK);
  lua_setmetatable(L,-2);
  lua_pushinteger(L,0);

  return 2;
}

/***********************************************************************
*
* Usage:	sock1,sock2,err = net.socketpair([dgram])
*
* Desc:		Create a pair of connected sockets
*
* Input:	dgram (boolean/optional) true if datagram semantics required
*
* Return:	sock1 (userdata(socket))
*		sock2 (userdata(socket))
*		err (ineteger) system error, 0 if okay
*
************************************************************************/

static int netlua_socketpair(lua_State *const L)
{
  int      type;
  int      sv[2];
  sock__t *s1;
  sock__t *s2;
  
  lua_settop(L,1);
  type = lua_toboolean(L,1)
       ? SOCK_DGRAM
       : SOCK_STREAM
       ;
  
  s1     = lua_newuserdata(L,sizeof(sock__t));
  s1->fh = -1;
  luaL_getmetatable(L,TYPE_SOCK);
  lua_setmetatable(L,-2);
  
  s2     = lua_newuserdata(L,sizeof(sock__t));
  s2->fh = -1;
  luaL_getmetatable(L,TYPE_SOCK);
  lua_setmetatable(L,-2);
  
  if (socketpair(AF_UNIX,type,0,sv) < 0)
  {
    lua_pushnil(L);
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    return 3;
  }
  
  s1->fh = sv[0];
  s2->fh = sv[1];
  lua_pushinteger(L,0);
  return 3;
}

/***********************************************************************
*
* Usage:	addr,err = net.address2(host,[family = 'any'],[proto],[port])
*
* Desc:		Return a list of addresses for the given host.
*
* Input:	host (string) hostname, IPv4 or IPv6 address
*		family (string)	'any' - return any address type    \
*				'ip'  - return only IPv4 addresses \
*				'ip6' - return only IPv6 addresses
*		proto (string number) name or number of protocol
*		port (string number)  name or number of port
*
* Return:	addr (table) array of results, nil on failure
*		err (integer) network error, 0 on success
*
* Note:		Use net.errno[] to translate returned error.
*
* Examples:
*		addr,err = net.address2('www.google.com')
*		addr,err = net.address2('www.google.com','ip','tcp','www')
*		addr,err = net.address2('127.0.0.1','ip','udp',53)
*
*		if not addr then
*			print("ERROR",net.errno[err])
*		else
*			for i = 1 , #addr do
*				print(addr[i])
*			end
*		end
*
**********************************************************************/

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
  
  protocol = net_toproto(L,3);
  
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
      memcpy(&addr->sa,results->ai_addr,results->ai_addrlen);
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
* Usage:	addr,err = net.address(address,proto[,port])
*
* Desc:		Create an address object.
*
* Input:	address (string)	IPv4, IPv6 or path
*		proto (string number)	name of protocol, or number of protocol
*		port (string number)	name or value of port
*
* Return:	addr (userdata)	address object, nil on error
*		err (integer)	system error, 0 on success
*
* Examples:
*
*		addr,err = net.address('127.0.0.1','tcp',25)
*		addr,err = net.address("fc00::1",'tcp','smtp')
*		addr,err = net.address("192.168.1.1","ospf")
*
*********************************************************************/

static int netlua_address(lua_State *const L)
{
  sockaddr_all__t *addr;
  const char      *host;
  size_t           hsize;
  int              proto;
  
  lua_settop(L,3);
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
  
  proto = net_toproto(L,2);
  
  if ((proto == IPPROTO_TCP) || (proto == IPPROTO_UDP))
    Inet_setport(addr,net_toport(L,3,proto));
  else
    Inet_setport(addr,proto);
  
  luaL_getmetatable(L,TYPE_ADDR);
  lua_setmetatable(L,-2);
  lua_pushinteger(L,0);
  return 2;
}

/***********************************************************************
*
*	addr,err = net.addressraw(binary,proto[,port])
*
*	binary = 4 bytes or 16 bytes of raw IP address
*	proto  = 'tcp', 'udp', ...
*	port   = string | number
*
************************************************************************/

static int netlua_addressraw(lua_State *const L)
{
  sockaddr_all__t *addr;
  const char      *bin;
  size_t           blen;
  int              proto;
  
  lua_settop(L,3);
  addr = lua_newuserdata(L,sizeof(sockaddr_all__t));
  bin  = luaL_checklstring(L,1,&blen);
  
  if (blen == 4)
  {
    addr->sin.sin_family = AF_INET;
    memcpy(&addr->sin.sin_addr.s_addr,bin,blen);
  }
  else if (blen == 16)
  {
    addr->sin6.sin6_family = AF_INET6;
    memcpy(&addr->sin6.sin6_addr.s6_addr,bin,blen);
  }
  else
  {
    lua_pushnil(L);
    lua_pushinteger(L,EINVAL);
    return 2;
  }
  
  proto = net_toproto(L,2);
  
  if ((proto == IPPROTO_TCP) || (proto == IPPROTO_UDP))
    Inet_setport(addr,net_toport(L,3,proto));
  else
    Inet_setport(addr,proto);
  
  luaL_getmetatable(L,TYPE_ADDR);
  lua_setmetatable(L,-2);
  lua_pushinteger(L,0);
  return 2;
}

/***********************************************************************/

static int netlua_htons(lua_State *const L)
{
  hns__u v;
  
  v.i = htons(luaL_checkinteger(L,1));
  lua_pushlstring(L,v.c,sizeof(v.c));
  return 1; 
}

/***********************************************************************/

static int netlua_htonl(lua_State *const L)
{
  hnl__u v;
  
  v.i = htonl(luaL_checkinteger(L,1));
  lua_pushlstring(L,v.c,sizeof(v.c));
  return 1;
}

/***********************************************************************/

static int netlua_ntohs(lua_State *const L)
{
  size_t      l;
  const char *s = luaL_checklstring(L,1,&l);
  hns__u      v;
  
  if (l == sizeof(uint16_t))
  {
    v.c[0] = s[0];
    v.c[1] = s[1];
    lua_pushinteger(L,ntohs(v.i));
    return 1;
  }
  return luaL_error(L,"invalid 16-bit quantity");
}

/***********************************************************************/

static int netlua_ntohl(lua_State *const L)
{
  size_t      l;
  const char *s = luaL_checklstring(L,1,&l);
  hnl__u      v;
  
  if (l == sizeof(uint32_t))
  {
    v.c[0] = s[0];
    v.c[1] = s[1];
    v.c[2] = s[2];
    v.c[3] = s[3];
    lua_pushnumber(L,ntohl(v.i));
    return 1;
  }
  return luaL_error(L,"invalid 32-bit quantity");
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
  SOPT_IOCTL,
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
#ifdef TCP_FASTOPEN
  /*---------------------------------------------------
  ; read http://lwn.net/Articles/508865/ for more info
  ;----------------------------------------------------*/
  { "fastopen"		, SOL_TCP	, 0		, TCP_FASTOPEN		, SOPT_INT	, true , true  } ,
#endif
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
#ifdef __linux
  { "recvqueue"		, SIOCINQ	, 0		, 0			, SOPT_IOCTL	, true , false } ,
#endif
  { "recvtimeout"	, SOL_SOCKET 	, 0		, SO_RCVTIMEO		, SOPT_INT	, true , true  } ,
  { "reuseaddr"		, SOL_SOCKET 	, 0		, SO_REUSEADDR		, SOPT_FLAG	, true , true  } ,
#ifdef SO_REUSEPORT  
  { "reuseport"		, SOL_SOCKET 	, 0		, SO_REUSEPORT		, SOPT_FLAG	, true , true  } ,
#endif
  { "sendbuffer"	, SOL_SOCKET 	, 0		, SO_SNDBUF		, SOPT_INT	, true , true  } ,
  { "sendlow"		, SOL_SOCKET 	, 0		, SO_SNDLOWAT		, SOPT_INT	, true , true  } ,
#ifdef __linux
  { "sendqueue"		, SIOCOUTQ	, 0		, 0			, SOPT_IOCTL	, true , false } ,
#endif
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
     
    case SOPT_IOCTL:
         if (ioctl(sock->fh,value->level,&ivalue) < 0)
           lua_pushnil(L);
         else
           lua_pushinteger(L,ivalue);
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
* Usage:	addr,err = sock:peer()
*
* Desc:		Returns the remote address of the connection.
*
* Return:	addr (userdata) Address of the other side, nil on error
*		err (integer) system error, 0 on success
*
* Note:		This only has meaning for connected connections.
*
*********************************************************************/

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
* Usage:	addr,err = sock:addr()
*
* Desc:		Returns the local address of the connection
*
* Return:	addr (userdata) Address of the local side
*		err (integer) system err, 0 on success
*
* Note:		This only has meaning for connected connections.
*
**********************************************************************/

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
* Usage:	err = sock:bind(addr)
*
* Desc:		Bind an address to a socket
*
* Input:	addr (userdata) Address to bind to
*
* Return:	err (integer) system error, 0 on success
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
      struct ipv6_mreq mreq6 __attribute__((unused));
      
      if (setsockopt(sock->fh,IPPROTO_IPV6,IPV6_MULTICAST_LOOP,&on,sizeof(on)) < 0)
      {
        lua_pushinteger(L,errno);
        return 1;
      }

#ifndef __APPLE__
      /*-------------------------------------------------------------
      ; I wonder if I should use IP_ADD_MEMBERSHIP for Darwin builds?
      ;-------------------------------------------------------------*/
      
      mreq6.ipv6mr_multiaddr = addr->sin6.sin6_addr;
      mreq6.ipv6mr_interface = 0;
      if (setsockopt(sock->fh,IPPROTO_IPV6,IPV6_ADD_MEMBERSHIP,&mreq6,sizeof(mreq6)) < 0)
      {
        lua_pushinteger(L,errno);
        return 1;
      }      
#endif
    }
  }
  
  return 1;
}

/**********************************************************************
*
* Usage:	err = sock:connect(addr)
*
* Desc:		Make a connected socket.
*
* Input:	addr (userdata) Remote address
*
* Return:	err (integer) system error, 0 on success
*
**********************************************************************/

static int socklua_connect(lua_State *const L)
{
  sock__t         *sock = luaL_checkudata(L,1,TYPE_SOCK);
  sockaddr_all__t *addr = luaL_checkudata(L,2,TYPE_ADDR);  

  errno = 0;
  connect(sock->fh,&addr->sa,Inet_len(addr));
  lua_pushinteger(L,errno);
  return 1;
}

/**********************************************************************
*
*	bytes,err = sock:fastconnect(addr,data)
*
*	sock = net.socket(...)
*	addr = net.address(...)
*
**********************************************************************/

#ifdef TCP_FASTOPEN
  static int socklua_fastconnect(lua_State *const L)
  {
    sock__t         *sock   = luaL_checkudata(L,1,TYPE_SOCK);
    sockaddr_all__t *remote = luaL_checkudata(L,2,TYPE_ADDR);
    size_t           bufsiz;
    const char      *buffer = luaL_checklstring(L,3,&bufsiz);
    struct sockaddr *remaddr;
    socklen_t        remsize;
    ssize_t          bytes;
    
    remaddr = &remote->sa;
    remsize = Inet_len(remote);
    errno   = 0;
    bytes   = sendto(sock->fh,buffer,bufsiz,MSG_FASTOPEN,remaddr,remsize);
    
    lua_pushinteger(L,bytes);
    lua_pushinteger(L,errno);
    return 2;
  }
#endif

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
  listen(sock->fh,luaL_optinteger(L,2,5));
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
*	remaddr,data,err = sock:recv([timeout = inf])
*
*	sock    = net.socket(...)
*	timeout = number (in seconds, -1 = inf)
*	err     = number
*
**********************************************************************/

static int socklua_recv(lua_State *const L)
{
  sockaddr_all__t *remaddr;
  socklen_t        remsize;
  sock__t         *sock;
  char             buffer[65535uL];
  ssize_t          bytes;
  
  sock = luaL_checkudata(L,1,TYPE_SOCK);
  
  if (lua_isnumber(L,2))
  {
    int           timeout = (int)(lua_tonumber(L,2) * 1000.0);
    struct pollfd fdlist;
    int           rc;
    
    fdlist.events = POLLIN;
    fdlist.fd     = sock->fh;
    
    rc = poll(&fdlist,1,timeout);
    if (rc < 1)
    {
      int err = (rc == 0) ? ETIMEDOUT : errno;
      lua_pushnil(L);
      lua_pushnil(L);
      lua_pushinteger(L,err);
      return 3;
    }
  }
  
  remaddr = lua_newuserdata(L,sizeof(sockaddr_all__t));
  remsize = sizeof(sockaddr_all__t);
  luaL_getmetatable(L,TYPE_ADDR);
  lua_setmetatable(L,-2);
  
  bytes = recvfrom(sock->fh,buffer,sizeof(buffer),0,&remaddr->sa,&remsize);
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
*	numbytes,err = sock:send(addr,data)
*
*	sock = net.socket(...)
*	addr = net.address(...)
*	data = string
*
***********************************************************************/

static int socklua_send(lua_State *const L)
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

static int addrmeta_display(lua_State *const L)
{
  sockaddr_all__t *addr    = luaL_checkudata(L,1,TYPE_ADDR);
  int              defport = luaL_optinteger(L,2,0);
  int              port    = Inet_port(addr);
  char             taddr[INET6_ADDRSTRLEN];
  
  if (port == defport)
  {
    switch(addr->sa.sa_family)
    {
      case AF_INET:  lua_pushfstring(L,"%s",Inet_addr(addr,taddr));   break;
      case AF_INET6: lua_pushfstring(L,"[%s]",Inet_addr(addr,taddr)); break;
      case AF_UNIX:  lua_pushfstring(L,"%s",Inet_addr(addr,taddr));   break;
      default: assert(0); lua_pushstring(L,"unknown");                break;
    }
  }
  else
  {
    switch(addr->sa.sa_family)
    {
      case AF_INET:  lua_pushfstring(L,"%s:%d",Inet_addr(addr,taddr),port);   break;
      case AF_INET6: lua_pushfstring(L,"[%s]:%d",Inet_addr(addr,taddr),port); break;
      case AF_UNIX:  lua_pushfstring(L,"%s",Inet_addr(addr,taddr));           break;
      default: assert(0); lua_pushstring(L,"unknown");                        break;
    }
  }
  
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
  else if (strcmp(sidx,"daddr") == 0)
  {
    const char *p;
    char        taddr[INET6_ADDRSTRLEN];
    
    p = Inet_addr(addr,taddr);
    if (p == NULL)
      lua_pushnil(L);
    else
    {
      if (addr->sa.sa_family == AF_INET6)
        lua_pushfstring(L,"[%s]",p);
      else
        lua_pushstring(L,p);
    }
  }   
  else if (strcmp(sidx,"addrbits") == 0)
  {
    switch(addr->sa.sa_family)
    {
      case AF_INET:  lua_pushlstring(L,(char *)&addr->sin.sin_addr.s_addr,sizeof(addr->sin.sin_addr.s_addr)); break;
      case AF_INET6: lua_pushlstring(L,(char *)addr->sin6.sin6_addr.s6_addr,sizeof(addr->sin6.sin6_addr.s6_addr)); break;
      case AF_UNIX:  lua_pushlstring(L,(char *)addr->ssun.sun_path,sizeof(addr->ssun.sun_path)); break;
      default: assert(0); lua_pushnil(L); break;
    }
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
  else if (strcmp(sidx,"display") == 0)
    lua_pushcfunction(L,addrmeta_display);
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
         lua_pushfstring(L,"unknown:%d",addr->sa.sa_family);
         break;
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

static int net_toproto(lua_State *const L,const int idx)
{
  if (lua_isnumber(L,idx))
    return lua_tointeger(L,idx);
  else if (lua_isstring(L,idx))
  {
    const char      *proto = lua_tostring(L,idx);
    struct protoent *presult;
    struct protoent  result;
    char             tmp[BUFSIZ] __attribute__((unused));
    
#if defined(__SunOS)
    presult = getprotobyname_r(proto,&result,tmp,sizeof(tmp));
    if (presult == NULL)
      return luaL_error(L,"invalid protocol");
#elif defined(__linux__)
    if (getprotobyname_r(proto,&result,tmp,sizeof(tmp),&presult) != 0)
      return luaL_error(L,"invalid protocol");
#else
    presult = getprotobyname(proto);
    if (presult == NULL)
      return luaL_error(L,"invalid protocol");
    result = *presult;
#endif
    return result.p_proto;
  }
  else
    return 0;
}

/*********************************************************************/

static int net_toport(lua_State *const L,int idx,const int proto)
{
  if (lua_isnumber(L,idx))
    return lua_tointeger(L,idx);
  else if (lua_isstring(L,idx))
  {
    const char     *serv = lua_tostring(L,idx);
    struct servent *presult;
    struct servent  result;
    char            tmp[BUFSIZ] __attribute__((unused));
    
#if defined(__SunOS)
    presult = getservbyname_r(serv,(proto == IPPROTO_TCP) ? "tcp" : "udp",&result,tmp,sizeof(tmp));
    if (presult == NULL)
      return luaL_error(L,"invalid service");
#elif defined(__linux__)
    if (getservbyname_r(serv,(proto == IPPROTO_TCP) ? "tcp" : "udp",&result,tmp,sizeof(tmp),&presult) != 0)
      return luaL_error(L,"invalid service");
#else
    presult = getservbyname(serv,(proto == IPPROTO_TCP) ? "tcp" : "udp");
    if (presult == NULL)
      return luaL_error(L,"invalid service");
    result = *presult;
#endif
    return ntohs((short)result.s_port);
  }
  else
    return 0;
}

/*********************************************************************/

int luaopen_org_conman_net(lua_State *const L)
{
#if LUA_VERSION_NUM == 501
  luaL_newmetatable(L,TYPE_SOCK);
  luaL_register(L,NULL,m_sock_meta);

  luaL_newmetatable(L,TYPE_ADDR);
  luaL_register(L,NULL,m_addr_meta);
  
  luaL_register(L,"org.conman.net",m_net_reg);
#else
  luaL_newmetatable(L,TYPE_SOCK);
  luaL_setfuncs(L,m_sock_meta,0);

  luaL_newmetatable(L,TYPE_ADDR);
  luaL_setfuncs(L,m_addr_meta,0);
  
  luaL_newlib(L,m_net_reg);
#endif

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

/*********************************************************************/
