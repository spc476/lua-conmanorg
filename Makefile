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
endif

ifeq ($(UNAME),Darwin)
  CC     = gcc -std=c99
  CFLAGS = -g -Wall -Wextra -pedantic
  LFLAGS =
  SHARED = -shared -fPIC -undefined dynamic_lookup -all_load
endif

LUALUA = /usr/local/share/lua/5.1
LUALIB = /usr/local/lib/lua/5.1

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
	install -d $(LUALUA)/org/conman	
	install -d $(LUALUA)/org/conman/dns
	install -d $(LUALUA)/org/conman/zip
	install lua/*.lua $(LUALUA)/org/conman	
	install lua/dns/*.lua $(LUALUA)/org/conman/dns
	install lua/zip/*.lua $(LUALUA)/org/conman/zip
	install -d $(LUALIB)/org/conman
	install -d $(LUALIB)/org/conman/fsys
	install lib/env.so     $(LUALIB)/org/conman
	install lib/errno.so   $(LUALIB)/org/conman
	install lib/fsys.so    $(LUALIB)/org/conman
	install lib/math.so    $(LUALIB)/org/conman
	install lib/syslog.so  $(LUALIB)/org/conman
	install lib/iconv.so   $(LUALIB)/org/conman
	install lib/crc.so     $(LUALIB)/org/conman
	install lib/hash.so    $(LUALIB)/org/conman
	install lib/magic.so   $(LUALIB)/org/conman/fsys
	install lib/process.so $(LUALIB)/org/conman
	install lib/net.so     $(LUALIB)/org/conman
	install lib/pollset.so $(LUALIB)/org/conman
	install lib/tcc.so     $(LUALIB)/org/conman
	install lib/sys.so     $(LUALIB)/org/conman
	install lib/strcore.so $(LUALIB)/org/conman
	install lib/base64.so  $(LUALIB)/org/conman
	install lib/signal.so  $(LUALIB)/org/conman
	install lib/clock.so   $(LUALIB)/org/conman
	install lib/ptscore.so $(LUALIB)/org/conman

remove:
	/bin/rm -rf $(LUALIB)/org/conman
	/bin/rm -rf $(LUALUA)/org/conman
