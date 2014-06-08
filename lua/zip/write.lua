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
-- ********************************************************************

local _VERSION     = _VERSION
local pairs        = pairs
local setmetatable = setmetatable
local type         = type

local math   = require "math"
local string = require "string"
local os     = require "os"
local table  = require "table"
local zip    = require "org.conman.zip"
local idiv   = zip.idiv

if _VERSION == "Lua 5.1" then
  module("org.conman.zip.write")
else
  _ENV = {}
end

-- ************************************************************************

local function w16(zf,data)
  local hi,lo = idiv(data,256)
  zf:write(string.char(lo),string.char(hi))
end

-- ************************************************************************

local function w32(zf,data)
  local b3,data = idiv(data,2^24)
  local b2,data = idiv(data,2^16)
  local b1,b0   = idiv(data,2^8)
  
  zf:write(
  	string.char(b0),
  	string.char(b1),
  	string.char(b2),
  	string.char(b3)
  )
end

-- ************************************************************************

local function s16(data)
  local hi,lo = idiv(data,256)
  return string.char(lo) .. string.char(hi)
end

-- ************************************************************************

local function s32(data)
  local b3,data = idiv(data,2^24)
  local b2,data = idiv(data,2^16)
  local b1,b0   = idiv(data,2^8)
  
  return string.char(b0)
      .. string.char(b1)
      .. string.char(b2)
      .. string.char(b3)
end

-- ************************************************************************

local function unixtoms(zf,mtime)
  local mtime = os.date("*t",mtime)
  
  local modtime = mtime.hour * 2^11
                + mtime.min  * 2^5
                + math.floor(mtime.sec / 2)
  
  local moddate = (mtime.year  - 1980) * 2^9
                + (mtime.month       ) * 2^5
                +  mtime.day
  
  w16(zf,modtime)
  w16(zf,moddate)
end

-- ************************************************************************

local function version(zf,version)
  zf:write(string.char(version.level * 10))
  zf:write(string.char(zip.os[version.os]))
end

-- ************************************************************************

local function flags(zf,flags)
  local f = 0
  
  if flags.encrypted        then f = f + 2^0 end
  f = f + flags.compress * 2^1
  if flags.data             then f = f + 2^3 end
  if flags.enhanced_deflate then f = f + 2^4 end
  if flags.patched          then f = f + 2^5 end
  if flags.strong_entrypt   then f = f + 2^6 end
  if flags.utf8             then f = f + 2^11 end
  if flags.pkware_compress  then f = f + 2^12 end
  if flags.hidden           then f = f + 2^13 end
  f = f + flags.pkware * 2^14
  
  w16(zf,f)
end

-- ************************************************************************

local function iattribute(zf,iattr)
  local ia = 0
  if iattr.text   then ia = ia + 1 end
  if iattr.record then ia = ia + 2 end
  w16(zf,ia)
end

-- ************************************************************************

function new(what,base)
  if what == 'eocd' then
    return {
  	disknum      = 0,
  	diskstart    = 0,
  	entries      = 0,
  	totalentries = 0,
  	size         = 0,
  	offset       = 0,
  	comment      = ""
    }
    
  elseif what == 'dir' then
    local base = base or {}
    return {
	byversion   = base.byversion   or { os = zip.os[0] , level = 2.0 },
	compression = base.compression or 8,
	modtime     = base.modtime     or os.time(),
    	crc         = base.crc         or 0,
    	csize       = base.csize       or 0,
    	usize       = base.usize       or 0,
    	name        = base.name        or "",
    	extra       = base.extra       or {},
        flags       = base.flags       or {
    					    encrypted        = false,
    					    compress         = 0,
    					    data             = false,
    					    enhanced_deflate = false,
    					    patch            = false,
    					    strong_encrypt   = false,
    					    utf8             = false,
    					    pkware_compress  = false,
    					    hidden           = false,
    					    pkware           = 0,
    					  },

	forversion  = { os = zip.os[0] , level = 2.0 },        	
    	comment     = "",
    	diskstart   = 0,
    	eattr       = 0,
    	offset      = 0,
    	
    	iattr = 
    	{
    	  text   = false,
    	  record = false,
    	},    	
    }
    
  elseif what == 'file' then
    return {
    	byversion = { os = zip.os[0] , level = 2.0 },
    	compression = 8,
    	modtime     = os.time(),
    	crc         = 0,
    	csize       = 0,
    	usize       = 0,
    	name        = "",
    	extra       = {},
    	flags = 
    	{
    	  encrypted        = false,
    	  compress         = 0,
    	  data             = false,
    	  enhanced_deflate = false,
    	  patch            = false,
    	  strong_encrypt   = false,
    	  utf8             = false,
    	  pkware_compress  = false,
    	  hidden           = false,
    	  pkware           = 0,
    	},    	
    }
  end	
