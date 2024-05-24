-- luacheck: ignore 611 411

local tap     = require "tap14"
local fsys    = require "org.conman.fsys"
local pollset = require "org.conman.pollset"

-- -------------------------------------------------------------
-- create some data to schlep around, and a pipe to select upon
-- -------------------------------------------------------------

local data = string.rep(" ",256)
local pipe = fsys.pipe()
pipe.read:setvbuf('no')
pipe.write:setvbuf('no')

-- ---------------
-- Run the tests
-- ----------------

tap.plan(6)
local set do
  set = pollset()
  tap.assertB(set,"set creation")
  tap.assertB(set._implementation,"We're testing the %s implementation",set._implemenation or "-")
end

tap.plan(7,"timeout test (5 seconds)") do
	local err = set:insert(pipe.read,"r")
	tap.assertB(err,"inserting the read event")
	
	local zen      = os.time()
	local okay,err = set:wait(5)
	local now      = os.time()
        
        tap.assert(okay,"waiting")
        tap.assert(err,"we got an error (even if success)")
        tap.assert((now - zen) >= 5,"we took five seconds")
        
	local events,state,var = set:events()
	tap.assert(events,"events exist")
	tap.assert(state,"state info")
	tap.assert(not var,"variable info")
	tap.done()
end

tap.plan(8,"testing read readiness") do
	pipe.write:write(data)

	local zen        = os.time()
	local okay,err   = set:wait(5)
	local now        = os.time()
	
	tap.assert(okay,"waiting for up to 5 seconds")
	tap.assert(not err,"no error")
	tap.assert((now-zen) <= 1,"we didn't actually wait")
	local events,state,var = set:events()
	tap.assert(events,"events exist")
	local e = events(state,var)
	tap.assert(e,"event item")
	tap.assert(e.read,"event is read event")
	tap.assert(events(state,var) == nil,"no further events")

	local blob = pipe.read:read(256)
	tap.assert(#blob == 256,"we have read data")
	tap.done()
end

do
	local err = set:remove(pipe.read)
	tap.assert(err == 0,"file removed from set")
end

tap.plan(16,"testing write readiness") do

	local err = set:insert(pipe.read,"r")
	tap.assertB(err == 0,"insert read file into set")
	
	local err = set:insert(pipe.write,"w")
	tap.assertB(err == 0,"insert write file into set")

	local zen = os.time()
	local okay,err = set:wait(5)
	local now = os.time()
	
	tap.assert(okay,"wait for events")
	tap.assert(not err,"no error while waiting")
	tap.assert((now-zen) <= 1,"no waiting for data")
	local events,state,var = set:events()
	tap.assert(events,"we have events")
	local e = events(state,var)
	tap.assert(e,"first event")
	tap.assert(e.write,"we can write")
	tap.assert(events(state,var) == nil,"no further events")
	
	pipe.write:write(data)
	set:remove(pipe.write)
	
	local zen = os.time()
	local okay,err = set:wait(5)
	local now = os.time()
	
	tap.assert(okay,"wait for more events")
	tap.assert(not err,"no error")
	tap.assert((now-zen) <= 1,"no waiting for data")
	local events,state,var = set:events()
	tap.assert(events,"we have events")
	local e = events(state,var)
	tap.assert(e,"first event")
	tap.assert(e.read,"it's a read event")
	
	local blob = pipe.read:read(256)
	tap.assert(#blob == 256,"we have data")
	tap.done()
end

os.exit(tap.done(),true)
