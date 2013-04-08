/*********************************************************************
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
* The main algorithm was placed into the public domain by Gary A. Parker
* and transcribed into machine readable format by Mark Grosbert from 
* The C Gazette Jun/Jul 1991, Volume 5, No 4., Pg 55, and further modified
* by Sean Conner for use with Lua.
*
*********************************************************************/

#include <ctype.h>
#include <string.h>
#include <stdbool.h>

#include <lua.h>
#include <lauxlib.h>

/********************************************************************/

static const char vsfn[26] =
{
   1,16,4,16,9,2,4,16,9,2,0,2,2,2,1,4,0,2,4,4,1,0,0,0,8,0
/* A  B C  D E F G  H I J K L M N O P Q R S T U V W X Y Z */
};

/* Testing macros */
#define vowel(x)   (vsfn[(x) - 'A'] & 1)
#define same(x)    (vsfn[(x) - 'A'] & 2)
#define varson(x)  (vsfn[(x) - 'A'] & 4)
#define frontv(x)  (vsfn[(x) - 'A'] & 8)
#define noghf(x)   (vsfn[(x) - 'A'] & 16)

/********************************************************************/

static int metaphone_lua(lua_State *const L)
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
      luaL_addchar(&metaph,*n);      
    }
    else
    {
      /* drop duplicates except for 'CC' */
      if ((n[-1] == *n) && (*n != 'C'))
        continue;
        
      if (same(*n) || ((n == n_start) && vowel(*n)))
        luaL_addchar(&metaph,*n);
      else 
      {
        switch (*n)
        {
          case 'B':
               if ((n < n_end) || (n[-1] != 'M'))
                 luaL_addchar(&metaph,*n);
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
                 luaL_addchar(&metaph,*n);
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

/********************************************************************/

int luaopen_org_conman_string_metaphone(lua_State *const L)
{
  lua_pushcfunction(L,metaphone_lua);
  return 1;
}

/********************************************************************/

