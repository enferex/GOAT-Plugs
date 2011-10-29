TOPDIR=/home/enferex/docs/edu/go/dev/gcc-install
GCC=$(TOPDIR)/bin/gcc
PLUGIN_SOURCE_FILES = nopper.c
PLUGIN_OBJECT_FILES = nopper.o
GCCPLUGINS_DIR = $(shell $(GCC) -print-file-name=plugin)
CFLAGS += -I$(GCCPLUGINS_DIR)/include -fPIC -g3 -O -Wall -pedantic -std=c99
DBG_CC = $(TOPDIR)/libexec/gcc/x86_64-unknown-linux-gnu/4.6.0/cc1 

nopper.so: $(PLUGIN_OBJECT_FILES)
	$(GCC) -g -shared $^ -o $@ $(CFLAGS)

test: clean nopper.so test.c
	$(GCC) test.c -o $@ -fplugin=./nopper.so -O -g3

debug: nopper.so test.c
	gdb --args $(DBG_CC) test.c -o test -fplugin=./nopper.so -O -g3

clean:
	rm -fv nopper.so *.o test
