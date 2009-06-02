#
# Makefile for the rfsdelta.ko kernel module for Linux 2.6
# by pts@fazekas.hu at Thu Jan 11 17:33:01 CET 2007
#
# Dat: see also linux/Documentation/kbuild/makefiles.txt
# Dat: see also The Linux Kernel Module Programming Guide
#      http://www.openaddict.com/documents/lkmpg/x181.html
#

# vvv Dat: override with `make KERNEL_VERSION=...'
KERNEL_VERSION = $(shell uname -r)
RFSDELTA_VERSION = 0.06
PRODUCT = rfsdelta-$(VERSION)

.PHONY: all clean install modules_install

# vvv Dat: used by kbuild ($RFSDELTA_EXTRA_CFLAGS isn't)
EXTRA_CFLAGS=-DRFSDELTA_VERSION=\"$(RFSDELTA_VERSION)\"
#"

# vvv Dat: `-m' is for module
# vvv Dat: the module-name is the 1st .o name in `obj-m'
obj-m += rfsdelta.o
# obj-m += hello-2.o

all:
	make -C /lib/modules/$(KERNEL_VERSION)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(KERNEL_VERSION)/build M=$(PWD) clean
	rm -f Module.symvers core DEADJOE a.out

install modules_install:
	make -C /lib/modules/$(KERNEL_VERSION)/build M=$(PWD) modules_install
