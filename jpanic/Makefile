GCC=gcc
PLUGIN_SOURCE_FILES = jpanic.c
PLUGIN_OBJECT_FILES = jpanic.o
GCCPLUGINS_DIR = $(shell $(GCC) -print-file-name=plugin)
CFLAGS += -I$(GCCPLUGINS_DIR)/include -fPIC -g3 -O0 -Wall -pedantic -std=c99 \
          $(EXTRA_CFLAGS)

jpanic.so: $(PLUGIN_OBJECT_FILES)
	$(GCC) -g -shared $^ -o $@ $(CFLAGS)

test: clean jpanic.so test.c
	$(GCC) test.c -o $@ -fplugin=./jpanic.so -O0 -g3 \
                            -fplugin-arg-jpanic-maxjunk=1000

clean:
	rm -fv jpanic.so *.o test
