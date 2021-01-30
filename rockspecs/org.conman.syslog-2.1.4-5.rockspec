package = "org.conman.syslog"
version = "2.1.4-5"

source = 
{
  url = "https://raw.github.com/spc476/lua-conmanorg/syslog-2.1.4/src/syslog.c"
}

supported_platforms = { "unix" }

description =
{
  homepage   = "https://github.com/spc476/lua-conmanorg/blob/syslog-2.1.4/src/syslog.c",
  maintainer = "Sean Conner <sean@conman.org>",
  license    = "LGPL3+",
  summary    = "Lua interface to syslog()",
  detailed   = [[
	A Lua module that interfaces with syslog().
  ]],
}

dependencies =
{
  "lua >= 5.1, <= 5.4",
}

build =
{
  type = "builtin",
  copy_directories = {},
  modules = { ['org.conman.syslog'] = 'syslog.c' }
}
