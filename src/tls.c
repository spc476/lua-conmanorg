/***************************************************************************
*
* Copyright 2018 by Sean Conner.
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

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include <tls.h>
#include <lua.h>
#include <lauxlib.h>

/*---------------------------------------------------------------------------
; See <http://boston.conman.org/2021/01/01.1> to see why the minimum support
; API is now 20180210 (which was released for LibreSSL 2.7.0).  For more
; background information, check <http://boston.conman.org/2018/08/07.1>.
;----------------------------------------------------------------------------*/

#if TLS_API < 20180210
#  error This require TLS_API 20180210 or higher.
#endif

#if LUA_VERSION_NUM == 501
#  define lua_getuservalue(state,idx) lua_getfenv((state),(idx))
#  define lua_setuservalue(state,idx) lua_setfenv((state),(idx))
#  define luaL_setfuncs(state,reg,_)  luaL_register((state),NULL,(reg))
#  define luaL_newlib(state,reg)      luaL_register((state),"org.conman.tls",(reg))
#endif

#define TYPE_TLS_CONF   "org.conman.tls:CONF"
#define TYPE_TLS        "org.conman.tls:TLS"
#define TYPE_TLS_MEM    "org.conman.tls:TLS_MEM"

/**************************************************************************/

struct Ltlsmem
{
  size_t   len;
  uint8_t *buf;
};

struct strint
{
  char const *const text;
  int  const        value;
};

/**************************************************************************/

static struct strint const m_tls_consts[] =
{
  { "ERROR"                             , -1                                    } ,
  { "BUFFERSIZE"                        , LUAL_BUFFERSIZE                       } ,
  { "API"                               , TLS_API                               } ,
  { "WANT_INPUT"                        , TLS_WANT_POLLIN                       } ,
  { "WANT_OUTPUT"                       , TLS_WANT_POLLOUT                      } ,
  { "MAX_SESSION_ID_LENGTH"             , TLS_MAX_SESSION_ID_LENGTH             } ,
  { "TICKET_KEY_SIZE"                   , TLS_TICKET_KEY_SIZE                   } ,
  { "OCSP_CERT_GOOD"                    , TLS_OCSP_CERT_GOOD                    } ,
  { "OCSP_CERT_REVOKED"                 , TLS_OCSP_CERT_REVOKED                 } ,
  { "OCSP_CERT_UNKNOWN"                 , TLS_OCSP_CERT_UNKNOWN                 } ,
  { "CRL_REASON_AA_COMPROMISE"          , TLS_CRL_REASON_AA_COMPROMISE          } ,
  { "CRL_REASON_AFFILIATION_CHANGED"    , TLS_CRL_REASON_AFFILIATION_CHANGED    } ,
  { "CRL_REASON_CA_COMPROMISE"          , TLS_CRL_REASON_CA_COMPROMISE          } ,
  { "CRL_REASON_CERTIFICATE_HOLD"       , TLS_CRL_REASON_CERTIFICATE_HOLD       } ,
  { "CRL_REASON_CESSATION_OF_OPERATION" , TLS_CRL_REASON_CESSATION_OF_OPERATION } ,
  { "CRL_REASON_KEY_COMPROMISE"         , TLS_CRL_REASON_KEY_COMPROMISE         } ,
  { "CRL_REASON_PRIVILEGE_WITHDRAWN"    , TLS_CRL_REASON_PRIVILEGE_WITHDRAWN    } ,
  { "CRL_REASON_REMOVE_FROM_CRL"        , TLS_CRL_REASON_REMOVE_FROM_CRL        } ,
  { "CRL_REASON_SUPERSEDED"             , TLS_CRL_REASON_SUPERSEDED             } ,
  { "CRL_REASON_UNSPECIFIED"            , TLS_CRL_REASON_UNSPECIFIED            } ,
  { "OCSP_RESPONSE_INTERNALERROR"       , TLS_OCSP_RESPONSE_INTERNALERROR       } ,
  { "OCSP_RESPONSE_MALFORMED"           , TLS_OCSP_RESPONSE_MALFORMED           } ,
  { "OCSP_RESPONSE_SIGREQUIRED"         , TLS_OCSP_RESPONSE_SIGREQUIRED         } ,
  { "OCSP_RESPONSE_SUCCESSFUL"          , TLS_OCSP_RESPONSE_SUCCESSFUL          } ,
  { "OCSP_RESPONSE_TRYLATER"            , TLS_OCSP_RESPONSE_TRYLATER            } ,
  { "OCSP_RESPONSE_UNAUTHORIZED"        , TLS_OCSP_RESPONSE_UNAUTHORIZED        } ,
  { NULL                                , 0                                     }
};

/**************************************************************************/

static ssize_t Xtls_read(struct tls *tls,void *buf,size_t buflen,void *cb_arg)
{
  lua_State  *L = cb_arg;
  char const *p;
  size_t      advise;
  size_t      len;
  
  /*---------------------------------------------------------
  ; Usage:      data,advise = readf(ctx,len,userdata)
  ; Desc:       Callback to read data
  ; Input:      ctx (userdata/TLS) TLS context data
  ;             len (number) amount of data to read
  ;             userdata (any) for readf() use
  ; Return:     data (string) data to be passed to TLS (note:  if no
  ;             | data, return ""
  ;             advise (number)
  ;                     *  0 - okay
  ;                     * tls.ERROR
  ;                     * tls.WANT_INPUT
  ;                     * tls.WANT_OUTPUT
  ; Note:       Do NOT return more data than is asked for.
  ;----------------------------------------------------------*/
  
  lua_pushlightuserdata(L,tls);
  lua_gettable(L,LUA_REGISTRYINDEX);
  lua_getfield(L,-1,"_readf");
  lua_getfield(L,-2,"_ctx");
  lua_pushinteger(L,buflen);
  lua_getfield(L,-4,"_userdata");
  lua_call(L,3,2);
  
  p      = lua_tolstring(L,-2,&len);
  advise = lua_tonumber(L,-1);
  
  if (p == NULL)
    return advise;
    
  assert(len <= buflen);
  
  memcpy(buf,p,len);
  lua_pop(L,2);
  return len;
}

/**************************************************************************/

static ssize_t Xtls_write(struct tls *tls,void const *buf,size_t buflen,void *cb_arg)
{
  lua_State *L = cb_arg;
  ssize_t    len;
  
  /*--------------------------------------------------------------
  ; Usage:      len = writef(ctx,buffer,userdata)
  ; Desc:       Callback to write data
  ; Input:      ctx (userdata/TLS) TLS context data
  ;             buffer (string) data to write
  ;             userdata (any) for writef() use
  ; Return:     len (number) number of bytes written or
  ;                     * tls.ERROR
  ;                     * tls.WANT_INPUT
  ;                     * tls.WANT_OUTPUT
  ;---------------------------------------------------------------*/
  
  lua_pushlightuserdata(L,tls);
  lua_gettable(L,LUA_REGISTRYINDEX);
  lua_getfield(L,-1,"_writef");
  lua_getfield(L,-2,"_ctx");
  lua_pushlstring(L,buf,buflen);
  lua_getfield(L,-4,"_userdata");
  lua_call(L,3,1);
  
  len = lua_tonumber(L,-1);
  lua_pop(L,2);
  return len;
}

/**************************************************************************
*
*                             TLS CONFIG OBJECT
*
***************************************************************************/

static int Ltlsconf___tostring(lua_State *L)
{
  lua_pushfstring(L,"tlsconf: %p",luaL_checkudata(L,1,TYPE_TLS_CONF));
  return 1;
}

/**************************************************************************/

static int Ltlsconf___gc(lua_State *L)
{
  struct tls_config **tlsconf = luaL_checkudata(L,1,TYPE_TLS_CONF);
  if (*tlsconf != NULL)
  {
    tls_config_free(*tlsconf);
    *tlsconf = NULL;
  }
  return 0;
}

