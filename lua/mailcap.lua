-- ************************************************************************
--
-- Handle mailcap files
-- Copyright 2019 by Sean Conner.  All Rights Reserved.
--
-- This library is free software; you can redistribute it and/or modify it
-- under the terms of the GNU Lesser General Public License as published by
-- the Free Software Foundation; either version 3 of the License, or (at your
-- option) any later version.
--
-- This library is distributed in the hope that it will be useful, but
-- WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
-- or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
-- License for more details.
--
-- You should have received a copy of the GNU Lesser General Public License
-- along with this library; if not, see <http://www.gnu.org/licenses/>.
--
-- Comments, questions and criticisms can be sent to: sean@conman.org
--
-- ************************************************************************
-- luacheck: ignore 611
--
-- https://www.commandlinux.com/man-page/man5/mailcap.5.html
-- RFC-1524
--
-- Lua 5.3 or higher

local lpeg = require "lpeg" -- semver: ~1.0.0

local Carg = lpeg.Carg
local Cc   = lpeg.Cc
local Cf   = lpeg.Cf
local Cg   = lpeg.Cg
local Cs   = lpeg.Cs
local Ct   = lpeg.Ct
local C    = lpeg.C
local P    = lpeg.P
local R    = lpeg.R

-- ************************************************************************

local mimetype do
  local iana_token = R("AZ","az","09","--","..")^1
  local majortype  = iana_token
  local minortype  = iana_token + P"*"
  
  mimetype = C(majortype) * P"/" * C(minortype)
end

-- ************************************************************************

local CAPS = {} do
  local EOLN        = P"\n" * Carg(1)
                    / function(c)
                        c._line = c._line + 1
                      end
  local SP          = P" "
                    + P"\\"  * EOLN
                    + P"\t"
  local SEMI        = P";"   * SP^0
  local char        = P"\\"  * EOLN / " "
                    + P[[\]] * C(1) / "%1"
                    + P"\t"         / " "
                    + R(" :","<\226","\192\255")
  local cmd         = Cs(char^1)
  local number      = R"09"^1 / tonumber
  local fields      = P'notes='              * Cg(cmd,     'notes')
                    + P'test='               * Cg(cmd,     'test')
                    + P'print='              * Cg(cmd,     'print')
                    + P'compose='            * Cg(cmd,     'compose')
                    + P'composetyped='       * Cg(cmd,     'composedtype')
                    + P'stream-buffer-size=' * Cg(number,  'stream_buffer_size')
                    + P'needsterminal'       * Cg(Cc(true),'needsterminal')
                    + P'copiousoutput'       * Cg(Cc(true),'copiousoutput')
                    + P'textualnewlines=1'   * Cg(Cc(true),'textualnewlines')
                    + P'textualnewlines=0' -- false by default
  local program     = Cg(cmd,'view')
  local data        = program  * (SEMI * fields)^0
  local entry       = mimetype *  SEMI * Ct(data) * EOLN
                    + Cc(false) * C(R("\t\t"," \126","\192\255")^0) * EOLN
                      
  local mailcapfile = Cf(Carg(1) * (Cg(entry))^0,function(caps,major,minor,node)
    if major then
      if not caps[major] then caps[major] = {} end
      if minor == '*' then
        local mt = getmetatable(caps[major])
        
        -- ----------------------------------------------------------------
        -- if mt exists, there's a duplicate entry, but since I'm not doing
        -- anything about, there's not much point in checking for it.
        -- ----------------------------------------------------------------
        
        if not mt then
          mt = {}
        end
        
        mt.__index = function()
          return node
        end
        
        setmetatable(caps[major],mt)
      else
        caps[major][minor] = node
      end
    end
    
    return caps
  end)
  
  local function readmailcap(fname)
    local caps = { _file = fname , _line = 0 }
    local f    = io.open(fname,"r")
    if f then
      mailcapfile:match(f:read("a"),1,caps)
      f:close()
    end
    return caps
  end
  
  -- **********************************************************************
  -- RFC-1524 states looking through $MAILCAPS and if not present, default
  -- to $HOME/.mailcap:/etc/mailcap:/usr/etc/mailcap:/usr/local/etc/mailcap
  -- I don't think that default order is great---I prefer going from local
  -- definitions to system definitions, so I change up the order to reflect
  -- that.  Also, I add /usr/share/etc/mailcap, juse because.
  -- **********************************************************************
  
  local mailcaps = os.getenv("MAILCAPS")
  
  if mailcaps then
    for path in mailcaps:gmatch "[^%:]+" do
      table.insert(CAPS,readmailcap(path))
    end
  else
    CAPS[1] = readmailcap(os.getenv("HOME") .. "/.mailcap")
    CAPS[2] = readmailcap("/usr/local/etc/mailcap")
    CAPS[3] = readmailcap("/usr/share/etc/mailcap")
    CAPS[4] = readmailcap("/usr/etc/mailcap")
    CAPS[5] = readmailcap("/etc/mailcap")
  end
end

-- ************************************************************************

local cmd do
  local char = P"%s" * Carg(1) * Carg(2)
             / function(s,f)
                 s.redirect = false
                 return f
               end
             + P"%{" * Carg(4) * C(R("AZ","az","09","__","--","..")^1) * P"}"
             / function(tab,cap)
                 return tab[cap] or ""
               end
             + P"%t" * Carg(3) / "%1"
             + R(" \126","\192\255")
             
  local redirect = Carg(1) * Carg(2)
                 / function(s,f)
                     if s.redirect then
                       return string.format(" %s %s",s.dir,f)
                     end
                   end
  cmd        = Carg(1) / function(s) s.redirect = true end
             * Cs(char^1 * redirect)
end

-- ************************************************************************

local function handle(what,dir,page,type,params)
  local function runthisp(node)
    if not node then return false end
    if node.test then
      local program = cmd:match(node.test,1,{ dir = "<" },page,type,params)
      return os.execute(program)
    else
      return true
    end
  end
  
  params = params or {}
  local major,minor = mimetype:match(type)
  
  for _,cap in ipairs(CAPS) do
    local node = cap[major] and cap[major][minor]
    if runthisp(node) then
      local program = cmd:match(node[what],1,{ dir = dir },page,type,params or {})
      return os.execute(program)
    end
  end
end

-- ************************************************************************

return {
  view = function(page,type,params)
    return handle('view','<',page,type,params)
  end,
  
  print = function(page,type,params)
    return handle('print','<',page,type,params)
  end,
  
  compose = function(page,type,params)
    return handle('compose','>',page,type,params)
  end,
  
  composetyped = function(page,type,params)
    return handle('composetyped','>',page,type,params)
  end,
}
