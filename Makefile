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
  CC     = gcc -std=c99
  CFLAGS = -g -Wall -Wextra -pedantic
  LFLAGS = 
  SHARED = -shared -fPIC
endif

ifeq ($(UNAME),SunOS)
  CC     = cc -xc99
  CFLAGS = -g -mt -m64 -I /usr/sfw/include
  LFLAGS =
  SHARED = -G -xcode=pic32
  lib/net.so : LDLIBS = -lsocket -lnsl
endif

ifeq ($(UNAME),Darwin)
  CC     = gcc -std=c99
  CFLAGS = -g -Wall -Wextra -pedantic
  LFLAGS =
  SHARED = -shared -fPIC -undefined dynamic_lookup -all_load
endif

INSTALL         = /usr/bin/install
INSTALL_PROGRAM = $(INSTALL)
INSTALL_DATA    = $(INSTALL) -m 644 
prefix          = /usr/local

LUALUA = $(prefix)/share/lua/$(shell lua -e "print(_VERSION:match '^Lua (.*)')")
LUALIB = $(prefix)/lib/lua/$(shell lua -e "print(_VERSION:match '^Lua (.*)')")

.PHONY:	all clean install remove

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

lib/%.so : src/%.c
	$(CC) $(CFLAGS) $(LFLAGS) $(SHARED) -o $@ $< $(LDLIBS)

lib/hash.so    : LDLIBS = -lcrypto
lib/magic.so   : LDLIBS = -lmagic
lib/tcc.so     : LDLIBS = -ltcc
lib/clock.so   : LDLIBS = -lrt

clean:
	/bin/rm -rf *~ lua/*~ src/*~ build/*~
	/bin/rm -rf lib/*
	/bin/rm -rf build/bin2c

install : all
	$(INSTALL) -d $(DESTDIR)$(LUALUA)/org/conman	
	$(INSTALL) -d $(DESTDIR)$(LUALUA)/org/conman/dns
	$(INSTALL) -d $(DESTDIR)$(LUALUA)/org/conman/zip
	$(INSTALL) -d $(DESTDIR)$(LUALIB)/org/conman
	$(INSTALL) -d $(DESTDIR)$(LUALIB)/org/conman/fsys
	$(INSTALL_PROGRAM) lib/env.so     $(DESTDIR)$(LUALIB)/org/conman
	$(INSTALL_PROGRAM) lib/errno.so   $(DESTDIR)$(LUALIB)/org/conman
	$(INSTALL_PROGRAM) lib/fsys.so    $(DESTDIR)$(LUALIB)/org/conman
	$(INSTALL_PROGRAM) lib/math.so    $(DESTDIR)$(LUALIB)/org/conman
	$(INSTALL_PROGRAM) lib/syslog.so  $(DESTDIR)$(LUALIB)/org/conman
	$(INSTALL_PROGRAM) lib/iconv.so   $(DESTDIR)$(LUALIB)/org/conman
	$(INSTALL_PROGRAM) lib/crc.so     $(DESTDIR)$(LUALIB)/org/conman
	$(INSTALL_PROGRAM) lib/hash.so    $(DESTDIR)$(LUALIB)/org/conman
	$(INSTALL_PROGRAM) lib/magic.so   $(DESTDIR)$(LUALIB)/org/conman/fsys
	$(INSTALL_PROGRAM) lib/process.so $(DESTDIR)$(LUALIB)/org/conman
	$(INSTALL_PROGRAM) lib/net.so     $(DESTDIR)$(LUALIB)/org/conman
	$(INSTALL_PROGRAM) lib/pollset.so $(DESTDIR)$(LUALIB)/org/conman
	$(INSTALL_PROGRAM) lib/tcc.so     $(DESTDIR)$(LUALIB)/org/conman
	$(INSTALL_PROGRAM) lib/sys.so     $(DESTDIR)$(LUALIB)/org/conman
	$(INSTALL_PROGRAM) lib/strcore.so $(DESTDIR)$(LUALIB)/org/conman
	$(INSTALL_PROGRAM) lib/base64.so  $(DESTDIR)$(LUALIB)/org/conman
	$(INSTALL_PROGRAM) lib/signal.so  $(DESTDIR)$(LUALIB)/org/conman
	$(INSTALL_PROGRAM) lib/clock.so   $(DESTDIR)$(LUALIB)/org/conman
	$(INSTALL_PROGRAM) lib/ptscore.so $(DESTDIR)$(LUALIB)/org/conman
	$(INSTALL_DATA)    lua/*.lua      $(DESTDIR)$(LUALUA)/org/conman	
	$(INSTALL_DATA)    lua/dns/*.lua  $(DESTDIR)$(LUALUA)/org/conman/dns
	$(INSTALL_DATA)    lua/zip/*.lua  $(DESTDIR)$(LUALUA)/org/conman/zip

remove:
	$(RM) -r $(DESTDIR)$(LUALIB)/org/conman
	$(RM) -r $(DESTDIR)$(LUALUA)/org/conman