/**************************************************************************
* Usage:
* Desc:
* Input:
* Return:
***************************************************************************/

static int Ltlsconf_ca_file(lua_State *L)
{
  lua_pushboolean(
          L,
          tls_config_set_ca_file(
                  *(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF),
                  luaL_checkstring(L,2)
          ) == 0);
  return 1;
}

/**************************************************************************
* Usage:
* Desc:
* Input:
* Return:
***************************************************************************/

static int Ltlsconf_ca_mem(lua_State *L)
{
  struct Ltlsmem *ca = luaL_checkudata(L,2,TYPE_TLS_MEM);
  
  lua_pushboolean(
          L,
          tls_config_set_ca_mem(
                  *(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF),
                  ca->buf , ca->len
          ) == 0);
  return 1;
}

/**************************************************************************
* Usage:        okay = config:ca_path(path)
* Desc:         Configure the CA path
* Input:        path (string)
* Return:       okay (boolean)
***************************************************************************/

static int Ltlsconf_ca_path(lua_State *L)
{
  lua_pushboolean(
          L,
          tls_config_set_ca_path(
                  *(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF),
                  luaL_checkstring(L,2)
  ) == 0);
  return 1;
}

/**************************************************************************
* Usage:
* Desc:
* Input:
* Return:
***************************************************************************/

static int Ltlsconf_cert_file(lua_State *L)
{
  lua_pushboolean(
          L,
          tls_config_set_cert_file(
                  *(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF),
                  luaL_checkstring(L,2)
          ) == 0);
  return 1;
}

/**************************************************************************
* Usage:
* Desc:
* Input:
* Return:
***************************************************************************/

static int Ltlsconf_cert_mem(lua_State *L)
{
  struct Ltlsmem *cert = luaL_checkudata(L,2,TYPE_TLS_MEM);
  
  lua_pushboolean(
          L,
          tls_config_set_cert_mem(
                  *(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF),
                  cert->buf , cert->len
          ) == 0);
  return 1;
}

/**************************************************************************
* Usage:
* Desc:
* Input:
* Return:
***************************************************************************/

static int Ltlsconf_ciphers(lua_State *L)
{
  lua_pushboolean(
          L,
          tls_config_set_ciphers(
                  *(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF),
                  luaL_checkstring(L,2)
          ) == 0);
  return 1;
}

/**************************************************************************
* Usage:
* Desc:
* Input:
* Return:
***************************************************************************/

static int Ltlsconf_clear_keys(lua_State *L)
{
  tls_config_clear_keys(*(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF));
  return 0;
}

/**************************************************************************
* Usage:
* Desc:
* Input:
* Return:
***************************************************************************/

static int Ltlsconf_dheparams(lua_State *L)
{
  lua_pushboolean(
          L,
          tls_config_set_dheparams(
                  *(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF),
                  luaL_checkstring(L,2)
          ) == 0);
  return 1;
}

/**************************************************************************
* Usage:
* Desc:
* Input:
* Return:
***************************************************************************/

static int Ltlsconf_ecdhecurve(lua_State *L)
{
  lua_pushboolean(
          L,
          tls_config_set_ecdhecurve(
                  *(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF),
                  luaL_checkstring(L,2)
          ) == 0);
  return 1;
}

/**************************************************************************
* Usage:        errmsg = config:error()
* Desc:         Return the last error on the configuration context
* Return:       errmsg (string) error message
* Note:         To retrive the error from the following functions,
*               use this function.
***************************************************************************/

static int Ltlsconf_error(lua_State *L)
{
  lua_pushstring(L,tls_config_error(*(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF)));
  return 1;
}

/**************************************************************************
* Usage:
* Desc:
* Input:
* Return:
***************************************************************************/

static int Ltlsconf_insecure_no_verify_cert(lua_State *L)
{
  tls_config_insecure_noverifycert(*(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF));
  return 0;
}

/**************************************************************************
* Usage:
* Desc:
* Input:
* Return:
***************************************************************************/

static int Ltlsconf_insecure_no_verify_name(lua_State *L)
{
  tls_config_insecure_noverifyname(*(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF));
  return 0;
}

/**************************************************************************
* Usage:
* Desc:
* Input:
* Return:
***************************************************************************/

static int Ltlsconf_insecure_no_verify_time(lua_State *L)
{
  tls_config_insecure_noverifytime(*(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF));
  return 0;
}

/**************************************************************************
* Usage:
* Desc:
* Input:
* Return:
***************************************************************************/

static int Ltlsconf_key_file(lua_State *L)
{
  lua_pushboolean(
          L,
          tls_config_set_key_file(
                  *(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF),
                  luaL_checkstring(L,2)
          ) == 0);
  return 1;
}

/**************************************************************************
* Usage:
* Desc:
* Input:
* Return:
***************************************************************************/

static int Ltlsconf_key_mem(lua_State *L)
{
  struct Ltlsmem *key = luaL_checkudata(L,2,TYPE_TLS_MEM);
  
  lua_pushboolean(
          L,
          tls_config_set_key_mem(
                  *(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF),
                  key->buf , key->len
          ) == 0);
  return 1;
}

/**************************************************************************
* Usage:        config:prefer_ciphers_client()
* Desc:         Prefer to use the client list of ciphers
***************************************************************************/

static int Ltlsconf_prefer_ciphers_client(lua_State *L)
{
  tls_config_prefer_ciphers_client(*(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF));
  return 0;
}

/**************************************************************************
* Usage:        config:prefer_ciphers_server()
* Desc:         Prefer to use the server list of ciphers
***************************************************************************/

static int Ltlsconf_prefer_ciphers_server(lua_State *L)
{
  tls_config_prefer_ciphers_server(*(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF));
  return 0;
}

/**************************************************************************
* Usage:        okay = config:protocols(protocols)
* Desc:         Define the protocols to use
* Input:        protocols (string) comma or colon separated list of
*                       * 'tlsv1.0'
*                       * 'tlsv1.1'
*                       * 'tlsv1.2'
*			* 'tlsv1.3'
*                       * 'all'
*                       * 'secure'  (same as 'tlsv1.2,tlsv1.3')
*                       * 'default' (same as 'secure')
*                       * 'legacy'  (same as 'all')
* Return:       okay (boolean) true of okay, false if error
* Note:         Use config:error() to return the error string
***************************************************************************/

static int Ltlsconf_protocols(lua_State *L)
{
  struct tls_config **tlsconf  = luaL_checkudata(L,1,TYPE_TLS_CONF);
  char const         *protostr = luaL_checkstring(L,2);
  uint32_t            proto;
  
  if (tls_config_parse_protocols(&proto,protostr) != 0)
    lua_pushboolean(L,false);
  else
  {
    tls_config_set_protocols(*tlsconf,proto);
    lua_pushboolean(L,true);
  }
  return 1;
}

/**************************************************************************
* Usage:
* Desc:
* Input:
* Return:
***************************************************************************/

static int Ltlsconf_verify(lua_State *L)
{
  tls_config_verify(*(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF));
  return 0;
}

/**************************************************************************
* Usage:
* Desc:
* Input:
* Return:
***************************************************************************/

static int Ltlsconf_verify_client(lua_State *L)
{
  tls_config_verify_client(*(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF));
  return 0;
}

/**************************************************************************
* Usage:
* Desc:
* Input:
* Return:
***************************************************************************/

static int Ltlsconf_verify_client_optional(lua_State *L)
{
  tls_config_verify_client_optional(*(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF));
  return 0;
}

/**************************************************************************
* Usage:
* Desc:
* Input:
* Return:
***************************************************************************/

static int Ltlsconf_verify_depth(lua_State *L)
{
  lua_pushboolean(
          L,
          tls_config_set_verify_depth(
                  *(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF),
                  lua_tointeger(L,2)
          ) == 0);
  return 1;
}

