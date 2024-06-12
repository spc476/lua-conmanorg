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

#define _GNU_SOURCE

#include <stdlib.h>
#include <wchar.h>

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

/************************************************************************/

enum eclass
{
  BAD,
  SPACE,
  COMBINING,
  SHY,
  HYPHEN,
  RESET,
  CHAR,
};

struct sclass
{
  wchar_t     c;
  enum eclass class;
};

/*************************************************************************/

static int cclasscmp(void const *left,void const *right)
{
  wchar_t const       *key   = left;
  struct sclass const *value = right;
  return *key - value->c;
}

/*************************************************************************/

static enum eclass charsmatch(unsigned char const *s,size_t i,size_t *n)
{
  static struct sclass const sclass[] =
  {
    {     9 , SPACE     } ,
    {    32 , SPACE     } ,
    {    45 , HYPHEN    } ,
    {   133 , RESET     } , /* NEL-definitely break here */
    {   173 , SHY       } ,
    {   768 , COMBINING } ,
    {   769 , COMBINING } ,
    {   770 , COMBINING } ,
    {   771 , COMBINING } ,
    {   772 , COMBINING } ,
    {   773 , COMBINING } ,
    {   774 , COMBINING } ,
    {   775 , COMBINING } ,
    {   776 , COMBINING } ,
    {   777 , COMBINING } ,
    {   778 , COMBINING } ,
    {   779 , COMBINING } ,
    {   780 , COMBINING } ,
    {   781 , COMBINING } ,
    {   782 , COMBINING } ,
    {   783 , COMBINING } ,
    {   784 , COMBINING } ,
    {   785 , COMBINING } ,
    {   786 , COMBINING } ,
    {   787 , COMBINING } ,
    {   788 , COMBINING } ,
    {   789 , COMBINING } ,
    {   790 , COMBINING } ,
    {   791 , COMBINING } ,
    {   792 , COMBINING } ,
    {   793 , COMBINING } ,
    {   794 , COMBINING } ,
    {   795 , COMBINING } ,
    {   796 , COMBINING } ,
    {   797 , COMBINING } ,
    {   798 , COMBINING } ,
    {   799 , COMBINING } ,
    {   800 , COMBINING } ,
    {   801 , COMBINING } ,
    {   802 , COMBINING } ,
    {   803 , COMBINING } ,
    {   804 , COMBINING } ,
    {   805 , COMBINING } ,
    {   806 , COMBINING } ,
    {   807 , COMBINING } ,
    {   808 , COMBINING } ,
    {   809 , COMBINING } ,
    {   810 , COMBINING } ,
    {   811 , COMBINING } ,
    {   812 , COMBINING } ,
    {   813 , COMBINING } ,
    {   814 , COMBINING } ,
    {   815 , COMBINING } ,
    {   816 , COMBINING } ,
    {   817 , COMBINING } ,
    {   818 , COMBINING } ,
    {   819 , COMBINING } ,
    {   820 , COMBINING } ,
    {   821 , COMBINING } ,
    {   822 , COMBINING } ,
    {   823 , COMBINING } ,
    {   824 , COMBINING } ,
    {   825 , COMBINING } ,
    {   826 , COMBINING } ,
    {   827 , COMBINING } ,
    {   828 , COMBINING } ,
    {   829 , COMBINING } ,
    {   830 , COMBINING } ,
    {   831 , COMBINING } ,
    {   832 , COMBINING } ,
    {   833 , COMBINING } ,
    {   834 , COMBINING } ,
    {   835 , COMBINING } ,
    {   836 , COMBINING } ,
    {   837 , COMBINING } ,
    {   838 , COMBINING } ,
    {   839 , COMBINING } ,
    {   840 , COMBINING } ,
    {   841 , COMBINING } ,
    {   842 , COMBINING } ,
    {   843 , COMBINING } ,
    {   844 , COMBINING } ,
    {   845 , COMBINING } ,
    {   846 , COMBINING } ,
    {   847 , COMBINING } ,
    {   848 , COMBINING } ,
    {  1418 , HYPHEN    } , /* Armenian */
    {  5760 , SPACE     } , /* Ogham */
    {  6150 , SHY       } ,
    {  6158 , SPACE     } , /* Mongolian vowel separator */
    {  6832 , COMBINING } ,
    {  6833 , COMBINING } ,
    {  6834 , COMBINING } ,
    {  6835 , COMBINING } ,
    {  6836 , COMBINING } ,
    {  6837 , COMBINING } ,
    {  6838 , COMBINING } ,
    {  6839 , COMBINING } ,
    {  6840 , COMBINING } ,
    {  6841 , COMBINING } ,
    {  6842 , COMBINING } ,
    {  6843 , COMBINING } ,
    {  6844 , COMBINING } ,
    {  6845 , COMBINING } ,
    {  6846 , COMBINING } ,
    {  7616 , COMBINING } ,
    {  7617 , COMBINING } ,
    {  7618 , COMBINING } ,
    {  7619 , COMBINING } ,
    {  7620 , COMBINING } ,
    {  7621 , COMBINING } ,
    {  7622 , COMBINING } ,
    {  7623 , COMBINING } ,
    {  7624 , COMBINING } ,
    {  7625 , COMBINING } ,
    {  7626 , COMBINING } ,
    {  7627 , COMBINING } ,
    {  7628 , COMBINING } ,
    {  7629 , COMBINING } ,
    {  7630 , COMBINING } ,
    {  7631 , COMBINING } ,
    {  7632 , COMBINING } ,
    {  7633 , COMBINING } ,
    {  7634 , COMBINING } ,
    {  7635 , COMBINING } ,
    {  7636 , COMBINING } ,
    {  7637 , COMBINING } ,
    {  7638 , COMBINING } ,
    {  7639 , COMBINING } ,
    {  7640 , COMBINING } ,
    {  7641 , COMBINING } ,
    {  7642 , COMBINING } ,
    {  7643 , COMBINING } ,
    {  7644 , COMBINING } ,
    {  7645 , COMBINING } ,
    {  7646 , COMBINING } ,
    {  7647 , COMBINING } ,
    {  7648 , COMBINING } ,
    {  7649 , COMBINING } ,
    {  7650 , COMBINING } ,
    {  7651 , COMBINING } ,
    {  7652 , COMBINING } ,
    {  7653 , COMBINING } ,
    {  7654 , COMBINING } ,
    {  7655 , COMBINING } ,
    {  7656 , COMBINING } ,
    {  7657 , COMBINING } ,
    {  7658 , COMBINING } ,
    {  7659 , COMBINING } ,
    {  7660 , COMBINING } ,
    {  7661 , COMBINING } ,
    {  7662 , COMBINING } ,
    {  7663 , COMBINING } ,
    {  7664 , COMBINING } ,
    {  7665 , COMBINING } ,
    {  7666 , COMBINING } ,
    {  7667 , COMBINING } ,
    {  7668 , COMBINING } ,
    {  7669 , COMBINING } ,
    {  7670 , COMBINING } ,
    {  7671 , COMBINING } ,
    {  7672 , COMBINING } ,
    {  7673 , COMBINING } ,
    {  7674 , COMBINING } ,
    {  7675 , COMBINING } ,
    {  7676 , COMBINING } ,
    {  7677 , COMBINING } ,
    {  7678 , COMBINING } ,
    {  7679 , COMBINING } ,
    {  8192 , SPACE     } , /* en quad */
    {  8193 , SPACE     } , /* em quad */
    {  8194 , SPACE     } , /* en */
    {  8195 , SPACE     } , /* em */
    {  8196 , SPACE     } , /* three-per-em */
    {  8197 , SPACE     } , /* four-per-em */
    {  8198 , SPACE     } , /* six-per-em */
    {  8200 , SPACE     } , /* punctuation */
    {  8201 , SPACE     } , /* thin space */
    {  8202 , SPACE     } , /* hair space */
    {  8203 , SPACE     } , /* zero width */
    {  8204 , SPACE     } , /* zero-width nonjoiner */
    {  8205 , SPACE     } , /* zero-width joiner */
    {  8208 , HYPHEN    } , /* hyphen */
    {  8210 , HYPHEN    } , /* figure dash */
    {  8211 , HYPHEN    } , /* en-dash */
    {  8212 , HYPHEN    } , /* em-dash */
    {  8287 , SPACE     } , /* medium mathematical */
    {  8400 , COMBINING } ,
    {  8401 , COMBINING } ,
    {  8402 , COMBINING } ,
    {  8403 , COMBINING } ,
    {  8404 , COMBINING } ,
    {  8405 , COMBINING } ,
    {  8406 , COMBINING } ,
    {  8407 , COMBINING } ,
    {  8408 , COMBINING } ,
    {  8409 , COMBINING } ,
    {  8410 , COMBINING } ,
    {  8411 , COMBINING } ,
    {  8412 , COMBINING } ,
    {  8413 , COMBINING } ,
    {  8414 , COMBINING } ,
    {  8415 , COMBINING } ,
    {  8416 , COMBINING } ,
    {  8417 , COMBINING } ,
    {  8418 , COMBINING } ,
    {  8419 , COMBINING } ,
    {  8420 , COMBINING } ,
    {  8421 , COMBINING } ,
    {  8422 , COMBINING } ,
    {  8423 , COMBINING } ,
    {  8424 , COMBINING } ,
    {  8425 , COMBINING } ,
    {  8426 , COMBINING } ,
    {  8427 , COMBINING } ,
    {  8428 , COMBINING } ,
    {  8429 , COMBINING } ,
    {  8430 , COMBINING } ,
    {  8431 , COMBINING } ,
    {  8432 , COMBINING } ,
    { 11799 , HYPHEN    } , /* double oblique */
    { 12288 , SPACE     } , /* ideographic */
    { 12539 , HYPHEN    } , /* Katakana middle dot */
    { 65056 , COMBINING } ,
    { 65057 , COMBINING } ,
    { 65058 , COMBINING } ,
    { 65059 , COMBINING } ,
    { 65060 , COMBINING } ,
    { 65061 , COMBINING } ,
    { 65062 , COMBINING } ,
    { 65063 , COMBINING } ,
    { 65064 , COMBINING } ,
    { 65065 , COMBINING } ,
    { 65066 , COMBINING } ,
    { 65067 , COMBINING } ,
    { 65068 , COMBINING } ,
    { 65069 , COMBINING } ,
    { 65070 , COMBINING } ,
    { 65071 , COMBINING } ,
    { 65123 , HYPHEN    } , /* small hyphen-minus */
    { 65293 , HYPHEN    } , /* fullwidth hyphen-minus */
    { 65381 , HYPHEN    } , /* halfwidth Katakana middle dot */
  };
  
