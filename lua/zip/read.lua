-- ***************************************************************
--
-- Copyright 2014 by Sean Conner.  All Rights Reserved.
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
-- --------------------------------------------------------------------
--
-- To effectively use this module, you should understand the ZIP file
-- format.  The definitive guide to this format is at
--
--		http://www.pkware.com/appnote
--
-- ********************************************************************

local _VERSION     = _VERSION
local type         = type
local setmetatable = setmetatable

local string = require "string"
local os     = require "os"
local table  = require "table"
local zip    = require "org.conman.zip"
local idiv   = zip.idiv

if _VERSION == "Lua 5.1" then
  module("org.conman.zip.read")
else
  _ENV = {}
end

-- ************************************************************************

local function r16(zf)
  return zf:read(1):byte() 
       + zf:read(1):byte() * 2^8
end

-- ************************************************************************

local function r32(zf)
  return zf:read(1):byte()
       + zf:read(1):byte() * 2^8
       + zf:read(1):byte() * 2^16
       + zf:read(1):byte() * 2^24
end

-- ************************************************************************

local function mstounix(zf)
  local modtime      = r16(zf)
  local moddate      = r16(zf)
  
  local hour,modtime = idiv(modtime,2^11)
  local min,sec      = idiv(modtime,2^5)
  local year,moddate = idiv(moddate,2^9)
  local month,day    = idiv(moddate,2^5)
  
  return os.time {
  	year  = year + 1980,
  	month = month,
  	day   = day,
  	hour  = hour,
  	min   = min,
  	sec   = sec * 2
  }
end

-- ************************************************************************

local function version(zf)
  local level = zf:read(1):byte()
  local os    = zf:read(1):byte()

  return {
    os    = zip.os[os],
    level = level / 10
  }
end

-- ************************************************************************

local function flags(zf)
  local function bool(n)
    local q,r = idiv(n,2)
    return r ~= 0,q
  end
  
  local function bits(x,n)
    local q,r = idiv(x,2^n)
    return r,q
  end
  
  local flags = {}
  local f     = r16(zf)
  
  flags.encrypted,f        = bool(f)
  flags.compress,f         = bits(f,2)
  flags.data,f             = bool(f)
  flags.enhanced_deflate,f = bool(f)
  flags.patched,f          = bool(f)
  flags.strong_encrypt,f   = bool(f)
  f                        = idiv(f,2^4) -- skip 4 bits
  flags.utf8,f             = bool(f)
  flags.pkware_compress,f  = bool(f)
  flags.hidden,f           = bool(f)
  flags.pkware,f           = bits(f,2)
  
  return flags
end

-- ************************************************************************

local function iattribute(zf)
  local function bool(n)
    local q,r = idiv(n,2)
    return r ~= 0,q
  end
  
  local iattr    = {}
  local f        = r16(zf)
  iattr.text,f   = bool(f)
  iattr.record,f = bool(f)
  
  return iattr
end

-- ************************************************************************

function eocd(zf)
  local function locate(pos,eof)
    local eof   = eof or zf:seek('end',0)
    local pos   = pos or zf:seek('end',-22)
    local magic = zf:read(4)
    
    if eof - pos > 22 + 65535 then
      return false
    end
    
    if magic == zip.magic.EOCD then
      -- possible false positive
      return true
    end
    
    if pos == 0 then
      return false
    end
    
    return locate(zf:seek('cur',-5),eof)
  end
  
  if locate() then
    local eocd = {}
    local here = zf:seek('cur',0) - 4 -- adjust for Magic bytes
    
    eocd.disknum      = r16(zf)
    eocd.diskstart    = r16(zf)
    eocd.entries      = r16(zf)
    eocd.totalentries = r16(zf)
    eocd.size         = r32(zf)
    eocd.offset       = r32(zf)
    
    local commentlen  = r16(zf)
    local eof         = zf:seek('end',0)
    
    -- ---------------------------------------------------------------------
    -- if the length of the EOCD + the comment doesn't match the end of the
    -- the file, then this might not be an actual ZIP file.  It is best to
    -- exit at this point.
    -- 
    -- The 22 is the size of the EOCD minus the comment.
    -- ---------------------------------------------------------------------
    
    if here + 22 + commentlen ~= eof then
      return nil,"bad comment len"
    end
    
    zf:seek('set',here + 22)
    eocd.comment = zf:read(commentlen)
    
    -- --------------------------------------------------------------------
    -- some further checking, here making sure our CDH (Current Directory
    -- Header) doesn't overlap the EOCD.
    -- --------------------------------------------------------------------
    
    if eocd.offset > here
    or eocd.offset + eocd.size > here then
      return nil,"bad offset"
    end
    
    zf:seek('set',eocd.offset)
    return eocd
  end
end

-- **********************************************************************

