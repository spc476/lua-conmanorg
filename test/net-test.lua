
net = require "org.conman.net"

-- ---------------------------------------------------------------------
-- Address tests
-- ---------------------------------------------------------------------

io.stdout:write("Testing address manipulation\n")

local function address_test(case)
  io.stdout:write("\ttesting ",case.desc," ... ")
  local a1 = net.address(case.addr,case.proto,case.port)
  local a2 = net.address(case.addr,case.proto,case.port)
  
  assert(a1  == a2)
  assert(#a1 == case.len)
  assert(#a2 == case.len)
  
  assert(a1.family              == case.family)
  assert(a1.addrbits            == case.addrbits)
  assert(a1.port                == case.iport)
  assert(tostring(a1)           == case.tostring)
  assert(a1:display()           == case.display)
  assert(a1:display(case.iport) == case.display_port)
  
  if a1.family == 'ip6' then
    assert(a1.daddr == '[' .. a1.addr .. ']')
  else
    assert(a1.daddr == a1.addr)
  end
  
  io.stdout:write("GO!\n")
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
	tostring     = "ip:127.0.0.1:53",
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
	tostring     = "ip:127.0.0.1:33333",
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
	tostring     = "ip:127.0.0.1:89",
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
	tostring     = "ip6:[fc00::c0ff:eeba:d015:dead:beef]:53",
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
	tostring     = "ip6:[fc00::c0ff:eeba:d015:dead:beef]:33333",
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
	tostring     = "ip6:[fc00::c0ff:eeba:d015:dead:beef]:89",
	display      = "[fc00::c0ff:eeba:d015:dead:beef]:89",
	display_port = "[fc00::c0ff:eeba:d015:dead:beef]",
}
