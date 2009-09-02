/* lcdfunc_ansi.c
 *
 * LCD functions to support ANSI control sequences
 *
 * Yan Seiner
 */

#include <stdio.h>
#include <stdlib.h>

#define lcd_setcurs(C) if (vflg) { \
    fprintf(stderr,"set LCDcursor to %d\n",C);\
  } \
  lcd_cmd(0x80 + C)

int lcdcurs;
extern char lcdbuf[];
extern int lcdbufsize;
int Cols,Lines;

int linepause;
extern int vflg;

/* lcd_su() scroll screen up
 */

lcd_su()
{
  int i;

  lcd_setcurs(0);
  lcd_puts(lcdbuf+0x40,Cols); /* write line1 -> line0 */
  for (i=0; i<Cols; i++) { lcdbuf[i]=lcdbuf[i+0x40]; lcdbuf[i+0x40]=' '; }  
  lcd_setcurs(0x40);  // curs to zero
  lcd_puts(lcdbuf+0x40,Cols); // clear line1
  lcd_setcurs(lcdcurs); // restore cursor position
}

lcd_eln()  // erase all of current line leaving cursor intact
{
  int i,n;

  i=lcdcurs&0x40;
  n=i+Cols;
  lcd_setcurs(i);
  for ( ; i<n ; i++ ) { lcdbuf[i]=' ' ; lcd_put(' '); }
  lcd_setcurs(lcdcurs); // restore cursor position
}

lcd_eeol() // erase from cursor to end of line
{
  int i,n;

  n=(lcdcurs&0x40)+Cols;
  lcd_setcurs(lcdcurs); // restore cursor
  for(i = lcdcurs; i < n ; i++) { lcdbuf[i]=' '; lcd_put(' '); }
  lcd_setcurs(lcdcurs); // restore cursor
}

lcd_esol() // erase from start of line to cursor
{
  int i;

  i=lcdcurs&0x40;
  lcd_setcurs(i);
  for (  ; i<lcdcurs ; i++) { 
    lcdbuf[i]=' '; lcd_put(' '); 
  }
  lcd_setcurs(lcdcurs); // restore cursor
}

lcd_erase() // erase all screen, cursor unchanged
{
   int i;

   lcd_cmd(0x01);
   for ( i = 0; i < lcdbufsize; i++) { lcdbuf[i]=' '; }
   lcd_setcurs(lcdcurs); // restore cursor
}

lcd_c2eos() // erase from cursor to end of screen, cursor unchanged
{
  int i;

  lcd_eeol();  // erase to end of current line
  if (!(lcdcurs&0x40)) {  //  if one first line
    i=lcdcurs;
    lcdcurs=0x40; lcd_setcurs(0x40); lcd_eeol(); // erase 2nd line
    lcdcurs=i;
  }
  lcd_setcurs(lcdcurs); // restore cursor
}
 
lcd_sos2c() // erase from start of screen to cursor, cursor unchanged
{
  int i;

  lcd_esol();  // erase to start of current line
  if (lcdcurs&0x40) {   // if on second line
    i=lcdcurs;
    lcdcurs=0; lcd_setcurs(0); lcd_eeol(); // erase 1nd line
    lcdcurs=i;
  }
  lcd_setcurs(lcdcurs); // restore cursor
}

lcd_cup(r)
int r;
{
  if (r) {
    if(lcdcurs >= 0x40) {
      lcdcurs -= 0x40;
      lcd_setcurs(lcdcurs);
    }
  }
}

lcd_cdn(r)
int r;
{
  if (r) {
    if(lcdcurs < 0x40) {
      lcdcurs += 0x40;
      lcd_setcurs(lcdcurs);
    }
  }
}

lcd_crt(c)
int c;
{
  for ( ; c>0; c--) {
    if (lcdcurs<(0x40+Cols-1)) {
      lcdcurs++;
      if (lcdcurs >= Cols) { lcdcurs=0x40; }
      lcd_setcurs(lcdcurs);
    }	
  }
}

lcd_clt(c)
int c;
{
  for ( ; c>0; c--) {
    if (lcdcurs) {
      lcdcurs--;
      if ( lcdcurs < 0x40 && lcdcurs >= Cols ) { lcdcurs=Cols-1; }
      lcd_setcurs(lcdcurs);
    }
  }
}

/* lcd_cpos(c,r)  goto position column c and row r
 *                counting from 0
 */

lcd_cpos(c,r)
int c,r;
{ 
  if (vflg) { 
    fprintf(stderr,"in lcd_cpos: row %d, col %d\n",r,c);
  }
  if (r>=2) r=1;
  else if (r<0) r=0;
  if (c>=Cols) c=Cols-1;
  else if (c<0) c=0;

  lcdcurs = r*0x40 + c;
  lcd_setcurs(lcdcurs);
}

lcd_bs()
{
  lcd_clt();
  lcd_put(' ');
  lcd_setcurs(lcdcurs);
}

lcd_cr()
{
  lcdcurs=(lcdcurs<0x40)?0:0x40;
  lcd_setcurs(lcdcurs);
  //  pausems(linepause);
}

lcd_ff()
{
  int i;
  
  lcd_erase();
  lcdcurs=0;
  lcd_setcurs(0);
  // pausems(linepause);
}

lcd_nl()
{
  int i;
  
  if (lcdcurs>=0x40) {
    lcd_setcurs(0);               /* set to start of line 1 */
    lcd_puts(lcdbuf+0x40,Cols); /* write line2 -> line 1 */
    for (i=0; i<Cols; i++) { lcdbuf[i]=lcdbuf[0x40+i]; lcdbuf[0x40+i]=' '; }
  }
  lcd_setcurs(0x40);
  lcd_puts(lcdbuf+0x40,Cols);
  lcdcurs=0x40;
  lcd_setcurs(lcdcurs);
}