/**************************************************************************
* Usage:
* Desc:
* Input:
* Return:
***************************************************************************/

static int Ltlsconf_keypair_file(lua_State *L)
{
  lua_pushboolean(
          L,
          tls_config_set_keypair_file(
                  *(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF),
                  luaL_checkstring(L,2),
                  luaL_checkstring(L,3)
          ) == 0);
  return 1;
}

/**************************************************************************
* Usage:
* Desc:
* Input:
* Return:
***************************************************************************/

static int Ltlsconf_keypair_mem(lua_State *L)
{
  struct Ltlsmem *cert = luaL_checkudata(L,2,TYPE_TLS_MEM);
  struct Ltlsmem *key  = luaL_checkudata(L,3,TYPE_TLS_MEM);
  
  lua_pushboolean(
          L,
          tls_config_set_keypair_mem(
                  *(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF),
                  cert->buf , cert->len ,
                  key->buf  , key->len
          ) == 0);
  return 1;
}

/**************************************************************************
* Usage:        okay = config:add_keypair_file(cert_file,key_file)
* Desc;         Add the cert and key file pair
* Input:        cert_file (string)
*               key_file (string)
* Return:       okay (boolean)
***************************************************************************/

static int Ltlsconf_add_keypair_file(lua_State *L)
{
  lua_pushboolean(
          L,
          tls_config_add_keypair_file(
                  *(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF),
                  luaL_checkstring(L,2),
                  luaL_checkstring(L,3)
          ) == 0);
  return 1;
}

/**************************************************************************
* Usage:
* Desc:
* Input:
* Return:
***************************************************************************/

static int Ltlsconf_add_keypair_mem(lua_State *L)
{
  struct Ltlsmem *cert = luaL_checkudata(L,2,TYPE_TLS_MEM);
  struct Ltlsmem *key  = luaL_checkudata(L,3,TYPE_TLS_MEM);
  
  lua_pushboolean(
          L,
          tls_config_add_keypair_mem(
                  *(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF),
                  cert->buf , cert->len ,
                  key->buf  , key->len
          ) == 0);
  return 1;
}

/**************************************************************************
* Usage:        okay = config:alpn(list)
* Desc:         Configure the ALPN protoclls
* Input:        list (string) comma separated list of protocols, in order
*               of preference.
* Return:       okay (boolean)
***************************************************************************/

static int Ltlsconf_alpn(lua_State *L)
{
  lua_pushboolean(
          L,
          tls_config_set_alpn(
                  *(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF),
                  luaL_checkstring(L,2)
  ) == 0);
  return 1;
}

/**************************************************************************
* Usage:
* Desc:
* Input:
* Return:
***************************************************************************/

static int Ltlsconf_add_keypair_ocsp_file(lua_State *L)
{
  lua_pushboolean(
          L,
          tls_config_add_keypair_ocsp_file(
                *(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF),
                luaL_checkstring(L,2),
                luaL_checkstring(L,3),
                luaL_checkstring(L,4)
          ) == 0);
  return 1;
}

/**************************************************************************
* Usage:
* Desc:
* Input:
* Return:
***************************************************************************/

static int Ltlsconf_add_keypair_ocsp_mem(lua_State *L)
{
  struct Ltlsmem *cert   = luaL_checkudata(L,2,TYPE_TLS_MEM);
  struct Ltlsmem *key    = luaL_checkudata(L,3,TYPE_TLS_MEM);
  struct Ltlsmem *staple = luaL_checkudata(L,4,TYPE_TLS_MEM);
  
  lua_pushboolean(
          L,
          tls_config_add_keypair_ocsp_mem(
                  *(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF),
                  cert->buf   , cert->len ,
                  key->buf    , key->len  ,
                  staple->buf , staple->len
          ) == 0);
  return 1;
}

/**************************************************************************
* Usage:
* Desc:
* Input:
* Return:
***************************************************************************/

static int Ltlsconf_add_ticket_key(lua_State *L)
{
  /*----------------------------------------------------------------------
  ; XXX Until I figure out what tls_confif_add_ticket_key() actually does,
  ; I'm leaving it undefiend.  My main question is---what does it do with
  ; the key parameter?  Is it modified?
  ;-----------------------------------------------------------------------*/
  
  lua_pushboolean(L,false);
  return 1;
}

/**************************************************************************
* Usage:
* Desc:
* Input:
* Return:
***************************************************************************/

static int Ltlsconf_keypair_ocsp_file(lua_State *L)
{
  lua_pushboolean(
          L,
          tls_config_set_keypair_ocsp_file(
                  *(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF),
                  luaL_checkstring(L,2),
                  luaL_checkstring(L,3),
                  luaL_checkstring(L,4)
          ) == 0);
  return 1;
}

/**************************************************************************
* Usage:
* Desc:
* Input:
* Return:
***************************************************************************/

static int Ltlsconf_keypair_ocsp_mem(lua_State *L)
{
  struct Ltlsmem *cert   = luaL_checkudata(L,2,TYPE_TLS_MEM);
  struct Ltlsmem *key    = luaL_checkudata(L,3,TYPE_TLS_MEM);
  struct Ltlsmem *staple = luaL_checkudata(L,4,TYPE_TLS_MEM);
  
  lua_pushboolean(
          L,
          tls_config_set_keypair_ocsp_mem(
                  *(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF),
                  cert->buf   , cert->len ,
                  key->buf    , key->len  ,
                  staple->buf , staple->len
          ) == 0);
  return 1;
}

/**************************************************************************
* Usage:
* Desc:
* Input:
* Return:
***************************************************************************/

static int Ltlsconf_ocsp_require_stapling(lua_State *L)
{
  tls_config_ocsp_require_stapling(*(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF));
  return 0;
}

/**************************************************************************
* Usage:
* Desc:
* Input:
* Return:
***************************************************************************/

static int Ltlsconf_ocsp_staple_file(lua_State *L)
{
  lua_pushboolean(
          L,
          tls_config_set_ocsp_staple_file(
                  *(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF),
                  luaL_checkstring(L,2)
          ) == 0);
  return 1;
}

/**************************************************************************
* Usage:
* Desc:
* Input:
* Return:
***************************************************************************/

static int Ltlsconf_ocsp_staple_mem(lua_State *L)
{
  struct Ltlsmem *staple = luaL_checkudata(L,2,TYPE_TLS_MEM);
  
  lua_pushboolean(
          L,
          tls_config_set_ocsp_staple_mem(
                  *(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF),
                  staple->buf , staple->len
          ) == 0);
  return 1;
}

/**************************************************************************
* Usage:
* Desc:
* Input:
* Return:
***************************************************************************/

static int Ltlsconf_session_id(lua_State *L)
{
  size_t len;
  void const *sid = luaL_checklstring(L,2,&len);
  
  lua_pushboolean(
          L,
          tls_config_set_session_id(
                  *(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF),
                  sid,len
          ) == 0);
  return 1;
}

/**************************************************************************
* Usage:
* Desc:
* Input:
* Return:
***************************************************************************/

static int Ltlsconf_session_lifetime(lua_State *L)
{
  lua_pushboolean(
          L,
          tls_config_set_session_lifetime(
                  *(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF),
                  luaL_checkinteger(L,2)
          ) == 0);
  return 1;
}

/**************************************************************************
* Usage:
* Desc:
* Input:
* Return:
***************************************************************************/

static int Ltlsconf_crl_file(lua_State *L)
{
  lua_pushboolean(
          L,
          tls_config_set_crl_file(
                  *(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF),
                  luaL_checkstring(L,2)
          ) == 0);
  return 1;
}

/**************************************************************************
* Usage:
* Desc:
* Input:
* Return:
***************************************************************************/

