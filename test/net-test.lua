
-- luacheck: ignore 611

local tap = require "tap14"
local net = require "org.conman.net"

local function compare_lists(a,b)
  if (#a ~= #b) then return false end
  for i = 1 , #a do
    if a[i] ~= b[i] then return false end
  end
  return true
end

-- ---------------------------------------------------------------------
-- Address tests
-- ---------------------------------------------------------------------

tap.plan(7)

local function address_test(case)
  tap.plan(10,case.desc)
  local a1 = net.address(case.addr,case.proto,case.port)
  local a2 = net.address(case.addr,case.proto,case.port)
  
  tap.assert(a1  == a2,"equality works")
  tap.assert(#a1 == case.len,"length works")
  tap.assert(#a2 == case.len,"length works (again)")
  
  tap.assert(a1.family              == case.family,"family")
  tap.assert(a1.addrbits            == case.addrbits,"addrbits")
  tap.assert(a1.port                == case.iport,"port")
  tap.assert(tostring(a1)           == case.tostring,"tostring")
  tap.assert(a1:display()           == case.display,"display")
  tap.assert(a1:display(case.iport) == case.display_port,"display port")
  
  if a1.family == 'ip6' then
    tap.assert(a1.daddr == '[' .. a1.addr .. ']',"daddr")
  else
    tap.assert(a1.daddr == a1.addr,"daddr")
  end
  tap.done()
end

address_test {
	desc         = "UDP/IP with named port",
	family       = 'ip',
	addr         = "127.0.0.1",
	addrbits     = "\127\0\0\1",
	proto        = 'udp',
	port         = 'domain',
	iport        = 53,
	len          = 4,
	tostring     = "127.0.0.1:53",
	display      = "127.0.0.1:53",
	display_port = "127.0.0.1"
}

address_test {
	desc         = "TCP/IP with numeric port",
	family       = 'ip',
	addr         = "127.0.0.1",
	addrbits     = "\127\0\0\1",
	proto        = 'tcp',
	port         = 33333,
	iport        = 33333,
	len          = 4,
	tostring     = "127.0.0.1:33333",
	display      = "127.0.0.1:33333",
	display_port = "127.0.0.1",
}

address_test {
	desc         = "OSPF/IP, named protocol",
	family       = 'ip',
	addr         = "127.0.0.1",
	addrbits     = "\127\0\0\1",
	proto        = 'ospf',
	iport        = 89,
	len          = 4,
	tostring     = "127.0.0.1:89",
	display      = "127.0.0.1:89",
	display_port = "127.0.0.1",
}

address_test {
	desc         = "UDP/IP6 with named port",
	family       = 'ip6',
	addr         = "fc00::c0ff:eeba:d015:dead:beef",
	addrbits     = "\252\0\0\0\0\0\192\255\238\186\208\21\222\173\190\239",
	proto        = 'udp',
	port         = 'domain',
	iport        = 53,
	len          = 16,
	tostring     = "[fc00::c0ff:eeba:d015:dead:beef]:53",
	display      = "[fc00::c0ff:eeba:d015:dead:beef]:53",
	display_port = "[fc00::c0ff:eeba:d015:dead:beef]",
}

address_test {
	desc         = "TCP/IP6 with numeric port",
	family       = 'ip6',
	addr         = "fc00::c0ff:eeba:d015:dead:beef",
	addrbits     = "\252\0\0\0\0\0\192\255\238\186\208\21\222\173\190\239",
	proto        = 'udp',
	port         = 33333,
	iport        = 33333,
	len          = 16,
	tostring     = "[fc00::c0ff:eeba:d015:dead:beef]:33333",
	display      = "[fc00::c0ff:eeba:d015:dead:beef]:33333",
	display_port = "[fc00::c0ff:eeba:d015:dead:beef]",
}

address_test {
	desc         = "OSPF/IP, named protocol",
	family       = 'ip6',
	addr         = "fc00::c0ff:eeba:d015:dead:beef",
	addrbits     = "\252\0\0\0\0\0\192\255\238\186\208\21\222\173\190\239",
	proto        = 'ospf',
	iport        = 89,
	len          = 16,
	tostring     = "[fc00::c0ff:eeba:d015:dead:beef]:89",
	display      = "[fc00::c0ff:eeba:d015:dead:beef]:89",
	display_port = "[fc00::c0ff:eeba:d015:dead:beef]",
}

local list1 =
{
  net.address('192.168.1.10','udp',3333),
  net.address('192.168.1.10','udp',2222),
  net.address('192.168.1.10','tcp',4444),
  net.address('192.168.1.10','tcp',1111),
  net.address('127.0.0.1','udp',17),
  net.address('127.0.0.1','udp',3),
  net.address('127.0.0.1','tcp',11),
  net.address('127.0.0.1','tcp',7),
  net.address('/dev/log','udp'),
  net.address('/dev/log','tcp'),
  net.address('fc00::1','udp',33),
  net.address('fc00::1','udp',3),
  net.address('fc00::1','tcp',22),
  net.address('fc00::1','tcp',5),
  net.address('::1','udp',33),
  net.address('::1','udp',3),
  net.address('::1','tcp',22),
  net.address('::1','tcp',5),
}

local list2 =
{
  net.address('/dev/log','tcp'),
  net.address('/dev/log','udp'),
  net.address('127.0.0.1','udp',3),
  net.address('127.0.0.1','tcp',7),
  net.address('127.0.0.1','tcp',11),
  net.address('127.0.0.1','udp',17),
  net.address('192.168.1.10','tcp',1111),
  net.address('192.168.1.10','udp',2222),
  net.address('192.168.1.10','udp',3333),
  net.address('192.168.1.10','tcp',4444),
  net.address('::1','udp',3),
  net.address('::1','tcp',5),
  net.address('::1','tcp',22),
  net.address('::1','udp',33),
  net.address('fc00::1','udp',3),
  net.address('fc00::1','tcp',5),
  net.address('fc00::1','tcp',22),
  net.address('fc00::1','udp',33),
}
  
table.remove(list1) -- because the last call to net.address() fills in the
table.remove(list2) -- last slot with 0.  So remove that 0.
table.sort(list1)

tap.assert(compare_lists(list1,list2),"test sortability")
os.exit(tap.done(),true)
