/* ts7200io.c
 * Functions for reading/writing to DIO lines, ADC, LCD
 *   on TS7200 SBC
 * Jim Jackson
 *
 */
/*
 * Copyright (C) 2004 Jim Jackson           jj@franjam.org.uk
 */
/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this program - see the file COPYING; if not, write to 
 *  the Free Software Foundation, Inc., 675 Mass Ave, Cambridge, 
 *  MA 02139, USA.
 */
/*
 *  History......................................................
 */

/***MANPAGE***/
/*
Jim Jackson   <jj@franjam.org.uk>
 */

/***INCLUDES***/
/* includes files 
 */

#include <stdio.h>
/* for SYSV variants .... */
/* #include <string.h> */
#include <strings.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/mman.h>

#include "ts7200io.h"

/***DEFINES***/
/* defines
 */

#define IOMMAP(A)  (mmap(0,getpagesize(),PROT_READ|PROT_WRITE, MAP_SHARED, \
				iofd,A))

#define ADC_LOOP_LIMIT (2048)
/*#define ADC_SLEEP*/

#define dio_set_bit8(A) setbiobit(dio_io_bit8,0,A)
#define dio_set_ddr8(A) setbiobit(dio_io_ddr8,0,A)

#define nop() __asm__ __volatile__("mov\tr0,r0\t@ nop\n\t");

#define COUNTDOWN(x)    asm volatile ( \
  "1:\n"\
  "subs %1, %1, #1;\n"\
  "bne 1b;\n"\
  : "=r" ((x)) : "r" ((x)) \
);

#define TRUE 1
#define FALSE 0

/***GLOBALS***/
/* global variables
 */

int iofd=-1;

int adcerror=0;

/* ADC Stuff
 */

volatile unsigned short *adc_io_stat,*adc_io_read;
volatile unsigned char *adc_io_cntl;

/* GPIO pointers..........
 */

volatile unsigned char *porth_ddr,*porth_byte;

/* DIO stuff......
 */

volatile unsigned char *gpiopage;
volatile unsigned char *dio_io_ddr;
volatile unsigned char *dio_io_byte;
volatile unsigned char *dio_io_ddr8;
volatile unsigned char *dio_io_bit8;

/* LCD port stuff.....
 * The enable, w/r, RS bits are bits 3-5 of porth
 */

volatile unsigned char *lcd_io_byte;
volatile unsigned char *lcd_io_ddr;

/* SysCon register pointers.....
 */

volatile unsigned int *sysconp;
volatile unsigned int *pwrcntp;
volatile unsigned int *clkset1p;
volatile unsigned int *clkset2p;
volatile unsigned int *devicecfgp;
volatile unsigned int *chipidp;
volatile unsigned int *syscfgp;

extern int debug;

/***DECLARATIONS***/
/* declare non-int functions 
 */

unsigned char dio_get_ddr(),dio_set_ddr(),dio_get_byte(),dio_set_byte();
  

/* ====================== General Routines ======================= */
/* getbiobit(ptr,n)  read the level of bit n in byte from char *ptr
 *                return 0 or 1, or -1 on error
 */

getbiobit(ptr,n)
unsigned char *ptr;
int n;
{
  int d;

  if (n>7 || n<0) return(-1);
  return(((*ptr)>>n)&1);
}

/* setbiobit(ptr,n,v) set bit n to value v in byte at char *ptr
 *                 return v, or -1 on error.
 * This does a read-modify-write sequence, so only the bit 
 * specified is altered. 
 */

setbiobit(ptr,n,v) 
unsigned char *ptr;
int n,v;
{
  unsigned char d;
  
  if (n>7 || n<0 || v>1 || v<0) return(-1);
  d=*ptr;
  d= (d&(~(1<<n))) | (v<<n);
  *ptr=d;
  return(v);
}

/* getwiobit(ptr,n)  read the level of bit n in word from int *ptr
 *                return 0 or 1, or -1 on error
 */

