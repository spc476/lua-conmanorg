#########################################################################
#
# Copyright 2011 by Sean Conner.  All Rights Reserved.
#
# This library is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 3 of the License, or (at your
# option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
# License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this library; if not, see <http://www.gnu.org/licenses/>.
#
# Comments, questions and criticisms can be sent to: sean@conman.org
#
########################################################################

UNAME := $(shell uname)

ifeq ($(UNAME),Linux)
  CC      = gcc -std=c99
  CFLAGS  = -g -Wall -Wextra -pedantic -Wwrite-strings
  SHARED  = -fPIC -shared -L/usr/local/lib
  LDFLAGS = -g
  lib/clock.so : LDLIBS = -lrt
endif

ifeq ($(UNAME),SunOS)
  CC      = cc -xc99
  CFLAGS  = -g -mt -m64 -I /usr/sfw/include
  SHARED  = -G -xcode=pic32
  LDFLAGS = -g
  lib/net.so   : LDLIBS = -lsocket -lnsl
  lib/clock.so : LDLIBS = -lrt
endif

ifeq ($(UNAME),Darwin)
  CC      = gcc -std=c99
  CFLAGS  = -g -Wall -Wextra -pedantic
  SHARED  = -fPIC -bundle -undefined dynamic_lookup -all_load
  LDFLAGS = -g
  lib/iconv.so : LDLIBS = -liconv
endif

# ===================================================

INSTALL         = /usr/bin/install
INSTALL_PROGRAM = $(INSTALL)
INSTALL_DATA    = $(INSTALL) -m 644

prefix      = /usr/local
libdir      = $(prefix)/lib
datarootdir = $(prefix)/share
dataroot    = $(datarootdir)

LUA         ?= lua
LUA_VERSION := $(shell $(LUA) -e "print(_VERSION:match '^Lua (.*)')")
LUADIR      ?= $(dataroot)/lua/$(LUA_VERSION)
LIBDIR      ?= $(libdir)/lua/$(LUA_VERSION)

ifneq ($(LUA_INCDIR),)
  override CFLAGS += -I$(LUA_INCDIR)
endif

# ===================================================

.PHONY:	all clean install uninstall obsolete install-obsolete

lib/%.so : src/%.c
	$(CC) $(CFLAGS) $(SHARED) -o $@ $< $(LDLIBS)

all : lib		\
	lib/base64.so	\
	lib/clock.so	\
	lib/crc.so	\
	lib/env.so	\
	lib/errno.so	\
	lib/fsys.so	\
	lib/hash.so	\
	lib/iconv.so	\
	lib/lfsr.so     \
	lib/idn.so      \
	lib/magic.so	\
	lib/math.so	\
	lib/net.so	\
	lib/pollset.so	\
	lib/process.so	\
	lib/signal.so	\
	lib/strcore.so	\
	lib/sys.so	\
	lib/syslog.so	\
	lib/tls.so	\
	build/bin2c

obsolete: lib lib/tcc.so

build/bin2c : LDLIBS = -lz
build/bin2c : build/bin2c.c

lib :
	mkdir lib

lib/hash.so  : LDLIBS = -lcrypto
lib/magic.so : LDLIBS = -lmagic
lib/tcc.so   : LDLIBS = -ltcc
lib/idn.so   : LDLIBS = -lidn
lib/tls.so   : LDLIBS = -lcrypto -ltls -lssl

# ===================================================

install : all
	$(INSTALL) -d $(DESTDIR)$(LUADIR)/org/conman	
	$(INSTALL) -d $(DESTDIR)$(LUADIR)/org/conman/dns
	$(INSTALL) -d $(DESTDIR)$(LUADIR)/org/conman/zip
	$(INSTALL) -d $(DESTDIR)$(LUADIR)/org/conman/const
	$(INSTALL) -d $(DESTDIR)$(LUADIR)/org/conman/net
	$(INSTALL) -d $(DESTDIR)$(LUADIR)/org/conman/nfl
	$(INSTALL) -d $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL) -d $(DESTDIR)$(LIBDIR)/org/conman/fsys
	$(INSTALL_PROGRAM) lib/base64.so   $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_PROGRAM) lib/clock.so    $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_PROGRAM) lib/crc.so      $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_PROGRAM) lib/env.so      $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_PROGRAM) lib/errno.so    $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_PROGRAM) lib/fsys.so     $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_PROGRAM) lib/hash.so     $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_PROGRAM) lib/iconv.so    $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_PROGRAM) lib/lfsr.so     $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_PROGRAM) lib/idn.so      $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_PROGRAM) lib/magic.so    $(DESTDIR)$(LIBDIR)/org/conman/fsys
	$(INSTALL_PROGRAM) lib/math.so     $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_PROGRAM) lib/net.so      $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_PROGRAM) lib/pollset.so  $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_PROGRAM) lib/process.so  $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_PROGRAM) lib/signal.so   $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_PROGRAM) lib/strcore.so  $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_PROGRAM) lib/sys.so      $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_PROGRAM) lib/syslog.so   $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_PROGRAM) lib/tls.so      $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_DATA)    lua/*.lua       $(DESTDIR)$(LUADIR)/org/conman	
	$(INSTALL_DATA)    lua/dns/*.lua   $(DESTDIR)$(LUADIR)/org/conman/dns
	$(INSTALL_DATA)    lua/zip/*.lua   $(DESTDIR)$(LUADIR)/org/conman/zip
	$(INSTALL_DATA)    lua/const/*.lua $(DESTDIR)$(LUADIR)/org/conman/const
	$(INSTALL_DATA)    lua/net/*.lua   $(DESTDIR)$(LUADIR)/org/conman/net
	$(INSTALL_DATA)    lua/nfl/*.lua   $(DESTDIR)$(LUADIR)/org/conman/nfl

install-obsolete: obsolete
	$(INSTALL) -d $(DESTDIR)$(LUADIR)/org/conman
	$(INSTALL) -d $(DESTDIR)$(LIBDIR)/org/conman	
	$(INSTALL_PROGRAM) lib/tcc.so $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_DATA)    lua/cc.lua $(DESTDIR)$(LUADIR)/org/conman

uninstall:
	$(RM) -r $(DESTDIR)$(LIBDIR)/org/conman
	$(RM) -r $(DESTDIR)$(LUADIR)/org/conman

clean:
	$(RM) $(shell find . -name '*~')
	$(RM) -r lib/*
	$(RM) build/bin2c
	$(RM) -r $(shell find . -name '*.dSYM')