local extra = setmetatable(
  {
    [0x0001] = "Zip64 extended information extra field",
    [0x0007] = "AV Info",
    [0x0008] = "Reserved for extended language encoding data (PFS) (see APPENDIX D)",
    [0x0009] = "OS/2",
    [0x000a] = "NTFS",
    [0x000c] = "OpenVMS",
    [0x000d] = "UNIX",
    [0x000e] = "Reserved for file stream and fork descriptors",
    [0x000f] = "Patch Descriptor",
    [0x0014] = "PKCS#7 Store for X.509 Certificates",
    [0x0015] = "X.509 Certificate ID and Signature for individual file",
    [0x0016] = "X.509 Certificate ID for Central Directory",
    [0x0017] = "Strong Encryption Header",
    [0x0018] = "Record Management Controls",
    [0x0019] = "PKCS#7 Encryption Recipient Certificate List",
    [0x0065] = "IBM S/390 (Z390), AS/400 (I400) attributes - uncompressed",
    [0x0066] = "Reserved for IBM S/390 (Z390), AS/400 (I400) attributes - compressed",
    [0x4690] = "POSZIP 4690 (reserved)",

    [0x07c8] = "Macintosh",
    [0x2605] = "ZipIt Macintosh",
    [0x2705] = "ZipIt Macintosh 1.3.5+",
    [0x2805] = "ZipIt Macintosh 1.3.5+",
    [0x334d] = "Info-ZIP Macintosh",
    [0x4341] = "Acorn/SparkFS",
    [0x4453] = "Windows NT security descriptor (binary ACL)",
    [0x4704] = "VM/CMS",
    [0x470f] = "MVS",
    [0x4b46] = "FWKCS MD5 (see below)",
    [0x4c41] = "OS/2 access control list (text ACL)",
    [0x4d49] = "Info-ZIP OpenVMS",
    [0x4f4c] = "Xceed original location extra field",
    [0x5356] = "AOS/VS (ACL)",
    [0x5455] = "extended timestamp",
    [0x554e] = "Xceed unicode extra field",
    [0x5855] = "Info-ZIP UNIX (original, also OS/2, NT, etc)",
    [0x6375] = "Info-ZIP Unicode Comment Extra Field",
    [0x6542] = "BeOS/BeBox",
    [0x7075] = "Info-ZIP Unicode Path Extra Field",
    [0x756e] = "ASi UNIX",
    [0x7855] = "Info-ZIP UNIX (new)",
    [0xa220] = "Microsoft Open Packaging Growth Hint",
    [0xfd4a] = "SMS/QDOS",

    [0x454C] = function(zip,len) -- Language extension
      local fields =
      {
        "version", 
        "license" , 
        "language" , 
        "lvmin" , 
        "lvmax" , 
        "cpu" , 
        "os" , 
        "osver"
      }
      
      local ext  = { }
      
      while len >= 2 do
        local field = zip:read(1):byte()
        local size  = zip:read(1):byte()
        
        len = len - 2
        if size > len then
          zip:read(len)
          return nil
        end
        
        local data = zip:read(size)
        
        if field > 0 and field <= #fields then
          ext[fields[field]] = data
        end
        len = len - size
      end
      
      return ext
    end,  
  },
  {
    __index = function(tab,key)
      return string.format("%04X",key)
    end
  }
)

-- ************************************************************************

function dir(zf,entries)
  local list = {}
  
  for i = 1 , entries do
    local magic = zf:read(4)
    local dir   = {}
    
    if magic == zip.magic.DIR then
      dir.byversion    = version(zf)
      dir.forversio    = version(zf)
      dir.flags        = flags(zf)
      dir.compression  = r16(zf)
      dir.modtime      = mstounix(zf)
      dir.crc          = r32(zf)
      dir.csize        = r32(zf)
      dir.usize        = r32(zf)
      
      local namelen    = r16(zf)
      local extralen   = r16(zf)
      local commentlen = r16(zf)
      
      dir.diskstart    = r16(zf)
      dir.iattr        = iattribute(zf)
      dir.eattr        = r32(zf)
      dir.offset       = r32(zf)
      dir.name         = zf:read(namelen)
      dir.extra        = { }
      
      while extralen > 0 do
        local id    = r16(zf)
        local len   = r16(zf)
        local trans = extra[id]
        
        if type(trans) == 'function' then
          dir.extra[id] = trans(zf,len)
        else
          dir.extra[id] = string.format("%s:%d",trans,#zf:read(len))
        end
                
        extralen = extralen - (len + 4)
      end
      
      dir.comment = zf:read(commentlen)      
      table.insert(list,dir)
    end
  end
  return list
end

-- **********************************************************************

function file(zf)
  local magic = zf:read(4)
  local file  = {}

  if magic == zip.magic.FILE then
    file.byversion   = version(zf)
    file.flags       = flags(zf)
    file.compression = r16(zf)
    file.modtime     = mstounix(zf)
    file.crc         = r32(zf)
    file.csize       = r32(zf)
    file.usize       = r32(zf)
    
    local namelen    = r16(zf)
    local extralen   = r16(zf)
    
    file.name        = zf:read(namelen)
    file.extra       = {}
    
    while extralen > 0 do
      local id    = r16(zf)
      local len   = r16(zf)
      local trans = extra[id]
      
      if type(trans) == 'function' then
        file.extra[id] = trans(zf,len)
      else
        file.extra[id] = zf:read(len)
      end
      
      extralen = extralen - (len + 4)
    end
    
    return file
  end
end

-- ************************************************************************

function data(zf)
  local magic = zf:read(4)
  local data  = {}
  
  if magic == zip.magic.DATA then
    data.crc   = r32(zf)
    data.csize = r32(zf)
    data.usize = r32(zf)
    return data
  end
end

-- ************************************************************************

function archive(zf)
  local magic = zf:read(4)
end

-- ************************************************************************

if _VERSION >= "Lua 5.2" then
  return _ENV
end