end

-- ************************************************************************

function eocd(zf,eocd)
  zf:write(zip.magic.EOCD)
  w16(zf,eocd.disknum)
  w16(zf,eocd.diskstart)
  w16(zf,eocd.entries)
  w16(zf,eocd.totalentries)
  w32(zf,eocd.size)
  w32(zf,eocd.offset)  
  w16(zf,#eocd.comment)
  zf:write(eocd.comment)
end

-- ************************************************************************

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

    [0x454C] = function(extra) -- Language extension
      local fields =
      {
        version  = "\001",
        license  = "\002", 
        language = "\003", 
        lvmin    = "\004", 
        lvmax    = "\005", 
        cpu      = "\006", 
        os       = "\007", 
        osver    = "\008",
      }
      
      data = {}
      for name,tag in pairs(fields) do
        if extra[name] then
          if #extra[name] > 255 then
            return
          end
          
          table.insert(data,tag)
          table.insert(data,string.char(#extra[name]))
          table.insert(data,extra[name])
        end
      end
      
      data = table.concat(data)
      if #data > 0 then
        return data
      end
    end,
  },
  {
    __index = function(tab,key)
      return nil
    end
  }
)

-- ************************************************************************

function dir(zf,list)
  local pos = zf:seek()
  
  for i = 1 , #list do
    local extradata = {}
    
    for id,data in pairs(list[i].extra) do
      if type(extra[id]) == 'function' then
        local data = extra[id](data)
        if data then
          table.insert(extradata,s16(id) .. s16(#data) .. data)
        end
      end
    end
    
    extradata = table.concat(extradata)
    
    zf:write(zip.magic.DIR)
    version(zf,list[i].byversion)
    version(zf,list[i].forversion)
    flags(zf,list[i].flags)
    w16(zf,list[i].compression or 8)
    unixtoms(zf,list[i].modtime)
    w32(zf,list[i].crc)
    w32(zf,list[i].csize)
    w32(zf,list[i].usize)
    w16(zf,#list[i].name)
    w16(zf,#extradata)
    w16(zf,#list[i].comment)
    w16(zf,list[i].diskstart)
    iattribute(zf,list[i].iattr)
    w32(zf,list[i].eattr)
    w32(zf,list[i].offset)
    zf:write(list[i].name)
    zf:write(extradata)
    zf:write(list[i].comment)
  end
  
  local eod = zf:seek()
  return pos,eod-pos
end

-- ************************************************************************

function file(zf,file)
  local pos       = zf:seek()
  local extradata = {}
  
  for id,data in pairs(file.extra) do
    if type(extra[id]) == 'function' then
      local data = extra[id](data)
      if data then
        table.insert(extradata,s16(id) .. s16(#data) .. data)
      end
    end
  end
  
  extradata = table.concat(extradata)
  
  zf:write(zip.magic.FILE)
  version(zf,file.byversion)
  flags(zf,file.flags)
  w16(zf,file.compression)
  unixtoms(zf,file.modtime)
  w32(zf,file.crc)
  w32(zf,file.csize)
  w32(zf,file.usize)
  w16(zf,#file.name)
  w16(zf,#extradata)
  zf:write(file.name)
  zf:write(extradata)
  return pos
end

-- ************************************************************************

function data(zf,data)
  zf:write(zip.magic.DATA)
  w32(zf,data.crc)
  w32(zf,data.csize)
  w32(zf,data.usize)
end

-- ************************************************************************

if _VERSION >= "Lua 5.2" then
  return _ENV
end