  struct sclass const *ctype;
  wchar_t              c = s[i];
  wchar_t              c2;
  
  if (c < 0x7F)
    *n = i+1;
  else
  {
    if ((c < 0xC2) || (c > 0xF4))
      return BAD;
    else if ((c & 0xE0) == 0xC0)
    {
      c = (c & 0x1F) << 6;
      c2 = s[i+1];
      if ((c2 & 0xC0) != 0x80)
        return BAD;
      c  = c | (c2 & 0x3F);
      *n = i + 2;
    }
    else if ((c & 0xF0) == 0xE0)
    {
      c = (c & 0x0F) << 12;
      c2 = s[i+1];
      if ((c2 & 0xC0) != 0x80)
        return BAD;
      c = c | ((c2 & 0x3F) << 6);
      c2 = s[i+2];
      if ((c2 & 0xC0) != 0x80)
        return BAD;
      c  = c | (c2 & 0x3F);
      *n = i + 3;
    }
    else
    {
      c = (c & 0xF1) << 18;
      c2 = s[i+1];
      if ((c2 & 0xC0) != 0x80)
        return BAD;
      c = c | ((c2 & 0x3F) << 12);
      c2 = s[i+2];
      if ((c2 & 0xC0) != 0x80)
        return BAD;
      c = c | ((c2 & 0x3F) << 6);
      c2 = s[i+3];
      if ((c2 & 0xC0) != 0x80)
        return BAD;
      c  = c | (c2 & 0x3F);
      *n = i + 4;
    }
  }
  
