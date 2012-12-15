/*********************************************************************
*
* Copyright 2012 by Sean Conner.  All Rights Reserved.
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

#define _GNU_SOURCE

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <time.h>
#include <assert.h>

#include <arpa/inet.h>
#include <openssl/evp.h>

#include <lua.h>
#include <lauxlib.h>

#ifndef __GNUC__
#  define __attribute__(x)
#endif

#define UUID_TYPE	"UUID"
#define UUID_EPOCH	0x01B21DD213814000LL

/***********************************************************************/

#if RAND_MAX == SHORT_MAX
  typedef unsigned short rand__t;
#else
  typedef unsigned int   rand__t;
#endif

struct uuid
{
  uint32_t time_low;
  uint16_t time_mid;
  uint16_t time_hi_and_version;
  uint8_t  clock_seq_hi_and_reserved;
  uint8_t  clock_seq_low;
  uint8_t  node[6];
} __attribute__((packed));

typedef union 
{
  struct uuid uuid;
  uint8_t     flat[sizeof(struct uuid)];
  rand__t     rnd [sizeof(struct uuid) / sizeof(rand__t)];
} uuid__t;

/*************************************************************************/

static void	uuidluaL_pushuuid	(lua_State *const,const uint8_t *const);
static int	uuidlua___tostring	(lua_State *const);
static int	uuidlua___eq		(lua_State *const);
static int	uuidlua___le		(lua_State *const);
static int	uuidlua___lt		(lua_State *const);
static int	uuidlua___call		(lua_State *const);
static int	uuidlua_parse		(lua_State *const);
static int	uuidlua_breakout	(lua_State *const);

static int	uuidlib_v1		(uuid__t *const);
static int	uuidlib_v2		(uuid__t *const) __attribute__((unused));
static int	uuidlib_v3		(uuid__t *const,const uuid__t *const,const void *const,const size_t);
static int	uuidlib_v4		(uuid__t *const);
static int	uuidlib_v5		(uuid__t *const,const uuid__t *const,const void *const,const size_t);
static int	uuidlib_cmp		(const uuid__t *const restrict,const uuid__t *const restrict);
static int	uuidlib_parse_seg	(uuid__t *const,size_t,const char **,const size_t);
static int	uuidlib_parse		(uuid__t *const,const char *);

/************************************************************************/

	/* per RFC-4122 */

static const uint8_t c_namespace_dns[] =
{
  0x6b , 0xa7 , 0xb8 , 0x10 , 0x9d , 0xad , 0x11 , 0xd1 ,
  0x80 , 0xb4 , 0x00 , 0xc0 , 0x4f , 0xd4 , 0x30 , 0xc8
};

static const uint8_t c_namespace_url[] =
{
  0x6b , 0xa7 , 0xb8 , 0x11 , 0x9d , 0xad , 0x11 , 0xd1 ,
  0x80 , 0xb4 , 0x00 , 0xc0 , 0x4f , 0xd4 , 0x30 , 0xc8
};

static const uint8_t c_namespace_oid[] =
{
   0x6b , 0xa7 , 0xb8 , 0x12 , 0x9d , 0xad , 0x11 , 0xd1 ,
   0x80 , 0xb4 , 0x00 , 0xc0 , 0x4f , 0xd4 , 0x30 , 0xc8
};

static const uint8_t c_namespace_x500[] =
{
   0x6b , 0xa7 , 0xb8 , 0x14 , 0x9d , 0xad , 0x11 , 0xd1 ,
   0x80 , 0xb4 , 0x00 , 0xc0 , 0x4f , 0xd4 , 0x30 , 0xc8
};

static const struct luaL_reg muuid_reg[] =
{
  { "parse"	, uuidlua_parse		} ,
  { "breakout"	, uuidlua_breakout	} ,
  { NULL	, NULL			}
};

static const struct luaL_reg muuid_meta[] =
{
  { "__tostring"	, uuidlua___tostring	} ,
  { "__eq"		, uuidlua___eq		} ,
  { "__lt"		, uuidlua___lt		} ,
  { "__le"		, uuidlua___le		} ,
  { NULL		, NULL			}
};