static int Ltlsconf_crl_mem(lua_State *L)
{
  struct Ltlsmem *crl = luaL_checkudata(L,2,TYPE_TLS_MEM);
  
  lua_pushboolean(
          L,
          tls_config_set_crl_mem(
                  *(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF),
                  crl->buf , crl->len
          ) == 0);
  return 1;
}

/**************************************************************************
* Usage:
* Desc:
* Input:
* Return:
***************************************************************************/

static int Ltlsconf_ecdhecurves(lua_State *L)
{
  lua_pushboolean(
          L,
          tls_config_set_ecdhecurves(
                  *(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF),
                  luaL_checkstring(L,2)
          ) == 0);
  return 1;
}

/**************************************************************************
* Usage:
* Desc:
* Input:
* Return:
***************************************************************************/

static int Ltlsconf_session_fd(lua_State *L)
{
  lua_pushboolean(
          L,
          tls_config_set_session_fd(
                  *(struct tls_config **)luaL_checkudata(L,1,TYPE_TLS_CONF),
                  luaL_checkinteger(L,2)
          ) == 0);
  return 1;
}

/**************************************************************************
*
*                            TLS CONTEXT OBJECT
*
***************************************************************************/

#if LUA_VERSION_NUM >= 502
  static int Ltls____next(lua_State *L)
  {
    return lua_next(L,1) ? 2 : 0;
  }
  
  static int Ltls___pairs(lua_State *L)
  {
    lua_pushcfunction(L,Ltls____next);
    lua_getuservalue(L,1);
    lua_pushnil(L);
    return 3;
  }
#endif

/**************************************************************************/

static int Ltls___index(lua_State *L)
{
  lua_getmetatable(L,1);
  lua_pushvalue(L,-2);
  lua_gettable(L,-2);
  
  if (lua_isnil(L,-1))
  {
    lua_pop(L,2);
    lua_getuservalue(L,1);
    lua_pushvalue(L,-2);
    lua_gettable(L,-2);
  }
  
  return 1;
}

/**************************************************************************/

static int Ltls___newindex(lua_State *L)
{
  lua_getuservalue(L,1);
  lua_replace(L,-4);
  lua_settable(L,-3);
  return 0;
}

/**************************************************************************/

static int Ltls___tostring(lua_State *L)
{
  lua_pushfstring(L,"tls: %p",luaL_checkudata(L,1,TYPE_TLS));
  return 1;
}

/**************************************************************************/

static int Ltls___gc(lua_State *L)
{
  struct tls **tls = luaL_checkudata(L,1,TYPE_TLS);
  if (*tls != NULL)
  {
    lua_pushlightuserdata(L,*tls);
    lua_pushnil(L);
    lua_settable(L,LUA_REGISTRYINDEX);
    tls_free(*tls);
    *tls = NULL;
  }
  return 0;
}

/**************************************************************************
* Usage:
* Desc:
* Input:
* Return:
***************************************************************************/

static int Ltls_accept_fds(lua_State *L)
{
  struct tls **ctls;
  struct tls **tls     = luaL_checkudata(L,1,TYPE_TLS);
  int          fdread  = luaL_checkinteger(L,2);
  int          fdwrite = luaL_checkinteger(L,3);
  
  ctls = lua_newuserdata(L,sizeof(struct tls *));
  
  if (tls_accept_fds(*tls,ctls,fdread,fdwrite) != 0)
    lua_pushnil(L);
  else
  {
    luaL_getmetatable(L,TYPE_TLS);
    lua_setmetatable(L,-2);
    lua_createtable(L,0,0);
    lua_setuservalue(L,-2);
  }
  
  return 1;
}

/**************************************************************************
* Usage:
* Desc:
* Input:
* Return:
***************************************************************************/

static int Ltls_accept_socket(lua_State *L)
{
  struct tls **ctls;
  struct tls **tls  = luaL_checkudata(L,1,TYPE_TLS);
  int          sock = luaL_checkinteger(L,2);
  
  ctls = lua_newuserdata(L,sizeof(struct tls *));
  
  if (tls_accept_socket(*tls,ctls,sock) != 0)
    lua_pushnil(L);
  else
  {
    luaL_getmetatable(L,TYPE_TLS);
    lua_setmetatable(L,-2);
    lua_createtable(L,0,0);
    lua_setuservalue(L,-2);
  }
  
  return 1;
}

/**************************************************************************
* Usage:        advise = ctx:close()
* Desc:         Close a connection.  If TLS is controlling the socket,
*               the socket will be closed as well; othersise, you are
*               still reponsible for closing the socket.
* Return:	advise (integer)
*			* 0 - okay
*			* tls.ERROR
*			* tls.WANT_INPUT
*			* tls.WANT_OUTPUT
*
* Note:		Only upon rc == 0 or tls.ERROR will the reference to the
*		context be removed.  This way, we can still deal with
*		closing out the connection with more I/O.
***************************************************************************/

static int Ltls_close(lua_State *L)
{
  struct tls **tls;
  int          rc;
  
  tls = luaL_checkudata(L,1,TYPE_TLS);
  if (*tls != NULL)
  {
    rc = tls_close(*tls);
    if ((rc != TLS_WANT_POLLIN) && (rc != TLS_WANT_POLLOUT))
      Ltls___gc(L);
    lua_pushinteger(L,rc);
  }
  else
    lua_pushinteger(L,0);
  
  return 1;
}

/**************************************************************************
* Usage:        okay = ctx:configure(conf)
* Desc:         Apply the configuration settings to a TLS context
* Input:        conf (userdata/TLS_CONF) configuration context
* Return:       okay (boolean) true if okay, false if error
* Note:         use ctx:error() to return the error
***************************************************************************/

static int Ltls_configure(lua_State *L)
{
  struct tls        **tls     = luaL_checkudata(L,1,TYPE_TLS);
  struct tls_config **tlsconf = luaL_checkudata(L,2,TYPE_TLS_CONF);
  
  lua_pushboolean(L,tls_configure(*tls,*tlsconf) == 0);
  return 1;
}

/**************************************************************************
* Usage:        cipher = ctx:conn_cipher()
* Desc:         Return the cipher in use with the connection
* Return:       cipher (string) cipher in use.
***************************************************************************/

static int Ltls_conn_cipher(lua_State *L)
{
  lua_pushstring(L,tls_conn_cipher(*(struct tls **)luaL_checkudata(L,1,TYPE_TLS)));
  return 1;
}

/**************************************************************************
* Usage:        version = ctx:conn_version()
* Desc:         Return the TLS version of the connection
* Return:       version (string) TLS version
***************************************************************************/

static int Ltls_conn_version(lua_State *L)
{
  lua_pushstring(L,tls_conn_version(*(struct tls **)luaL_checkudata(L,1,TYPE_TLS)));
  return 1;
}

/**************************************************************************
* Usage:        okay = cts:connect(host,port[,servername])
* Desc:         Connect to the given host
* Input:        host (string) host to connect to
*               port (string number) port to connect to
*               servername (string/optional) canonical server name
* Return:       okay (boolean) true if okay, false if error
* Note:         use ctx:error() to return the error
*
*               TLS will have full control of the socket if this method
*               is used.
*
*               The servername should be used if the host given is an
*               IP address or an alias to the actual servername.
***************************************************************************/

static int Ltls_connect(lua_State *L)
{
  struct tls **tls = luaL_checkudata(L,1,TYPE_TLS);
  int          rc;
  
  if (lua_isnoneornil(L,4))
  {
    rc = tls_connect(
            *tls,
            luaL_checkstring(L,2),
            luaL_checkstring(L,3)
    );
  }
  else
  {
    rc = tls_connect_servername(
            *tls,
            luaL_checkstring(L,2),
            luaL_checkstring(L,3),
            luaL_checkstring(L,4)
    );
  }
  
  lua_pushboolean(L,rc == 0);
  return 1;
}

