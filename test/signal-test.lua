-- luacheck: ignore 611

local tap    = require "tap14"
local signal = require "org.conman.signal"

local ANSI =
{
  'abrt'  , 'ill'     , 'int'       , 'segv'     , 'term' , 'fpe' ,
  'abort' , 'illegal' , 'interrupt' , 'segfault' , 'terminate'
}

local POSIX =
{
  'alrm' , 'bus'  , 'chld' , 'cont' , 'hup'  , 'pipe' , 'poll' , 'quit' ,
  'sys'  , 'trap' , 'tstp' , 'ttin' , 'ttou' , 'urg'  , 'usr1' , 'usr2' ,
  'vtalrm' , 'xcpu' , 'xfsz',
  
  'alarm'    , 'breakpoint' , 'child' , 'cputime' ,
  'filesize' , 'hangup'     , 'profile'
}

tap.plan(0)
tap.assert(signal._implementation,"implementatin information")
tap.assert(type(signal._implementation) == 'string',"implementation information is string")
tap.assert(type(signal.catch)   == 'function',"catch() exists")
tap.assert(type(signal.caught)  == 'function',"caught() exists")
tap.assert(type(signal.default) == 'function',"default() exists")
tap.assert(type(signal.defined) == 'function',"defined() exists")
tap.assert(type(signal.ignore)  == 'function',"ignore() exists")
tap.assert(type(signal.raise)   == 'function',"raise() exists")

