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
  CFLAGS  = -g -Wall -Wextra -pedantic
  LDFLAGS = -g -shared
endif

ifeq ($(UNAME),SunOS)
  CC      = cc -xc99
  CFLAGS  = -g -mt -m64 -I /usr/sfw/include
  LDFLAGS = -G -xcode=pic32
  lib/net.so : LDLIBS = -lsocket -lnsl
endif

ifeq ($(UNAME),Darwin)
  CC      = gcc -std=c99
  CFLAGS  = -g -Wall -Wextra -pedantic
  LDFLAGS = -g -bundle -undefined dynamic_lookup -all_load
  lib/iconv.so : LDLIBS = -liconv
endif

override CFLAGS += -fPIC

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

.PHONY:	all clean install uninstall

lib/%.so : src/%.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

all : lib		\
	lib/env.so	\
	lib/errno.so	\
	lib/fsys.so	\
	lib/math.so	\
	lib/syslog.so	\
	lib/iconv.so	\
	lib/crc.so	\
	lib/hash.so	\
	lib/magic.so	\
	lib/process.so	\
	lib/net.so	\
	lib/pollset.so	\
	lib/tcc.so	\
	lib/sys.so	\
	lib/strcore.so	\
	lib/base64.so	\
	lib/signal.so	\
	lib/clock.so	\
	lib/ptscore.so	\
	build/bin2c

build/bin2c : build/bin2c.c
	$(CC) $(CFLAGS) -o $@ $< -lz

lib :
	mkdir lib

lib/hash.so    : LDLIBS = -lcrypto
lib/magic.so   : LDLIBS = -lmagic
lib/tcc.so     : LDLIBS = -ltcc
lib/clock.so   : LDLIBS = -lrt

# ===================================================

install : all
	$(INSTALL) -d $(DESTDIR)$(LUADIR)/org/conman	
	$(INSTALL) -d $(DESTDIR)$(LUADIR)/org/conman/dns
	$(INSTALL) -d $(DESTDIR)$(LUADIR)/org/conman/zip
	$(INSTALL) -d $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL) -d $(DESTDIR)$(LIBDIR)/org/conman/fsys
	$(INSTALL_PROGRAM) lib/env.so     $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_PROGRAM) lib/errno.so   $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_PROGRAM) lib/fsys.so    $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_PROGRAM) lib/math.so    $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_PROGRAM) lib/syslog.so  $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_PROGRAM) lib/iconv.so   $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_PROGRAM) lib/crc.so     $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_PROGRAM) lib/hash.so    $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_PROGRAM) lib/magic.so   $(DESTDIR)$(LIBDIR)/org/conman/fsys
	$(INSTALL_PROGRAM) lib/process.so $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_PROGRAM) lib/net.so     $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_PROGRAM) lib/pollset.so $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_PROGRAM) lib/tcc.so     $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_PROGRAM) lib/sys.so     $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_PROGRAM) lib/strcore.so $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_PROGRAM) lib/base64.so  $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_PROGRAM) lib/signal.so  $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_PROGRAM) lib/clock.so   $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_PROGRAM) lib/ptscore.so $(DESTDIR)$(LIBDIR)/org/conman
	$(INSTALL_DATA)    lua/*.lua      $(DESTDIR)$(LUADIR)/org/conman	
	$(INSTALL_DATA)    lua/dns/*.lua  $(DESTDIR)$(LUADIR)/org/conman/dns
	$(INSTALL_DATA)    lua/zip/*.lua  $(DESTDIR)$(LUADIR)/org/conman/zip

uninstall:
	$(RM) -r $(DESTDIR)$(LIBDIR)/org/conman
	$(RM) -r $(DESTDIR)$(LUADIR)/org/conman

clean:
	$(RM) $(shell find . -name '*~')
	$(RM) lib/*
	$(RM) build/bin2c