/**************************************************************************
* Usage:        okay = ctx:connect_fds(fdread,fdwrite,servername)
* Desc:         Initiate TLS over the pair of file descriptors
* Input:        fdread (integer) file descriptor for reading
*               fdwrite (integer) file descriptor for writing
*               servername (string) servername
* Return:       okay (boolean) true if okay, false if error
* Note:         use ctx:error() to return the error
*
*               TLS will have full control of the socket if this method
*               is used.
***************************************************************************/

static int Ltls_connect_fds(lua_State *L)
{
  lua_pushboolean(
          L,
          tls_connect_fds(
                  *(struct tls **)luaL_checkudata(L,1,TYPE_TLS),
                  luaL_checkinteger(L,2),
                  luaL_checkinteger(L,3),
                  luaL_checkstring(L,4)
          ) == 0);
  return 1;
}

/**************************************************************************
* Usage:        okay = ctx:connect_socket(socket,servername)
* Desc:         Initiate TLS over the given socket
* Input         socket (integer) socket file descriptor
*               servername (string) server name
* Return:       okay (boolean) true if okay, false if error
* Note:         use ctx:error() to return the error
*
*               TLS will have full control of the socket if this method
*               is used.
***************************************************************************/

static int Ltls_connect_socket(lua_State *L)
{
  lua_pushboolean(L,tls_connect_socket(
          *(struct tls **)luaL_checkudata(L,1,TYPE_TLS),
          luaL_checkinteger(L,2),
          luaL_checkstring(L,3)
        ) == 0);
  return 1;
}

/**************************************************************************
* Usage:        errmsg = ctx:error()
* Desc:         Return the last error on the TLS context
* Return:       errmsg (string) error message
***************************************************************************/

static int Ltls_error(lua_State *L)
{
  lua_pushstring(L,tls_error(*(struct tls **)luaL_checkudata(L,1,TYPE_TLS)));
  return 1;
}

/**************************************************************************
* Usage:        advise = ctx:handshake()
* Desc:         Check if a TLS handshake has been done
* Return:	advise (integer)
*			* 0 - okay
*			* tls.ERROR
*			* tls.WANT_INPUT
*			* tls.WANT_OUTPUT
*
* Note:         There is no need to call this function unless you
*               absolutely require to know the handshake has happened.
***************************************************************************/

static int Ltls_handshake(lua_State *L)
{
  lua_pushinteger(L,tls_handshake(*(struct tls **)luaL_checkudata(L,1,TYPE_TLS)));
  return 1;
}

/**************************************************************************
* Usage:        exists = ctx:peer_cert_contains_name(name)
* Desc:         Return if the name appears in the SAN or CN of the certificate
* Input:        name (string) name to check for
* Return:       exists (boolean) name exists or not
***************************************************************************/

static int Ltls_peer_cert_contains_name(lua_State *L)
{
  lua_pushboolean(
          L,
          tls_peer_cert_contains_name(
                  *(struct tls **)luaL_checkudata(L,1,TYPE_TLS),
                  luaL_checkstring(L,2)
          ));
  return 1;
}

/**************************************************************************
* Usage:        hash = ctx:peer_cert_hash()
* Desc:         Return the raw has from the peer.
* Return:       hash (string) hash from peer
* Note:         It returns the name of the hash, followed by a ':', followed
*               by the hex string of the hash.  Example:
*                       SHA256:0285e5 ... 1f33
***************************************************************************/

static int Ltls_peer_cert_hash(lua_State *L)
{
  lua_pushstring(L,tls_peer_cert_hash(*(struct tls **)luaL_checkudata(L,1,TYPE_TLS)));
  return 1;
}

/**************************************************************************
* Usage:        issuer = ctx:peer_cert_issuer()
* Desc:         Return the issuer of the certificate
* Return:       issuer (string) Issuer of certificate
***************************************************************************/

static int Ltls_peer_cert_issuer(lua_State *L)
{
  lua_pushstring(L,tls_peer_cert_issuer(*(struct tls **)luaL_checkudata(L,1,TYPE_TLS)));
  return 1;
}

/**************************************************************************
* Usage:        provided = ctx:peer_cert_provided()
* Desc:         Has the peer provided a certificate?
* Return:       provided (boolean) yeah or nay
***************************************************************************/

static int Ltls_peer_cert_provided(lua_State *L)
{
  lua_pushboolean(L,tls_peer_cert_provided(*(struct tls **)luaL_checkudata(L,1,TYPE_TLS)));
  return 1;
}

/**************************************************************************
* Usage:        subject = ctx:peer_cert_subject()
* Desc:         Return the subject of the peer certificate
* Return:       subject (string) subject of peer certificate
***************************************************************************/

static int Ltls_peer_cert_subject(lua_State *L)
{
  lua_pushstring(L,tls_peer_cert_subject(*(struct tls **)luaL_checkudata(L,1,TYPE_TLS)));
  return 1;
}

/**************************************************************************
* Usage:        data,size = tls.read([amount])
* Desc:         Read data from a TLS context
* Input:        amount (integer/optinal) Amount of data to read
* Return:       data (string) data from TLS, "" on EOF or error
*               size (integer) amount of data read, or
*                       * tls.ERROR
*                       * tls.WANT_INPUT
*                       * tls.WANT_OUTPUT
*
* Note:         The default value is tls.BUFFERSIZE.
*
*               If you receive tls.WANT_INPUT or tls.WANT_OUTPUT, you should
*               immedately recall the function.
*
*               Use ctx:error() to return the error.
***************************************************************************/

static int Ltls_read(lua_State *L)
{
  struct tls  **tls = luaL_checkudata(L,1,TYPE_TLS);
  size_t        len = luaL_optinteger(L,2,LUAL_BUFFERSIZE);
  luaL_Buffer   buf;
  char         *p;
  ssize_t       in;
  
  if (len > LUAL_BUFFERSIZE)
    return luaL_error(L,"buffer size %d exceeds maximum %d",len,LUAL_BUFFERSIZE);
    
  luaL_buffinit(L,&buf);
  
  p  = luaL_prepbuffer(&buf);
  in = tls_read(*tls,p,len);
  
  luaL_addsize(&buf,in > 0 ? in : 0);
  luaL_pushresult(&buf);
  lua_pushinteger(L,in);
  return 2;
}

/**************************************************************************
* Usage:        ctx:reset()
* Desc:         Reset a TLS context for reuse
***************************************************************************/

static int Ltls_reset(lua_State *L)
{
  struct tls **tls = luaL_checkudata(L,1,TYPE_TLS);
  
  tls_reset(*tls);
  lua_pushlightuserdata(L,*tls);
  lua_pushnil(L);
  lua_settable(L,LUA_REGISTRYINDEX);
  
  lua_getuservalue(L,1);
  lua_pushnil(L);
  lua_setfield(L,-2,"_ctx");
  lua_pushnil(L);
  lua_setfield(L,-2,"_servername");
  lua_pushnil(L);
  lua_setfield(L,-2,"_userdata");
  lua_pushnil(L);
  lua_setfield(L,-2,"_readf");
  lua_pushnil(L);
  lua_setfield(L,-2,"_writef");
  return 0;
}

/**************************************************************************
* Usage:        amount = tls.write(data)
* Desc:         Write data to a TLS context
* Input:        data (string) data to write
* Return:       amount (integer) amount of data written, or
*                       * tls.ERROR
*                       * tls.WANT_INPUT
*                       * tls.WANT_OUTPUT
* Note:         If you receive tls.WANT_INPUT or tls.WANT_OUTPUT, you should
*               immedately recall the function.
*
*               Use ctx:error() to return the error.
***************************************************************************/

static int Ltls_write(lua_State *L)
{
  struct tls **tls  = luaL_checkudata(L,1,TYPE_TLS);
  size_t       len;
  char const  *data = luaL_checklstring(L,2,&len);
  
  lua_pushinteger(L,tls_write(*tls,data,len));
  return 1;
}

