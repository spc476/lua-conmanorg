package = "org.conman.iconv"
version = "2.0.0-1"

source =
{
  url = "https://raw.github.com/spc476/lua-conmanorg/iconv-2.0.0/src/iconv.c"
}

description =
{
  homepage = "https://github.com/spc476/lua-conmanorg/blob/iconv-2.0.0/src/iconv.c",
  maintainer = "Sean Conner <sean@conman.org>",
  license    = "LGPL",
  summary    = "Lua wrapper for IConv",
  detailed   = [[
	A Lua module that wraps the iconv library call.  It's simple to use:

	iconv = require "org.conman.iconv"

	trans = iconv("utf-8","iso-8859-1") -- from ISO-8869-1 to UTF-8

	x     = This is \255 test" -- in ISO-8869-1

	y     = trans(x)           -- to UTF-8
  ]]
}

dependencies =
{
  "lua >= 5.1, < 5.4"
}

external_dependencies =
{
  ICONV = { header = "iconv.h" }
}

build =
{
  type = "builtin",
  copy_directories = {},
  modules =
  {
    ['org.conman.iconv'] = "iconv.c"
  },
}
