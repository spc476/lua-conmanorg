
package = "org.conman.tls"
version = "1.1.1-1"

source =
{
  url ="https://raw.github.com/spc476/lua-conmanorg/tls-1.1.1/src/tls.c"
}

description =
{
  homepage = "https://github.com/spc476/lua-conmanorg/blob/tls-1.1.1/src/tls.c",
  maintainer = "Sean Conner <sean@conman.org>",
  license = "LGPL3",
  summary = "A Lua module for libtls from LibreSSL",
  detailed = [[

	This module implements the libtls API (from LibreSSSL).  You are
	probably better off reading the man pages for the libtls functions
	as this is straight port of the API.

  ]],
}

dependencies =
{
  "lua >= 5.1, < 5.4"
}

external_dependencies =
{
  TLS = 
  {
    header  = "tls.h",
  },
  
  LIBRESSL =
  {
    header = "openssl/opensslv.h",
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
      incdirs   = { "$(TLS_INCDIR)" , "$(LIBRESSL_INCDIR)" },
      libraries = { 'tls' },
    }
  }  
}
