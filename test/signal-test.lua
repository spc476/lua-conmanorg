
signal = require "org.conman.signal"

-- ----------------------------------------------------------------------
-- The signal API, reguardless of implementation, should support at least
-- this level of activity.
-- ----------------------------------------------------------------------

io.stdout:write("Testing ANSI level support\n")
io.stdout:write("\ttesting for known functions ...")
io.stdout:flush()

assert(type(signal._implementation) == 'string')
assert(type(signal.caught)          == 'function')
assert(type(signal.catch)           == 'function')
assert(type(signal.ignore)          == 'function')
assert(type(signal.default)         == 'function')
assert(type(signal.raise)           == 'function')
assert(type(signal.defined)         == 'function')

io.stdout:write(" GO!\n")

for _,sig in ipairs { 
	'abrt' , 'fpe' , 'ill' , 'int' , 'segv' , 'term' ,
	'abort' , 'illegal', 'interrupt' , 'segfault' , 'terminate' ,
} do
  io.stdout:write("\ttesting ",sig, " ...")
  io.stdout:flush()
  
  -- ---------------------------------------------------------------
  -- simple catch and test.
  -- ---------------------------------------------------------------
  
  assert(signal.defined(sig)) -- ANSI C signals should always be defined
  assert(signal.catch(sig))
  assert(signal.raise(sig))
  assert(signal.caught(sig))
  assert(signal.raise(sig))
  assert(signal.caught())
  
  -- -------------------------------------------------------------
  -- test a signal handler.
  -- -------------------------------------------------------------

  local res = {}
  
  assert(signal.catch(sig,function(s) res[#res + 1] = s end))
  assert(signal.raise(sig))
  assert(signal.raise(sig))
  assert(#res == 2)
  assert(res[1] == sig)
  assert(res[2] == sig)
  
  -- --------------------------------------------------------------------
  -- test catching a previously caught signal with a function, this time
  -- catching it without a signal.
  -- --------------------------------------------------------------------

  assert(signal.catch(sig))
  assert(signal.raise(sig))
  assert(signal.caught(sig))
  assert(signal.raise(sig))
  assert(signal.caught())

  -- ------------------------------------------------------------
  -- test signal ignoring.  We can't test a default action since
  -- the default action tends to be "not ignore".
  -- ------------------------------------------------------------
  
  signal.ignore(sig)
  assert(signal.raise(sig))
  assert(not signal.caught(sig))
  assert(signal.raise(sig))
  assert(not signal.caught())
  signal.default(sig)
  io.stdout:write(" GO!\n")
end

-- --------------------------------------------------------------
-- The only other API we have is POSIX.  
-- --------------------------------------------------------------

if signal._implementation == 'ANSI' then
  os.exit(0)
end

if signal._implementation ~= 'POSIX' then
  io.stdout:write("NEED TO WRITE TESTS FOR ",x,"\n")
  os.exit(1)
end

io.stdout:write("Testing for POSIX level support\n")
io.stdout:write("\ttesting for known functions ...")

assert(type(signal.allow)   == 'function')
assert(type(signal.block)   == 'function')
assert(type(signal.mask)    == 'function')
assert(type(signal.pending) == 'function')
assert(type(signal.suspend) == 'function')
assert(type(signal.set)     == 'function')

io.stdout:write("GO!\n")
io.stdout:write("\ttesting signal sets ... ")

local a = signal.set('term','int') 
	assert(a)
	assert(    a['term'])
	assert(    a['int'])
	assert(not a['abrt'])
	assert(not a['fpe'])
	assert(not a['ill'])
	assert(not a['segv'])
	
local b = signal.set('abrt','fpe') 
	assert(b)
	assert(    b['abrt'])
	assert(    b['fpe'])
	assert(not b['term'])
	assert(not b['int'])
	assert(not b['ill'])
	assert(not b['segv'])
	
local c = a + b
	assert(c)
	assert(    c['term'])
	assert(    c['int'])
	assert(    c['abrt'])
	assert(    c['fpe'])
	assert(not c['ill'])
	assert(not c['segv'])

	assert(    a['term'])
	assert(    a['int'])
	assert(not a['abrt'])
	assert(not a['fpe'])
	assert(not a['ill'])
	assert(not a['segv'])

	assert(    b['abrt'])
	assert(    b['fpe'])
	assert(not b['term'])
	assert(not b['int'])
	assert(not b['ill'])
	assert(not b['segv'])

local d = c - a
	assert(d)
	assert(not d['term'])
	assert(not d['int'])
	assert(    d['abrt'])
	assert(    d['fpe'])
	assert(not d['ill'])
	assert(not d['segv'])

	assert(    c['term'])
	assert(    c['int'])
	assert(    c['abrt'])
	assert(    c['fpe'])
	assert(not c['ill'])
	assert(not c['segv'])

	assert(    a['term'])
	assert(    a['int'])
	assert(not a['abrt'])
	assert(not a['fpe'])
	assert(not a['ill'])
	assert(not a['segv'])
	
local e = -c
	assert(e)
	assert(not e['term'])
	assert(not e['int'])
	assert(not e['abrt'])
	assert(not e['fpe'])
	assert(    e['ill'])
	assert(    e['segv'])

	assert(    c['term'])
	assert(    c['int'])
	assert(    c['abrt'])
	assert(    c['fpe'])
	assert(not c['ill'])
	assert(not c['segv'])

io.stdout:write("GO!\n")

-- ----------------------------------------------------------------------
-- A list of signals to test.  'stop' and 'kill' can't be manipulated by
-- user processes, so they aren't listed.
-- ----------------------------------------------------------------------

list =
{  
  -- ANSI signals
  
  { sig = 'abrt'          , level = "ANSI" } ,
  { sig = 'fpe'           , level = "ANSI" } ,
  { sig = 'ill'           , level = "ANSI" } ,
  { sig = 'int'           , level = "ANSI" } ,
  { sig = 'segv'          , level = "ANSI" } ,
  { sig = 'term'          , level = "ANSI" } ,
  
  { sig = 'abort'         , level = "ANSI" } ,
  { sig = 'illegal'       , level = "ANSI" } ,
  { sig = 'interrupt'     , level = "ANSI" } ,
  { sig = 'segfault'      , level = "ANSI" } ,
  { sig = 'terminate'     , level = "ANSI" } ,
  
  -- POSIX signals
  
  { sig = 'alrm'          , level = "POSIX" } ,
  { sig = 'bus'           , level = "POSIX" } ,
  { sig = 'chld'          , level = "POSIX" } ,
  { sig = 'cont'          , level = "POSIX" } ,
  { sig = 'hup'           , level = "POSIX" } ,
  { sig = 'pipe'          , level = "POSIX" } ,
  { sig = 'poll'          , level = "POSIX" } ,
  { sig = 'quit'          , level = "POSIX" } ,
  { sig = 'sys'           , level = "POSIX" } ,
  { sig = 'trap'          , level = "POSIX" } ,
  { sig = 'tstp'          , level = "POSIX" } ,
  { sig = 'ttin'          , level = "POSIX" } ,
  { sig = 'ttou'          , level = "POSIX" } ,
  { sig = 'urg'           , level = "POSIX" } ,
  { sig = 'usr1'          , level = "POSIX" } ,
  { sig = 'usr2'          , level = "POSIX" } ,
  { sig = 'vtalrm'        , level = "POSIX" } ,
  { sig = 'xcpu'          , level = "POSIX" } ,
  { sig = 'xfsz'          , level = "POSIX" } ,
  
  { sig = 'alarm'         , level = "POSIX" } ,
  { sig = 'breakpoint'    , level = "POSIX" } ,
  { sig = 'child'         , level = "POSIX" } ,
  { sig = 'cputime'       , level = "POSIX" } ,
  { sig = 'filesize'      , level = "POSIX" } ,
  { sig = 'hangup'        , level = "POSIX" } ,
  { sig = 'profile'       , level = "POSIX" } ,
  
  -- Maybe defined, maybe not ... 
  -- your OS will decide
  
  { sig = 'cld'           , level = "non-standard" } ,
  { sig = 'emt'           , level = "non-standard" } ,
  { sig = 'info'          , level = "non-standard" } ,
  { sig = 'io'            , level = "non-standard" } ,
  { sig = 'iot'           , level = "non-standard" } ,
  { sig = 'lost'          , level = "non-standard" } ,
  { sig = 'pwr'           , level = "non-standard" } ,
  { sig = 'stkflt'        , level = "non-standard" } ,
  { sig = 'winch'         , level = "non-standard" } ,
  
  { sig = 'copstackfault' , level = "non-standard" } ,
  { sig = 'emt'           , level = "non-standard" } ,
  { sig = 'filelock'      , level = "non-standard" } ,
  { sig = 'information'   , level = "non-standard" } ,
  { sig = 'power'         , level = "non-standard" } ,
  { sig = 'stkflt'        , level = "non-standard" } ,
  { sig = 'windowchange'  , level = "non-standard" } ,
}

all   = signal.set(true)	-- set of all signals
empty = signal.set()		-- set of no signals
none  = signal.set(false)	-- set of no signals

assert(all)
assert(empty)
assert(none)

local function reset(sig1,sig2)
  signal.default(sig1,sig2)
  signal.mask(empty)
  return {}
end

for _,entry in ipairs(list) do
  local sig      = entry.sig
  local standard = entry.level
  
  io.stdout:write("\ttesting ",sig," (",standard,") ...")

  local exists = signal.defined(sig)
  if not exists then
    if standard == 'non-standard' then
      io.stdout:write(" SKIP\n")
    else
      io.stdout:write(" no support!  STOP!\n")
      os.exit(1)
    end
  else
  
    assert(all[sig])
    assert(not empty[sig])
    assert(not none[sig])
    
    -- -----------------------------------------------------------------
    -- test the flags by testing oneshot and resethandler flags.  This can
    -- only be done with SIGWINCH and SIGURG since their default behavior is
    -- SIG_IGN.  Testing the other flags involves child processes.
    -- -----------------------------------------------------------------
    
    if sig == 'winch' or sig == 'urg' then
      local r = {}
      assert(signal.catch(sig,function(s) r[#r + 1] = s end,'oneshot'))
      assert(signal.raise(sig))
      assert(signal.raise(sig))
      assert(#r == 1)
      assert(r[1] == sig)
      
      local r = {}
      assert(signal.catch(sig,function(s) r[#r + 1] = s end,'resethandler'))
      assert(signal.raise(sig))
      assert(signal.raise(sig))
      assert(#r == 1)
      assert(r[1] == sig)
    end
    
    -- ---------------------------------------------------------------------
    -- Test blocking. Catch two signals, block the second one from being run
    -- while the first one is being run.
    -- ---------------------------------------------------------------------
    
    local sig2
    
    if sig == 'winch' or sig == 'windowchange' then
      sig2 = 'urg'
    else
      sig2 = 'winch'
    end
    
    local block = signal.set(sig2)
    
    assert(block)
    assert(block[sig2])    
    
    local r = {}
    local function handle1(s)
      signal.raise(sig2)
      r[#r + 1] = s
    end
    local function handle2(s)
      r[#r + 1] = s
    end
    
    r = reset(sig,sig2)
    
    assert(signal.catch(sig,handle1,block))
    assert(signal.catch(sig2,handle2))
    assert(signal.raise(sig))    
    assert(#r == 2)
    assert(r[1] == sig)
    r = reset(sig,sig2)
    
    -- signal.block(), signal.pending() and signal.allow()
    
    assert(signal.catch(sig,handle1))
    assert(signal.catch(sig2,handle2))
    signal.block(sig)
    assert(signal.raise(sig))    
    assert(#r == 0)
    local pending = signal.pending()
    assert(pending[sig])
    signal.allow(sig)
    assert(#r == 2)
    assert(r[1] == sig)    
    r = reset(sig,sig2)

    -- signal.mask(set)
        
    assert(signal.catch(sig,handle1))
    assert(signal.catch(sig2,handle2))
    assert(signal.mask(block))
    assert(signal.raise(sig))
    assert(#r == 1)
    assert(r[1] == sig)
    r = reset(sig,sig2)

    assert(signal.catch(sig,handle1))
    assert(signal.catch(sig2,handle2))
    assert(signal.mask('set',block))
    assert(signal.raise(sig))
    assert(#r == 1)
    assert(r[1] == sig)
    r = reset(sig,sig2)

    -- signal.mask('block') and signal.mask('unblock')

    assert(signal.catch(sig,handle1))
    assert(signal.catch(sig2,handle2))
    assert(signal.mask('block',block))
    assert(signal.raise(sig))    
    assert(#r == 1)
    assert(r[1] == sig)
    r = {}
    assert(signal.mask('unblock',block))
    assert(signal.raise(sig))    
    assert(#r == 3)
    assert(r[1] == sig2)
    assert(r[2] == sig)
    assert(r[3] == sig2)
    r = reset(sig,sig2)    
 
    local function handler_info(s,i)
      r[#r + 1] = s
      r[#r + 1] = i
    end

    assert(signal.catch(sig,handler_info,'info'))
    assert(signal.raise(sig))
    assert(#r == 2)
    assert(r[1] == sig)
    assert(r[2].signal == sig)
    
    io.stdout:write(" GO!\n")    
  end
end
