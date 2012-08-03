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
CFLAGS = -g -Wall -Wextra -pedantic -fpic
LFLAGS = -shared 
LNET   =
endif

ifeq ($(UNAME),SunOS)
CC     = cc -xc99
CFLAGS =  -g -mt -m64 -xcode=pic32 -I /home/spc/work/ecid/third_party/lua-5.1.4/src -I /usr/sfw/include
LFLAGS = -G -mt -m64 -L /usr/sfw/lib/64
LNET   = -lsocket -lnsl
endif

LUALUA = /usr/local/share/lua/5.1
LUALIB = /usr/local/lib/lua/5.1

.PHONY:	all clean

all : lib/env.so	\
	lib/errno.so	\
	lib/fsys.so	\
	lib/math.so	\
	lib/syslog.so	\
	lib/trim.so	\
	lib/wrap.so	\
	lib/remchar.so	\
	lib/iconv.so	\
	lib/crc.so	\
	lib/hash.so	\
	lib/magic.so	\
	lib/process.so	\
	lib/net.so	\
	lib/tcc.so	\
	build/bin2c

build/bin2c : build/bin2c.c
	$(CC) $(CFLAGS) -o $@ $<

lib/env.so : src/env.c
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $<

lib/errno.so : src/errno.c
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $<
	
lib/fsys.so : src/fsys.c
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $<
	
lib/math.so : src/math.c
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $<
	
lib/syslog.so : src/syslog.c
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $<
	
lib/trim.so : src/trim.c
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $<
	
lib/wrap.so : src/wrap.c
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $<

lib/remchar.so : src/remchar.c
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $<
	
lib/iconv.so : src/iconv.c
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $<

lib/crc.so : src/crc.c
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $<
	
lib/hash.so : src/hash.c
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $< -lcrypto

lib/magic.so : src/magic.c
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $< -lmagic

lib/process.so : src/process.c
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $< -lrt

lib/net.so : src/net.c
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $< 

lib/tcc.so : src/tcc.c
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $<

clean:
	/bin/rm -rf *~ lua/*~ src/*~ build/*~
	/bin/rm -rf lib/*
	/bin/rm -rf build/bin2c

install : all
	install -d $(LUALUA)/org/conman	
	install -d $(LUALUA)/org/conman/dns
	install lua/*.lua $(LUALUA)/org/conman	
	install lua/dns/*.lua $(LUALUA)/org/conman/dns
	install -d $(LUALIB)/org/conman
	install -d $(LUALIB)/org/conman/string
	install -d $(LUALIB)/org/conman/fsys
	install lib/env.so     $(LUALIB)/org/conman
	install lib/errno.so   $(LUALIB)/org/conman
	install lib/fsys.so    $(LUALIB)/org/conman
	install lib/math.so    $(LUALIB)/org/conman
	install lib/syslog.so  $(LUALIB)/org/conman
	install lib/trim.so    $(LUALIB)/org/conman/string
	install lib/wrap.so    $(LUALIB)/org/conman/string
	install lib/remchar.so $(LUALIB)/org/conman/string
	install lib/iconv.so   $(LUALIB)/org/conman
	install lib/crc.so     $(LUALIB)/org/conman
	install lib/hash.so    $(LUALIB)/org/conman
	install lib/magic.so   $(LUALIB)/org/conman/fsys
	install lib/process.so $(LUALIB)/org/conman
	install lib/net.so     $(LUALIB)/org/conman
	install lib/tcc.so     $(LUALIB)/org/conman

remove:
	/bin/rm -rf $(LUALIB)/org/conman
	/bin/rm -rf $(LUALUA)/org/conman
	
