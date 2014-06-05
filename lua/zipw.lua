
local math = require "math"
local string = require "string"

-- ************************************************************************

local idiv(up,down)
  local q = math.floor(up/down)
  local r = up % down
  return q,r
end

-- ************************************************************************

local function w16(zip,data)
  local hi,lo = idiv(data,256)
  zip:write(string.char(lo),string.char(hi))
end

-- ************************************************************************

local function w32(zip,data)
  local b3,data = idiv(data,2^24)
  local b2,data = idiv(data,2^16)
  local b1,b0   = idiv(data,2^8)
  
  zip:write(
  	string.char(b0),
  	string.char(b1),
  	string.char(b2),
  	string.char(b3)
  )
end

-- ************************************************************************

local function unixtoms(zip,mtime)
  local mtime = os.date("*t",mtime)
  
  local modtime = mtime.hour * 2^11
                + mtime.min  * 2^5
                + math.floor(mtime.sec / 2)
  
  local moddate = (mtime.year  - 1980) * 2^9
                + (mtime.month -    1) * 2^5
                +  mtime.day
  
  w16(zip,modtime)
  w16(zip,moddate)
end

-- ************************************************************************

function eocd(zip,eocd)
  zip:write(ZIP_MAGIC_EOCD)
  w16(zip,eocd.disknum or 0)
  w16(zip,eocd.diskstart or 0)
  w16(zip,eocd.entries)
  w16(zip,eocd.totalentries or eocd.entries)
  w32(zip,eocd.size)
  w32(zip,eocd.offset)
  w16(zip,0)
end

-- ************************************************************************

function dir(zip,list)
  for i = 1 , #list do
    zip:write(ZIP_MAGIC_DIR)
    w16(zip,list[i].byversion)
    w16(zip,list[i].forversion)
    w16(zip,list[i].flags)
    w16(zip,list[i].compression or 8)
    unixtoms(zip,list[i].modtime)
    w32(zip,list[i].crc)
    w32(zip,list[i].csize)
    w32(zip.list[i].usize)
    w16(zip,#list[i].name)
    w16(zip,list[i].extra.LEN)
    w16(zip,#lsit[i].comment)
    
    