static uint8_t m_mac[6] = { 0xDE , 0xCA , 0xFB , 0xAD , 0x02 , 0x01 };

/*************************************************************************/

int luaopen_org_conman_uuid(lua_State *const L)
{
  uuid__t *ns;
  
  luaL_newmetatable(L,UUID_TYPE);
  luaL_register(L,NULL,muuid_meta);
  
  luaL_register(L,"org.conman.uuid",muuid_reg);
  
  uuidluaL_pushuuid(L,c_namespace_dns);
  lua_setfield(L,-2,"DNS");
  uuidluaL_pushuuid(L,c_namespace_url);
  lua_setfield(L,-2,"URL");
  uuidluaL_pushuuid(L,c_namespace_oid);
  lua_setfield(L,-2,"OID");
  uuidluaL_pushuuid(L,c_namespace_x500);
  lua_setfield(L,-2,"X500");
  
  ns = lua_newuserdata(L,sizeof(uuid__t));
  memset(ns->flat,0,sizeof(ns->flat));
  luaL_getmetatable(L,UUID_TYPE);
  lua_setmetatable(L,-2);
  lua_setfield(L,-2,"NIL");
  
  lua_createtable(L,0,1);
  lua_pushcfunction(L,uuidlua___call);
  lua_setfield(L,-2,"__call");
  lua_setmetatable(L,-2);
  
  return 1;
}

/*************************************************************************/

static void uuidluaL_pushuuid(lua_State *const L,const uint8_t *const uuid)
{
  uuid__t *ns;
  
  ns = lua_newuserdata(L,sizeof(uuid__t));
  memcpy(ns->flat,uuid,sizeof(ns->flat));
  luaL_getmetatable(L,UUID_TYPE);
  lua_setmetatable(L,-2);
}

/*************************************************************************/

static int uuidlua___tostring(lua_State *const L)
{
  uuid__t *uuid;
  char     buffer[37];
  char    *p;
  
  uuid  = luaL_checkudata(L,1,UUID_TYPE);
  p     = buffer;
  
  for (size_t i = 0 ; i < 4 ; i++)
    p += sprintf(p,"%02X",uuid->flat[i]);
  
  *p++ = '-';
  
  for (size_t i = 4 ; i < 6 ; i++)
    p += sprintf(p,"%02X",uuid->flat[i]);
  
  *p++ = '-';
  
  for (size_t i = 6 ; i < 8 ; i++)
    p += sprintf(p,"%02X",uuid->flat[i]);
  
  *p++ = '-';
  
  for (size_t i = 8 ; i < 10 ; i++)
    p += sprintf(p,"%02X",uuid->flat[i]);
  
  *p++ = '-';
  
  for (size_t i = 10 ; i < 16 ; i++)
    p += sprintf(p,"%02X",uuid->flat[i]);
  
  lua_pushlstring(L,buffer,36);
  return 1;  
}

/*************************************************************************/

static int uuidlua___eq(lua_State *const L)
{
  lua_pushboolean(
    L,
    uuidlib_cmp(
      luaL_checkudata(L,1,UUID_TYPE),
      luaL_checkudata(L,2,UUID_TYPE)
    ) == 0
  );
  return 1;
}

/*************************************************************************/

static int uuidlua___le(lua_State *const L)
{
  lua_pushboolean(
   L,
    uuidlib_cmp(
      luaL_checkudata(L,1,UUID_TYPE),
      luaL_checkudata(L,2,UUID_TYPE)
    ) <= 0
  );
  return 1;
}

/*************************************************************************/

static int uuidlua___lt(lua_State *const L)
{
  lua_pushboolean(
    L,
    uuidlib_cmp(
      luaL_checkudata(L,1,UUID_TYPE),
      luaL_checkudata(L,2,UUID_TYPE)
    ) < 0
  );
  return 1;
}

/*************************************************************************/

