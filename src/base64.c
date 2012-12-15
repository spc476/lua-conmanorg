
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

#include <lua.h>
#include <lauxlib.h>

/***************************************************************************/

static const char *const mtrans = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                  "abcdefghijklmnopqrstuvwxyz"
                                  "0123456789+/";

/***************************************************************************/

static int base64_encode(lua_State *const L)
{
  const uint8_t *data;
  size_t      size;
  luaL_Buffer b;
  uint8_t A,B,C,D;
  
  data = (uint8_t *)luaL_checklstring(L,1,&size);
  luaL_buffinit(L,&b);
  
  while(true)
  {
    switch(size)
    {
      case 0:
           luaL_pushresult(&b);
           return 1;
           
      case 1:
           A = (data[0] >> 2);
           B = (data[0] << 4) & 0x30;
           assert(A < 64);
           assert(B < 64);
      
           luaL_addchar(&b,mtrans[A]);
           luaL_addchar(&b,mtrans[B]);
           luaL_addchar(&b,'=');
           luaL_addchar(&b,'=');
           luaL_pushresult(&b);
           return 1;
      
      case 2:
           A =  (data[0] >> 2);
           B = ((data[0] << 4) & 0x30) | ((data[1] >> 4) );
           C =  (data[1] << 2) & 0x3C;
           
           assert(A < 64);
           assert(B < 64);
           assert(C < 64);
           
           luaL_addchar(&b,mtrans[A]);
           luaL_addchar(&b,mtrans[B]);
           luaL_addchar(&b,mtrans[C]);
           luaL_addchar(&b,'=');       
           luaL_pushresult(&b);
           return 1;
      
      default:
           A =  (data[0] >> 2) ;
           B = ((data[0] << 4) & 0x30) | ((data[1] >> 4) );
           C = ((data[1] << 2) & 0x3C) | ((data[2] >> 6) );
           D =   data[2]       & 0x3F;
           
           assert(A < 64);
           assert(B < 64);
           assert(C < 64);
           assert(D < 64);
           
           luaL_addchar(&b,mtrans[A]);
           luaL_addchar(&b,mtrans[B]);
           luaL_addchar(&b,mtrans[C]);
           luaL_addchar(&b,mtrans[D]);
           size -= 3;
           data += 3;
           break;
    }
  }
}

/*************************************************************************/

static int base64_decode(lua_State *const L)
{
  const char *data;
  size_t      size;
  uint8_t     buf[4];
  luaL_Buffer b;
  
  data = luaL_checklstring(L,1,&size);
  if ((size % 4) != 0)
    return luaL_error(L,"invalid string size for Base-64");
    
  luaL_buffinit(L,&b);
  
  while(size)
  {
    for(size_t i = 0 ; i < 4 ; i++)
    {
      if (data[i] == '=')
        buf[i] = 0;
      else
      {
        char *p = strchr(mtrans,data[i]);
        assert(p != NULL);
        buf[i] = (p - mtrans);
        assert(buf[i] < 64);
      }
    }
        
    luaL_addchar(&b,(buf[0] << 2) | (buf[1] >> 4));
    luaL_addchar(&b,(buf[1] << 4) | (buf[2] >> 2));
    luaL_addchar(&b,(buf[2] << 6) | (buf[3]     ));
    
    data += 4;
    size -= 4;
  }
  
  luaL_pushresult(&b);
  return 1;
}

/************************************************************************/

static const luaL_Reg m_base64[] =
{
  { "encode" , base64_encode } ,
  { "decode" , base64_decode } ,
  { NULL     , NULL          }
};

int luaopen_org_conman_base64(lua_State *const L)
{
  luaL_register(L,"org.conman.base64",m_base64);
  return 1;
}

/************************************************************************/