/**************************************************************************
* Usage:        when = ctx:peer_cert_notafter()
* Desc:         Return the time of the end of the certificate validity period
* Return:       when (number) time (per os.time())
***************************************************************************/

static int Ltls_peer_cert_notafter(lua_State *L)
{
  lua_pushinteger(L,tls_peer_cert_notafter(*(struct tls **)luaL_checkudata(L,1,TYPE_TLS)));
  return 1;
}

/**************************************************************************
* Usage:        when = ctx:peer_cert_notbefore()
* Desc:         Return the time of the start of the certificate validity period
* Return:       when (number) time (per os.time())
***************************************************************************/

static int Ltls_peer_cert_notbefore(lua_State *L)
{
  lua_pushinteger(L,tls_peer_cert_notbefore(*(struct tls **)luaL_checkudata(L,1,TYPE_TLS)));
  return 1;
}

/**************************************************************************
* Usage:        cctx = cts:accept_cbs(userdata,readf,writef)
***************************************************************************/

static int Ltls_accept_cbs(lua_State *L)
{
  struct tls **tls = luaL_checkudata(L,1,TYPE_TLS);
  struct tls **ctls;
  
  lua_settop(L,4);
  
  ctls = lua_newuserdata(L,sizeof(struct tls *));
  
  if (tls_accept_cbs(*tls,ctls,Xtls_read,Xtls_write,L) != 0)
    lua_pushnil(L);
  else
  {
    luaL_getmetatable(L,TYPE_TLS);
    lua_setmetatable(L,-2);
    
    lua_pushlightuserdata(L,*ctls); /* key */
    lua_createtable(L,0,0);         /* value */
    lua_pushvalue(L,-3);
    lua_setfield(L,-2,"_ctx");
    lua_pushvalue(L,2);
    lua_setfield(L,-2,"_userdata");
    lua_pushvalue(L,3);
    lua_setfield(L,-2,"_readf");
    lua_pushvalue(L,4);
    lua_setfield(L,-2,"_writef");
    lua_pushvalue(L,-1);
    lua_setuservalue(L,-4);
    
    lua_settable(L,LUA_REGISTRYINDEX);
  }
  
  return 1;
}

/**************************************************************************
* Usage:        alpn = ctx:conn_alpn_selected()
* Desc:         Return the ALPN protocol selected with the peer
* Return:       alpn (string) ALPN protocol
***************************************************************************/

static int Ltls_conn_alpn_selected(lua_State *L)
{
  lua_pushstring(L,tls_conn_alpn_selected(*(struct tls **)luaL_checkudata(L,1,TYPE_TLS)));
  return 1;
}

/**************************************************************************
* Usage:        server = ctx:conn_servername()
* Desc:         Return the servername of the peer
* Return:       server (string) server name
***************************************************************************/

static int Ltls_conn_servername(lua_State *L)
{
  lua_pushstring(L,tls_conn_servername(*(struct tls **)luaL_checkudata(L,1,TYPE_TLS)));
  return 1;
}

/**************************************************************************
* Usage:        okay = ctx:connect_cbs(servername,userdata,readf,writef)
* Desc:         Initiate TLS connection, mediating the low level access
*               | to the underlying transport.
* Input:        servername (string) server name
*               userdata (any) any Lua value passed to the callback functions
*               readf (function) callback to send data into TLS
*               writef (function) callback to send data out of TLS
* Return:       okay (boolean) true if okay, false if error
* Note:         use ctx:error() to return the error
*
*               The underlying transport is NOT controlled by TLS.
*
*               You may be tempted to call coroutine.yield() in the
*               callbacks.  Don't.  It is a fool's errand.
***************************************************************************/

static int Ltls_connect_cbs(lua_State *L)
{
  struct tls **tls = luaL_checkudata(L,1,TYPE_TLS);
  
  lua_settop(L,5);
  
  if (tls_connect_cbs(*tls,Xtls_read,Xtls_write,L,luaL_checkstring(L,2)) != 0)
    lua_pushboolean(L,false);
  else
  {
    lua_pushlightuserdata(L,*tls); /* key */
    lua_getuservalue(L,1);         /* value */
    
    lua_pushvalue(L,1);
    lua_setfield(L,-2,"_ctx");
    lua_pushvalue(L,2);
    lua_setfield(L,-2,"_servername");
    lua_pushvalue(L,3);
    lua_setfield(L,-2,"_userdata");
    lua_pushvalue(L,4);
    lua_setfield(L,-2,"_readf");
    lua_pushvalue(L,5);
    lua_setfield(L,-2,"_writef");
    
    lua_settable(L,LUA_REGISTRYINDEX);
    lua_pushboolean(L,true);
  }
  
  return 1;
}

/**************************************************************************
* Usage:        okay = ctx:ocsp_process_response(data)
* Desc:         Process a raw OCSP response (???)
* Input:        data (binary) ???
* Return:       okay (boolean) success or failure
* Note:         I am unsure of what this routine requires or what it
*               even does.  Use at your own risk.
***************************************************************************/

static int Ltls_ocsp_process_response(lua_State *L)
{
  size_t      len;
  char const *response = luaL_checklstring(L,2,&len);
  
  lua_pushboolean(
          L,
          tls_ocsp_process_response(
                  *(struct tls **)luaL_checkudata(L,1,TYPE_TLS),
                  (unsigned char const *)response,
                  len
          ) == 0);
  return 1;
}

/**************************************************************************
* Usage:        status = ctx:peer_ocsp_cert_status()
* Desc:         Return OCSP certificate status
* Return:       status (number) cert status (tls.OCSP_CERT_*)
***************************************************************************/

static int Ltls_peer_ocsp_cert_status(lua_State *L)
{
  lua_pushinteger(L,tls_peer_ocsp_cert_status(*(struct tls **)luaL_checkudata(L,1,TYPE_TLS)));
  return 1;
}

/**************************************************************************
* Usage:        reason = ctc:peer_ocsp_crl_reason()
* Desc:         Return OCSP certificate revocation reason
* Return:       reason (number) reason (tls.CRL_REASON_*)
***************************************************************************/

static int Ltls_peer_ocsp_crl_reason(lua_State *L)
{
  lua_pushinteger(L,tls_peer_ocsp_crl_reason(*(struct tls **)luaL_checkudata(L,1,TYPE_TLS)));
  return 1;
}

/**************************************************************************
* Usage:        nextupdate = ctx:peer_ocsp_next_update()
* Desc:         Return the OCSP next update time
* Return:       nxtupdate (number) time (per os.time())
***************************************************************************/

static int Ltls_peer_ocsp_next_update(lua_State *L)
{
  lua_pushinteger(
          L,
          tls_peer_ocsp_next_update(*(struct tls **)luaL_checkudata(L,1,TYPE_TLS))
  );
  return 1;
}

/**************************************************************************
* Usage:        status = ctx:peer_ocsp_response_status()
* Desc:         Return the OCSP response status
* Return:       status (number) OCSP response status (tls.OCSP_RESPONSE_*)
***************************************************************************/

static int Ltls_peer_ocsp_response_status(lua_State *L)
{
  lua_pushinteger(L,tls_peer_ocsp_response_status(*(struct tls **)luaL_checkudata(L,1,TYPE_TLS)));
  return 1;
}

/**************************************************************************
* Usage:        revoketime = ctx:peer_ocsp_revocation_time()
* Desc:         Returns the OCSP revocation time
* Return:       revoketime (number) time (per os.time())
***************************************************************************/

static int Ltls_peer_ocsp_revocation_time(lua_State *L)
{
  lua_pushinteger(L,tls_peer_ocsp_revocation_time(*(struct tls **)luaL_checkudata(L,1,TYPE_TLS)));
  return 1;
}

/**************************************************************************
* Usage:        updatetime = ctx:peer_ocsp_this_update()
* Desc:         Return the OCSP next update time
* Return:       updatetime (number) time (as per os.time())
***************************************************************************/

