

CC     = gcc -std=c99
CFLAGS = -g -Wall -Wextra -pedantic 
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
	lib/wrap.so
	
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
	
clean:
	/bin/rm -rf *~ lua/*~ src/*~
	/bin/rm -rf lib/*

install : all
	install -d $(LUALUA)/org/conman	
	install lua/*.lua $(LUALUA)/org/conman	
	install -d $(LUALIB)/org/conman
	install -d $(LUALIB)/org/conman/string
	install lib/env.so    $(LUALIB)/org/conman
	install lib/errno.so  $(LUALIB)/org/conman
	install lib/fsys.so   $(LUALIB)/org/conman
	install lib/math.so   $(LUALIB)/org/conman
	install lib/syslog.so $(LUALIB)/org/conman
	install lib/trim.so   $(LUALIB)/org/conman/string
	install lib/wrap.so   $(LUALIB)/org/conman/string

remove:
	/bin/rm -rf $(LUALIB)/org/conman
	/bin/rm -rf $(LUALUA)/org/conman
	
