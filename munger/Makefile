GCC=gcc
PLUGIN_SOURCE_FILES = munger.c munger_builtins.c
PLUGIN_OBJECT_FILES = munger.o munger_builtins.o
GCCPLUGINS_DIR = $(shell $(GCC) -print-file-name=plugin)
CFLAGS += -I$(GCCPLUGINS_DIR)/include -fPIC -g3 -O0 -Wall -pedantic -std=c99 \
          $(EXTRA_CFLAGS)

munger.so: $(PLUGIN_OBJECT_FILES)
	$(GCC) -g -shared $^ -o $@ $(CFLAGS)

test: clean munger.so test.c
	$(GCC) test.c -o $@ -fplugin=./munger.so -O -g3 munger_builtins.o

clean:
	rm -fv munger.so *.o test
