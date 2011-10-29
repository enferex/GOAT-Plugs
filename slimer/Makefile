GCC=gcc
PLUGIN_SOURCE_FILES = slimer.c slimer_builtins.c
PLUGIN_OBJECT_FILES = slimer.o slimer_builtins.o
GCCPLUGINS_DIR = $(shell $(GCC) -print-file-name=plugin)
CFLAGS += -I$(GCCPLUGINS_DIR)/include -fPIC -g3 -O0 -Wall -pedantic -std=c99 \
          $(EXTRA_CFLAGS)

slimer.so: $(PLUGIN_OBJECT_FILES)
	$(GCC) -g -shared $^ -o $@ $(CFLAGS)

test: clean slimer.so test.c
	$(GCC) test.c -o $@ -fplugin=./slimer.so -g3 -O0 slimer_builtins.o \
                            -fplugin-arg-slimer-numfuncs=20 \
                            -fplugin-arg-slimer-maxcalls=1000

clean:
	rm -fv slimer.so *.o test