static int uuidlua___call(lua_State *const L)
{
  uuid__t    *uuid;
  uuid__t    *pns;
  uuid__t     ns;
  const char *name;
  size_t      len;
  int         rc;
  bool        sha1;
  
  sha1 = lua_isnoneornil(L,4);
  uuid = lua_newuserdata(L,sizeof(uuid__t));
  
  if (lua_isnoneornil(L,2))
    uuidlib_v4(uuid);
  else if (lua_isnumber(L,2))
    uuidlib_v1(uuid);
  else if (lua_isstring(L,2))
  {
    rc = uuidlib_parse(&ns,luaL_checkstring(L,2));
    if (rc != 0)
    {
      lua_pushnil(L);
      lua_pushinteger(L,rc);
      return 2;
    }
    
    name = luaL_checklstring(L,3,&len);

    if (sha1)
      uuidlib_v5(uuid,&ns,name,len);
    else
      uuidlib_v3(uuid,&ns,name,len);
  }
  else if (lua_isuserdata(L,2))
  {
    pns  = luaL_checkudata(L,2,UUID_TYPE);
    name = luaL_checklstring(L,3,&len);
      
    if (sha1)
      uuidlib_v5(uuid,pns,name,len);
    else
      uuidlib_v3(uuid,pns,name,len);
  }
  else
    uuidlib_v1(uuid);
    
  luaL_getmetatable(L,UUID_TYPE);
  lua_setmetatable(L,-2);
  return 1;
}

/************************************************************************/

static int uuidlua_parse(lua_State *const L)
{
  uuid__t    *puuid;
  uuid__t     uuid;
  const char *data;
  size_t      size;
  int         rc;
  
  data = luaL_checklstring(L,1,&size);
  if (lua_toboolean(L,2))
    memcpy(uuid.flat,data,sizeof(struct uuid));
  else
  {
    rc = uuidlib_parse(&uuid,data);
    if (rc != 0)
    {
      lua_pushnil(L);
      lua_pushinteger(L,rc);
      return 2;
    }
  }
  
  puuid = lua_newuserdata(L,sizeof(uuid__t));
  *puuid = uuid;
  luaL_getmetatable(L,UUID_TYPE);
  lua_setmetatable(L,-2);
  lua_pushinteger(L,0);
  return 2;
}

/**************************************************************************/

static int uuidlua_breakout(lua_State *const L)
{
  uuid__t  *uuid;
  int64_t   timestamp;
  uint16_t  sequence;
  char      node[13];
  size_t    bytes;
  
  uuid = luaL_checkudata(L,1,UUID_TYPE);
  lua_createtable(L,0,0);
  
  lua_pushlstring(L,(char *)uuid->flat,sizeof(struct uuid));
  lua_setfield(L,-2,"bits");
  
  if ((uuid->flat[8] & 0x80) == 0x00)
  {
    lua_pushboolean(L,true);
    lua_setfield(L,-2,"NCS_reserved");
    return 1;
  }
  else if ((uuid->flat[8] & 0xE0) == 0xC0)
  {
    lua_pushboolean(L,true);
    lua_setfield(L,-2,"Microsoft");
    return 1;
  }
  else if ((uuid->flat[8] & 0xE0) == 0xE0)
  {
    lua_pushboolean(L,true);
    lua_setfield(L,-2,"reserved");
    return 1;
  }
  
  assert((uuid->flat[8] & 0x80) == 0x80);

  switch(uuid->flat[6] & 0xF0)  
  {
    case 0x10:
         lua_pushinteger(L,1);
         lua_setfield(L,-2,"version");
         timestamp = ((int64_t)ntohs(uuid->uuid.time_hi_and_version) << 48)
                   | ((int64_t)ntohs(uuid->uuid.time_mid)            << 32)
                   | ((int64_t)ntohl(uuid->uuid.time_low));
         timestamp -= UUID_EPOCH;
         timestamp &= 0x0FFFFFFFFFFFFFFFLL;
         
         lua_pushnumber(L,(lua_Number)timestamp / 10000000.0);
         lua_setfield(L,-2,"timestamp");
         sequence = (uuid->uuid.clock_seq_hi_and_reserved << 8)
                  | (uuid->uuid.clock_seq_low);
         sequence &= 0x3FFF;
         lua_pushinteger(L,sequence);
         lua_setfield(L,-2,"clock_sequence");
         bytes = snprintf(
         	node,
         	sizeof(node),
         	"%02X%02X%02X%02X%02X%02X",
         	uuid->uuid.node[0],
         	uuid->uuid.node[1],
         	uuid->uuid.node[2],
         	uuid->uuid.node[3],
         	uuid->uuid.node[4],
         	uuid->uuid.node[5]
         );
         lua_pushlstring(L,node,bytes);
         lua_setfield(L,-2,"node");
         break;
         
    case 0x20:
         lua_pushinteger(L,2);
         lua_setfield(L,-2,"version");
         break;
         
    case 0x30:
         lua_pushinteger(L,3);
         lua_setfield(L,-2,"version");
         break;
         
    case 0x40:
         lua_pushinteger(L,4);
         lua_setfield(L,-2,"version");
         break;
         
    case 0x50:
         lua_pushinteger(L,5);
         lua_setfield(L,-2,"version");
         break;
         
    default:
         break;
  }
  
  return 1;
}

