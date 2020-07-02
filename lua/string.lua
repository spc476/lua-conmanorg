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
-- luacheck: globals wrapt metaphone compare comparen mksplit split
-- luacheck: globals template filetemplate wrap safeascii safeutf8
-- luacheck: ignore 611

local uchar    = require "org.conman.parsers.utf8"
local lpeg     = require "lpeg"
local string   = require "string"
local table    = require "table"
local io       = require "io"

local _VERSION = _VERSION
local require  = require
local type     = type
local tostring = tostring
local assert   = assert

if _VERSION == "Lua 5.1" then
  module("org.conman.string")
else
  _ENV = {}
end

local x   = require "org.conman.strcore"
metaphone = x.metaphone
compare   = x.compare
comparen  = x.comparen

-- ********************************************************************

local function escape(s)
  return s:gsub(".",function(c)
    return string.format("\\%03d",c:byte())
  end)
end

-- ********************************************************************

local C0    = lpeg.P"\a" / "\\a"  -- acutally, only the ones with a
            + lpeg.P"\b" / "\\b"  -- predefined escpae is here.
            + lpeg.P"\t" / "\\t"
            + lpeg.P"\n" / "\\n"
            + lpeg.P"\v" / "\\v"
            + lpeg.P"\f" / "\\f"
            + lpeg.P"\r" / "\\r"
local ascii = lpeg.P"\\" / "\\\\" -- escape the escape
            + lpeg.R" ~"
local other = lpeg.P(1) / escape

local make_ascii_safe = lpeg.Cs((ascii         + C0 + other)^0)
local make_utf8_safe  = lpeg.Cs((ascii + uchar + C0 + other)^0)

-- ********************************************************************

function safeascii(s)
  return make_ascii_safe:match(s)
end

-- ********************************************************************

function safeutf8(s)
  return make_utf8_safe:match(s)
end

-- ********************************************************************

function mksplit(delim)
  delim      = lpeg.P(delim or ':')
  local char = lpeg.C((lpeg.P(1) - delim)^0)
  
  return lpeg.Ct(char * (delim * char)^0)
end

-- ********************************************************************

function split(s,delim)
  local sp = mksplit(delim)
  return sp:match(s)
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
                 + P"\225\154\128" -- Ogham
                 + P"\225\160\142" -- Mongolian vowel separator
                 + P"\226\128\128" -- en quad
                 + P"\226\128\129" -- em quad
                 + P"\226\128\130" -- en
                 + P"\226\128\131" -- em
                 + P"\226\128\132" -- three-per-em
                 + P"\226\128\133" -- four-per-em
                 + P"\226\128\134" -- six-per-em
                 + P"\226\128\136" -- punctuation
                 + P"\226\128\137" -- thin space
                 + P"\226\128\138" -- hair space
                 + P"\226\128\139" -- zero width
                 + P"\226\128\140" -- zero-width nonjoiner
                 + P"\226\128\141" -- zero-width joiner
                 + P"\226\129\159" -- medium mathematical
                 + P"\227\128\128" -- ideographic
                 
local combining  = P"\204"     * R"\128\191"
                 + P"\205"     * R("\128\142","\144\175") -- ignore CGJ
                 + P"\225\170" * R"\176\190"
                 + P"\225\183" * R"\128\191"
                 + P"\226\131" * R"\144\176"
                 + P"\239\184" * R"\160\175"
                 
local ignore     = P"\205\143"
                 
local shy        = P"\194\173"     -- shy hyphen
                 + P"\225\160\134" -- Mongolian TODO soft hyphen
                 
local hyphen     = P"-"
                 + P"\214\138"     -- Armenian
                 + P"\226\128\144" -- hyphen
                 + P"\226\128\146" -- figure dash
                 + P"\226\128\147" -- en-dash
                 + P"\226\128\148" -- em-dash
                 + P"\226\184\151" -- double oblique
                 + P"\227\131\187" -- Katakana middle dot
                 + P"\239\185\163" -- small hyphen-minus
                 + P"\239\188\141" -- fullwidth hyphen-minus
                 + P"\239\189\165" -- halfwidth Katakana middle dot
                 
local reset      = P"\194\133"     -- NEL-definitely break here
                 
local chars      = whitespace * lpeg.Cp() * lpeg.Cc'space'
                 + shy        * lpeg.Cp() * lpeg.Cc'shy'
                 + hyphen     * lpeg.Cp() * lpeg.Cc'hyphen'
                 + combining  * lpeg.Cp() * lpeg.Cc'combine'
                 + ignore     * lpeg.Cp() * lpeg.Cc'ignore'
                 + reset      * lpeg.Cp() * lpeg.Cc'reset'
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
        
      -- ---------------------------------------------
      -- This handles the case of a clump of dashes.
      -- ---------------------------------------------

      elseif cnt == margin then
        breakhere = i
        resume    = n
        cnt       = margin + 1
      end
    elseif ctype == 'char' then
      cnt  = cnt  + 1
    elseif ctype == 'combining' then -- luacheck: ignore
      -- combining chars don't count
    elseif ctype == 'ignore' then -- luacheck: ignore
      -- ignore characters are ignored
    elseif ctype == 'reset' then
      -- ----------------------------------------------------------
      -- the only control character we accept here, the NEL, which
      -- here means "definitely break here!"
      -- ----------------------------------------------------------
      
      breakhere = i
      resume    = n
      cnt       = margin + 1
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

if _VERSION >= "Lua 5.2" then
  return _ENV
end
