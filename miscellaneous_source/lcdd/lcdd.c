/* lcdd.c 
 * Simple userspace daemon which acts as a driver for HD44780
 * based char LCDs parallel connected to the "LCD" port of the TS72x0
 * board.
 *
 * This version supports ANSI control sequences
 * 
 * Jim Jackson
 */
/*
 * Copyright (C) 2005 Jim Jackson           jj@franjam.org.uk
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
 *  Date     Vers    Comment
 *  -----    -----   ---------------
 *  30Mar05  0.1     Initial Program Built from adio.c
 *  10Jan07  0.3ansi Folded in Patches from Yan Seiner <yan@seiner.com>
 *                   to support ANSI Control sequences
 *  12Jan07  0.4ansi Much debugging/fixing and finishing
 *                   fixed horrendous JJ bug decimal 40 should hex 40!!!
 */

/***MANPAGE***/
/*
lcdd: (version 0.4ansi) 12Jan07

see the file lcdd.txt

 */

/***INCLUDES***/
/* includes files 
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
/* for SYSV variants .... */
#include <string.h>
//#include <strings.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>
#include <math.h>
#include <sys/time.h>
#include "ts7200io.h"

/***DEFINES***/
/* defines
 */

#define VERSION "0.4ANSI"

#define PVERSION(A) fprintf(stderr,"%s version %s\n",sys,A)

#define DEF_CONFIGFILE "/etc/lcdd.conf"

#define MAXLCDDEFS (12)

#define NAMED_PIPE "/dev/lcd"

#define PIPE_MODE (0622)

#define LINE_PAUSE (1)

#define LCDBUFSIZE (128)

#define DEF_COLS (24)

#define DEF_LINES (2)

#define EFAIL -1

#define TRUE 1
#define FALSE 0
#define nl puts("")

#define chkword(a,b) ((n>=a)&&(strncmp(p,b,n)==0))
#define index(A,B) strchr(A,B)
#define rindex(A,B) strrchr(A,B)

#define EMODE_LR 0
#define EMODE_RR 1
#define EMODE_RN 2
#define EMODE_LN 3

/***GLOBALS***/
/* global variables
 */

char *sys;
int donl;                        /* 0 if last char output was '\n' else 1 */
int vflg;                        /* set if -v flag used        */
int timestamp;                   /* -t add timestamp to each line */
int linepause;
int maxwaitime;
char *configfile;

char *pipename;
char *geom;
extern int Lines;                 /* default LCD size */
extern int Cols;
int LCD;

struct timeval start,tm,tmlast;
struct timezone tz;

int cmdmode=FALSE;
int cmdlen=0;
int linewrap=TRUE;
int fontoffset=0;
int emode=EMODE_RN;

char cmdbuf[512];

/* LCD struct for details of whole lcd or for portions
 */

struct LCDdef {
  char *dev;
  int fd;
  int startx;
  int starty;
  int cols;
  int rows;
};

int numLCDS=0;
struct LCDdef *LCDS[MAXLCDDEFS];

int lcdbufsize=LCDBUFSIZE;
char lcdbuf[LCDBUFSIZE];
extern int lcdcurs;
int lcdnlpending=0;

/***DECLARATIONS***/
/* declare non-int functions 
 */

char *delnl(),*strupper(),*reads(),*sindex(),
  *sindex_nc(),*tab(),*ctime(),*getcmdline();
FILE *fopen(),*mfopen();

/***MAIN***/
/* main - for program  lcdd
 */

main(argc,argv)
int argc;
char **argv;

