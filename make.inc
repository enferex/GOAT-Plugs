##############################################################################
### Should be g++ from gcc 4.9 (put the full path in if it is not default) ###
##############################################################################
CC=g++
##############################################################################


CCPLUGINS_DIR = $(shell $(CC) -print-file-name=plugin)
CFLAGS += -I$(CCPLUGINS_DIR)/include -fPIC -g3 -O0 \
          -Wall $(EXTRA_CFLAGS) -fno-rtti

$(PLUGIN): $(PLUGIN_OBJECT_FILES)
	$(CC) -g -shared $^ -o $@ $(CFLAGS)
                    
test: clean $(PLUGIN) test.c
	$(CC) test.c -o $@ -fplugin=./$(PLUGIN) \
        -g3 -O0 $(EXTRA_OBJS) $(EXTRA_ARGS)

clean:
	rm -fv $(PLUGIN) *.o test


# Targets and macros for my personal entertainment
# CC=/home/enferex/proj/gcc-4.8.0-install/bin/g++
#debug:
#	exec gdb --args /home/enferex/docs/edu/go/dev/gcc-obj/gcc/cc1 \
#        -fplugin=./munger.so test.c
