

CC     = gcc -std=c99
CFLAGS = -g -Wall -Wextra -pedantic 
LFLAGS = -shared 

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
	