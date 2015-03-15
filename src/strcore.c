/********************************************
*
* Copyright 2013 by Sean Conner.  All Rights Reserved.
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
* ===================================================================
*
* The main algorithm for metaphone was placed into the public domain by Gary
* A. Parker and transcribed into machine readable format by Mark Grosberg
* from The C Gazette Jun/Jul 1991, Volume 5, No 4., Pg 55, and further
* modified by Sean Conner for use with Lua.
*
*********************************************************************/

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include <lua.h>
#include <lauxlib.h>

#if !defined(LUA_VERSION_NUM) || LUA_VERSION_NUM < 501
#  error You need to compile against Lua 5.1 or higher
#endif

#define DEF_MARGIN	78

/************************************************************************/

static int	strcore_wrapt		(lua_State *const);
static int	strcore_metaphone	(lua_State *const);
static int	strcore_soundex		(lua_State *cosnt);

/************************************************************************/

static const luaL_Reg m_strcore_reg[] =
{
  { "wrapt"	, strcore_wrapt		} ,
  { "metaphone"	, strcore_metaphone	} ,
  { "soundex"	, strcore_soundex	} ,
  { NULL	, NULL			}
};

/************************************************************************/

int luaopen_org_conman_strcore(lua_State *const L)
{
#if LUA_VERSION_NUM == 501
  luaL_register(L,"org.conman.string",m_strcore_reg);
#else
  luaL_newlib(L,m_strcore_reg);
#endif
  return 1;
}

/************************************************************************/

static bool find_break_point(
	size_t     *const restrict pidx,
	const char *const restrict txt
)
{
  size_t idx;
  
  assert(pidx  != NULL);
  assert(*pidx >  0);
  assert(txt   != NULL);

  for (idx = *pidx ; idx ; idx--)
    if (isspace(txt[idx])) break;
  
  if (idx)
  {
    *pidx = idx + 1;
    return true;
  }
  
  return false;
}

/************************************************************************/

static int strcore_wrapt(lua_State *const L)
{
  const char *src;
  size_t      ssz;
  size_t      margin;
  size_t      breakp;
  int         i;
  
  src    = luaL_checklstring(L,1,&ssz);
  margin = luaL_optinteger(L,2,DEF_MARGIN);
  breakp = margin;
  
  lua_createtable(L,0,0);
  
  for (i = 1 ; ssz > breakp ; i++)
  {
    if (find_break_point(&breakp,src))
    {
      lua_pushinteger(L,i);
      lua_pushlstring(L,src,breakp - 1);
      lua_settable(L,-3);
      
      src    += breakp;
      ssz    -= breakp;
      breakp  = margin;
    }
    else
      break;
  }
  
  lua_pushinteger(L,i);
  lua_pushlstring(L,src,ssz);
  lua_settable(L,-3);
  
  return 1;
}

/************************************************************************/  

static int strcore_soundex(lua_State *const L)
{
  static const char        ignore[] = "AEIOUWYH";
  static const char *const use[6]   =
  {
    "BPFV",
    "CSKGJQXZ",
    "DT",
    "L",
    "MN",
    "R"
  };
  
  char        sdx[4];
  const char *word = luaL_checkstring(L,1);
  char        c;
  char        last;
  size_t      idx;
  
  sdx[0] = last   = toupper(*word++);
  sdx[1] = sdx[2] = sdx[3] = '0';
  idx    = 1;
  
  while((idx < 4) && ((c = toupper(*word++)) != '\0'))
  {
    if (strchr(ignore,c) != NULL)
      continue;
    
    for (size_t i = 0 ; i < 6 ; i++)
    {
      if (strchr(use[i],c) != NULL)
      {
        if (strchr(use[i],last) != NULL)
          continue;
        last = c;
        sdx[idx++] = '1' + i;
      }
    }
  }
  
  lua_pushlstring(L,sdx,4);
  return 1;  
}

/************************************************************************/

static const char vsfn[26] =
{
   1,16,4,16,9,2,4,16,9,2,0,2,2,2,1,4,0,2,4,4,1,0,0,0,8,0
/* A  B C  D E F G  H I J K L M N O P Q R S T U V W X Y Z */
};

static inline bool vowel(int x) { return isalpha(x) && (vsfn[(x) - 'A'] & 1); }
static inline bool same(int x)  { return isalpha(x) && (vsfn[(x) - 'A'] & 2); }
static inline bool varson(int x){ return isalpha(x) && (vsfn[(x) - 'A'] & 4); }
static inline bool frontv(int x){ return isalpha(x) && (vsfn[(x) - 'A'] & 8); }
static inline bool noghf(int x) { return isalpha(x) && (vsfn[(x) - 'A'] & 16);}

/************************************************************************/