static int Ltls_peer_ocsp_this_update(lua_State *L)
{
  lua_pushinteger(L,tls_peer_ocsp_this_update(*(struct tls **)luaL_checkudata(L,1,TYPE_TLS)));
  return 1;
}

/**************************************************************************
* Usage:        url = ctx:peer_ocsp_url()
* Desc:         Return the URL for OCSP valiation
* Return:       url (string) URL
***************************************************************************/

static int Ltls_peer_ocsp_url(lua_State *L)
{
  lua_pushstring(L,tls_peer_ocsp_url(*(struct tls **)luaL_checkudata(L,1,TYPE_TLS)));
  return 1;
}

/**************************************************************************
* Usage:        pem = ctx:peer_cert_chain_pem()
* Desc:         Return the raw PEM encoded certificate chain for the peer
*               certificate.
* Return:       pem (string) raw PEM data
***************************************************************************/

static int Ltls_peer_cert_chain_pem(lua_State *L)
{
  char const *chain;
  size_t      len;
  
  chain = (char const *)tls_peer_cert_chain_pem(*(struct tls **)luaL_checkudata(L,1,TYPE_TLS),&len);
  if (chain != NULL)
    lua_pushlstring(L,chain,len);
  else
    lua_pushnil(L);
  return 1;
}

/**************************************************************************
* Usage:        resumed = ctx:conn_session_resumed()
* Desc:         Has this session resumed from a previous session?
* Return:       resumed (boolean) yeah or nay
***************************************************************************/

static int Ltls_conn_session_resumed(lua_State *L)
{
  lua_pushboolean(
          L,
          tls_conn_session_resumed(*(struct tls **)luaL_checkudata(L,1,TYPE_TLS))
  );
  return 1;
}

#if TLS_API >= 20200120
/**************************************************************************
* Usage:        resumed = ctx:conn_session_resumed()
* Desc:         Has this session resumed from a previous session?
* Return:       resumed (boolean) yeah or nay
***************************************************************************/

static int Ltls_conn_cipher_strength(lua_State *L)
{
  lua_pushinteger(
          L,
          tls_conn_cipher_strength(*(struct tls **)luaL_checkudata(L,1,TYPE_TLS))
  );
  return 1;
}
#endif
          
/**************************************************************************
*
*                                  TLS API
*
***************************************************************************
* Usage:        str = tostring(memobj)
* Desc:         Return the contents of a Ltlsmem object
* Input:        memobj (userdata/Ltlsmem) contents of loaded file
* Return:       str (string)
***************************************************************************/

static int Ltlsmem___tostring(lua_State *L)
{
  struct Ltlsmem *mem = luaL_checkudata(L,1,TYPE_TLS_MEM);
  lua_pushlstring(L,(char const *)mem->buf,mem->len);
  return 1;
}

/**************************************************************************
* Usage:        config = tls.config()
* Desc:         Create a configuration context
* Return:       config (userdata/TLS_CONF)
***************************************************************************/

static int Ltlstop_config(lua_State *L)
{
  struct tls_config *tlsconf = tls_config_new();
  
  if (tlsconf != NULL)
  {
    struct tls_config **ptlsconf = lua_newuserdata(L,sizeof(struct tls_config *));
    *ptlsconf = tlsconf;
    luaL_getmetatable(L,TYPE_TLS_CONF);
    lua_setmetatable(L,-2);
  }
  else
    lua_pushnil(L);
    
  return 1;
}

/**************************************************************************
* Usage:        ctx = tls.client()
* Desc:         Return a TLS client context
* Return:       ctx (userdata/TLS) client context, nil on error
***************************************************************************/

static int Ltlstop_client(lua_State *L)
{
  struct tls *tls = tls_client();
  
  if (tls != NULL)
  {
    struct tls **ptls = lua_newuserdata(L,sizeof(struct tls *));
    *ptls = tls;
    luaL_getmetatable(L,TYPE_TLS);
    lua_setmetatable(L,-2);
    lua_createtable(L,0,0);
    lua_setuservalue(L,-2);
  }
  else
    lua_pushnil(L);
    
  return 1;
}

/**************************************************************************
* Usage:        mem = tls.load_file(filename[,password])
* Desc;         Load certificate or key files from disk and return the
*               contents into memory.
* Input:        filename (string) filename to load
*               password (string/optional) password to decode private key
* Return:       mem (userdata/TLSmem) contents of loaded file
***************************************************************************/

static int Ltlstop_load_file(lua_State *L)
{
  struct Ltlsmem mem;
  
  lua_settop(L,2);
  
  mem.buf = tls_load_file(
                luaL_checkstring(L,1),
                &mem.len,
                (char *)lua_tostring(L,2)
            );
            
  if (mem.buf != NULL)
  {
    struct Ltlsmem *pmem = lua_newuserdata(L,sizeof(struct Ltlsmem));
    luaL_getmetatable(L,TYPE_TLS_MEM);
    lua_setmetatable(L,-2);
    *pmem = mem;
  }
  else
    lua_pushnil(L);
    
  return 1;
}

/**************************************************************************
* Usage:        ctx = tls.server()
* Desc:         Return a TLS server context
* Return:       ctx (userdata/TLS) server context, nil on error
***************************************************************************/

static int Ltlstop_server(lua_State *L)
{
  struct tls *tls = tls_server();
  
  if (tls != NULL)
  {
    struct tls **ptls = lua_newuserdata(L,sizeof(struct tls *));
    *ptls = tls;
    luaL_getmetatable(L,TYPE_TLS);
    lua_setmetatable(L,-2);
    lua_createtable(L,0,0);
    lua_setuservalue(L,-2);
  }
  else
    lua_pushnil(L);
    
  return 1;
}

/**************************************************************************
* Usage:        tls.unload_file(mem)
* Desc;         Unload a certicate or key (opened via tls.load_file()) from
*               memory.
* Input:        mem (userdata/TLSmem) contented of loaded file
***************************************************************************/

static int Ltlstop_unload_file(lua_State *L)
{
  struct Ltlsmem *mem = luaL_checkudata(L,1,TYPE_TLS_MEM);
  tls_unload_file(mem->buf,mem->len);
  mem->buf = NULL;
  mem->len = 0;
  return 0;
}

/**************************************************************************
* Usage:        cert = tls.default_ca_cert_file()
* Desc;         Return the name of the default CA cert file
* Return:       cert (string) name of the default CA cert file
***************************************************************************/

static int Ltlstop_default_ca_cert_file(lua_State *L)
{
  lua_pushstring(L,tls_default_ca_cert_file());
  return 1;
}

/**************************************************************************/

static luaL_Reg const m_tlsmemmeta[] =
{
  { "__tostring"                , Ltlsmem___tostring               } ,
  { "__gc"                      , Ltlstop_unload_file              } ,
#if LUA_VERSION_NUM >= 504
  { "__close"                   , Ltlstop_unload_file              } ,
#endif
  { NULL                        , NULL                             }
};

/*------------------------------------------------------------------------*/

