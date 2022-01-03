package = "org.conman.const.exit"
version = "1.1.1-1"

source = 
{
  url = "https://raw.github.com/spc476/lua-conmanorg/exit-1.1.1/lua/const/exit.lua",
  md5 = "5887830a38db812d3b2486215b803bd5"
}

description =
{
  homepage = "https://github.com/spc476/lua-conmanorg/blob/exit-1.1.1/lua/const/exit.lua",
  maintainer = "Sean Conner <sean@conman.org>",
  license = "LGPL3+",
  summary = "Some standard process exit return values",
  detailed = [[
	A Lua table that defines some common exit return codes for (mostly)
	a POSIX system (and Linux).
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