{ int i,j,n,t,st;
 FILE *f; 
  char *p;
  char buf[130];

  argv[argc]=NULL;
  sys=*argv++; argc--;
  if ((p=rindex(sys,'/'))!=NULL) { sys=p+1; }

  vflg=FALSE;
  LCD=-1;
  geom=NULL;
  Cols=DEF_COLS;
  Lines=DEF_LINES;
  linepause=LINE_PAUSE;
  donl=1;
  configfile="";
  
  /* put global and local var. init. here */

  while (argc && **argv=='-') {          /* all flags and options must come */
    n=strlen(p=(*argv++)+1); argc--;     /* before parameters                */
    if (chkword(1,"size")) {             /* -size option              */
      if (argc && (**argv!='-') && isdigit(**argv)) { 
	geom=*argv++; argc--;
      }
    } else if (chkword(1,"pause")) {          /* pause between lines */
      if (argc && (**argv!='-') && isdigit(**argv)) {
        linepause=atoi(*argv++); argc--;
      }
    } else if (chkword(1,"config")) {          /* name of config file */
      if (argc && (**argv!='-')) {
        configfile=(*argv++); argc--;
      }
    } else {                             /* check for single char. flags    */
      for (; *p; p++) {
	if (*p=='h') exit(help(EFAIL));
	else if (*p=='v') vflg=TRUE;
	else {
          *buf='-'; *(buf+1)=*p; *(buf+2)=0;
          exit(help(err_rpt(EINVAL,buf)));
	}
      }
    }
  }
  
  if ( argc>1 ) {
    exit(err_rpt(EINVAL,"Too many command line parameters."));
  }

  if (argc) {
    pipename=*argv++; argc--;
  } else {
    pipename=NAMED_PIPE;
  }

  if (*configfile) {
    if ((f=fopen(configfile,"r"))==NULL) { exit(err_rpt(errno,configfile)); }
    if (readconfig(f)) { exit(err_rpt(errno,configfile)); }
    fclose(f);
  } else if ((f=fopen(DEF_CONFIGFILE,"r"))!=NULL) {
    if (readconfig(f)) { exit(err_rpt(errno,configfile)); }
    fclose(f);
  }

  if ( geom != NULL ) {
    setgeom(geom,&Cols,&Lines);
  }
  
  if (vflg) {
    fprintf(stderr,"%d Cols by %d Lines LCD on named pipe %s\n",
		    Cols,Lines,pipename);
  }

  if ((LCD=open(pipename,O_RDONLY|O_NONBLOCK,PIPE_MODE))==-1) {
    if (mkfifo(pipename,PIPE_MODE)) {
      exit(err_rpt(errno,pipename));
    }
    if ((LCD=open(pipename,O_RDONLY|O_NONBLOCK,PIPE_MODE))==-1) {
      exit(err_rpt(errno,pipename));
    }
  }

  if (!isfifo(LCD)) {
    sprintf(buf,"\"%s\" is not a named pipe.",pipename);
    exit(err_rpt(EINVAL,buf));
  }

  if (fchmod(LCD,PIPE_MODE)) {
    sprintf(buf,"changing permissions of \"%s\"",pipename);
    err_rpt(errno,buf);
  }
      
  if (vflg) {
    PVERSION(VERSION);
    fprintf(stderr,"Named pipe is is \"%s\"\n",pipename);
  }
  fflush(stderr);
  
  gettimeofday(&start, &tz);
  tmlast=start;

  if (!vflg && daemon(FALSE,FALSE)<0) {
    exit(err_rpt(errno,"Trying to become a daemon."));
  }

  if (st=dolcd()) {
    err_rpt(st,"Accessing the LCD device.");
  }

  doexit(st);
}

/* doexit(st)  quit the program with exit status st
 */

doexit(st) 
int st;
{
  if (donl) {
    st=st;       /* put in LF to LCD here ****/
  }
  exit(st);
}

