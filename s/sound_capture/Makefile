MAIN_BINARY=sound_capture
TARGET ?= arm
SOURCE=$(MAIN_BINARY).c alsa.c ts7200.c option.c util.c sunrise.c
OBJECTS=$(SOURCE:%.c=$(TARGET)/%.o)
INCLUDES=$(MAIN_BINARY).h
FILES=Makefile $(SOURCE) watchdog.c $(INCLUDES) prototypes.h i.h    
INCLUDE_PATH = $(EXTRA_INCLUDES)

CC=$(CROSS_COMPILE)gcc
COMMON_CC_OPTS='-std=gnu99' -Wall -Wstrict-prototypes -Wmissing-prototypes $(LIB_PATH) $(INCLUDE_PATH)
CC_OPTS=-O3 -g $(COMMON_CC_OPTS)
DEBUG_OPTS=-O3 -ggdb -fmudflap -lmudflap $(COMMON_CC_OPTS)

ifeq (arm,$(TARGET))
CC_OPTS += -mtune=arm940t -DTS7200
LIBS ?= -lm $(EXTRA_LIBS)
else
LIBS ?= -lm -lasound $(EXTRA_LIBS)
endif

standard: $(TARGET)/$(MAIN_BINARY) 

profile: $(TARGET)/$(MAIN_BINARY).profile

debug: $(TARGET)/$(MAIN_BINARY).debug

all: standard profile debug watchdog

$(TARGET)/%.o: %.c $(INCLUDES)
	$(CC) $(CC_OPTS) -o $@ -c $< 

$(TARGET)/$(MAIN_BINARY): prototypes.h $(OBJECTS) Makefile
	$(CC) $(CC_OPTS) -o $@ $(OBJECTS) $(LIBS)
	
$(TARGET)/$(MAIN_BINARY).debug: prototypes.h $(SOURCE) Makefile $(INCLUDES)
	$(CC) $(DEBUG_OPTS) -o $@ $(SOURCE) $(LIBS)
	
$(TARGET)/$(MAIN_BINARY).profile: prototypes.h $(SOURCE) Makefile $(INCLUDES)
	$(CC) -pg $(CC_OPTS) -o $@ $(SOURCE) $(LIBS)
	
prototypes.h: $(SOURCE) Makefile
	/home/andrewt/bin/prototypes $(SOURCE) >$@

clean:
	rm -f $(MAIN_BINARY) $(TARGET)/*.o

install: arm/$(MAIN_BINARY)
	cp -p arm/$(MAIN_BINARY) $(INSTALL_DIR)
	
.PHONY: all install clobber tags cross
.PRECIOUS: $(TARGET)/%.o $(TARGET)/debug/%.o $(TARGET)/profile/%.o
