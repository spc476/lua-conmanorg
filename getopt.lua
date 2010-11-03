
module("org.conman.getopt",package.seeall)

local SHORT_OPT = 1
local LONG_OPT  = 2
local ARGUMENT  = 3
local CALLBACK  = 4

-- ************************************************************************

local function long_match(lnopt,arg,i,err)
  local tag
  local value
  
  tag,value = arg[i]:match("^%-%-([^%=]+)%=?(.*)")
  
  if lnopt == nil then 
    err(tag)
    return i + 1
  end

  if lnopt[ARGUMENT] and value == "" then
    i = i + 1
    value = arg[i]
  else
    value = nil
  end
  
  lnopt[CALLBACK](value)

  return i + 1
end

-- ***********************************************************************

local function short_match(shopt,arg,i,err)
  for n = 2 , #arg[i] do
    local opt = arg[i]:sub(n,n)
    
    if shopt[opt] == nil then
      err(opt)
    elseif not shopt[opt][ARGUMENT] then
      shopt[opt][CALLBACK]()
    else
      if arg[i]:sub(n+1,n+1) == '=' then
        value = arg[i]:sub(n+2)
      else
        i = i + 1
        value = arg[i]
      end
      
      shopt[opt][CALLBACK](value)
      return i + 1
    end
  end
  return i + 1
end 

-- ***********************************************************************

function getopt(arg,options,err)
  local shopt     = {}
  local lnopt     = {}
  local err       = err or function(opt)
                             io.stderr:write("unsupported option: ",opt,"\n")
                           end
  
  for i = 1 , #options do
    shopt[options[i][SHORT_OPT]] = options[i]
    if options[i][LONG_OPT] ~= nil then
      if type(options[i][LONG_OPT]) == 'string' then
        lnopt[options[i][LONG_OPT]] = options[i]
      elseif type(options[i][LONG_OPT]) == 'table' then
        for j = 1 , #options[i][LONG_OPT] do
          lnopt[options[i][LONG_OPT][j]] = options[i]
        end
      end
    end
  end
  
  local i = 1
  
  while(i <= #arg) do
    local al
    local as
    
    al = arg[i]:match("^%-%-([^%-]+)")
    as = arg[i]:match("^%-([^%-])")
    
    if al ~= nil then
      i = long_match(lnopt[al],arg,i,err)
    elseif as ~= nil then
      i = short_match(shopt,arg,i,err)
    else
      return i
    end
  end
  
  return i
end

-- ***********************************************************************
