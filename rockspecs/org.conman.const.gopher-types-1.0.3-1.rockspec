package = "org.conman.const.gopher-types"
version = "1.0.3-1"

source =
{
  url = "https://raw.github.com/spc476/lua-conmanorg/gopher-types-1.0.3/lua/const/gopher-types.lua"
}

description =
{
  homepage = "https://github.com/spc476/lua-conmanorg/blob/gopher-types-1.0.3/lua/const/gopher-types.lua",
  maintainer = "Sean Conner <sean@conman.org>",
  license = "LGPL",
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
  type = 'none',
  copy_directories = {},
  install =
  {
    lua =
    {
      ['org.conman.const.gopher-types'] = "gopher-types.lua"
    }
  }
}
