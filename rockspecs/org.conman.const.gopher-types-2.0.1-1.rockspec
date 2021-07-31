package = "org.conman.const.gopher-types"
version = "2.0.1-1"

source =
{
  url = "https://raw.github.com/spc476/lua-conmanorg/gopher-types-2.0.1/lua/const/gopher-types.lua",
  md5 = "2602c7b56ca46f5cbbdbcbf04a3821d3",
}

description =
{
  homepage = "https://github.com/spc476/lua-conmanorg/blob/gopher-types-2.0.1/lua/const/gopher-types.lua",
  maintainer = "Sean Conner <sean@conman.org>",
  license = "LGPL3+",
  summary = "Gopher Protocol selector types",
  detailed = [[
  	A Lua table that maps Gopher Protocol selector types to a
  	human-readable string.  It can also map the human-readable string
  	back to a Gopher Protocol selector type.
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
  modules = { ['org.conman.const.gopher-types'] = "gopher-types.lua" },
}