/* dolcd - main loop
 *         loop on read named pipe open on LCD, writing chars 
 *         read to the LCD device.
 *
 * This is going to end up as sort of terminal emulator.
 * HD44780 based LCDs are mainly either 1 or 2 line displays
 * with 8, 16, 20, 32 or 40 chars per line. 
 * The displays have memory locations 0x0-0x27 for line 1, 0x40-0x67 for line 2
 *
 * Basic operation is ascii 0x20 - 0x7E are display chars and are written
 * sequentially along the current line until getting to last position
 * on the line, where they either stick (nowrap mode) or start on next line 
 * (wrap mode). At end of second line in scroll mode, the top line is lost,
 * the 2nd line is copied to the first line, and new line empty 2nd line 
 * is started. Or in noscroll mode chars just keep overwiritng the last
 * position on screen.
 * * Control Char functions:
 * 
 *  13 0x0D \r  -  CR cursor moves to col 0 on current line
 *  10 0x0A \n  -  LF move down a line to same col on line below
 *                 if in scroll mode, and already on second line, move 
 *                 line 2 to line 1 and clear second line
 *                  LF implementation is flagged and action delayed till 
 *                  the first char of the next line. Otherwise the
 *                  last line of the display is always blank - as most apps
 *                  send each line LF terminated!
 *  12 0x0C \f  -  FF move cursor to start of line 1 and clear all data 
 *                 to spaces
 *   8 0x08 \b  -  BS move cursor back one position, unless at start of line
 *  
 * this needs some research - do I really want to write a an ANSI driver????
 *
 * Well Yan Seiner did. 
 *
 * ANSI command stuff courtesy of Yan - bugs an' all :-)
 */

dolcd()
{
  char *p,c,buf[2];
  int i,n;

  if (n=lcd_init()) { return(n); }

  for (i=0; i<lcdbufsize; i++) { lcdbuf[i]=' '; }
  lcd_cmd(1);
  lcdcurs=0;
  if (vflg) {
    sprintf(lcdbuf,"%s v%s",sys,VERSION);
    fprintf(stderr,"%-*s\n",Cols,lcdbuf);
    lcd_puts(lcdbuf,Cols); 
  }
  lcd_cr();

  for (p=buf;;) {
    if ((n=read(LCD,p,1))>0) {
      if (vflg) { 
	fprintf(stderr,"--> 0x%02x \n",*p);
      }

      if ( cmdmode ) {
	cmdbuf[cmdlen++] = *p;
	cmdbuf[cmdlen] = '\0';
	if ( ! isdigit(*p) && *p!=';' && *p!='[' ) { 
	  do_ansi();
	  cmdmode=FALSE; cmdlen=0;
	} else if (cmdlen > 510) {
	  cmdmode = FALSE;
	  cmdlen = 0;
	}
      } else {
	switch (*p) {
	case 0x9B: // CSI - 8 bit replacement for ESC [
	  cmdmode = TRUE;
	  cmdbuf[0] = '\e';
	  cmdbuf[1] = 0;
	  cmdlen = 1;
	  break;
	case '\e': // command mode
	  cmdmode = TRUE;
	  cmdbuf[0] = '\0';
	  cmdlen = 0;
	  break;
	case '\n':
	  lcdnlpending++;
	  break;
	case '\r':
	  for ( ; lcdnlpending; lcdnlpending--) { lcd_nl(); }
	  lcd_cr();
	  break;
	case 12:
	  lcd_ff();
	  lcdnlpending=0;
	  break;
	case 8:
	  for ( ; lcdnlpending; lcdnlpending--) { lcd_nl(); }
	  lcd_bs();
	  break;
	default:
	  for ( ; lcdnlpending; lcdnlpending--) { lcd_nl(); }
	  if (lcdcurs==Cols || lcdcurs==0x40+Cols) { lcd_nl(); }
	  lcd_put(*p); lcdbuf[lcdcurs++]=*p;
	}
      }
    } else {
      if (n<0 && errno!=EAGAIN) return(errno); 
      if (n<0 && vflg) err_rpt(errno,pipename);
      pausems(10);
    }
  }
  return(ENOMSG);
}