getwiobit(ptr,n)
unsigned int *ptr;
int n;
{
  if (n>31 || n<0) return(-1);
  return(((*ptr)>>n)&1);
}

/* setwiobit(ptr,n,v) set bit n to value v in word at int *ptr
 *                 return v, or -1 on error.
 * This does a read-modify-write sequence, so only the bit 
 * specified is altered. 
 */

setwiobit(ptr,n,v) 
unsigned int *ptr;
int n,v;
{
  unsigned int d;
  
  if (n>31 || n<0 || v>1 || v<0) return(-1);
  d=*ptr;
  d= (d&(~(1<<n))) | (v<<n);
  *ptr=d;
  return(v);
}

/* ====================== ADC routines ============================ */

/* adc_init   checks that the hardware is there
 *            then sets up some pointers for subsequent use
 *            returns 0 if all ok else returns ENXIO
 */

adc_init()
{
  if (iofd==-1) {
    iofd=open("/dev/mem", O_SYNC | O_RDWR);
    if (iofd==-1) { return(errno); }
  }
  adc_io_cntl=(unsigned char *)IOMMAP(0x22400000);
  if ( ! (*adc_io_cntl & 1) ) { return(ENXIO); }

  adc_io_cntl=(unsigned char *)IOMMAP(0x10c00000);
  adc_io_stat=(unsigned short *)IOMMAP(0x10800000);
  adc_io_read=(unsigned short *)IOMMAP(0x10c00000);

  return(0);
}

/* adc_read(c,r) tell ADC to start a measurent on channel c range r
 *               and wait for it to complete then return answer
 *
 * on error return a value > 65536. Error will be returned value >> 16
 * and also in global variable adcerror
 */

int adc_read(c,r)
int c,r;
{
  int i;
  unsigned char s;
  unsigned short ss;
  short sv;

  s=(r<<3)+c+0x40;
  *adc_io_cntl=s;
  for (i=ADC_LOOP_LIMIT; i && ( *adc_io_stat & 0x80 ); i-- ) {
#ifdef ADC_SLEEP
    usleep(0);
#endif
  } 
  if (i==0) { return(adcerror=(ETIME<<16)); }
  sv=*adc_io_read;
  return((int)sv);
}

/* readvolt(c,r)   get a reading for channel c with range r
 *              and convert the reading to voltage depending on the 
 *              range selected. Return millivolts.
 */

readvolt(c,r)
int c,r;
{
  int v;
  int a;

  v=adc_read(c,r);
  if (v>(1<<16)) return(v);
  switch (r) {
   case 0: a=((v*50000/4096)+5)/10; break;
   case 1: a=((v*100000/4096)+5)/10; break;
   case 2: a=((v*100000/4096)+5)/10; break;
   case 3: a=((v*200000/4096)+5)/10; break;
   otherwise: a=(ECHRNG<<16);
  }
  return(a);
}

/* ======================= DIO routines =========================*/
/* dio_init   sets up some pointers for subsequent use
 *            returns 0 if all ok else returns an error number
 */

dio_init()
{
  return(gpio_init());
}

/* dio_get_ddr()   return the ddr for lines 0-7 on DIO interface
 */

unsigned char dio_get_ddr()
{
  return(*dio_io_ddr);
}

/* dio_set_ddr(b)  set the ddr for  lines 0-7 on DIO interface
 */

unsigned char dio_set_ddr(b)
unsigned char b;
{
  *dio_io_ddr=b;
  return(b);
}

/* dio_get_byte()   return data from lines 0-7 on DIO interface
 */

unsigned char dio_get_byte()
{
  return(*dio_io_byte);
}

/* dio_set_byte(b)  set the data on lines 0-7 on DIO interface
 */

unsigned char dio_set_byte(b)
unsigned char b;
{
  *dio_io_byte=b;
  return(b);
}

/* dio_get_ddr8()   return the ddr for line 8 on DIO header
 */