/***********************************************************************/

static int uuidlib_v1(uuid__t *const uuid)
{
  struct timespec now;
  int64_t         timestamp;
  
  clock_gettime(CLOCK_REALTIME,&now);
  timestamp = (now.tv_sec * 10000000LL)
            + (now.tv_nsec / 100LL)
            + UUID_EPOCH;
  uuid->uuid.time_hi_and_version       = htons(timestamp >> 48);
  uuid->uuid.time_mid                  = htons((timestamp >> 32) & 0xFFFFLL);
  uuid->uuid.time_low                  = htonl(timestamp & 0xFFFFFFFFLL);
  uuid->uuid.clock_seq_hi_and_reserved = rand() & 0xFF;
  uuid->uuid.clock_seq_low             = rand() & 0xFF;
  memcpy(uuid->uuid.node,m_mac,6);
  
  uuid->flat[6] = (uuid->flat[6] & 0x0F) | 0x10;
  uuid->flat[8] = (uuid->flat[8] & 0x3F) | 0x80;
  return 0;
}

/*************************************************************************/

static int uuidlib_v2(uuid__t *const uuid)
{
  memset(uuid,0,sizeof(uuid__t));
  return ENOSYS;
}

/**************************************************************************/

static int uuidlib_v3(
	uuid__t       *const uuid,
	const uuid__t *const namespace,
	const void    *const name,
	const size_t         len
)
{
  const EVP_MD *m = EVP_md5();
  EVP_MD_CTX    ctx;
  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int  hashsize;
  
  EVP_DigestInit(&ctx,m);
  EVP_DigestUpdate(&ctx,namespace->flat,sizeof(struct uuid));
  EVP_DigestUpdate(&ctx,name,len);
  EVP_DigestFinal(&ctx,hash,&hashsize);
  
  memcpy(uuid->flat,hash,sizeof(struct uuid));
  uuid->flat[6] = (uuid->flat[6] & 0x0F) | 0x30;
  uuid->flat[8] = (uuid->flat[8] & 0x3F) | 0x80;
  return 0;
}

/**************************************************************************/

static int uuidlib_v4(uuid__t *const uuid)
{
  for (size_t i = 0 ; i < (sizeof(struct uuid) / sizeof(rand__t)) ; i++)
    uuid->rnd[i] = (unsigned)rand() + (unsigned)rand();
  
  uuid->flat[6] = (uuid->flat[6] & 0x0F) | 0x40;
  uuid->flat[8] = (uuid->flat[8] & 0x3F) | 0x80;
  return 0;
}

/*************************************************************************/

