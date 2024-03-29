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
#  define _DEFAULT_SOURCE
#  define _BSD_SOURCE
#  define _POSIX_SOURCE
#  include <sys/ioctl.h>
#  include <linux/sockios.h>
#endif

#include <math.h>
#include <string.h>
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
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef __APPLE__
#  include <sys/ioctl.h>
#endif

#include <lua.h>
#include <lauxlib.h>

#if !defined(LUA_VERSION_NUM) || LUA_VERSION_NUM < 501
#  error You need to compile against Lua 5.1 or higher
#endif

#define TYPE_SOCK       "org.conman.net:sock"
#define TYPE_ADDR       "org.conman.net:addr"

#ifdef __SunOS
#  define SUN_LEN(x)    sizeof(struct sockaddr_un)
#endif

#if LUA_VERSION_NUM == 501
#  include <lualib.h>
#  define lua_rawlen(L,idx)       lua_objlen((L),(idx))
#  define luaL_setfuncs(L,reg,up) luaI_openlib((L),NULL,(reg),(up))
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
  char const *const text;
  int  const        value;
};

/************************************************************************/

static inline size_t Inet_addrlen(sockaddr_all__t const *addr)
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

static inline socklen_t Inet_len(sockaddr_all__t const *addr)
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

static inline socklen_t Inet_lensa(struct sockaddr const *) __attribute__((unused));
static inline socklen_t Inet_lensa(struct sockaddr const *addr)
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

static inline int Inet_port(sockaddr_all__t const *addr)
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

static inline void Inet_setport(sockaddr_all__t *addr,int port)
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

static inline void Inet_setportn(sockaddr_all__t *,int) __attribute__((unused));
static inline void Inet_setportn(sockaddr_all__t *addr,int port)
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