do_ansi()
{
   /* ANSI sequence decoding. 
    * Only a subset of the ANSI sequences are decoded here.
    * If it's not here then send JJ a patch or a request for that ANSI/VT
    * sequence to implemented. 
    *
    * fixed length commands
    * Enable Line Wrap		[7h            [?7h
    * Disable Line Wrap		[7l            [?7i
    *
    * Font Set G0		(
    * Font Set G1		)
    *
    * Scroll Down		D
    * Scroll Up			M
    *
    * Erase to end of screen    [0J  or [J
    * Erase cursor to start     [1J
    * Erase Screen		[2J
    *
    * Erase to End of Line	[0K  or [K
    * Erase cursor to start     [1K
    * Erase Line                [2K
    *
    * non-standard codes for entry mode
    *
    * Left Reverse		[4E
    * Right Reverse		[5E
    * Right Normal		[6E
    * Left Normal		[7E
    * 
    * underline cursor		[X
    * block cursor		[Y
    * hide cursor		[Z
    *
    * Variable length commands
    *
    * Cursor Position Set	[%d;%dH
    * Cursor Up			[%dA
    * Cursor Down		[%dB
    * Cursor Right		[%dC
    * Cursor Left		[%dD
    */

   int l, r, c;
   char *p,*s,cmd;

   p=cmdbuf;
   l = strlen(cmdbuf) - 1; cmd=cmdbuf[l];
   if (vflg) {
     fprintf(stderr,"Curs %d ANSI cmd \"%c\" seq %d bytes 0x1b, 0x%02.2x",
	     lcdcurs,cmd,l+2,*p);
     for (r=1; r<=l; r++) fprintf(stderr," , 0x%02.2x",cmdbuf[r]);
     fprintf(stderr,"\n");
   }
   switch(*p++) {
   case ')':
	fontoffset = 0;
	break;
   case '(':
	fontoffset = 0x40;
	break;
   case 'D':
	lcd_nl();
	break;
   case 'M':
	lcd_su();
	break;
   case '[':
        switch (*p) {
	case '?':
	  c=strtol(p,&s,10);
	  if ( *s=='h' ) { r=TRUE; }
	  else if ( *s=='l' ) { r=FALSE; } 
	  else { break; }
	  
	  switch (c) {
	  case 7:
	    linewrap=r;
	    break;
	  }
	  break;
	}
	cmd=cmdbuf[l];
        switch (cmd) {
	case 'h':   //???? linewrap ANSI is ESC [ ? 7 h surely
	  if(*p == '7') linewrap = TRUE;
	  break;
	case 'l':
	  if(*p == '7') linewrap = FALSE;
	  break;
	case 'J':
	  switch (*p) {
	  case '2':
	    lcd_erase(); // erase all screen
	    break;
	  case 'J':  // if no number default is '0'
	  case '0':
	    // erase from cursor to end of screen
	    lcd_c2eos();
	    break;
	  case '1':
	    // erase from cursor to start of screen
	    lcd_sos2c();
	    break;
	  }
	  break;
	case 'K':
	  switch (*p) {
	  case '2':
	    lcd_eln(); // erase all line
	    break;
	  case 'K':  // if no number default is '0'
	  case '0':
	    // erase from cursor to end of line
	    lcd_eeol();
	    break;
	  case '1':
	    // erase from cursor to start of line
	    lcd_esol();
	    break;
	  }
	  break;
	case 'E':    // non-standard extra FUNCTION to set the LCD
                     // entry mode
	  c=*p - '0';
	  if ( c >= '4' && c <= '7') { lcd_cmd(c); }
	  break;
	case 'A':
	case 'B':
	case 'C':
	case 'D':
	  c=strtol(p,&s,10);
	  if (c==0) c=1;
	  if (*s == cmd) {
	    switch (cmd) {
	    case 'A':
	      lcd_cup(c); break;
	    case 'B':
	      lcd_cdn(c); break;
	    case 'C':
	      lcd_crt(c); break;
	    case 'D':
	      lcd_clt(c); break;
	    }
	  }
	  break;
	case 'X':                  // underline cursor
	  lcd_cmd(0x0e);
	  break;
	case 'Y':                  // block cursor
	  lcd_cmd(0x0f);
	  break;	
	case 'Z':                  // hide cursor
	  lcd_cmd(0x0c);
	  break;
	case 'H':           // Cursor positioning - in general CSIr;cH
                            // r and c are ascii strings for row and 
	                    // column number (counting from 1 - hence need 
	                    // to -1 before passing to lcd_cpos
	                    // if r and/or c are missing their default is 1
	  c=1;
	  r=strtol(p,&s,10);
	  if ( *s == ';' ) { p=++s; c = strtol(p,&s,10); }
	  if (r<=0) r=1;
	  if (c<=0) c=1;
	  if (vflg) { 
	    fprintf(stderr,"handling cur pos: row %d, col %d\n",r,c);
	  }
		if(c <= 0x40 && r <= 2) lcd_cpos(c-1,r-1);
		break;
	}
   }	
}

