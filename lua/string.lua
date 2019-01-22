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
-- WARNING: Lua 5.3 or above

local uchar    = require "org.conman.parsers.utf8"
local lpeg     = require "lpeg"
local string   = require "string"
local table    = require "table"
local io       = require "io"

local require  = require
local type     = type

_ENV = {}
local x   = require "org.conman.strcore"
metaphone = x.metaphone
compare   = x.compare
comparen  = x.comparen

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

local Cs = lpeg.Cs
local P  = lpeg.P
local R  = lpeg.R

local whitespace = P" "
                 + P"\t"
                 + P"\u{1680}" -- Ogham
                 + P"\u{180E}" -- Mongolian vowel separator
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
                 + P"\u{200B}" -- zero width
                 + P"\u{200C}" -- zero-width nonjoiner
                 + P"\u{200D}" -- zero-width joiner
                 + P"\u{205F}" -- medium mathematical
                 + P"\u{3000}" -- ideographic
                 
local combining  = P"\204"     * R"\128\191"
                 + P"\205"     * R("\128\142","\144\175") -- ignore CGJ
                 + P"\225\170" * R"\176\190"
                 + P"\225\183" * R"\128\191"
                 + P"\226\131" * R"\144\176"
                 + P"\239\184" * R"\160\175"
                 
local ignore     = P"\205\143"
                 
local shy        = P"\u{00AD}" -- shy hyphen
                 + P"\u{1806}" -- Mongolian TODO soft hyphen
                 
local hyphen     = P"-"
                 + P"\u{058A}" -- Armenian
                 + P"\u{2010}" -- hyphen
                 + P"\u{2012}" -- figure dash
                 + P"\u{2013}" -- en-dash
                 + P"\u{2014}" -- em-dash
                 + P"\u{2E17}" -- double oblique
                 + P"\u{30FB}" -- Katakana middle dot
                 + P"\u{FE63}" -- small hyphen-minus
                 + P"\u{FF0D}" -- fullwidth hyphen-minus
                 + P"\u{FF65}" -- halfwidth Katakana middle dot
                 
local chars      = whitespace * lpeg.Cp() * lpeg.Cc'space'
                 + shy        * lpeg.Cp() * lpeg.Cc'shy'
                 + hyphen     * lpeg.Cp() * lpeg.Cc'hyphen'
                 + combining  * lpeg.Cp() * lpeg.Cc'combine'
                 + ignore     * lpeg.Cp() * lpeg.Cc'ignore'
                 + uchar      * lpeg.Cp() * lpeg.Cc'char'
                 +              lpeg.Cp() * lpeg.Cc'bad'
                 
local remshy     = Cs((shy * #P(1)/ "" + P(1))^0) -- keep last soft hyphen

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
      
    -- ---------------------------------------------------------------------
    -- The difference between soft hyphens and hyphens is that soft hyphens
    -- are not normally visible unless they're at the end of the line.  So
    -- soft hyphens don't cound against the glyph count, but do mark a
    -- potential location for a break.  Regular hyphens do count as a
    -- character, and also count towards the glyph count.
    --
    -- In both cases, we include the character for this line, unlike for
    -- whitespace.
    -- ---------------------------------------------------------------------
    
    elseif ctype == 'shy' then
      if cnt < margin then
        breakhere = n
        resume    = n
      end
    elseif ctype == 'hyphen' then
      if cnt < margin then
        breakhere = n
        resume    = n
        cnt       = cnt + 1
      end
      
    elseif ctype == 'char' then
      cnt  = cnt  + 1
    elseif ctype == 'combining' then -- luacheck: ignore
      -- combining chars don't count
    elseif ctype == 'ignore' then -- luacheck: ignore
      -- ignore characters are ignored
    elseif ctype == 'bad' then
      assert(false,"bad character")
    end
    
    -- ---------------------------------------------------------------------
    -- If we've past the margin, wrap the line at the previous breakhere
    -- point.  If there isn't one, just break were we are.  It looks ugly,
    -- but this line violates our contraints because there are no possible
    -- breakpoints upon which to break.
    --
    -- Soft hypnens are also removed at this point, because they're not
    -- supposed to be visible unless they're at the end of a line.
    -- ---------------------------------------------------------------------
    
    if cnt > margin then
      if breakhere then
        table.insert(res,remshy:match(s:sub(front,breakhere - 1)))
        front     = resume
        i         = resume
        cnt       = 0
        breakhere = nil
      else
        table.insert(res,remshy:match(s:sub(front,i - 1)))
        front     = i
        cnt       = 0
        breakhere = nil
      end
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

return _ENV
