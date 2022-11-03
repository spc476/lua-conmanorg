/***************************************************************************
*
* Copyright 2020 by Sean Conner.
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

#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <stringprep.h>
#include <punycode.h>

#include <lua.h>
#include <lauxlib.h>

#if !defined(LUA_VERSION_NUM) || LUA_VERSION_NUM < 501
#  error You need to compile against Lua 5.1 or higher
#endif

/************************************************************************/

union wstrp
{
  punycode_uint *pc;
  uint32_t      *uc;
};

union wstr
{
  punycode_uint pc[64];
  uint32_t      uc[64];
};

/************************************************************************/

static bool add_segment(luaL_Buffer *buf,char const *seg,size_t len)
{
  char        *normalized;
  union wstrp  domain;
  char         topuny[64];
  size_t       domainsize;
  size_t       topunysize;
  bool         rc;
  
  rc         = false;
  normalized = stringprep_utf8_nfkc_normalize(seg,len);
  
  if (normalized != NULL)
  {
    domain.uc  = stringprep_utf8_to_ucs4(normalized,-1,&domainsize);
    if (domain.uc != NULL)
    {
      topunysize = 64;
  
      if (punycode_encode(domainsize,domain.pc,NULL,&topunysize,topuny) == PUNYCODE_SUCCESS)
      {
        luaL_addlstring(buf,"xn--",4);
        luaL_addlstring(buf,topuny,topunysize);
        rc = true;
      }
      free(domain.uc);
    }
    
    free(normalized);
  }
  
  return rc;
}

/************************************************************************/

static int idn_encode(lua_State *L)
{
  char const  *orig = luaL_checkstring(L,1);
  luaL_Buffer  buf;
  size_t       si;
  size_t       di;
  bool         utf8;
  
  luaL_buffinit(L,&buf);
  
  for (si = di = 0 , utf8 = false ; ; di++)
  {
    if (orig[di] == '\0')
    {
      if (utf8)
      {
        if (!add_segment(&buf,&orig[si],di - si))
          return 0;
      }
      else
        luaL_addlstring(&buf,&orig[si],di - si);
      
      luaL_pushresult(&buf);
      return 1;
    }
    
    if (orig[di] == '.')
    {
      if (utf8)
      {
        if (!add_segment(&buf,&orig[si],di - si))
          return 0;
      }
      else
        luaL_addlstring(&buf,&orig[si],di - si);
      
      luaL_addchar(&buf,'.');
      si    = di + 1;
      utf8  = false;
    }
    else
      utf8 |= ((unsigned)orig[di]) > 127;      
  }
}

/************************************************************************/

static int idn_decode(lua_State *L)
{
  char const *orig = luaL_checkstring(L,1);
  luaL_Buffer buf;
  
  luaL_buffinit(L,&buf);
  
  while(true)
  {
    char const *p = strchr(orig,'.');
    size_t      len = p == NULL ? strlen(orig) : (size_t)(p - orig);

    if (strncmp(orig,"xn--",4) == 0)
    {
      union wstr  frompuny;
      size_t      frompunysize = 64;
      char       *result;
      
      if (punycode_decode(len-4,&orig[4],&frompunysize,frompuny.pc,NULL) != PUNYCODE_SUCCESS)
        return 0;

      result = stringprep_ucs4_to_utf8(frompuny.uc,frompunysize,NULL,NULL);

      if (result != NULL)
      {
        luaL_addstring(&buf,result);
        free(result);
      }
    }
    else
      luaL_addlstring(&buf,orig,len);
    
    if (p != NULL)
    {
      luaL_addchar(&buf,'.');
      orig = ++p;
    }
    else
    {
      luaL_pushresult(&buf);
      return 1;
    }
  }
}

/************************************************************************/

int luaopen_org_conman_idn(lua_State *L)
{
  static luaL_Reg const m_idn_reg[] =
  {
    { "encode" , idn_encode },
    { "decode" , idn_decode },
    { NULL     , NULL       }
  };
  
#if LUA_VERSION_NUM == 501
  luaL_register(L,"org.conman.idn",m_idn_reg);
#else
  luaL_newlib(L,m_idn_reg);
#endif
  return 1;
}

/************************************************************************/
