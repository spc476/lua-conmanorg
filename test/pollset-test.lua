
local fsys    = require "org.conman.fsys"
local pollset = require "org.conman.pollset"

-- -------------------------------------------------------------
-- create some data to schlep around, and a pipe to select upon
-- -------------------------------------------------------------

data = string.rep(" ",256)
pipe = fsys.pipe()
pipe.read:setvbuf('no')
pipe.write:setvbuf('no')

-- ---------------
-- Run the tests
-- ----------------

io.stdout:write("Testing minimal (select) level of support\n")
io.stdout:write("\ttesting type of implementation ... ")
do
	set     = pollset()
	assert(set._implementation)
	io.stdout:write("GO!\n")
end

io.stdout:write("\ttesting timeout (will take 5 seconds) ... ")
io.stdout:flush()
do
	local err = set:insert(fsys.fileno(pipe.read),"r")
	assert(err == 0)

	local zen        = os.time()
	local events,err = set:events(5)
	local now        = os.time()

	assert(err == 0)
	assert((now - zen) >= 5)
	assert(#events == 0)
	io.stdout:write("GO!\n")
end

io.stdout:write("\ttesting read readiness ... ")
do
	pipe.write:write(data)

	local zen        = os.time()
	local events,err = set:events(5)
	local now        = os.time()
	
	assert(err == 0)
	assert((now-zen) <= 1)
	assert(#events == 1)
	assert(events[1].read)

	local blob = pipe.read:read(256)
	assert(#blob == 256)
	io.stdout:write("GO!\n")
end

io.stdout:write("\ttesting set removal ... ")
do
	local err = set:remove(fsys.fileno(pipe.read))
	assert(err == 0)
	io.stdout:write("GO!\n")
end

io.stdout:write("\ttesting write readiness ... ")
do

	local err = set:insert(fsys.fileno(pipe.read),"r")
	assert(err == 0)
	
	local err = set:insert(fsys.fileno(pipe.write),"w")
	assert(err == 0)

	local zen = os.time()
	local events,err = set:events(5)
	local now = os.time()
	
	assert(err == 0)
	assert((now-zen) <= 1)
	assert(#events == 1)
	assert(events[1].write)
	
	pipe.write:write(data)
	
	local zen = os.time()
	local events,err = set:events(5)
	local now = os.time()
	
	assert(err == 0)
	assert((now-zen) <= 1)
	assert(events[1].read)
	
	local blob = pipe.read:read(256)
	assert(#blob == 256)
	io.stdout:write("GO!\n")
end
