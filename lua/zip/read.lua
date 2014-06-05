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

local _VERSION = _VERSION
local type     = type

local string = require "string"
local os     = require "os"
local zip    = require "org.conman.zip"
local idiv   = zip.idiv

if _VERSION == "Lua 5.1" then
  module("org.conman.zip.read")
else
  _ENV = {}
end

-- ************************************************************************

local function r16(zip)
  return zip:read(1):byte() 
       + zip:read(1):byte() * 2^8
end

-- ************************************************************************

local function r32(zip)
  return zip:read(1):byte()
       + zip:read(1):byte() * 2^8
       + zip:read(1):byte() * 2^16
       + zip:read(1):byte() * 2^24
end

-- ************************************************************************

local function mstounix(zip)
  local modtime      = r16(zip)
  local moddate      = r16(zip)
  
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

local function version(zip)
  local level = zip:read(1):byte()
  local os    = zip:read(1):byte()
  
  return {
    os    = zip.os[os],
    level = level / 10
  }
end

-- ************************************************************************

local function flags(zip)
  local function bool(n)
    local q,r = idiv(n,2)
    return q,r ~= 0
  end
  
  local function bits(x,n)
    local q,r = idiv(x,2^n)
    return r,q
  end
  
  local flags = {}
  local f     = r16(zip)
  
  flags.encrypted,f        = bool(f)
  flags.compress,f         = bits(f,2)
  flags.data,f             = bool(f)
  flags.enhanced_deflate,f = bool(f)
  flags.patched,f          = bool(f)
  flags.strong_encrypt,f   = bool(f)
  f                        = idiv(f,2^4) -- skip 4 bits
  flags.utf8               = bool(f)
  flags.pkware_compress    = bool(f)
  flags.hidden             = bool(f)
  flags.pkware             = bits(f,2)
  
  return flags
end

-- ************************************************************************

function eocd(zip)
  local function locate(pos)
    local pos = pos or zip:seek('end',-22)
    local magic = zip:read(4)
    
    if magic == zip.magic.EOCD then
      -- possible false positive
      return true
    end
    
    if pos == 0 then
      return false
    end
    
    return locate(zip:seek('cur',-5))
  end
  
  if locate() then
    local eocd = {}

    eocd.disknum      = r16(zip)
    eocd.diskstart    = r16(zip)
    eocd.entries      = r16(zip)
    eocd.totalentries = r16(zip)
    eocd.size         = r32(zip)
    eocd.offset       = r32(zip)
    
    local commentlen  = r16(zip)
    -- check for consistency here
    
    eocd.comment      = zip:read(commentlen)
    
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
      
      local ext  = { LEN = len }
      
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

function dir(zip,entries)
  local list = {}
  
  for i = 1 , entries do
    local magic = zip:read(4)
    local dir   = {}
    
    if magic == zip.magic.DIR then
      dir.byversion    = version(zip)
      dir.forversio    = version(zip)
      dir.flags        = flags(zip)
      dir.compression  = r16(zip)
      dir.modtime      = mstounix(zip)
      dir.crc          = r32(zip)
      dir.csize        = r32(zip)
      dir.usize        = r32(zip)
      
      local namelen    = r16(zip)
      local extralen   = r16(zip)
      local commentlen = r16(zip)
      
      dir.diskstart    = r16(zip)
      dir.iattr        = r16(zip)
      dir.eattr        = r32(zip)
      dir.offset       = r32(zip)
      
      dir.name         = zip:read(namelen)
      dir.extra        = { LEN = extralen }
      
      while extralen > 0 do
        local id    = r16(zip)
        local len   = r16(zip)
        local trans = extra[id]
        
        if type(trans) == 'function' then
          dir.extra[id] = trans(zip,len)
        else
          dir.extra[id] = zip:read(len)
        end
                
        extralen = extralen - (len + 4)
      end
      
      dir.comment = zip:read(commentlen)      
      table.insert(list,dir)
    end
  end
  return list
end

-- **********************************************************************

function file(zip)
  local magic = zip:read(4)
  local file  = {}
  
  if magic == zip.magic.FILE then
    file.byversion   = version(zip)
    file.flags       = flags(zip)
    file.compression = r16(zip)
    file.modtime     = mstounix(zip)
    file.crc         = r32(zip)
    file.csize       = r32(zip)
    file.usize       = r32(zip)
    
    local namelen    = r16(zip)
    local extralen   = r16(zip)
    
    file.name        = zip:read(namelen)
    file.extra       = {}
    
    while extralen > 0 do
      local id    = r16(zip)
      local len   = r16(zip)
      local trans = extra[id]
      
      if type(trans) == 'function' then
        dir.extra[id] = trans(zip,len)
      else
        dir.extra[id] = zip:read(len)
      end
      
      extralen = extralen - (len + 4)
    end
    
    return file
  end
end

-- ************************************************************************

function data(zip)
  local magic = zip:read(4)
  local data  = {}
  
  if magic == zip.magic.DATA then
    data.crc   = r32(zip)
    data.csize = r32(zip)
    data.usize = r32(zip)
    return data
  end
end

-- ************************************************************************

function archive(zip)
  local magic = zip:read(4)
end

-- ************************************************************************

if _VERSION >= "Lua 5.2" then
  return _ENV
end

