MAIN_BINARY=ts7200_watchdog
TARGET ?= arm
SOURCE=$(MAIN_BINARY).c util.c option.c
INCLUDES=$(MAIN_BINARY).h i.h
FILES=$(SOURCE) $(INCLUDES) prototypes.h
DEPENDENCIES:=$(FILES:%.c=$(TARGET)/%.o)

CC=$(CROSS_COMPILE)gcc
COMMON_CC_OPTS='-std=c99' -Wall -Wstrict-prototypes -Wmissing-prototypes -DWATCHDOG_ONLY
ifeq (arm,$(TARGET))
ifeq (7800,$(PLATFORM))
COMMON_CC_OPTS+=-mtune=arm940t -DTS7800
else
COMMON_CC_OPTS+=-mtune=arm940t -DTS7200
endif
endif
CC_STANDARD_OPTS=-O3 -g $(COMMON_CC_OPTS) -funroll-loops
CC_DEBUG_OPTS=-O3 -ggdb -fmudflap $(COMMON_CC_OPTS)
CC_PROFILE_OPTS=-pg $(COMMON_CC_OPTS)


LIBS=-lm

all: $(TARGET)/$(MAIN_BINARY)

#ts7800_watchdog: $(SOURCE)
#	$(CC) $(CC_STANDARD_OPTS) -o ts7800_watchdog -DTS7800 $(SOURCE) $(LIBS)
debug: $(TARGET)/$(MAIN_BINARY).debug

CC_ARGS=-o $(TARGET)/$(@F) $(filter %.o, $^) $(LIBS)

$(TARGET)/%.o: %.c $(INCLUDES)
	$(CC) $(CC_STANDARD_OPTS) -o $@ -c $< 

$(TARGET)/debug/%.o: %.c $(INCLUDES)
	$(CC) $(CC_DEBUG_OPTS) -o $@ -c $< 

$(TARGET)/profile/%.o: %.c $(INCLUDES)
	$(CC) $(CC_PROFILE_OPTS) -o $@ -c $< 

$(TARGET)/$(MAIN_BINARY): prototypes.h $(SOURCE:%.c=$(TARGET)/%.o) $(INCLUDES) Makefile
	$(CC) $(CC_STANDARD_OPTS) $(CC_ARGS)
	
$(TARGET)/$(MAIN_BINARY).debug:  $(SOURCE:%.c=$(TARGET)/debug/%.o) $(INCLUDES) Makefile prototypes.h
	$(CC) $(CC_DEBUG_OPTS) $(CC_ARGS) -lmudflap

$(TARGET)/$(MAIN_BINARY).profile:  $(SOURCE:%.c=$(TARGET)/profile/%.o) $(INCLUDES) Makefile prototypes.h
	$(CC) $(CC_PROFILE_OPTS) $(CC_ARGS)

#prototypes.h: $(SOURCE)
#	/home/andrewt/bin/prototypes $(SOURCE) >$@

install: $(TARGET)/$(MAIN_BINARY)
	cp -p $(TARGET)/$(MAIN_BINARY) $(INSTALL_DIR)
	
clean:
	rm -f core *.o */*.o */*/*.o gmon.out

.PHONY: all install publish clobber tags
.PRECIOUS: $(TARGET)/%.o $(TARGET)/debug/%.o $(TARGET)/profile/%.o