static luaL_Reg const m_tlsconfmeta[] =
{
  { "__tostring"                , Ltlsconf___tostring              } ,
  { "__gc"                      , Ltlsconf___gc                    } ,
#if LUA_VERSION_NUM >= 504
  { "__close"                   , Ltlsconf___gc                    } ,
#endif
  { "add_keypair_file"          , Ltlsconf_add_keypair_file        } ,
  { "add_keypair_mem"           , Ltlsconf_add_keypair_mem         } ,
  { "add_keypair_ocsp_file"     , Ltlsconf_add_keypair_ocsp_file   } ,
  { "add_keypair_ocsp_mem"      , Ltlsconf_add_keypair_ocsp_mem    } ,
  { "add_ticket_key"            , Ltlsconf_add_ticket_key          } , // XXX
  { "alpn"                      , Ltlsconf_alpn                    } ,
  { "ca_file"                   , Ltlsconf_ca_file                 } ,
  { "ca_mem"                    , Ltlsconf_ca_mem                  } ,
  { "ca_path"                   , Ltlsconf_ca_path                 } ,
  { "cert_file"                 , Ltlsconf_cert_file               } ,
  { "cert_mem"                  , Ltlsconf_cert_mem                } ,
  { "ciphers"                   , Ltlsconf_ciphers                 } ,
  { "clear_keys"                , Ltlsconf_clear_keys              } ,
  { "crl_file"                  , Ltlsconf_crl_file                } ,
  { "crl_mem"                   , Ltlsconf_crl_mem                 } ,
  { "dheparams"                 , Ltlsconf_dheparams               } ,
  { "ecdhecurve"                , Ltlsconf_ecdhecurve              } ,
  { "ecdhecurves"               , Ltlsconf_ecdhecurves             } ,
  { "error"                     , Ltlsconf_error                   } ,
  { "free"                      , Ltlsconf___gc                    } ,
  { "insecure_no_verify_cert"   , Ltlsconf_insecure_no_verify_cert } ,
  { "insecure_no_verify_name"   , Ltlsconf_insecure_no_verify_name } ,
  { "insecure_no_verify_time"   , Ltlsconf_insecure_no_verify_time } ,
  { "key_file"                  , Ltlsconf_key_file                } ,
  { "key_mem"                   , Ltlsconf_key_mem                 } ,
  { "keypair_file"              , Ltlsconf_keypair_file            } ,
  { "keypair_mem"               , Ltlsconf_keypair_mem             } ,
  { "keypair_ocsp_file"         , Ltlsconf_keypair_ocsp_file       } ,
  { "keypair_ocsp_mem"          , Ltlsconf_keypair_ocsp_mem        } ,
  { "ocsp_require_stapling"     , Ltlsconf_ocsp_require_stapling   } ,
  { "ocsp_staple_file"          , Ltlsconf_ocsp_staple_file        } ,
  { "ocsp_staple_mem"           , Ltlsconf_ocsp_staple_mem         } ,
  { "prefer_ciphers_client"     , Ltlsconf_prefer_ciphers_client   } ,
  { "prefer_ciphers_server"     , Ltlsconf_prefer_ciphers_server   } ,
  { "protocols"                 , Ltlsconf_protocols               } ,
  { "session_fd"                , Ltlsconf_session_fd              } ,
  { "session_id"                , Ltlsconf_session_id              } ,
  { "session_lifetime"          , Ltlsconf_session_lifetime        } ,
  { "verify"                    , Ltlsconf_verify                  } ,
  { "verify_client"             , Ltlsconf_verify_client           } ,
  { "verify_client_optional"    , Ltlsconf_verify_client_optional  } ,
  { "verify_depth"              , Ltlsconf_verify_depth            } ,
  { NULL                        , NULL                             }
};

/*------------------------------------------------------------------------*/

static luaL_Reg const m_tlsmeta[] =
{
#if LUA_VERSION_NUM >= 502
  { "__pairs"                   , Ltls___pairs                     } ,
#endif
  { "__index"                   , Ltls___index                     } ,
  { "__newindex"                , Ltls___newindex                  } ,
  { "__tostring"                , Ltls___tostring                  } ,
  { "__gc"                      , Ltls___gc                        } ,
#if LUA_VERSION_NUM >= 504
  { "__close"                   , Ltls___gc                        } ,
#endif
  { "accept_cbs"                , Ltls_accept_cbs                  } ,
  { "accept_fds"                , Ltls_accept_fds                  } ,
  { "accept_socket"             , Ltls_accept_socket               } ,
  { "close"                     , Ltls_close                       } ,
  { "configure"                 , Ltls_configure                   } ,
  { "conn_alpn_selected"        , Ltls_conn_alpn_selected          } ,
  { "conn_cipher"               , Ltls_conn_cipher                 } ,
  { "conn_servername"           , Ltls_conn_servername             } ,
  { "conn_session_resumed"      , Ltls_conn_session_resumed        } ,
  { "conn_version"              , Ltls_conn_version                } ,
  { "connect"                   , Ltls_connect                     } ,
  { "connect_cbs"               , Ltls_connect_cbs                 } ,
  { "connect_fds"               , Ltls_connect_fds                 } ,
  { "connect_socket"            , Ltls_connect_socket              } ,
  { "error"                     , Ltls_error                       } ,
  { "free"                      , Ltls___gc                        } ,
  { "handshake"                 , Ltls_handshake                   } ,
  { "ocsp_process_response"     , Ltls_ocsp_process_response       } , // XXX
  { "peer_cert_chain_pem"       , Ltls_peer_cert_chain_pem         } ,
  { "peer_cert_contains_name"   , Ltls_peer_cert_contains_name     } ,
  { "peer_cert_hash"            , Ltls_peer_cert_hash              } ,
  { "peer_cert_issuer"          , Ltls_peer_cert_issuer            } ,
  { "peer_cert_notafter"        , Ltls_peer_cert_notafter          } ,
  { "peer_cert_notbefore"       , Ltls_peer_cert_notbefore         } ,
  { "peer_cert_provided"        , Ltls_peer_cert_provided          } ,
  { "peer_cert_subject"         , Ltls_peer_cert_subject           } ,
  { "peer_ocsp_cert_status"     , Ltls_peer_ocsp_cert_status       } ,
  { "peer_ocsp_crl_reason"      , Ltls_peer_ocsp_crl_reason        } ,
  { "peer_ocsp_next_update"     , Ltls_peer_ocsp_next_update       } ,
  { "peer_ocsp_response_status" , Ltls_peer_ocsp_response_status   } ,
  { "peer_ocsp_revocation_time" , Ltls_peer_ocsp_revocation_time   } ,
  { "peer_ocsp_this_update"     , Ltls_peer_ocsp_this_update       } ,
  { "peer_ocsp_url"             , Ltls_peer_ocsp_url               } ,
  { "read"                      , Ltls_read                        } ,
  { "reset"                     , Ltls_reset                       } ,
  { "write"                     , Ltls_write                       } ,
#if TLS_API >= 20200120
  { "conn_cipher_strength"      , Ltls_conn_cipher_strength        } ,
#endif
  { NULL                        , NULL                             }
};

/*------------------------------------------------------------------------*/

static luaL_Reg const m_tlsreg[] =
{
  { "client"                    , Ltlstop_client                   } ,
  { "config"                    , Ltlstop_config                   } ,
  { "load_file"                 , Ltlstop_load_file                } ,
  { "server"                    , Ltlstop_server                   } ,
  { "unload_file"               , Ltlstop_unload_file              } ,
  { "default_ca_cert_file"      , Ltlstop_default_ca_cert_file     } ,
  { NULL                        , NULL                             }
};

/**************************************************************************/

int luaopen_org_conman_tls(lua_State *L)
{
  if (tls_init() != 0)
  {
    lua_pushnil(L);
    return 1;
  }
  
  luaL_newmetatable(L,TYPE_TLS_MEM);
  luaL_setfuncs(L,m_tlsmemmeta,0);
  luaL_newmetatable(L,TYPE_TLS_CONF);
  luaL_setfuncs(L,m_tlsconfmeta,0);
  lua_pushvalue(L,-1);
  lua_setfield(L,-2,"__index");
  luaL_newmetatable(L,TYPE_TLS);
  luaL_setfuncs(L,m_tlsmeta,0);
  luaL_newlib(L,m_tlsreg);

  for (size_t i = 0 ; m_tls_consts[i].text != NULL ; i++)
  {
    lua_pushinteger(L,m_tls_consts[i].value);
    lua_setfield(L,-2,m_tls_consts[i].text);
  }
  
  return 1;
}

/**************************************************************************/