  ctype = bsearch(&c,sclass,sizeof(sclass)/sizeof(sclass[0]),sizeof(sclass[0]),cclasscmp);
  if (ctype != NULL)
    return ctype->class;
  else
    return CHAR;
}

/*************************************************************************/

static void remshy(lua_State *L,char const *s,size_t i,size_t e)
{
  size_t      n;
  luaL_Buffer buff;
  
  if (i >= e)
    luaL_error(L,"bad length: i=%I e=%I",(lua_Integer)i,(lua_Integer)e);
    
  luaL_buffinit(L,&buff);
  while(i < e)
  {
    enum eclass cc = charsmatch((unsigned char const *)s,i,&n);
    if (cc == BAD)
      luaL_error(L,"bad character (should not happen)");
      
    if (cc != SHY)
      luaL_addlstring(&buff,&s[i],n-i);
    else
    {
      if (n >= e)
        luaL_addlstring(&buff,&s[i],n-i);
    }
    
    if (n <= i)
      luaL_error(L,"remshy(): i=%I n=%I",(lua_Integer)i,(lua_Integer)n);
    i = n;
  }
  
  luaL_pushresult(&buff);
}

/*************************************************************************/

static int strcore_wrapt(lua_State *L)
{

  size_t      len;
  char const *s         = luaL_checklstring(L,1,&len);
  size_t      margin    = luaL_optinteger(L,2,78);
  size_t      i         = 0;
  size_t      front     = 0;
  size_t      cnt       = 0;
  size_t      breakhere = 0;
  size_t      resume    = 0;
  size_t      n;
  lua_Integer ri        = 1;
  
  lua_createtable(L,0,0);
  
  while(i < len)
  {
    switch(charsmatch((unsigned char const *)s,i,&n))
    {
      case BAD:
           luaL_error(L,"bad character");
           break;
           
      case SPACE:
           breakhere = i;
           resume    = n;
           cnt++;
           break;
           
      case COMBINING:
           break;
           
      /*-------------------------------------------------------------------
      ; The difference between soft hyphens and hyphens is that soft hyphens
      ; are not normally visible unless they're at the end of the line.  So
      ; soft hyphens don't cound against the glyph count, but do mark a
      ; potential location for a break.  Regular hyphens do count as a
      ; character, and also count towards the glyph count.
      ;
      ; In both cases, we include the character for this line, unlike for
      ; whitespace.
      ;-------------------------------------------------------------------*/
      
      case SHY:
           if (cnt < margin)
           {
             breakhere = n;
             resume    = n;
           }
           break;
           
      case HYPHEN:
           if (cnt < margin)
           {
             breakhere = n;
             cnt++;
           }
           
           /*----------------------------------------------
           ; This handles the case of a clump of dashes.
           ;-----------------------------------------------*/
           
           else if (cnt == margin)
           {
             breakhere = i;
             cnt       = margin + 1;
           }
           
           resume = n;
           break;
           
      case RESET:
           /*-----------------------------------------------------------
           ; the only control character we accept here, the NEL, which
           ; here means "definitely break here!"
           ;------------------------------------------------------------*/
           
           breakhere = i;
           resume    = n;
           cnt       = margin + 1;
           break;
           
      case CHAR:
           cnt++;
           break;
    }
    
    /*--------------------------------------------------------------------
    ; If we've past the margin, wrap the line at the previous breakhere
    ; point.  If there isn't one, just break were we are.  It looks ugly,
    ; but this line violates our contraints because there are no possible
    ; breakpoints upon which to break.
    ;
    ; Soft hypnens are also removed at this point, because they're not
    ; supposed to be visible unless they're at the end of a line.
    ;---------------------------------------------------------------------*/
    
    if (cnt > margin)
    {
      if (breakhere > 0)
      {
        if (breakhere > len)
          luaL_error(L,"breadhere=%I len=%I",(lua_Integer)breakhere,(lua_Integer)len);
          
        remshy(L,s,front,breakhere);
        front = resume;
        i     = resume;
      }
      else
      {
        if (i > len)
          luaL_error(L,"a) i=%I len=%I",(lua_Integer)i,(lua_Integer)len);
          
        remshy(L,s,front,i);
        front = i;
      }
      lua_seti(L,-2,ri++);
      cnt       = 0;
      breakhere = 0;
    }
    else
    {
      if (n <= i)
        luaL_error(L,"wrapt(): i=%I n=%I",(lua_Integer)i,(lua_Integer)n);
      i = n;
    }
  }
  
  if (i > len)
    luaL_error(L,"b) i=%I len=%I",(lua_Integer)i,(lua_Integer)len);
    
  remshy(L,s,front,i);
  lua_seti(L,-2,ri);
  return 1;
}

