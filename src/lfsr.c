/*********************************************************************
*
* Copyright 2010 by Sean Conner.  All Rights Reserved.
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
* ---------------------------------------------------------------------
*
* This module implements LFSR of 8, 16 and 32 bits.  These have the 
* following features:
*
*	All sequence of bits between 1 and 2^bits are generated
*	It is very fast.
*
        lfsr  = require "org.conman.lfsr"
        rnd8  = lfsr( 8[,seed[,taps]])
        rnd16 = lfsr(16[,seed[,taps]])
        rnd32 = lfsr(32[,seed[,taps]])
        rnd32 = lfsr() -- 32, 0xB4000021
*
* NOTE: taps for 8b, 16b and 32bs can be found here:
*	http://users.ece.cmu.edu/~koopman/lfsr/index.html
*
* ALSO SEE:
*	https://en.wikipedia.org/wiki/Linear-feedback_shift_register
*********************************************************************/

#include <assert.h>
#include <lua.h>
#include <lauxlib.h>

/*********************************************************************/

static int lfsrnext(lua_State *L)
{
  lua_Integer mask = lua_tointeger(L,lua_upvalueindex(1));
  lua_Integer taps = lua_tointeger(L,lua_upvalueindex(2));
  lua_Integer lfsr = lua_tointeger(L,lua_upvalueindex(3));
  lua_Integer lsb  = lfsr & 1;
  
  lfsr >>= 1;
  lfsr  ^= (-lsb) & taps;
  lfsr  &= mask;
  lua_pushinteger(L,lfsr);
  lua_pushinteger(L,lfsr);
  lua_replace(L,lua_upvalueindex(3));
  return 1;
}

/*********************************************************************/

static int lfsr(lua_State *L)
{
  lua_Integer bits = luaL_optinteger(L,1,32);
  lua_Integer seed = luaL_optinteger(L,2,0);
  lua_Integer taps = luaL_optinteger(L,3,0);
  lua_Integer mask;
  
  if (seed == 0)
  {
    FILE *fp = fopen("/dev/urandom","rb");
    
    if (fp)
    {
      fread(&seed,sizeof(seed),1,fp);
      fclose(fp);
    }
    
    if (seed == 0) seed++;
  }
  
  switch(bits)
  {
    case 8:
         if (taps == 0) taps = 0xB4; /* Pitfall! */
         mask = 0xFF;
         break;
         
    case 16:
         if (taps == 0) taps = 0xB400; /* more Pitfall! */
         mask = 0xFFFF;
         break;
         
    case 32:
         if (taps == 0) taps = 0xB4000021;
         mask = 0xFFFFFFFF;
         break;
         
    default:
         assert(0);
         return luaL_error(L,"bad mask value");
  }
  
  lua_pushinteger(L,mask);
  lua_pushinteger(L,taps);
  lua_pushinteger(L,seed);
  lua_pushcclosure(L,lfsrnext,3);
  return 1;
}

/*********************************************************************/

int luaopen_org_conman_lfsr(lua_State *L)
{
  lua_pushcfunction(L,lfsr);
  return 1;
}
