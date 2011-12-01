GCC=gcc
GCCPLUGINS_DIR = $(shell $(GCC) -print-file-name=plugin)
CFLAGS += -I$(GCCPLUGINS_DIR)/include -fPIC -g3 -O0 \
          -Wall -pedantic -std=c99 $(EXTRA_CFLAGS)

$(PLUGIN): $(PLUGIN_OBJECT_FILES)
	$(GCC) -g -shared $^ -o $@ $(CFLAGS)
                    
test: clean $(PLUGIN) test.c
	$(GCC) test.c -o $@ -fplugin=./$(PLUGIN) \
        -g3 -O0 $(EXTRA_OBJS) $(EXTRA_ARGS)

clean:
	rm -fv $(PLUGIN) *.o test

debug:
	exec gdb --args /home/enferex/docs/edu/go/dev/gcc-obj/gcc/cc1 \
        -fplugin=./munger.so test.c