unsigned char dio_get_ddr8()
{
  return((*dio_io_ddr8)&1);
}

/* dio_get_bit8()   return data from line 8 on DIO interface
 */

unsigned char dio_get_bit8()
{
  return((*dio_io_bit8)&1);
}

/* =================
 *
 * DIO routines that deal with each DIO line individually
 * the DIO lines are numbered 0-8 and are the lines brought out to the
 * TS7200 DIO header.
 */

/* getdioline(n)  read the level of the DIO Line n
 *                return 0 or 1, or -1 on error
 */

getdioline(n)
int n;
{
  int d;

  if (n>8 || n<0) return(-1);
  if (n==8) return((*dio_io_bit8)&1);
  return((*dio_io_byte>>n)&1);
}

/* setdioline(n,v) set DIO Line n to value v
 *                 return v, or -1 on error.
 * This does a read-modify-write sequence, so only the line 
 * specified is altered. If the line is not set as output in the DDR
 * then this routine sets it as output
 */

setdioline(n,v)
int n,v;
{
  unsigned char d;
  
  if (n>8 || n<0 || v>1 || v<0) return(-1);
  if (n==8) {
    d=(*dio_io_ddr8)&1;
    if (!d) *dio_io_ddr8=d|1;
    d=*dio_io_bit8 & 0xFE;
    *dio_io_bit8=d|v;
  } else {
    d=*dio_io_ddr;
    if ( ! (d&(1<<n)) ) {
      d|=(1<<n);
      *dio_io_ddr=d;
    }
    d=*dio_io_byte;
    d= (d&(~(1<<n))) | (v<<n);
    *dio_io_byte=d;
  }
  return(v);
}

/* getddrline(n)  read the DDR of the DIO Line n
 *                return 0 or 1, or -1 on error
 */

getddrline(n)
int n;
{
  int d;

  if (n>8 || n<0) return(-1);
  if (n==8) return(dio_get_ddr8());
  return((dio_get_ddr()>>n)&1);
}

/* setddrline(n,v) set DDR for line n to value v
 *                 return v, or -1 on error.
 * This does a read-modify-write sequence, so only the line 
 * specified is altered. 
 */

setddrline(n,v)
int n,v;
{
  unsigned char d;
  
  if (n>8 || n<0 || v>1 || v<0) return(-1);
  if (n==8) {
    dio_set_ddr8(v);
  } else {
    d=dio_get_ddr();
    d= (d&(~(1<<n))) | (v<<n);
    dio_set_ddr(d);
  }
  return(v);
}

/* ======================= LCD routines =========================*/
/* lcd_init   sets up some pointers for subsequent use
 *            then initialises the HS44780 based LCD device
 *      (the initialise code much based on Technologics lcdmesg.c)
 *            returns 0 if all ok else returns an error number
 *
 * Info:  porth bit 3 (0x08)  ENable bit active high
 *        porth bit 4 (0x10)  RS bit     high data / low command 
 *        porth bit 5 (0x20)  WRite bit  low  data -> LCD
 *                                       high data <- LCD
 */

/* some values taken from Technologics lcdmesg.c for use with
 * the COUNTDOWN macro for controlling read/writes to the LCD
 */

#define LCD_SETUP (15)
#define LCD_PULSE (36)
#define LCD_HOLD  (22)

lcd_init()
{
  unsigned int ddr;
  int st;

  st=gpio_init();
  if (st==0) {     
    *porth_ddr |= 0x38;   /* set the LCD control bits 3,4,5 as o/p */
    *porth_byte &= ~0x18; /* clear EN & RS */
    *lcd_io_ddr=0;        /* LCD data port set as inputs */

    usleep(15000);
    lcd_cmd(0x38);    /* set 8bit mode, 2 lines, and 5x7 chars */
    usleep(10000);
    lcd_cmd(0x0C);    /* set display on, cursor off, blinking off */
    usleep(10000);
    lcd_cmd(0x01);    /* clear display */
    usleep(10000);
    lcd_cmd(0x06);    /* increment mode, entire shift off */
  }
  return(st);
} 

