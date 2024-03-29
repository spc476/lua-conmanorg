
local tap     = require "tap14"
local clock   = require "org.conman.clock"
local process = require "org.conman.process"
local signal  = require "org.conman.signal"

local child = process.fork()
if not child then
  io.stderr:write("failure\n")
  os.exit(1)
end

local info

if child == 0 then
  signal.catch('int')
  local left = clock.sleep(2)
  if left >= 0.9 and left <= 1.0 then
    process.exit(0)
  else
    process.exit(1)
  end
else
  clock.sleep(1)
  signal.raise('int',child)
  info = process.wait(child)
end

tap.plan(1)
tap.assert(info.rc == 0)
tap.done()
