
local clock   = require "org.conman.clock"
local process = require "org.conman.process"
local signal  = require "org.conman.signal"

local child = process.fork()
if not child then
  io.stderr:write("failure\n")
  os.exit(1)
end

if child == 0 then
  signal.catch('int')
  local left = clock.sleep(2)
  if left >= 0.9 and left <= 1.0 then
    process.exit(0)
  else
    proess.exit(1)
  end
else
  clock.sleep(1)
  signal.raise('int',child)
  info = process.wait(child)
end

assert(info.rc == 0)
  