/* lcd_cmd(c)
 */

lcd_cmd(c)
unsigned char c;
{
  int i;

  lcd_wait();            /* wait till LCD is NOT busy */
  *lcd_io_ddr=0xFF;      /* set data to outputs */
  *lcd_io_byte=c;        /* set data lines to value */
  *porth_byte &= ~0x30;  /* clear RS and WR to 0 */
  i=LCD_SETUP; COUNTDOWN(i);
  *porth_byte |= 0x08;   /* set EN */
  i=LCD_PULSE; COUNTDOWN(i);
  *porth_byte &= ~0x08;  /* unset EN */
  i=LCD_HOLD; COUNTDOWN(i);
}

/* lcd_put(c)
 */

lcd_put(c)
unsigned char c;
{
  int i;

  lcd_wait();            /* wait till LCD is NOT busy */
  *lcd_io_ddr=0xFF;      /* set data to outputs */
  *lcd_io_byte=c;        /* set data lines to value */
  *porth_byte &= ~0x20;  /* clear WR to 0 */
  *porth_byte |= 0x10;   /* set RS to 1 for data */
  i=LCD_SETUP; COUNTDOWN(i);
  *porth_byte |= 0x08;   /* set EN */
  i=LCD_PULSE; COUNTDOWN(i);
  *porth_byte &= ~0x08;  /* unset EN */
  i=LCD_HOLD; COUNTDOWN(i);
}

/* lcd_wait()
 * return TRUE if LCD is budy despite waiting. 
 */

lcd_wait()
{
  int i,n;
  unsigned char c=0x80;

  *lcd_io_ddr=0x00;      /* set data to inputs */
 
  for ( c=0x80, n=1024; c&0x80 || n; n--) {
    *porth_byte &= ~0x18;  /* clear EN and RS to 0 */
    *porth_byte |= 0x20;   /* set W/R to 1 */
    i=LCD_SETUP; COUNTDOWN(i);
    *porth_byte |= 0x08;   /* set EN */
    i=LCD_PULSE; COUNTDOWN(i);
    c=*lcd_io_byte;        /* read value from LCD */
    *porth_byte &= ~0x08;  /* unset EN */
    i=LCD_HOLD; COUNTDOWN(i);
  }
  return((c&0x80)==0x80);
}

/* ======================= GPIO routines========================= */
/* gpio_init   sets up some pointers for use in accessing the
 *             the EP930X gpio registers.
 *             These pointers are used by other gpio_X functions below
 *            returns 0 if all ok else returns an error number
 */

gpio_init()
{
  if (iofd==-1) {
    iofd=open("/dev/mem", O_SYNC | O_RDWR);
    if (iofd==-1) { return(errno); }
  }

  gpiopage=(unsigned char *)IOMMAP(0x80840000);
  dio_io_ddr=gpiopage+0x14;
  dio_io_byte=gpiopage+0x04;
  dio_io_ddr8=gpiopage+0x34;
  dio_io_bit8=gpiopage+0x30;
  porth_ddr=gpiopage+0x44;
  porth_byte=gpiopage+0x40;
  lcd_io_byte=gpiopage;
  lcd_io_ddr=gpiopage+0x10;

  return(0);
}

/* gpio_get_byte()   return data from lines 0-7 on DIO interface
 */
/*
unsigned char gpio_get_byte()
{
  return(*dio_io_byte);
}
*/
/* dio_set_byte(b)  set the data on lines 0-7 on DIO interface
 */
/*
unsigned char gpio_set_byte(b)
unsigned char b;
{
  *gpio_io_byte=b;
  return(b);
}
*/

/* getetherpwr()  read the TS7200 ether PHY power status
 *                Port H bit 2
 */

