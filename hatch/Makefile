GCC=gcc
PLUGIN_SOURCE_FILES = hatch.c hatch_builtins.c
PLUGIN_OBJECT_FILES = hatch.o hatch_builtins.o
GCCPLUGINS_DIR = $(shell $(GCC) -print-file-name=plugin)
CFLAGS += -I$(GCCPLUGINS_DIR)/include -fPIC -g3 -O0 -Wall -pedantic -std=c99

hatch.so: $(PLUGIN_OBJECT_FILES)
	$(GCC) -g -shared $^ -o $@ $(CFLAGS)

test: clean hatch.so hatch_builtins.o test.c
	$(GCC) test.c -o $@ -fplugin=./hatch.so -g3 hatch_builtins.o -static -O

clean:
	rm -fv hatch.so *.o test
