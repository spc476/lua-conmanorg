
-- RFC-1952
-- luacheck: globals idiv read
-- luacheck: ignore 611

local _VERSION     = _VERSION
local pairs        = pairs
local setmetatable = setmetatable

local math  = require "math"
local table = require "table"

if _VERSION == "Lua 5.1" then
  module("org.conman.gzip")
else
  _ENV = {}
end

-- ************************************************************************

local function reverse_index(tab)
  local keys = {}
  for name in pairs(tab) do
    table.insert(keys,name)
  end
  
  for i = 1 , #keys do
    local k = keys[i]
    local v = tab[k]
    tab[v] = k
  end
  
  return setmetatable(tab,{
    __index = function(_,key)
      if type(key) == 'number' then
        return '-'
      elseif type(key) == 'string' then
        return 0
      end
    end
  })
end

os = reverse_index { -- luacheck: ignore
  ["MS-DOS"]          =  0,
  ["Amiga"]           =  1,
  ["OpenVMS"]         =  2,
  ["UNIX"]            =  3,
  ["VM/CMS"]          =  4,
  ["Atari ST"]        =  5,
  ["OS/2"]            =  6,
  ["Macintosh"]       =  7,
  ["Z-System"]        =  8,
  ["CP/M"]            =  9,
  ["Windows"]         = 10,
  ["MVS"]             = 11,
  ["VSE"]             = 12,
  ["Acorn Risc"]      = 13,
  ["VFAT"]            = 14,
  ["alternative MVS"] = 15,
  ["BeOS"]            = 16,
  ["Tandem"]          = 17,
  ["OS/400"]          = 18,
  ["Darwin"]          = 19,
}

-- ************************************************************************

function idiv(up,down)
  local q = math.floor(up/down)
  local r = up % down
  return q,r
end

-- ************************************************************************

local function r16(gf)
  return gf:read(1):byte()
       + gf:read(1):byte() * 2*8
end

-- ************************************************************************

local function r32(gf)
  return gf:read(1):byte()
       + gf:read(1):byte() * 2^8
       + gf:read(1):byte() * 2^16
       + gf:read(1):byte() * 2^24
end

-- ************************************************************************

local function bool(n)
  local q,r = idiv(n,2)
  return r ~= 0 , q
end

-- ************************************************************************

local function read_asciiz(gf,acc)
  acc     = acc or ""
  local c = gf:read(1)
  if c == "\0" then
    return acc
  else
    return read_asciiz(gf,acc .. c)
  end
end

-- ************************************************************************

function read(gf)
  local id = gf:read(2)
  if id ~= "\31\139" then
    return nil
  end
  
  local comp = gf:read(1)
  if comp ~= "\8" then
    return nil
  end
  
  local meta       = {}
  local f          = gf:read(1):byte()
  meta.text,f      = bool(f)
  local fhcrc,f    = bool(f) -- luacheck: ignore
  local fextra,f   = bool(f) -- luacheck: ignore
  local fname,f    = bool(f) -- luacheck: ignore
  local fcomment,_ = bool(f)
  meta.mtime       = r32(gf)
  meta.xfl         = gf:read(1):byte()
  meta.os          = os[gf:read(1):byte()]
  
  if fextra then
    meta.extra = {}
    
    local len = r16(gf)
    while len > 0 do
      local id2  = gf:read(2)
      local size = r16(gf)
      local data = gf:read(size)
      meta.extra[id2] = data
      len = len - 4
      len = len - size
    end
  end
  
  if fname then
    meta.name = read_asciiz(gf)
  end
  
  if fcomment then
    meta.comment = read_asciiz(gf)
  end
  
  if fhcrc then
    meta.crc16 = r16(gf)
  end
  
  local here = gf:seek()
  
  gf:seek('end',-8)
  meta.crc32 = r32(gf)
  meta.size  = r32(gf)
  gf:seek('set',here)
  
  return meta
end

-- ************************************************************************

if _VERSION >= "Lua 5.2" then
  return _ENV
end
