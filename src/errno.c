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

#include <string.h>
#include <errno.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/***************************************************************************/

struct strint
{
  const char *const text;
  const int         value;
};

/***************************************************************************/

extern const struct strint m_errors[];

/***************************************************************************/

static int errno_strerror(lua_State *L)
{
  lua_pushstring(L,strerror(luaL_checkint(L,1)));
  return 1;
}

/***********************************************************************/

static const struct luaL_reg m_reg_errno[] = 
{
  { "strerror"	, errno_strerror } ,
  { NULL	, NULL		 }
};

/***********************************************************************/

int luaopen_org_conman_errno(lua_State *L)
{
  size_t i;
  
  luaL_register(L,"org.conman.errno",m_reg_errno);
  
  for (i = 0 ; m_errors[i].text != NULL ; i++)
  {
    lua_pushinteger(L,m_errors[i].value);
    lua_setfield(L,-2,m_errors[i].text);
  }
  
  return 1;
}

/*************************************************************************/

const struct strint m_errors[] = 
{
  { "EDOM"   , EDOM   } ,
  { "ERANGE" , ERANGE } ,
  
#ifdef EACCES
    { "EACCES" , EACCES } ,
#endif

#ifdef EADDRINUSE
    { "EADDRINUSE" , EADDRINUSE } ,
#endif

#ifdef EADDRNOTAVAIL
    { "EADDRNOTAVAIL" , EADDRNOTAVAIL } ,
#endif

#ifdef EADV
    { "EADV" , EADV } ,
#endif

#ifdef EAFNOSUPPORT
    { "EAFNOSUPPORT" , EAFNOSUPPORT } ,
#endif

#ifdef EAGAIN
    { "EAGAIN" , EAGAIN } ,
#endif

#ifdef EALREADY
    { "EALREADY" , EALREADY } ,
#endif

#ifdef EBADE
    { "EBADE" , EBADE } ,
#endif

#ifdef EBADF
    { "EBADF" , EBADF } ,
#endif

#ifdef EBADFD
    { "EBADFD" , EBADFD } ,
#endif

#ifdef EBADMSG
    { "EBADMSG" , EBADMSG } ,
#endif

#ifdef EBADR
    { "EBADR" , EBADR } ,
#endif

#ifdef EBADRQC
    { "EBADRQC" , EBADRQC } ,
#endif

#ifdef EBADSLT
    { "EBADSLT" , EBADSLT } ,
#endif

#ifdef EBFONT
    { "EBFONT" , EBFONT } ,
#endif

#ifdef EBUSY
    { "EBUSY" , EBUSY } ,
#endif

#ifdef ECANCELED
    { "ECANCELED" , ECANCELED } ,
#endif

#ifdef ECHILD
    { "ECHILD" , ECHILD } ,
#endif

#ifdef ECHRNG
    { "ECHRNG" , ECHRNG } ,
#endif

#ifdef ECOMM
    { "ECOMM" , ECOMM } ,
#endif

#ifdef ECONNABORTED
    { "ECONNABORTED" , ECONNABORTED } ,
#endif

#ifdef ECONNREFUSED
    { "ECONNREFUSED" , ECONNREFUSED } ,
#endif

#ifdef ECONNRESET
    { "ECONNRESET" , ECONNRESET } ,
#endif

#ifdef EDEADLK
    { "EDEADLK" , EDEADLK } ,
#endif

#ifdef EDEADLOCK
    { "EDEADLOCK" , EDEADLOCK } ,
#endif

#ifdef EDESTADDRREQ
    { "EDESTADDRREQ" , EDESTADDRREQ } ,
#endif

#ifdef EDOTDOT
    { "EDOTDOT" , EDOTDOT } ,
#endif

#ifdef EDQUOT
    { "EDQUOT" , EDQUOT } ,
#endif

#ifdef EEXIST
    { "EEXIST" , EEXIST } ,
#endif

#ifdef EFAULT
    { "EFAULT" , EFAULT } ,
#endif

#ifdef EFBIG
    { "EFBIG" , EFBIG } ,
#endif

#ifdef EHOSTDOWN
    { "EHOSTDOWN" , EHOSTDOWN } ,
#endif

#ifdef EHOSTUNREACH
    { "EHOSTUNREACH" , EHOSTUNREACH } ,
#endif

#ifdef EIDRM
    { "EIDRM" , EIDRM } ,
#endif

#ifdef EILSEQ
    { "EILSEQ" , EILSEQ } ,
#endif

#ifdef EINPROGRESS
    { "EINPROGRESS" , EINPROGRESS } ,
#endif

#ifdef EINTR
    { "EINTR" , EINTR } ,
#endif

#ifdef EINVAL
    { "EINVAL" , EINVAL } ,
#endif

#ifdef EIO
    { "EIO" , EIO } ,
#endif

#ifdef EISCONN
    { "EISCONN" , EISCONN } ,
#endif

#ifdef EISDIR
    { "EISDIR" , EISDIR } ,
#endif

#ifdef EISNAM
    { "EISNAM" , EISNAM } ,
#endif

#ifdef EKEYEXPIRED
    { "EKEYEXPIRED" , EKEYEXPIRED } ,
#endif

#ifdef EKEYREJECTED
    { "EKEYREJECTED" , EKEYREJECTED } ,
#endif

#ifdef EKEYREVOKED
    { "EKEYREVOKED" , EKEYREVOKED } ,
#endif

#ifdef ELIBACC
    { "ELIBACC" , ELIBACC } ,
#endif

#ifdef ELIBBAD
    { "ELIBBAD" , ELIBBAD } ,
#endif

#ifdef ELIBEXEC
    { "ELIBEXEC" , ELIBEXEC } ,
#endif

#ifdef ELIBMAX
    { "ELIBMAX" , ELIBMAX } ,
#endif

#ifdef ELIBSCN
    { "ELIBSCN" , ELIBSCN } ,
#endif

#ifdef ELNRNG
    { "ELNRNG" , ELNRNG } ,
#endif

#ifdef ELOOP
    { "ELOOP" , ELOOP } ,
#endif

#ifdef EMEDIUMTYPE
    { "EMEDIUMTYPE" , EMEDIUMTYPE } ,
#endif

#ifdef EMFILE
    { "EMFILE" , EMFILE } ,
#endif

#ifdef EMLINK
    { "EMLINK" , EMLINK } ,
#endif

#ifdef EMSGSIZE
    { "EMSGSIZE" , EMSGSIZE } ,
#endif

#ifdef EMULTIHOP
    { "EMULTIHOP" , EMULTIHOP } ,
#endif

#ifdef ENAMETOOLONG
    { "ENAMETOOLONG" , ENAMETOOLONG } ,
#endif

#ifdef ENAVAIL
    { "ENAVAIL" , ENAVAIL } ,
#endif

#ifdef ENETDOWN
    { "ENETDOWN" , ENETDOWN } ,
#endif

#ifdef ENETRESET
    { "ENETRESET" , ENETRESET } ,
#endif

#ifdef ENETUNREACH
    { "ENETUNREACH" , ENETUNREACH } ,
#endif

#ifdef ENFILE
    { "ENFILE" , ENFILE } ,
#endif

#ifdef ENOANO
    { "ENOANO" , ENOANO } ,
#endif

#ifdef ENOBUFS
    { "ENOBUFS" , ENOBUFS } ,
#endif

#ifdef ENOCSI
    { "ENOCSI" , ENOCSI } ,
#endif

#ifdef ENODATA
    { "ENODATA" , ENODATA } ,
#endif

#ifdef ENODEV
    { "ENODEV" , ENODEV } ,
#endif

#ifdef ENOENT
    { "ENOENT" , ENOENT } ,
#endif

#ifdef ENOEXEC
    { "ENOEXEC" , ENOEXEC } ,
#endif

#ifdef ENOKEY
    { "ENOKEY" , ENOKEY } ,
#endif

#ifdef ENOLCK
    { "ENOLCK" , ENOLCK } ,
#endif

#ifdef ENOLINK
    { "ENOLINK" , ENOLINK } ,
#endif

#ifdef ENOMEDIUM
    { "ENOMEDIUM" , ENOMEDIUM } ,
#endif

#ifdef ENOMEM
    { "ENOMEM" , ENOMEM } ,
#endif

#ifdef ENOMSG
    { "ENOMSG" , ENOMSG } ,
#endif

#ifdef ENONET
    { "ENONET" , ENONET } ,
#endif

#ifdef ENOPKG
    { "ENOPKG" , ENOPKG } ,
#endif

#ifdef ENOPROTOOPT
    { "ENOPROTOOPT" , ENOPROTOOPT } ,
#endif

#ifdef ENOSPC
    { "ENOSPC" , ENOSPC } ,
#endif

#ifdef ENOSR
    { "ENOSR" , ENOSR } ,
#endif

#ifdef ENOSTR
    { "ENOSTR" , ENOSTR } ,
#endif

#ifdef ENOSYS
    { "ENOSYS" , ENOSYS } ,
#endif

#ifdef ENOTBLK
    { "ENOTBLK" , ENOTBLK } ,
#endif

#ifdef ENOTCONN
    { "ENOTCONN" , ENOTCONN } ,
#endif

#ifdef ENOTDIR
    { "ENOTDIR" , ENOTDIR } ,
#endif

#ifdef ENOTEMPTY
    { "ENOTEMPTY" , ENOTEMPTY } ,
#endif

#ifdef ENOTNAM
    { "ENOTNAM" , ENOTNAM } ,
#endif

#ifdef ENOTSOCK
    { "ENOTSOCK" , ENOTSOCK } ,
#endif

#ifdef ENOTTY
    { "ENOTTY" , ENOTTY } ,
#endif

#ifdef ENOTUNIQ
    { "ENOTUNIQ" , ENOTUNIQ } ,
#endif

#ifdef ENXIO
    { "ENXIO" , ENXIO } ,
#endif

#ifdef EOPNOTSUPP
    { "EOPNOTSUPP" , EOPNOTSUPP } ,
#endif

#ifdef EOVERFLOW
    { "EOVERFLOW" , EOVERFLOW } ,
#endif

#ifdef EPERM
    { "EPERM" , EPERM } ,
#endif

#ifdef EPFNOSUPPORT
    { "EPFNOSUPPORT" , EPFNOSUPPORT } ,
#endif

#ifdef EPIPE
    { "EPIPE" , EPIPE } ,
#endif

#ifdef EPROTO
    { "EPROTO" , EPROTO } ,
#endif

#ifdef EPROTONOSUPPORT
    { "EPROTONOSUPPORT" , EPROTONOSUPPORT } ,
#endif

#ifdef EPROTOTYPE
    { "EPROTOTYPE" , EPROTOTYPE } ,
#endif

#ifdef EREMCHG
    { "EREMCHG" , EREMCHG } ,
#endif

#ifdef EREMOTE
    { "EREMOTE" , EREMOTE } ,
#endif

#ifdef EREMOTEIO
    { "EREMOTEIO" , EREMOTEIO } ,
#endif

#ifdef ERESTART
    { "ERESTART" , ERESTART } ,
#endif

#ifdef EROFS
    { "EROFS" , EROFS } ,
#endif

#ifdef ESHUTDOWN
    { "ESHUTDOWN" , ESHUTDOWN } ,
#endif

#ifdef ESOCKTNOSUPPORT
    { "ESOCKTNOSUPPORT" , ESOCKTNOSUPPORT } ,
#endif

#ifdef ESPIPE
    { "ESPIPE" , ESPIPE } ,
#endif

#ifdef ESRCH
    { "ESRCH" , ESRCH } ,
#endif

#ifdef ESRMNT
    { "ESRMNT" , ESRMNT } ,
#endif

#ifdef ESTALE
    { "ESTALE" , ESTALE } ,
#endif

#ifdef ESTRPIPE
    { "ESTRPIPE" , ESTRPIPE } ,
#endif

#ifdef ETIME
    { "ETIME" , ETIME } ,
#endif

#ifdef ETIMEDOUT
    { "ETIMEDOUT" , ETIMEDOUT } ,
#endif

#ifdef ETOOMANYREFS
    { "ETOOMANYREFS" , ETOOMANYREFS } ,
#endif

#ifdef ETXTBSY
    { "ETXTBSY" , ETXTBSY } ,
#endif

#ifdef EUCLEAN
    { "EUCLEAN" , EUCLEAN } ,
#endif

#ifdef EUNATCH
    { "EUNATCH" , EUNATCH } ,
#endif

#ifdef EUSERS
    { "EUSERS" , EUSERS } ,
#endif

#ifdef EXDEV
    { "EXDEV" , EXDEV } ,
#endif

#ifdef EXFULL
    { "EXFULL" , EXFULL } ,
#endif
    { NULL , NULL }
};

