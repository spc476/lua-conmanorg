package = "org.conman.const.exit"
version = "1.0.0-2"

source = 
{
  url = "https://raw.github.com/spc476/lua-conmanorg/exit-1.0.0/lua/const/exit.lua",
  md5 = "25e893f8cdd09466e7716c075d95077f"
}

description =
{
  homepage = "https://github.com/spc476/lua-conmanorg/blob/exit-1.0.0/lua/const/exit.lua",
  maintainer = "Sean Conner <sean@conman.org>",
  license = "LGPL3+",
  summary = "Some standard process exit return values",
  detailed = [[
	A Lua table that defines some common exit return codes for (mostly)
	a POSIX system.
	]]
}

dependencies =
{
  "lua",
}

build =
{
  type = 'builtin',
  copy_directories = {},
  modules = { ['org.conman.const.exit'] = "exit.lua" },
}