lcd_puts(s,n)
char *s;
int n;
{
  int i;

  for (i=0; i<n && s[i] ; i++) { lcd_put(s[i]); }
}

/* pausems(t) wait for a number of millisecs specified in string s
 */

pausems(t)
unsigned int t;
{
  usleep(1000*t);
}

/* readconfig(f)   read config file f
 *
 * ************ TODO *************************
 */

readconfig(f)
FILE *f;
{
  return(0);
}

mklcd(name,sx,sy,x,y)
char *name;
int sx,sy,x,y;
{
  int n;
  struct LCDdef *l;
  char *p;

  if (name==NULL || *name==0 || (x*y)==0) {
    return(err_rpt(EINVAL,"Bad parameter for LCD device"));
  }
  if (numLCDS>=MAXLCDDEFS) {
    return(err_rpt(EINVAL,"Too many LCD Devices"));
  }
  if (numLCDS && ((sx+x)>LCDS[0]->cols || (sy+y)>LCDS[0]->rows)) {
    return(err_rpt(EINVAL,"Bad parameter for LCD device"));
  }
  n=sizeof(struct LCDdef) + strlen(name) + 2;
  if ((l=(struct LCDdef *)malloc(n))==NULL) { 
    return(err_rpt(ENOSPC,"No Memory"));
  }
  p=(char *)l+(sizeof(struct LCDdef));
  strcpy(name,p);
  l->dev=p;
  l->startx=sx;
  l->starty=sy;
  l->cols=x;
  l->rows=y;
  LCDS[numLCDS++]=l;
  return(0);  
}

/* isfifo(fd) check if fd is a FIFO - return true/false
 */

isfifo(fd)
int fd;
{
  struct stat stats;
  mode_t m;

  if ((fd<0) || fstat(fd,&stats)) {
    return(FALSE);
  }
  m=stats.st_mode;
  return(S_ISFIFO(m));
}

/* setgeom(g,cp,lp)  decode geometry string g into number of Cols
 *                   and number of Lines and set *cp and *lp
 */

setgeom(g,cp,lp)
char *g;
int *cp,*lp;
{
  char *p;
  int c,l;
  
  c=strtol(g,&p,10);
  if (*p != 'x' && *p != 'X') { return; }
  l=strtol(p,&g,10);
  if (*g) { return; }
  *cp=c; *lp=l;
  return;
}

/***HELP***/
/* help(e)  print brief help message - return parameter given
 */

help(e)
int e;
{
  PVERSION(VERSION);
  puts("\n\
lcdd -  Simple userspace daemon which acts as a driver for\n\
        HD44780 based char LCDs parallel connected to the\n\
        LCD port of the TS72x0 board.\n\
");
  printf("\
Usage:   %s [-v] [-s COLSxLINES] [-p millisecs] [named_pipe]\n\n\
",sys);
  return(e);
}

/***ERR_RPT***/
/* err_rpt(err,msg)
 */

err_rpt(err,msg)
short int err;
char *msg;

{ 
  if (err) fprintf(stderr,"[%s] (%d) %s : %s\n",sys,err,strerror(err),msg);
  return(err);
}

