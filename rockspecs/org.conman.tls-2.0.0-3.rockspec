
package = "org.conman.tls"
version = "2.0.0-3"

source =
{
  url ="https://raw.github.com/spc476/lua-conmanorg/tls-2.0.0/src/tls.c"
}

description =
{
  homepage = "https://github.com/spc476/lua-conmanorg/blob/tls-2.0.0/src/tls.c",
  maintainer = "Sean Conner <sean@conman.org>",
  license = "LGPL3+",
  summary = "A Lua module that implements TLS via the libtls API.",
  detailed = [[

	This module handles TLS via the libtls API.  There are several
	libraries now implementing this API, including LibreSSL (where it
	was originally developed), OpenSSL and BearSSL.

	You are probably better off reading the man pages for the libtls
	functions as this is straight port of the API.

  ]],
}

dependencies =
{
  "lua >= 5.1, <= 5.4"
}

external_dependencies =
{
  TLS = 
  {
    header  = "tls.h",
  },  
}

build =
{
  type = "builtin",
  copy_directory = {},
  modules = 
  {
    ['org.conman.tls'] =
    {
      sources   = "tls.c",
      incdirs   = { "$(TLS_INCDIR)" },
      libraries = { 'tls' },
    }
  }  
}
