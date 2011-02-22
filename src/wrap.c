
#include <stddef.h>
#include <ctype.h>
#include <stdbool.h>
#include <assert.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#define DEF_MARGIN	78

/*************************************************************************/

static bool find_break_point(
	size_t     *const restrict pidx,
	const char *const restrict txt,
	const size_t               max
)
{
  size_t idx;
  
  assert(pidx  != NULL);
  assert(*pidx >  0);
  assert(txt   != NULL);
  assert(max   >  0);
  assert(max   >  *pidx);

  for (idx = *pidx ; idx ; idx--)
    if (isspace(txt[idx])) break;
  
  if (idx)
  {
    *pidx = idx + 1;
    return true;
  }
  
  return false;
}

/**************************************************************************/

static int wrap(lua_State *L)
{
  const char *src;
  size_t      ssz;
  size_t      margin;
  size_t      breakp;
  luaL_Buffer buf;
  
  src    = luaL_checklstring(L,1,&ssz);
  margin = luaL_optinteger(L,2,DEF_MARGIN);
  breakp = margin;
  
  luaL_buffinit(L,&buf);
  
  while(true)
  {
    if (ssz < breakp)
      break;

    if (find_break_point(&breakp,src,ssz))
    {
      luaL_addlstring(&buf,src,breakp - 1);
      luaL_addchar(&buf,'\n');
      src    += breakp;
      ssz    -= breakp;
      breakp  = margin;
    }
    else
      break;
  }
  
  luaL_addlstring(&buf,src,ssz);
  luaL_addchar(&buf,'\n');
  luaL_pushresult(&buf);
  return 1;
}

/**************************************************************************/

int luaopen_org_conman_string_wrap(lua_State *L)
{
  assert(L != NULL);

  lua_pushcfunction(L,wrap);
  return 1;
}

/**************************************************************************/

