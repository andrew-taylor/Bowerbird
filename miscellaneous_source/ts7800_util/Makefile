MAIN_BINARY=ts7800_util
SOURCE=$(MAIN_BINARY).c
INCLUDES=
TARGET?=arm
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


$(MAIN_BINARY): $(SOURCE) $(INCLUDES) Makefile
	$(CC) $(CC_STANDARD_OPTS) -o $@ $(SOURCE) $(INCLUDE_PATH)

clean:
	rm -f $(MAIN_BINARY)

install: $(MAIN_BINARY)
	cp -p $(MAIN_BINARY) $(INSTALL_DIR)