getetherpwr() 
{
  return(getbiobit(porth_byte,2));
}

/* setetherpwr(v) set the TS7200 ether PHY power to off (v==0) or on (v==1)
 *                Port H bit 2
 *                don't upset other bits.
 * first check that port H bit 2 is set to an output line
 * if not then that an problem.
 * Then set the value of port H bit 2
 */

setetherpwr(v)  
int v;
{
  if (v<0 || v>1 || getbiobit(porth_ddr,2)!=1) return(-1);
  return(setbiobit(porth_byte,2,v));
}

/* ======================= SYSCON routines ========================= */
/* syscon_init   sets up some pointers for use in accessing the
 *             the EP930X SYSCON registers.
 *             These pointers are used by other functions below
 *            returns 0 if all ok else returns an error number
 */

syscon_init()
{
  if (iofd==-1) {
    iofd=open("/dev/mem", O_SYNC | O_RDWR);
    if (iofd==-1) { return(errno); }
  }

  sysconp=(unsigned int *)IOMMAP(0x80930000);
  pwrcntp=sysconp+1;
  clkset1p=sysconp+8;
  clkset2p=sysconp+9;
  devicecfgp=sysconp+0x20;
  chipidp=sysconp+0x25;
  syscfgp=sysconp+0x27;

  return(0);
}

/* syscon_get_pwrsts()   return the PwrSts word
 */

unsigned int syscon_get_pwrsts()
{
  return(*sysconp);
}

unsigned int syscon_get_pwrcnt()
{
  return(*pwrcntp);
}

unsigned int syscon_get_clkset1()
{
  return(*clkset1p);
}

unsigned int syscon_set_clkset1(v) 
unsigned int v;
{
  *clkset1p=v;
  nop(); nop(); nop(); nop(); nop(); nop(); nop(); nop();
  return(v);
}

unsigned int syscon_get_clkset2()
{
  return(*clkset2p);
}

unsigned int syscon_set_clkset2(v) 
unsigned int v;
{
  *clkset2p=v;
  /* really need to loop waiting for pll2 to stabilise here */
  nop(); nop(); nop(); nop(); nop(); nop(); nop(); nop();
  return(v);
}

unsigned int syscon_get_chipid()
{
  return(*chipidp);
}

unsigned int syscon_get_syscfg()
{
  return(*syscfgp);
}

/* getusbpwr()  return the value of bit 28 in the pwrcnt word.
 *              this indicates if the USB part of the core is being clocked.
 */

getusbpwr() 
{
  return(getwiobit(pwrcntp,28));
}

/* getirdapwr()  return the value of bit 31 in the pwrcnt word.
 *              this indicates if the irda part of the core isbeing clocked.
 */

getirdapwr() 
{
  return(getwiobit(pwrcntp,31));
}

/* getuartbaudhilo()  return the value of bit 29 in the pwrcnt word.
 *              this indicates if the UART clocks are driven from
 *              14.7468Mhz xtal direct (1) or div by 2 (0)
 */

getuartbaudhilo() 
{
  return(getwiobit(pwrcntp,29));
}

/* getusbpwr(v)  set the value of bit 28 in the pwrcnt word.
 *              this sets whether the USB part of the core is being clocked.
 */

setusbpwr(v) 
int v; 
{
  return(setwiobit(pwrcntp,28,v));
}

/* setirdapwr(v)  set the value of bit 31 in the pwrcnt word.
 *              this sets whether the irda part of the core is being clocked.
 */

setirdapwr(v) 
int v;
{
  return(setwiobit(pwrcntp,31,v));
}

/* setuartbaudhilo(v)  set the value of bit 29 in the pwrcnt word.
 *              this indicates if the UART clocks are driven from
 *              14.7468Mhz xtal direct (1) or div by 2 (0)
 */

setuartbaudhilo(v) 
int v;
{
  return(setwiobit(pwrcntp,29,v));
}

/* ============================ END ============================ */
