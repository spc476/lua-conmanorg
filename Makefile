#########################################################################
#
# Copyright 2011 by Sean Conner.  All Rights Reserved.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# Comments, questions and criticisms can be sent to: sean@conman.org
#
########################################################################

CC     = gcc -std=c99
CFLAGS = -g -Wall -Wextra -pedantic -fPIC -DNDEBUG -O3
LFLAGS = -shared 

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
	
clean:
	/bin/rm -rf *~ lua/*~ src/*~
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

remove:
	/bin/rm -rf $(LUALIB)/org/conman
	/bin/rm -rf $(LUALUA)/org/conman
	
