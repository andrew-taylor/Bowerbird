NAME=comb_filter
ARCH := $(shell uname -m|sed 's/^arm.*/arm/;s/.*86/x86/')
SOURCE=$(NAME).c templates.c option.c util.c gnuplot.c buffer.c sound.c
OBJECTS=$(SOURCE:%.c=$(ARCH)/%.o)
INCLUDES=$(NAME).h

LIB_PATH=
FILES=Makefile $(SOURCE) $(INCLUDES) prototypes.h i.h    
INCLUDE_PATH=
LIBS0=-lm -lsndfile -lm -lgsl  -lgslcblas -lfftw3

CC=gcc-4.3
COMMON_CC_OPTS='-std=gnu99' -Wall -Wstrict-prototypes -Wmissing-prototypes $(LIB_PATH) $(INCLUDE_PATH)
CC_OPTS=-O3 -g $(COMMON_CC_OPTS)
DEBUG_OPTS=-O3 -ggdb -fmudflap -lmudflap $(COMMON_CC_OPTS)
#ARM_CC=/usr/local/opt/crosstool/arm-linux/gcc-3.3.4-glibc-2.3.2/bin/arm-linux-gcc
VERSION := $(shell grep '[^a-z]version = ' $(NAME).c|sed 's/[^"]*"//;s/".*//')
PUBLISH_DIR=/home/andrewt/public_html/$(NAME)/

ifeq (arm,$(ARCH))
INSTALL_DIR=/home/andrewt/pleb.arm.linux/bin/
else
INSTALL_DIR=/home/andrewt/pc.i86.linux/bin/
endif

ifeq (arm,$(ARCH))
CC_OPTS+= -mtune=arm940t -DTS7200
LIBS=-Wl,-Bstatic $(LIBS0)  -Wl,-Bdynamic -ldl
LIBS=$(LIBS0)
else
LIBS=$(LIBS0)
endif

all: standard debug

standard: $(ARCH)/$(NAME)

profile: $(ARCH)/$(NAME).profile

debug: $(ARCH)/$(NAME).debug

$(ARCH)/%.o: %.c $(INCLUDES)
	$(CC) $(CC_OPTS) -o $@ -c $< 

$(ARCH)/$(NAME): prototypes.h $(OBJECTS) Makefile
	$(CC) $(CC_OPTS) -o $@ $(OBJECTS) $(LIBS)
	
$(ARCH)/$(NAME).debug: prototypes.h $(SOURCE) Makefile $(INCLUDES)
	$(CC) $(DEBUG_OPTS) -o $@ $(SOURCE) $(LIBS)
	
$(ARCH)/$(NAME).profile: prototypes.h $(SOURCE) Makefile $(INCLUDES)
	$(CC) -pg $(CC_OPTS) -o $@ $(SOURCE) $(LIBS)
	
prototypes.h: $(SOURCE) Makefile
	/home/andrewt/bin/prototypes $(SOURCE) >$@

clean:
	rm -f $(ARCH)/*.o

install: $(ARCH)/$(NAME) version
	cp -p $(ARCH)/$(NAME) $(INSTALL_DIR)

version:
	mkdir -p versions/$(VERSION)
	cp -p $(FILES) versions/$(VERSION)

publish:version
	mkdir -p $(PUBLISH_DIR)/$(VERSION)
	chmod 755 $(PUBLISH_DIR)/$(VERSION)
	cp -p $(FILES) $(PUBLISH_DIR)/$(VERSION)
	echo put $(PUBLISH_DIR)/$(VERSION)
	echo put $(VERSION)

test: $(ARCH)/$(NAME)