static int uuidlib_v5(
	uuid__t       *const uuid,
	const uuid__t *const namespace,
	const void    *const name,
	const size_t         len
)
{
  const EVP_MD *m = EVP_sha1();
  EVP_MD_CTX    ctx;
  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int  hashsize;
  
  EVP_DigestInit(&ctx,m);
  EVP_DigestUpdate(&ctx,namespace->flat,sizeof(struct uuid));
  EVP_DigestUpdate(&ctx,name,len);
  EVP_DigestFinal(&ctx,hash,&hashsize);
  
  memcpy(uuid->flat,hash,sizeof(struct uuid));
  uuid->flat[6] = (uuid->flat[6] & 0x0F) | 0x50;
  uuid->flat[8] = (uuid->flat[8] & 0x3F) | 0x80;
  return 0;
}

/**************************************************************************/

static int uuidlib_cmp(
	const uuid__t *const restrict uuid1,
	const uuid__t *const restrict uuid2
)
{
  uint32_t a32;
  uint32_t b32;
  uint16_t a16;
  uint16_t b16;
  
  a32 = ntohl(uuid1->uuid.time_low);
  b32 = ntohl(uuid2->uuid.time_low);
  
  if (a32 < b32)
    return -1;
  else if (a32 > b32)
    return 1;
  
  a16 = ntohs(uuid1->uuid.time_mid);
  b16 = ntohs(uuid2->uuid.time_mid);
  
  if (a16 < b16)
    return -1;
  else if (a16 > b16)
    return 1;
  
  a16 = ntohs(uuid1->uuid.time_hi_and_version);
  b16 = ntohs(uuid2->uuid.time_hi_and_version);
  
  if (a16 < b16)
    return -1;
  else if (a16 > b16)
    return 1;
      
  if (uuid1->uuid.clock_seq_hi_and_reserved < uuid2->uuid.clock_seq_hi_and_reserved)
    return -1;
  else if (uuid1->uuid.clock_seq_hi_and_reserved > uuid2->uuid.clock_seq_hi_and_reserved)
    return 1;
  
  if (uuid1->uuid.clock_seq_low < uuid2->uuid.clock_seq_low)
    return -1;
  else if (uuid1->uuid.clock_seq_low > uuid2->uuid.clock_seq_low)
    return 1;
  
  return memcmp(uuid1->uuid.node,uuid2->uuid.node,6);
}

/*************************************************************************/

static int uuidlib_parse_seg(
	uuid__t     *const uuid,
	size_t             idx,
	const char       **ptext,
	const size_t       bytes
)
{
  char        buf[3];
  const char *p;
  
  assert(uuid   != NULL);
  assert(idx    <  sizeof(struct uuid));
  assert(ptext  != NULL);
  assert(*ptext != NULL);
  assert(bytes  >  0);
  
  errno = 0;
  p     = *ptext;
  
  for (size_t i = 0 ; i < bytes ; i++)
  {
    buf[0] = *p++;
    buf[1] = *p++;
    buf[2] = '\0';
    
    errno = 0;
    uuid->flat[idx++] = strtoul(buf,NULL,16);
    if (errno != 0) return errno;
  }
  
  if ((*p != '-') && (*p != '\0')) 
    return EINVAL;
  
  *ptext = p + 1;
  return 0;
}

/************************************************************************/

static int uuidlib_parse(uuid__t *const uuid,const char *text)
{
  int rc;
  
  assert(uuid != NULL);
  assert(text != NULL);
  
  if ((rc = uuidlib_parse_seg(uuid, 0,&text,4)) != 0) return rc;
  if ((rc = uuidlib_parse_seg(uuid, 4,&text,2)) != 0) return rc;
  if ((rc = uuidlib_parse_seg(uuid, 6,&text,2)) != 0) return rc;
  if ((rc = uuidlib_parse_seg(uuid, 8,&text,2)) != 0) return rc;
  if ((rc = uuidlib_parse_seg(uuid,10,&text,6)) != 0) return rc;
  
  return 0;
}

/***************************************************************************/
