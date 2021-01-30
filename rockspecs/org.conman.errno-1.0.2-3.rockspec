package = "org.conman.errno"
version = "1.0.2-3"

source = 
{
  url = "https://raw.githubusercontent.com/spc476/lua-conmanorg/errno-1.0.2/src/errno.c"
}

description =
{
  homepage   = "https://github.com/spc476/lua-conmanorg/blob/errno-1.0.2/src/errno.c",
  maintainer = "Sean Conner <sean@conman.org>",
  license    = "LGPL3+",
  summary    = "C and POSIX system error codes.",
  detailed   = [[
	A Lua module that enumerates all C and POSIX system error codes in
	a single table.
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
  modules = 
  {
    ['org.conman.errno'] = "errno.c"
  }
}