tap.plan(#ANSI,"ANSI level support") do
  for _,sig in ipairs(ANSI) do
    tap.plan(3,sig) do
      tap.plan(6,"simple catch and raise") do
        tap.assert(signal.defined(sig),"%s defined",sig)
        tap.assert(signal.catch(sig),"catch %s",sig)
        tap.assert(signal.raise(sig),"raise %s",sig)
        tap.assert(signal.caught(sig),"caught %s",sig)
        tap.assert(signal.raise(sig),"raise %s",sig)
        tap.assert(signal.caught(),"caught")
        tap.done()
      end
      tap.plan(6,'test signal handler') do
        local res = {}
        tap.assert(signal.catch(sig,function(s) res[#res + 1] = s end),'%s catch function',sig)
        tap.assert(signal.raise(sig),'raise %s',sig)
        tap.assert(signal.raise(sig),'raise %s again',sig)
        tap.assert(#res == 2,"caught two signals")
        tap.assert(res[1] == sig,"%s caught once",sig)
        tap.assert(res[2] == sig,"%s caught twice",sig)
        tap.done()
      end
      tap.plan(5,'test signal ignoring') do
        tap.assert(not signal.ignore(sig),"ignore %s",sig)
        tap.assert(signal.raise(sig),"raise %s",sig)
        tap.assert(not signal.caught(sig),"%s not caught",sig)
        tap.assert(signal.raise(sig),"raise %s again",sig)
        tap.assert(not signal.caught(),"no signal caught")
        tap.done()
      end
    end
    tap.done()
  end
  tap.done()
end

if signal._implementation == 'ANSI' then
  os.exit(tap.done())
end

tap.assertB(signal._implementation == 'POSIX',"NEED TO WRITE TESTS FOR %s",signal._implementation)

tap.plan(7,"POSIX level support") do
  
  tap.plan(#POSIX,"POSIX signal definitions") do
    for _,sig in ipairs(POSIX) do
      tap.assert(signal.defined(sig),"%s defined",sig)
    end
    tap.done()
  end
  
  tap.plan(6,"function support") do
    tap.assert(type(signal.allow)   == 'function',"allow() exists")
    tap.assert(type(signal.block)   == 'function',"block() exists")
    tap.assert(type(signal.mask)    == 'function',"mask() exists")
    tap.assert(type(signal.pending) == 'function',"pending() exists")
    tap.assert(type(signal.suspend) == 'function',"suspend() exists")
    tap.assert(type(signal.set)     == 'function',"set() exists")
    tap.done()
  end
  
  tap.plan(70,"manipulation of signal sets") do
    local a = signal.set('term','int')
    tap.assert(a,"set a exists")
    tap.assert(a['term'],"term in set a")
    tap.assert(a['int'],"int in set a")
    tap.assert(not a['abrt'],"abrt not in set a")
    tap.assert(not a['fpe'], "fpe not in set a")
    tap.assert(not a['ill'], "ill not in set a")
    tap.assert(not a['segv'],"segv not in set a")
    
    local b = signal.set('abrt','fpe')
    tap.assert(b,"set b exists")
    tap.assert(b['abrt'],"abrt in set b")
    tap.assert(b['fpe'],"fpe in set b")
    tap.assert(not b['term'],"term not in set b")
    tap.assert(not b['int'] ,"int not in set b")
    tap.assert(not b['ill'] ,"ill not in set b")
    tap.assert(not b['segv'],"setv not in set b")
    
    local c = a + b
    tap.assert(c,"c exists, = a + b")
    tap.assert(c['term'],    "term in c=a+b")
    tap.assert(c['int'],     "int in c=a+b")
    tap.assert(c['abrt'],    "abrt in c=a+b")
    tap.assert(c['fpe'],     "fpe in c=a+b")
    tap.assert(not c['ill'], "ill not in c=a+b")
    tap.assert(not c['segv'],"segv not in c=a+b")
    
    tap.assert(a,"set a exists")
    tap.assert(a['term'],"term in set a")
    tap.assert(a['int'],"int in set a")
    tap.assert(not a['abrt'],"abrt not in set a")
    tap.assert(not a['fpe'], "fpe not in set a")
    tap.assert(not a['ill'], "ill not in set a")
    tap.assert(not a['segv'],"segv not in set a")
    
    tap.assert(b,"set b exists")
    tap.assert(b['abrt'],"abrt in set b")
    tap.assert(b['fpe'],"fpe in set b")
    tap.assert(not b['term'],"term not in set b")
    tap.assert(not b['int'] ,"int not in set b")
    tap.assert(not b['ill'] ,"ill not in set b")
    tap.assert(not b['segv'],"setv not in set b")
    
    local d = c - a
    tap.assert(d,"set d exists")
    tap.assert(d['abrt'],"abrt in set d=c-a")
    tap.assert(d['fpe'],"fpe in set d=c-a")
    tap.assert(not d['term'],"term not in set d=c-a")
    tap.assert(not d['int'] ,"int not in set d=c-a")
    tap.assert(not d['ill'] ,"ill not in set d=c-a")
    tap.assert(not d['segv'],"setv not in set d=c-a")
    
    tap.assert(c,"c exists")
    tap.assert(c['term'],    "term in c")
    tap.assert(c['int'],     "int in c")
    tap.assert(c['abrt'],    "abrt in c")
    tap.assert(c['fpe'],     "fpe in c")
    tap.assert(not c['ill'], "ill not in c")
    tap.assert(not c['segv'],"segv not in c")
    
    tap.assert(a,"set a exists")
    tap.assert(a['term'],"term in set a")
    tap.assert(a['int'],"int in set a")
    tap.assert(not a['abrt'],"abrt not in set a")
    tap.assert(not a['fpe'], "fpe not in set a")
    tap.assert(not a['ill'], "ill not in set a")
    tap.assert(not a['segv'],"segv not in set a")
    
    local e = -c
    tap.assert(e,"c exists")
    tap.assert(not e['term'],"term in e=-c")
    tap.assert(not e['int'], "int in e=-c")
    tap.assert(not e['abrt'],"abrt in e=-c")
    tap.assert(not e['fpe'], "fpe in e=-c")
    tap.assert(e['ill'],     "ill not in e=-c")
    tap.assert(e['segv'],    "segv not in e=-c")
    
    tap.assert(c,"c exists")
    tap.assert(c['term'],    "term in c")
    tap.assert(c['int'],     "int in c")
    tap.assert(c['abrt'],    "abrt in c")
    tap.assert(c['fpe'],     "fpe in c")
    tap.assert(not c['ill'], "ill not in c")
    tap.assert(not c['segv'],"segv not in c")
    
    tap.done()
  end
  
  tap.plan(3,"oneshot") do
    local r = {}
    tap.assert(signal.catch('winch',function(s) r[#r + 1] = s end,'oneshot'),"arm oneshot")
    signal.raise('winch')
    signal.raise('winch')
    tap.assert(#r == 1,"oneshot test")
    tap.assert(r[1] == 'winch',"oneshot from winch")
    tap.done()
  end
  
  tap.plan(3,'resethandler')  do
    local r = {}
    tap.assert(signal.catch('winch',function(s) r[#r + 1] = s end,'resethandler'),"arm resethandler")
    signal.raise('winch')
    signal.raise('winch')
    tap.assert(#r == 1,"resethandler test")
    tap.assert(r[1] == 'winch',"resethandler from which")
    tap.done()
  end
  
  tap.plan(#ANSI * 3 + #POSIX * 3 + 3,'signal set tests') do
    local all   = signal.set(true)
    local empty = signal.set()
    local none  = signal.set(false)
    tap.assert(all,"all set exists")
    tap.assert(empty,"empty set exists")
    tap.assert(none,"none set exists")
    for _,sig in ipairs(ANSI) do
      tap.assert(all[sig],"%s in set all",sig)
      tap.assert(not empty[sig],"%s not in set empty",sig)
      tap.assert(not none[sig],"%s not in set empty",sig)
    end
    for _,sig in ipairs(POSIX) do
      tap.assert(all[sig],"%s in set all",sig)
      tap.assert(not empty[sig],"%s not in set empty",sig)
      tap.assert(not none[sig],"%s not in set none",sig)
    end
    tap.done()
  end
  
  tap.plan(29,'signal pending, allow, mask') do
    local empty = signal.set()
    local r
    
    local function reset(s1,s2)
      signal.default(s1,s2)
      signal.mask(empty)
      return {}
    end
    
    local function handle1(s)
      signal.raise('urg')
      r[#r + 1] = s
    end
    local function handle2(s)
      r[#r + 1] = s
    end
    
    local block = signal.set('urg')
    
    -- signal.catch(sig,handler,block)
    
    r = reset('winch','urg')
    tap.assert(signal.catch('winch',handle1,block),"catch winch block urg")
    tap.assert(signal.catch('urg',handle2),"catch urg")
    tap.assert(signal.raise('winch'),"raise winch")
    tap.assert(#r == 2,"caught 2 signals")
    tap.assert(r[1] == 'winch',"caught winch")
    tap.assert(r[2] == 'urg',"caught urg")
    
    -- signal.block(), signal.pendind(), signal.allow()
    
    r = reset('winch','urg')
    signal.catch('winch',handle1)
    signal.catch('urg',handle2)
    signal.block('winch')
    signal.raise('winch')
    tap.assert(#r == 0,"blocked winch")
    local pending = signal.pending()
    tap.assert(pending['winch'],"winch is pending")
    signal.allow('winch')
    tap.assert(#r == 2,"signal delivered")
    tap.assert(r[1] == 'winch',"winch delivered")
    tap.assert(r[2] == 'urg',"urg also delivered")
    
    -- signal.mask(set)
    
    r = reset('winch','urg')
    signal.catch('winch',handle1)
    signal.catch('urg',handle2)
    tap.assert(signal.mask(block),"signal.mask(block)")
    signal.raise('winch')
    tap.assert(#r == 1,"one signal")
    tap.assert(r[1] == 'winch',"winch raised, no urg")
    
    -- signal.mask('set',set)
    
    r = reset('winch','urg')
    signal.catch('winch',handle1)
    signal.catch('urg',handle2)
    tap.assert(signal.mask('set',block),"signal.mask('set',block)")
    signal.raise('winch')
    tap.assert(#r == 1,"one signal")
    tap.assert(r[1] == 'winch',"winch raised, no urg")
    
    -- signal.mask('block',set), signal.mask('unblock',set)
    
    r = reset('winch','urg')
    signal.catch('winch',handle1)
    signal.catch('urg',handle2)
    tap.assert(signal.mask('block',block),"signal.mask('block',block)")
    signal.raise('winch')
    tap.assert(#r == 1,"one signal")
    tap.assert(r[1] == 'winch',"winch raised, no urg")
    r = {}
    tap.assert(signal.mask('unblock',block),"signal.mask('unblock',block")
    signal.raise('winch')
    tap.assert(#r == 3,"three signals should be delivered")
    tap.assert(r[1] == 'urg',"first is urg")
    tap.assert(r[2] == 'winch',"second is winch")
    tap.assert(r[3] == 'urg',"third is urg")
    
    -- get signal info
    
    local function handler_info(s,i)
      r[#r + 1] = s
      r[#r + 1] = i
    end
    
    r = reset('winch','urg')
    tap.assert(signal.catch('winch',handler_info,'info'),"catch signal with info")
    signal.raise('winch')
    tap.assert(#r == 2,"got back to items from handler")
    tap.assert(r[1] == 'winch',"signal was winch")
    tap.assert(r[2].signal == 'winch',"info was returned")
    tap.done()
  end
  tap.done()
end

return os.exit(tap.done())
