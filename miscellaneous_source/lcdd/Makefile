MAIN_BINARY=lcdd
SOURCE=lcdd.c lcd_func_ansi.c ts7200io.c
INCLUDES=ts7200io.h

CC=$(CROSS_COMPILE)gcc
CC_OPTS=-O   $(EXTRA_INCLUDES) $(EXTRA_LIBS)

$(MAIN_BINARY): $(SOURCE) $(INCLUDES) Makefile
	$(CC) $(CC_OPTS) -o $@ $(SOURCE)

clean:
	rm -f $(MAIN_BINARY)

install: $(MAIN_BINARY)
	cp -p $(MAIN_BINARY) $(INSTALL_DIR)


#
# Make file for lcdd
# daemon that reads named pipe (fifo) and sends anything read from 
#  that to a parallel connected HD44 compatible char. LCD
# Software specific to TS-72x0 ARM SBC
# 
# Makefile for arm-linux cross compilation on i386 debian box,
# using the arm crosstool kit provided by Technologic Systems
# for the TS-72x0 ARM SBC