static inline char const *Inet_addr(
        sockaddr_all__t *addr,
        char            *dest
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

static inline void const *Inet_address(sockaddr_all__t const *addr)
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

/*------------------------------------------------------------------------*/

static inline int Inet_comp(
        sockaddr_all__t const *restrict a,
        sockaddr_all__t const *restrict b
)
{
  int rc;
  
  assert(a != NULL);
  assert(b != NULL);
  
  if ((rc = a->sa.sa_family - b->sa.sa_family) != 0) return rc;
  
  socklen_t   la  = Inet_addrlen(a);
  socklen_t   lb  = Inet_addrlen(b);
  socklen_t   len = la < lb ? la : lb;
  void const *pa  = Inet_address(a);
  void const *pb  = Inet_address(b);
  
  if ((rc = memcmp(pa,pb,len)) != 0)
    return rc;
    
  if (la < lb)
    return -1;
  else if (la > lb)
    return 1;
    
  return Inet_port(a) - Inet_port(b);
}

/**********************************************************************/

static int net_toproto(lua_State *L,int idx)
{
  if (lua_isnil(L,idx))
    return 0;
  else if (lua_isnumber(L,idx))
    return lua_tointeger(L,idx);
  else if (lua_isstring(L,idx))
  {
    char const      *proto = lua_tostring(L,idx);
    struct protoent *presult;
    struct protoent  result;
    char             tmp[BUFSIZ] __attribute__((unused));
    
#if defined(__SunOS)
    presult = getprotobyname_r(proto,&result,tmp,sizeof(tmp));
    if (presult == NULL)
      return luaL_error(L,"protocol: %s",strerror(errno));
#elif defined(__linux__)
    if (getprotobyname_r(proto,&result,tmp,sizeof(tmp),&presult) != 0)
      return luaL_error(L,"protocol: %s",strerror(errno));
#else
    presult = getprotobyname(proto);
    if (presult == NULL)
      return luaL_error(L,"protocol: %s",strerror(errno));
    result = *presult;
#endif
    return result.p_proto;
  }
  else
    return 0;
}

/*********************************************************************/

static int net_toport(lua_State *L,int idx,int proto)
{
  if (lua_isnumber(L,idx))
    return lua_tointeger(L,idx);
  else if (lua_isstring(L,idx))
  {
    char const     *serv = lua_tostring(L,idx);
    char const     *tproto;
    struct servent *presult;
    struct servent  result;
    char            tmp[BUFSIZ] __attribute__((unused));
    
    switch(proto)
    {
      case IPPROTO_TCP:   tproto = "tcp";  break;
      case IPPROTO_UDP:   tproto = "udp";  break;
#ifdef IPPROTO_SCTP
      case IPPROTO_SCTP:  tproto = "sctp"; break;
#endif
      default:
           assert(0);
           return luaL_error(L,"invalid protocol");
           break;
    }
#if defined(__SunOS)
    presult = getservbyname_r(serv,tproto,&result,tmp,sizeof(tmp));
    if (presult == NULL)
      return luaL_error(L,"invalid service");
#elif defined(__linux__)
    if (getservbyname_r(serv,tproto,&result,tmp,sizeof(tmp),&presult) != 0)
      return luaL_error(L,"invalid service");
#else
    presult = getservbyname(serv,tproto);
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

static int err_meta___index(lua_State *L)
{
  int err = luaL_checkinteger(L,2);
  if (err < 0)
    lua_pushstring(L,gai_strerror(err));
  else
    lua_pushstring(L,strerror(err));
  return 1;
}

/**********************************************************************
* Usage:        sock,err = net.socket(family,proto)
* Desc:         Creates a socket to support the given family and protocol.
* Input:        family (string)         'ip' | 'ipv6' | 'unix'
*               proto (string number)   name or value of protocol
* Return:       sock (userdata) socket, nil on error
*               err (integer)   system error, 0 on success
**********************************************************************/

static int netlua_socket(lua_State *L)
{
  static char const *const m_netfamilytext[] = { "ip"    , "ip6"    , "unix" , NULL };
  static int const         m_netfamily[]     = { AF_INET , AF_INET6 , AF_UNIX };
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
#ifdef IPPROTO_SCTP
  else if (proto == IPPROTO_SCTP)
    type = SOCK_SEQPACKET;
#endif
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
*       sock,err = net.socketfile(fp)
*
*       fp = io.open(...) or io.stdin or io.stdout or io.stderr
*
********************************************************************/

static int netlua_socketfile(lua_State *L)
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
*       sock,err = net._fromfd(fh)
*
*       fh = <integer>
*
********************************************************************/

static int netlua__fromfd(lua_State *L)
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
* Usage:        sock1,sock2,err = net.socketpair([dgram])
* Desc:         Create a pair of connected sockets
* Input:        dgram (boolean/optional) true if datagram semantics required
* Return:       sock1 (userdata(socket))
*               sock2 (userdata(socket))
*               err (ineteger) system error, 0 if okay
************************************************************************/

static int netlua_socketpair(lua_State *L)
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
* Usage:        addr,err = net.address2(host,[family = 'any'],[proto],[port])
* Desc:         Return a list of addresses for the given host.
* Input:        host (string) hostname, IPv4 or IPv6 address
*               family (string) 'any' - return any address type    \
*                               'ip'  - return only IPv4 addresses \
*                               'ip6' - return only IPv6 addresses
*               proto (string number) name or number of protocol
*               port (string number)  name or number of port
* Return:       addr (table) array of results, nil on failure
*               err (integer) network error, 0 on success
* Note:         Use net.errno[] to translate returned error.
* Examples:
*               addr,err = net.address2('www.google.com')
*               addr,err = net.address2('www.google.com','ip','tcp','www')
*               addr,err = net.address2('127.0.0.1','ip','udp',53)
*
*               if not addr then
*                       print("ERROR",net.errno[err])
*               else
*                       for i = 1 , #addr do
*                               print(addr[i])
*                       end
*               end
*
**********************************************************************/

static int netlua_address2(lua_State *L)
{
  struct addrinfo  hints;
  struct addrinfo *results;
  struct addrinfo *addrloop;
  char const     *hostname;
  char const     *family;
  int              protocol;
  char const     *port;
  int              rc;
  
  lua_settop(L,4);
  memset(&hints,0,sizeof(hints));
  results = NULL;
  
  hostname = luaL_checkstring(L,1);
  
  /*------------------------------------------------
  ; set the address family, this can be
  ;
  ;     ip
  ;     ip6
  ;     any
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
  ;     udp
  ;     tcp
  ;---------------------------------------------*/
  
  protocol = net_toproto(L,3);
  
  if (protocol == IPPROTO_TCP)
    hints.ai_socktype = SOCK_STREAM;
  else if (protocol == IPPROTO_UDP)
    hints.ai_socktype = SOCK_DGRAM;
#ifdef IPPROTO_SCTP
  else if (protocol == IPPROTO_SCTP)
    hints.ai_socktype = SOCK_SEQPACKET;
#endif
  else if (protocol != 0)
    hints.ai_socktype = SOCK_RAW;
    
  /*-------------------------------------------------
  ; now set the port (or service), examples:
  ;
  ;     finger
  ;     www
  ;     smtp
  ;-------------------------------------------------*/
  
  port  = lua_tostring(L,4);
  errno = 0;
  rc    = getaddrinfo(hostname,port,&hints,&results);
  
  if (rc != 0)
  {
    lua_pushnil(L);
    lua_pushinteger(L,(rc == EAI_SYSTEM) ? errno : rc);
    
    /* --------------------------------------------------
    ; Sigh---some systems seem to fail when this is NULL.
    ;----------------------------------------------------*/
    
    if (results)
      freeaddrinfo(results);
    return 2;
  }
  
  lua_createtable(L,0,0);
  
  if (results == NULL)
  {
    lua_pushinteger(L,0);
    return 2;
  }
  
  addrloop = results;
  
  for (int i = 1 ; addrloop != NULL ; addrloop = addrloop->ai_next , i++)
  {
    lua_pushinteger(L,i);
    sockaddr_all__t *addr = lua_newuserdata(L,sizeof(sockaddr_all__t));
    luaL_getmetatable(L,TYPE_ADDR);
    lua_setmetatable(L,-2);
    memcpy(&addr->sa,addrloop->ai_addr,addrloop->ai_addrlen);
    lua_settable(L,-3);
  }
  
  freeaddrinfo(results);
  lua_pushinteger(L,0);
  return 2;
}

/***********************************************************************
* Usage:        addr,err = net.address(address,proto[,port])
* Desc:         Create an address object.
* Input:        address (string)        IPv4, IPv6 or path
*               proto (string number)   name of protocol, or number of protocol
*               port (string number)    name or value of port
* Return:       addr (userdata) address object, nil on error
*               err (integer)   system error, 0 on success
* Examples:
*
*               addr,err = net.address('127.0.0.1','tcp',25)
*               addr,err = net.address("fc00::1",'tcp','smtp')
*               addr,err = net.address("192.168.1.1","ospf")
*
*********************************************************************/

static int netlua_address(lua_State *L)
{
  sockaddr_all__t *addr;
  char const      *host;
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
  
  if (
          (proto == IPPROTO_TCP)
       || (proto == IPPROTO_UDP)
#ifdef IPPROTO_SCTP
       || (proto == IPPROTO_SCTP)
#endif
  )
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
*       addr,err = net.addressraw(binary,proto[,port])
*
*       binary = 4 bytes or 16 bytes of raw IP address
*       proto  = 'tcp', 'udp', ...
*       port   = string | number
*
************************************************************************/

static int netlua_addressraw(lua_State *L)
{
  sockaddr_all__t *addr;
  char const      *bin;
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
  
  if (
          (proto == IPPROTO_TCP)
       || (proto == IPPROTO_UDP)
#ifdef IPPROTO_SCTP
       || (proto == IPPROTO_SCTP)
#endif
  )
    Inet_setport(addr,net_toport(L,3,proto));
  else
    Inet_setport(addr,proto);
    
  luaL_getmetatable(L,TYPE_ADDR);
  lua_setmetatable(L,-2);
  lua_pushinteger(L,0);
  return 2;
}

/***********************************************************************/

static int socklua___tostring(lua_State *L)
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
  char    const *const name;
  int     const        level;
  int     const        setlevel;
  int     const        option;
  sopt__t const        type;
  bool    const        get;
  bool    const        set;
};

static struct sockoptions const m_sockoptions[] =
{
  { "broadcast"         , SOL_SOCKET    , 0             , SO_BROADCAST          , SOPT_FLAG     , true , true  } ,
#ifdef FD_CLOEXEC
  { "closeexec"         , F_GETFD       , F_SETFD       , FD_CLOEXEC            , SOPT_FCNTL    , true , true  } ,
#endif
  { "debug"             , SOL_SOCKET    , 0             , SO_DEBUG              , SOPT_FLAG     , true , true  } ,
  { "dontroute"         , SOL_SOCKET    , 0             , SO_DONTROUTE          , SOPT_FLAG     , true , true  } ,
  { "error"             , SOL_SOCKET    , 0             , SO_ERROR              , SOPT_INT      , true , false } ,
  { "keepalive"         , SOL_SOCKET    , 0             , SO_KEEPALIVE          , SOPT_FLAG     , true , true  } ,
  { "linger"            , SOL_SOCKET    , 0             , SO_LINGER             , SOPT_LINGER   , true , true  } ,
  { "maxsegment"        , IPPROTO_TCP   , 0             , TCP_MAXSEG            , SOPT_INT      , true , true  } ,
  { "nodelay"           , IPPROTO_TCP   , 0             , TCP_NODELAY           , SOPT_FLAG     , true , true  } ,
#ifdef TCP_QUICKACK
  { "quickack"          , IPPROTO_TCP   , 0             , TCP_QUICKACK          , SOPT_FLAG     , true , true  } ,
#endif
  { "nonblock"          , F_GETFL       , F_SETFL       , O_NONBLOCK            , SOPT_FCNTL    , true , true  } ,
#ifdef SO_NOSIGPIPE
  { "nosigpipe"         , SOL_SOCKET    , 0             , SO_NOSIGPIPE          , SOPT_FLAG     , true , true  } ,
#endif
  { "oobinline"         , SOL_SOCKET    , 0             , SO_OOBINLINE          , SOPT_FLAG     , true , true  } ,
  { "recvbuffer"        , SOL_SOCKET    , 0             , SO_RCVBUF             , SOPT_INT      , true , true  } ,
  { "recvlow"           , SOL_SOCKET    , 0             , SO_RCVLOWAT           , SOPT_INT      , true , true  } ,
#ifdef __linux
  { "recvqueue"         , SIOCINQ       , 0             , 0                     , SOPT_IOCTL    , true , false } ,
#endif
  { "recvtimeout"       , SOL_SOCKET    , 0             , SO_RCVTIMEO           , SOPT_INT      , true , true  } ,
  { "reuseaddr"         , SOL_SOCKET    , 0             , SO_REUSEADDR          , SOPT_FLAG     , true , true  } ,
#ifdef SO_REUSEPORT
  { "reuseport"         , SOL_SOCKET    , 0             , SO_REUSEPORT          , SOPT_FLAG     , true , true  } ,
#endif
  { "sendbuffer"        , SOL_SOCKET    , 0             , SO_SNDBUF             , SOPT_INT      , true , true  } ,
  { "sendlow"           , SOL_SOCKET    , 0             , SO_SNDLOWAT           , SOPT_INT      , true , true  } ,
#ifdef __linux
  { "sendqueue"         , SIOCOUTQ      , 0             , 0                     , SOPT_IOCTL    , true , false } ,
#endif
  { "sendtimeout"       , SOL_SOCKET    , 0             , SO_SNDTIMEO           , SOPT_INT      , true , true  } ,
  { "type"              , SOL_SOCKET    , 0             , SO_TYPE               , SOPT_INT      , true , false } ,
#ifdef SO_USELOOPBACK
  { "useloopback"       , SOL_SOCKET    , 0             , SO_USELOOPBACK        , SOPT_FLAG     , true , true  } ,
#endif
};

#define MAX_SOPTS       (sizeof(m_sockoptions) / sizeof(struct sockoptions))

/*********************************************************************/

static int sopt_compare(void const *restrict needle,void const *restrict haystack)
{
  char const               *const key   = needle;
  struct sockoptions const *const value = haystack;
  
  return strcmp(key,value->name);
}

/*********************************************************************/

static int socklua___index(lua_State *L)
{
  sock__t                  *sock;
  char const               *tkey;
  struct sockoptions const *value;
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

static int socklua___newindex(lua_State *L)
{
  sock__t                  *sock;
  char const               *tkey;
  struct sockoptions const *value;
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
* Usage:        addr,err = sock:peer()
* Desc:         Returns the remote address of the connection.
* Return:       addr (userdata) Address of the other side, nil on error
*               err (integer) system error, 0 on success
* Note:         This only has meaning for connected connections.
*********************************************************************/

static int socklua_peer(lua_State *L)
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
* Usage:        addr,err = sock:addr()
* Desc:         Returns the local address of the connection
* Return:       addr (userdata) Address of the local side
*               err (integer) system err, 0 on success
* Note:         This only has meaning for connected connections.
**********************************************************************/

static int socklua_addr(lua_State *L)
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
* Usage:        err = sock:bind(addr)
* Desc:         Bind an address to a socket
* Input:        addr (userdata) Address to bind to
* Return:       err (integer) system error, 0 on success
***********************************************************************/

static int socklua_bind(lua_State *L)
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
* Usage:        err = sock:connect(addr)
* Desc:         Make a connected socket.
* Input:        addr (userdata) Remote address
* Return:       err (integer) system error, 0 on success
**********************************************************************/

static int socklua_connect(lua_State *L)
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
*       err = sock:listen([backlog = 5])
*
*       sock = net.socket(...)
*
**********************************************************************/

static int socklua_listen(lua_State *L)
{
  sock__t *sock = luaL_checkudata(L,1,TYPE_SOCK);
  
  errno = 0;
  listen(sock->fh,luaL_optinteger(L,2,5));
  lua_pushinteger(L,errno);
  return 1;
}

/**********************************************************************
*
*       newsock,addr,err = sock:accept()
*
*       sock = net.socket(...)
*
***********************************************************************/

static int socklua_accept(lua_State *L)
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
*       remaddr,data,err = sock:recv([timeout = inf])
*
*       sock    = net.socket(...)
*       timeout = number (in seconds, -1 = inf)
*       err     = number
*
**********************************************************************/

static int socklua_recv(lua_State *L)
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
  memset(remaddr,0,sizeof(sockaddr_all__t));
  
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
*       numbytes,err = sock:send(addr,data)
*
*       sock = net.socket(...)
*       addr = net.address(...)
*       data = string
*
***********************************************************************/

static int socklua_send(lua_State *L)
{
  sockaddr_all__t *remote;
  struct sockaddr *remaddr;
  socklen_t        remsize;
  sock__t         *sock;
  char const      *buffer;
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
*       err = sock:shutdown([how = "rw"])
*
*       sock = net.socket(...)
*       how  = "r" (close read) | "w" (close write) | "rw" (close both)
*
**********************************************************************/

static int socklua_shutdown(lua_State *L)
{
  static char const *const opts[] = { "r" , "w" , "rw" };
  sock__t *sock = luaL_checkudata(L,1,TYPE_SOCK);
  
  errno = 0;
  shutdown(sock->fh,luaL_checkoption(L,2,"rw",opts));
  lua_pushinteger(L,errno);
  return 1;
}

/*******************************************************************
*
*       err = sock:close()
*
*       sock = net.socket(...)
*
*******************************************************************/

static int socklua_close(lua_State *L)
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

static int socklua__tofd(lua_State *L)
{
  sock__t *sock = luaL_checkudata(L,1,TYPE_SOCK);
  lua_pushinteger(L,sock->fh);
  return 1;
}

/***********************************************************************/

static int socklua__dup(lua_State *L)
{
  sock__t *sock = luaL_checkudata(L,1,TYPE_SOCK);
  int      fh   = dup(sock->fh);
  
  if (fh == -1)
  {
    lua_pushnil(L);
    lua_pushinteger(L,errno);
    return 2;
  }
  
  lua_pushinteger(L,fh);
  netlua__fromfd(L);
  return 2;
}

/***********************************************************************/

static int addrmeta_display(lua_State *L)
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

/**************************************************************************
*
* Why do we use a function, and not just use the table?  Because I want to
* be able to do:
*
*       family = address.family
*
* If we set __index to the metatable, then we return the function that this
* represents, not the actual family.  Thus, we check for fields and return
* the appropriate information.
*
***************************************************************************/

static int addrlua___index(lua_State *L)
{
  sockaddr_all__t *addr;
  char const      *sidx;
  
  addr = luaL_checkudata(L,1,TYPE_ADDR);
  if (!lua_isstring(L,2))
  {
    lua_pushnil(L);
    return 1;
  }
  
  sidx = lua_tostring(L,2);
  if (strcmp(sidx,"addr") == 0)
  {
    char const *p;
    char        taddr[INET6_ADDRSTRLEN];
    
    p = Inet_addr(addr,taddr);
    if (p == NULL)
      lua_pushnil(L);
    else
      lua_pushstring(L,p);
  }
  else if (strcmp(sidx,"daddr") == 0)
  {
    char const *p;
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

static int addrlua___tostring(lua_State *L)
{
  sockaddr_all__t *addr;
  char             taddr[INET6_ADDRSTRLEN];
  
  addr = luaL_checkudata(L,1,TYPE_ADDR);
  switch(addr->sa.sa_family)
  {
    case AF_INET:
         lua_pushfstring(L,"%s:%d",Inet_addr(addr,taddr),Inet_port(addr));
         break;
    case AF_INET6:
         lua_pushfstring(L,"[%s]:%d",Inet_addr(addr,taddr),Inet_port(addr));
         break;
    case AF_UNIX:
         lua_pushfstring(L,"%s",Inet_addr(addr,taddr));
         break;
    default:
         lua_pushfstring(L,"unknown:%d",addr->sa.sa_family);
         break;
  }
  return 1;
}

/**********************************************************************/

static int addrlua___eq(lua_State *L)
{
  lua_pushboolean(L,Inet_comp(
          luaL_checkudata(L,1,TYPE_ADDR),
          luaL_checkudata(L,2,TYPE_ADDR)) == 0);
  return 1;
}

/**********************************************************************/

static int addrlua___lt(lua_State *L)
{
  lua_pushboolean(L,Inet_comp(
          luaL_checkudata(L,1,TYPE_ADDR),
          luaL_checkudata(L,2,TYPE_ADDR)) < 0);
  return 1;
}

/**********************************************************************/

static int addrlua___le(lua_State *L)
{
  lua_pushboolean(L,Inet_comp(
          luaL_checkudata(L,1,TYPE_ADDR),
          luaL_checkudata(L,2,TYPE_ADDR)) <= 0);
  return 1;
}

/**********************************************************************/

static int addrlua___len(lua_State *L)
{
  lua_pushinteger(L,Inet_addrlen(luaL_checkudata(L,1,TYPE_ADDR)));
  return 1;
}

/*********************************************************************/

int luaopen_org_conman_net(lua_State *L)
{
  static luaL_Reg const m_net_reg[] =
  {
    { "socket"            , netlua_socket         } ,
    { "socketfile"        , netlua_socketfile     } ,
    { "socketpair"        , netlua_socketpair     } ,
    { "address2"          , netlua_address2       } , /* rename? */
    { "address"           , netlua_address        } ,
    { "addressraw"        , netlua_addressraw     } ,
    { "_fromfd"           , netlua__fromfd        } ,
    { NULL                , NULL                  }
  };
  
  static luaL_Reg const m_sock_meta[] =
  {
    { "__tostring"        , socklua___tostring    } ,
    { "__gc"              , socklua_close         } ,
#if LUA_VERSION_NUM >= 504
    { "__close"           , socklua_close         } ,
#endif
    { "__index"           , socklua___index       } ,
    { "__newindex"        , socklua___newindex    } ,
    { "peer"              , socklua_peer          } ,
    { "addr"              , socklua_addr          } ,
    { "bind"              , socklua_bind          } ,
    { "connect"           , socklua_connect       } ,
    { "listen"            , socklua_listen        } ,
    { "accept"            , socklua_accept        } ,
    { "recv"              , socklua_recv          } ,
    { "send"              , socklua_send          } ,
    { "shutdown"          , socklua_shutdown      } ,
    { "close"             , socklua_close         } ,
    { "_tofd"             , socklua__tofd         } ,
    { "_dup"              , socklua__dup          } ,
    { NULL                , NULL                  }
  };
  
  static luaL_Reg const m_addr_meta[] =
  {
    { "__index"           , addrlua___index       } ,
    { "__tostring"        , addrlua___tostring    } ,
    { "__eq"              , addrlua___eq          } ,
    { "__lt"              , addrlua___lt          } ,
    { "__le"              , addrlua___le          } ,
    { "__len"             , addrlua___len         } ,
    { NULL                , NULL                  }
  };
  
  static struct strint const m_errors[] =
  {
    { "EAI_BADFLAGS"      , EAI_BADFLAGS          } ,
    { "EAI_NONAME"        , EAI_NONAME            } ,
    { "EAI_AGAIN"         , EAI_AGAIN             } ,
    { "EAI_FAIL"          , EAI_FAIL              } ,
#ifdef EAI_NODATA
    { "EAI_NODATA"        , EAI_NODATA            } ,
#endif
    { "EAI_FAMILY"        , EAI_FAMILY            } ,
    { "EAI_SOCKTYPE"      , EAI_SOCKTYPE          } ,
    { "EAI_SERVICE"       , EAI_SERVICE           } ,
#ifdef EAI_ADDRFAMILY
    { "EAI_ADDRFAMILY"    , EAI_ADDRFAMILY        } ,
#endif
    { "EAI_MEMORY"        , EAI_MEMORY            } ,
    { "EAI_SYSTEM"        , EAI_SYSTEM            } ,
    { "EAI_OVERFLOW"      , EAI_OVERFLOW          } ,
    { NULL                , 0                     }
  };
  
  luaL_newmetatable(L,TYPE_SOCK);
  luaL_setfuncs(L,m_sock_meta,0);
  
  luaL_newmetatable(L,TYPE_ADDR);
  luaL_setfuncs(L,m_addr_meta,0);
  
#if LUA_VERSION_NUM == 501
  luaL_register(L,"org.conman.net",m_net_reg);
#else
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
