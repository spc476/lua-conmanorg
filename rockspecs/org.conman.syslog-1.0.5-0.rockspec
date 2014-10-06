package = "org.conman.syslog"
version = "1.0.5-0"

source = 
{
  url = "https://raw.githubusercontent.com/spc476/lua-conmanorg/syslog-1.0.5/src/syslog.c"
}

description =
{
  homepage   = "https://github.com/spc476/lua-conmanorg/blob/syslog-1.0.5/src/syslog.c",
  maintainer = "Sean Conner <sean@conman.org>",
  license    = "LGPL",
  summary    = "Lua interface to syslog()",
  detailed   = [[
	A Lua module that interfaces with syslog().
  ]],
}

dependencies =
{
  "lua >= 5.1, < 5.3",
}

build =
{
  type = "builtin",
  copy_directories = {},
  modules = { ['org.conman.syslog'] = 'syslog.c' }
}
