
module("org.conman.string",package.seeall)

trim = require "org.conman.string.trim"
wrap = require "org.conman.string.wrap"

function split(s,delim)
  local results = {}
  local delim   = delim or "%:"
  local pattern = "([^" .. delim .. "]+)" .. delim .. "?"
  
  for segment in string.gmatch(s,pattern) do
    table.insert(results,segment)
  end
  
  return results
end

