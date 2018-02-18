/* putbits.c, bit-level output                                              */

/* Copyright (C) 1996, MPEG Software Simulation Group. All Rights Reserved. */

/*
 * Disclaimer of Warranty
 *
 * These software programs are available to the user without any license fee or
 * royalty on an "as is" basis.  The MPEG Software Simulation Group disclaims
 * any and all warranties, whether express, implied, or statuary, including any
 * implied warranties or merchantability or of fitness for a particular
 * purpose.  In no event shall the copyright-holder be liable for any
 * incidental, punitive, or consequential damages of any kind whatsoever
 * arising from the use of these programs.
 *
 * This disclaimer of warranty extends to the user of these programs and user's
 * customers, employees, agents, transferees, successors, and assigns.
 *
 * The MPEG Software Simulation Group does not represent or warrant that the
 * programs furnished hereunder are free of infringement of any third-party
 * patents.
 *
 * Commercial implementations of MPEG-1 and MPEG-2 video, including shareware,
 * are subject to royalty fees to patent holders.  Many of these patents are
 * general enough such that they are unavoidable regardless of implementation
 * design.
 *
 */

#include <stdio.h>
#include <string.h>
#include "config.h"
#ifdef LTHREAD
#include <malloc.h>
#endif

extern FILE *outfile; /* the only global var we need here */

/* private data */
static unsigned char outbfr;
static int outcnt;
static int bytecnt;

#ifdef LTHREAD
#define OUT_FB_LEN 128
int buf_ptr[NUM_THREADS];  /* pointer to outfrmbuf */
int cur_size[NUM_THREADS]; 
unsigned char *outfrmbuf[NUM_THREADS];

static unsigned char out_bfr[NUM_THREADS];
static int out_cnt[NUM_THREADS];
int byte_cnt[NUM_THREADS];

void write_buf(unsigned char val, int id) {

  if (buf_ptr[id]==cur_size[id]-1) {
    unsigned char* tmp;

    cur_size[id] <<= 1;
    tmp = malloc(cur_size[id] * sizeof(unsigned char));
    memcpy(tmp, outfrmbuf[id], cur_size[id]>>1);
    free(outfrmbuf[id]);
    outfrmbuf[id]=tmp;
    /*    outfrmbuf[id] = (unsigned char *) realloc(outfrmbuf[id], \
	  cur_size[id] * sizeof(unsigned char));*/
  }
    outfrmbuf[id][buf_ptr[id]++] = val;
}

/* write rightmost n (0<=n<=32) bits of val to outfile */
void put_bits(val,n,id)
     int val;
int n;
int id;
{
  int i;
  unsigned int mask;

  mask = 1 << (n-1); /* selects first (leftmost) bit */
  
  for (i=0; i<n; i++)
  {
    out_bfr[id] <<= 1;
    
    if (val & mask)
      out_bfr[id]|= 1;
    
    mask >>= 1; /* select next bit */
    out_cnt[id]--;

    if (out_cnt[id]==0) /* 8 bit buffer full */
      {
	write_buf(out_bfr[id],id);
	out_cnt[id] = 8;
	byte_cnt[id]++;
      }
  }

}

/* zero bit stuffing to next byte boundary (5.2.3, 6.2.1) */
void align_bits(int id)
{
  if (out_cnt[id]!=8)
    put_bits(0,out_cnt[id],id);
}

void flushbits(int id) {
  int i;

  align_bits(id);
  for (i = 0; i<buf_ptr[id]; i++) {
    fputc(outfrmbuf[id][i],outfile);
  }
  buf_ptr[id] = 0;

}

#endif

/* initialize buffer, call once before first putbits or alignbits */
void initbits()
{
#ifdef LTHREAD
  int id;
#endif
  
  outcnt = 8;
  bytecnt = 0;
#ifdef LTHREAD
  for (id=0; id<NUM_THREADS; id++) {
    out_cnt[id] = 8;
    byte_cnt[id] = 0;
    buf_ptr[id] = 0;
    outfrmbuf[id] = (unsigned char *) malloc(OUT_FB_LEN*sizeof(unsigned char));
    cur_size[id] = OUT_FB_LEN;
  }
#endif
}


/* write rightmost n (0<=n<=32) bits of val to outfile */
void putbits(val,n)
     int val;
     int n;
{
  int i;
  unsigned int mask;

  mask = 1 << (n-1); /* selects first (leftmost) bit */
  
  for (i=0; i<n; i++)
    {
      outbfr <<= 1;
      
      if (val & mask)
	outbfr|= 1;
      
      mask >>= 1; /* select next bit */
      outcnt--;
      
      if (outcnt==0) /* 8 bit buffer full */
	{
	  fputc(outbfr,outfile);
	  outcnt = 8;
	  bytecnt++;
	}
    }
}

/* zero bit stuffing to next byte boundary (5.2.3, 6.2.1) */
void alignbits()
{

  if (outcnt!=8)
    putbits(0,outcnt);
}

/* return total number of generated bits */
int bitcount()
{
  return 8*bytecnt + (8-outcnt);
}