static int strcore_metaphone(lua_State *const L)
{
  luaL_Buffer  metaph;
  const char  *word;
  size_t       wordsize;
  
  word = luaL_checklstring(L,1,&wordsize);
  luaL_buffinit(L,&metaph);
  
  char ntrans[wordsize + 4];
  char *n;
  char *n_start;
  char *n_end;
  
  ntrans[0] = '\0';
  
  for (
    n = ntrans + 1 , n_end = ntrans + wordsize + 1; 
    (*word != '\0') && (n < n_end);
    word++
  )  
  {
    if (isalpha(*word))
      *n++ = toupper(*word);
  }
  
  if (n == ntrans + 1)
  {
    luaL_pushresult(&metaph);
    return 1;
  }
  
  /* Begin preprocessing */
  n_end = n;
  *n++  = '\0';
  *n    = '\0';
  n     = ntrans + 1;
  
  switch (*n)
  {
    case 'P': 
    case 'K': 
    case 'G':
         if (n[1] == 'N') 
           *n++ = '\0';  
         break;
      
    case 'A':
         if (n[1] == 'E')
           *n++ = '\0';
         break;
      
    case 'W':
         if (n[1] == 'R')
           *n++ = '\0';
         else if (n[1] == 'H')	/* bug fix - 19991121.1056 spc */
         {
           n[1] = *n;
           *n++  = '\0';
         } 
         break;
      
    case 'X':
         *n = 'S';
         break;
  }     
  
  /* Now, iterate over the string, stopping at the end of the string or
   * when we have computed sufficient characters.
   */
  
  bool KSFlag = false;
  
  for (n_start = n ; n <= n_end ; n++)
  {
    if (KSFlag)
    {
      KSFlag = false;
      if (*n)
        luaL_addchar(&metaph,*n);      
    }
    else
    {
      /* drop duplicates except for 'CC' */
      if ((n[-1] == *n) && (*n != 'C'))
        continue;
        
      if (same(*n) || ((n == n_start) && vowel(*n)))
      {
        if (*n)
          luaL_addchar(&metaph,*n);
      }
      else 
      {
        switch (*n)
        {
          case 'B':
               if ((n < n_end) || (n[-1] != 'M'))
               {
                 if (*n)
                   luaL_addchar(&metaph,*n);
               }
               break;
               
          case 'C':
               if ((n[-1] != 'S') || (!frontv(n[1])))
                {
                  if ((n[1] == 'I') || (n[2] == 'A'))
                    luaL_addchar(&metaph,'X');
                  else if (frontv(n[1]))
                    luaL_addchar(&metaph,'S');
                  else if (n[1] == 'H')
                    luaL_addchar(&metaph,
                                 (((n == n_start) && (!vowel(n[2]))) ||
                                 (n[-1] == 'S')) ? 'K' : 'X');
                  else
                    luaL_addchar(&metaph,'K');
                }
               break;
             
          case 'D':
               luaL_addchar(&metaph,((n[1] == 'G') && frontv(n[2])) ? 'J' : 'T');
               break;
               
          case 'G':
               if (((n[1]  != 'H') || vowel(n[2])) &&
                   ((n[1]  != 'N') || ((n + 1) < n_end &&
                   ((n[2]  != 'E') || n[3] != 'D'))) &&
                   ((n[-1] != 'D') || !frontv(n[1])))
                     luaL_addchar(&metaph,(frontv(n[1]) && (n[2] != 'G')) ? 'J' : 'K');
               else if (n[1] == 'H' && !noghf(n[-1]) && (n[-4]) != 'H')
                     luaL_addchar(&metaph,'F');
               break;

          case 'H':
               if (!varson(n[-1]) && (!vowel(n[-1]) || vowel(n[1])))
                 luaL_addchar(&metaph,'H');
               break;
               
          case 'K':
               if (n[-1] != 'C')
                 luaL_addchar(&metaph,'K');
               break;
               
          case 'P':
               luaL_addchar(&metaph, (n[1] == 'H') ? 'F' : 'P');
               break;
               
          case 'Q':
               luaL_addchar(&metaph,'K');
               break;
               
          case 'S':
               luaL_addchar(&metaph,((n[1] == 'H') || ((n[1] == 'I') && 
                      (((n[2] == 'O') || (n[2] == 'A'))))) ? 'X' : 'S');
               break;
               
          case 'T':
               if ((n[1] == 'I') && ((n[2] == 'O') || (n[2] == 'A')))
                 luaL_addchar(&metaph,'X');
               else if (n[1] == 'H')
                 luaL_addchar(&metaph,'O');
               else if ((n[1] != 'C') || (n[2] != 'H'))
                 luaL_addchar(&metaph,'T');
               break;
               
          case 'V':
               luaL_addchar(&metaph,'F');
               break;
               
          case 'W':
          case 'Y':
               if (vowel(n[1]))
               {
                 if (*n)
                   luaL_addchar(&metaph,*n);
               }
               break;
               
          case 'X':
               if (n == n_start) 
                 luaL_addchar(&metaph,'S');
               else
                {
                  luaL_addchar(&metaph,'K');
                  KSFlag    = true;
                }
               break;
               
              case 'Z':
                luaL_addchar(&metaph,'S');
                break;
                                  
        }
      }
    }
  }
  
  luaL_pushresult(&metaph);
  return 1; 
}

/************************************************************************/