/*************************************************************************/

static char const vsfn[26] =
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

static int strcore_metaphone(lua_State *L)
{
  luaL_Buffer  metaph;
  char const  *word;
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
         else if (n[1] == 'H')  /* bug fix - 19991121.1056 spc */
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

static int strcore_compare(lua_State *L)
{
  lua_pushinteger(
          L,
          strcmp(
                  luaL_checkstring(L,1),
                  luaL_checkstring(L,2)
                )
  );
  return 1;
}

/************************************************************************/

static int strcore_comparen(lua_State *L)
{
  size_t      destsz;
  size_t      srcsz;
  size_t      len;
  char const *dest = luaL_checklstring(L,1,&destsz);
  char const *src  = luaL_checklstring(L,2,&srcsz);
  
  len = (destsz < srcsz)
      ? destsz
      : srcsz
      ;
  lua_pushinteger(L,strncmp(dest,src,len));
  return 1;
}

/************************************************************************/

static int strcore_comparei(lua_State *L)
{
  lua_pushinteger(
          L,
          strcasecmp(
                  luaL_checkstring(L,1),
                  luaL_checkstring(L,2)
          )
  );
  return 1;
}

/************************************************************************/

static void C0(luaL_Buffer *buf,unsigned char c)
{
  assert(buf != NULL);
  
  char num[4];
  luaL_addchar(buf,'\\');
  
  switch(c)
  {
    case '\a': luaL_addchar(buf,'a'); break;
    case '\b': luaL_addchar(buf,'b'); break;
    case '\f': luaL_addchar(buf,'f'); break;
    case '\n': luaL_addchar(buf,'n'); break;
    case '\r': luaL_addchar(buf,'r'); break;
    case '\t': luaL_addchar(buf,'t'); break;
    case '\v': luaL_addchar(buf,'v'); break;
    default:
         snprintf(num,sizeof(num),"%03d",c);
         luaL_addlstring(buf,num,3);
         break;
  }
}

/************************************************************************/

static int strcore_safeascii(lua_State *L)
{
  size_t              len;
  unsigned char const *s = (unsigned char const *)luaL_checklstring(L,1,&len);
  luaL_Buffer          buf;
  
  luaL_buffinit(L,&buf);
  
  for ( ; len > 0 ; len--)
  {
    if (*s < ' ')
      C0(&buf,*s++);
    else if (*s > '~')
      C0(&buf,*s++);
    else
    {
      if (*s == '\\')
        luaL_addlstring(&buf,"\\\\",2);
      else
        luaL_addchar(&buf,*s);
      s++;
    }
  }
  
  luaL_pushresult(&buf);
  return 1;
}

/************************************************************************/

static void UTF8(luaL_Buffer *buf,unsigned char const **ps,size_t *plen)
{
  assert(buf   != NULL);
  assert(ps    != NULL);
  assert(*ps   != NULL);
  assert(**ps  >= 0xC2);
  assert(**ps  <= 0xF4);
  assert(plen  != NULL);
  assert(*plen >  0);
  
  size_t delta;
  
  if (((*ps)[0] & 0xE0) == 0xC0)
  {
    if (*plen < 2)
      goto bad;
    if (((*ps)[1] & 0xC0) != 0x80)
      goto bad;
    delta = 2;
  }
  else if (((*ps)[0] & 0xF0) == 0xE0)
  {
    if (*plen < 3)
      goto bad;
    if (((*ps)[1] & 0xC0) != 0x80)
      goto bad;
    if (((*ps)[2] & 0xC0) != 0x80)
      goto bad;
    delta = 3;
  }
  else
  {
    if (*plen < 4)
      goto bad;
    if (((*ps)[1] & 0xC0) != 0x80)
      goto bad;
    if (((*ps)[2] & 0xC0) != 0x80)
      goto bad;
    if (((*ps)[3] & 0xC0) != 0x80)
      goto bad;
    delta = 4;
  }
  
  luaL_addlstring(buf,(char *)*ps,delta);
  (*ps)   += delta;
  (*plen) -= delta;
  return;
  
bad:

  C0(buf,**ps);
  (*ps)++;
  (*plen)--;
}

/************************************************************************/

static int strcore_safeutf8(lua_State *L)
{
  size_t               len;
  unsigned char const *s = (unsigned char const *)luaL_checklstring(L,1,&len);
  luaL_Buffer          buf;
  
  luaL_buffinit(L,&buf);
  
  for ( ; len > 0 ; )
  {
    if (*s < ' ')
    {
      C0(&buf,*s++);
      len--;
    }
    else if (*s < 0x7F)
    {
      if (*s == '\\')
        luaL_addlstring(&buf,"\\\\",2);
      else
        luaL_addchar(&buf,*s);
      s++;
      len--;
    }
    else if (*s < 0xC2)
    {
      C0(&buf,*s++);
      len--;
    }
    else if (*s < 0xF5)
      UTF8(&buf,&s,&len);
    else
    {
      C0(&buf,*s++);
      len--;
    }
  }
  
  luaL_pushresult(&buf);
  return 1;
}

/************************************************************************/

int luaopen_org_conman_strcore(lua_State *L)
{
  static luaL_Reg const strcore_reg[] =
  {
    { "wrapt"     , strcore_wrapt         } ,
    { "metaphone" , strcore_metaphone     } ,
    { "compare"   , strcore_compare       } ,
    { "comparen"  , strcore_comparen      } ,
    { "comparei"  , strcore_comparei      } ,
    { "safeascii" , strcore_safeascii     } ,
    { "safeutf8"  , strcore_safeutf8      } ,
    { NULL        , NULL                  }
  };
  
  
#if LUA_VERSION_NUM == 501
  luaL_register(L,"org.conman.string",strcore_reg);
#else
  luaL_newlib(L,strcore_reg);
#endif
  return 1;
}

/************************************************************************/
