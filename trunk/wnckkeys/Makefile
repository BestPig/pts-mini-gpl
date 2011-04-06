#
# Makefile (in GNU Make syntax) for wnckkeys
# by pts@fazekas.hu at Wed Apr  6 14:08:25 CEST 2011
#

# Defines $(CC), $(CFLAGS) and $(LIBS)
include config.mk

.PHONY: all clean install

OBJS = xbindkeys.o keys.o options.o get_key.o grab_key.o internalcmd.o

all: wnckkeys
	@echo 'compile OK, now run: sudo make install'

clean:
	rm -f $(OBJS) wnckkeys

install: wnckkeys
	install -d $(prefix)/bin
	install -t $(prefix)/bin wnckkeys

wnckkeys: $(OBJS)
	$(CC) -s $(LIBS) -o $@ $(OBJS)

%.o: %.c
	gcc -W -Wall -O2 $(CFLAGS) -c $<
