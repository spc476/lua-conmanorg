-- ***************************************************************
--
-- Copyright 2010 by Sean Conner.  All Rights Reserved.
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
-- ********************************************************************
-- luacheck: globals wrapt metaphone compare comparen split
-- luacheck: globals template filetemplate wrap
-- luacheck: ignore 611

local uchar    = require "org.conman.parsers.utf8"
local lpeg     = require "lpeg"
local string   = require "string"
local table    = require "table"
local io       = require "io"

local _VERSION = _VERSION
local require  = require
local type     = type

if _VERSION == "Lua 5.1" then
  module("org.conman.string")
  require "org.conman.strcore"
else
  _ENV = {}
  local x   = require "org.conman.strcore"
  metaphone = x.metaphone
  compare   = x.compare
  comparen  = x.comparen
end

-- ********************************************************************

function split(s,delim)
  local results = {}
  delim         = delim or "%:"
  local pattern = "([^" .. delim .. "]*)" .. delim .. "?"
  
  for segment in string.gmatch(s,pattern) do
    table.insert(results,segment)
  end
  
  return results
end

-- ********************************************************************

function template(temp,callbacks,data)
  local function cmd(tag)
    local word = string.sub(tag,3,-3)
    
    if type(callbacks[word]) == 'string' then
      return callbacks[word]
    elseif type(callbacks[word]) == 'function' then
      return callbacks[word](data)
    else
      return tostring(callbacks[word])
    end
  end
  
  local s = string.gsub(temp,"%%{[%w%.]+}%%",cmd)
  return s
end

-- ********************************************************************

function filetemplate(temp,callbacks,data)
  local f = io.open(temp,"r")
  local d = f:read("*a")
  f:close()
  return template(d,callbacks,data)
end

-- ********************************************************************

local P = lpeg.P
local R = lpeg.R

local whitespace = P"\t"
                 + P" "
                 + P"\u{1680}" -- Ogham
                 + P"\u{2000}" -- en quad
                 + P"\u{2001}" -- em quad
                 + P"\u{2002}" -- en
                 + P"\u{2003}" -- em
                 + P"\u{2004}" -- three-per-em
                 + P"\u{2005}" -- four-per-em
                 + P"\u{2006}" -- six-per-em
                 + P"\u{2008}" -- punctuation
                 + P"\u{2009}" -- thin space
                 + P"\u{200A}" -- hair space
                 + P"\u{205F}" -- medium mathematical
                 + P"\u{3000}" -- ideographic
                 + P"\u{180E}" -- Mongolian vowel separator
                 + P"\u{200B}" -- zero width
                 + P"\u{200C}" -- zero-width nonjoiner
                 + P"\u{200D}" -- zero-width joiner
                 
local combining  = P"\204"     * R"\128\191"
                 + P"\205"     * R("\128\142","\144\175") -- ignore CGJ
                 + P"\225\170" * R"\176\190"
                 + P"\225\183" * R"\128\191"
                 + P"\226\131" * R"\144\176"
                 + P"\239\184" * R"\160\175"
                 
local ignore     = P"\205\143"

local chars      = whitespace * lpeg.Cp() * lpeg.Cc'space'
                 + combining  * lpeg.Cp() * lpeg.Cc'combine'
                 + ignore     * lpeg.Cp() * lpeg.Cc'ignore'
                 + uchar      * lpeg.Cp() * lpeg.Cc'char'
                 +              lpeg.Cp() * lpeg.Cc'bad'
                 
function wrapt(s,margin)
  local res       = {}
  local front     = 1
  local i         = 1
  local cnt       = 0
  local breakhere
  local resume
  margin = margin or 78
  
  while i <= #s do
    local n,ctype = chars:match(s,i)
    
    if ctype == 'space' then
      breakhere = i
      resume    = n
      cnt       = cnt + 1
    elseif ctype == 'char' then
      cnt  = cnt  + 1
    elseif ctype == 'combining' then -- luacheck: ignore
      -- combining chars don't count
    elseif ctype == 'ignore' then -- luacheck: ignore
      -- ignore characters are ignored
    elseif ctype == 'bad' then
      assert(false,"bad character")
    end
    
    if cnt >= margin and breakhere then
      table.insert(res,s:sub(front,breakhere - 1))
      front     = resume
      i         = resume
      cnt       = 0
      breakhere = nil
    else
      i = n
    end
  end
  
  table.insert(res,s:sub(front,i - 1))
  return res
end

-- ********************************************************************

function wrap(s,margin,lead)
  lead      = lead or ""
  local res = wrapt(s,margin)
  
  -- -----------------------------------------------------------------------
  -- insert lead into the first position.  We then convert the table into a
  -- string, separated by a newline and the lead.  This conforms to the
  -- behavior of the C based version of this function.
  -- -----------------------------------------------------------------------
  
  table.insert(res,1,lead)
  return table.concat(res,"\n" .. lead)
end

-- ********************************************************************

if _VERSION >= "Lua 5.2" then
  return _ENV
end
