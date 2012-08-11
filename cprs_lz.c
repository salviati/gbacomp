/*
   Copyright (c) Utkan Güngördü <utkan@freeconsole.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 3 or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of

   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the

   GNU General Public License for more details


   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/


//
//! \file grit_lz.cpp
//!   LZSS compression, VRAM safe
//! \date 20050814 - 20050903
//! \author cearn
//
// === NOTES === 

/*
   AGBCOMP compatible LZSS compressor
   Compresses files in a format that the GBA BIOS can decode.

   Original code by Haruhiko Okumura, 4/6/1989.
   12-2-404 Green Heights, 580 Nagasawa, Yokosuka 239, Japan.

   Anonymous, December 2001:
     Converted I/O to load the entire file into memory and operate
     on buffers.
     Modified to output in the GBA BIOS's native format.

   Damian Yerrick, July 2002:
     Translated the program into C from C++.
     Removed use of non-ANSI <sys/stat.h> that some compilers don't 
       support.
     Made messages more informational.
     Corrected an off-by-one error in the allocation of the output 
       size.
     Removed comp.h.  Now it's one file.

   Damian Yerrick, December 2002:
     Prevented matches that reference data before start of file.
     Began to standardize comments.

   J. Vijn, Feb 2005
   * Removed main() and added lzgb_compress() so that the file acts
     more like a plugin.
     // declaration:
     unsigned char *lzgb_compress(const char *src, int src_len, 
       int *pdst_len);
   * Removed InsertNode for ii=[N-2*F, N-F-1] because they create 
     spurious nodes that allow strings to start _before_ the actual
     data. Using uninitialized data would fit nicely into the Bad 
     Things category. Also removed the safeguards to counter this, 
     since they're not necessary anymore.
   * Made the code VRAM safe (I hope)
     On this issue: remember that VRAM cannot be written to in bytes. 
     Therefore, LZ77UnCompVram prepares an u16 before writing. The 
     problem occurs when the matched position is one before the current 
     position, in which case the destination bytes haven't been copied 
     in yet. There's that using uninitialized data again. 
       First a little more about the process. Nodes are built for each 
     bytes. rson[N+1..N+256] are the trees for each byte and each node in 
     that tree represents a possible match. There will be as many nodes 
     in the tree as there are of those bytes in the ring buffer. 
     Naturally the last node added is the one before the current one: r-1.
      The routine will traverse the tree of the byte at [r], looking 
     for two things: a) the longest match and b) the closest match if there
     are matches of equal length. And this is where shit happens. Both of 
     them can go wrong, but always for a very special case. As said, the 
     problem occurs when the match is the previous position. This can only 
     happen under one condition: for _stretches_ of the same bytes. The 
     solution is to use the latest length UNLESS the current node p is the 
     last one added (r-1). This will work out for both new partial stretches 
     (which will only have one pass and as a result the 2nd byte will have 
     match_length=0) and F-long stretches. It's basically a 1 line fix.
     Gawd I hate those. (20050312: ok, so it turned out to be a 2 line fix)


   Use, distribute, and modify this code freely.

   "I do not accept responsibility for any effects, adverse or otherwise, 
    that this code may have on you, your computer, your sanity, your dog, 
    and anything else that you can think of. Use it at your own risk."
*/


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "cprs.h"

/// === TYPES =========================================================


// --------------------------------------------------------------------
// CONSTANTS
// --------------------------------------------------------------------


// Define information for compression
//   (dont modify from 4096/18/2 if AGBCOMP format is required)
#define RING_MAX        4096   // size of ring buffer (12 bit)
#define FRAME_MAX         18   // upper limit for match_length
#define THRESHOLD          2   // encode string into position and length
                               //   if matched length is greater than this 
#define NIL         RING_MAX   // index for root of binary search trees 
#define TEXT_BUF_CLEAR     0   // byte to initialize the area before text_buf with
#define NMASK           (RING_MAX-1)  // for wrapping


// --------------------------------------------------------------------
// GLOBALS
// --------------------------------------------------------------------

/* Compressor global variables.  If you actually want to USE this
   code in a non-trivial app, put these global variables in a struct,
   as the Allegro library did.
*/
static unsigned long int codesize = 0;  // code size counter

// Ring buffer of size RING_MAX with extra FRAME_MAX-1 bytes to 
// facilitate string comparison
static BYTE text_buf[RING_MAX + FRAME_MAX - 1];
static int match_position;  // global string match position
static int match_length;  // global string match length


// left & right children & parents -- These constitute binary search trees.
static int lson[RING_MAX+1], rson[RING_MAX+256+1], dad[RING_MAX+1];  


static BYTE *InBuf, *OutBuf;
static int InSize, OutSize, InOffset;


// --------------------------------------------------------------------
// PROTOTYPES
// --------------------------------------------------------------------


/* Binary search tree functions */
static void InitTree(void);
static void InsertNode(int r);
static void DeleteNode(int p);

/* Misc Functions */
static void CompressLZ77(void);
static int InChar(void);


// --------------------------------------------------------------------
// FUNCTIONS
// --------------------------------------------------------------------


// Initializes InBuf, InSize; allocates OutBuf.
// the rest is done in CompressLZ77.
uint lz77gba_compress(RECORD *dst, const RECORD *src)
{
	// Fail on the obvious
	if(src==NULL || src->data==NULL || dst==NULL)
		return 0;
	
	InSize= rec_size(src);
	OutSize = InSize + InSize/8 + 16;
	OutBuf = (BYTE*)malloc(OutSize);
	if(OutBuf == NULL)
		return 0;
	InBuf= (BYTE*)src->data;

	CompressLZ77();
	OutSize= ALIGN4(OutSize);

	u8 *dstD= (u8*)malloc(OutSize);
	memcpy(dstD, OutBuf, OutSize);
	rec_attach(dst, dstD, 1, OutSize);

	free(OutBuf);

	return OutSize;
}

//! Decompress GBA LZ77 data.
uint lz77gba_decompress(RECORD *dst, const RECORD *src)
{
	assert(dst && src && src->data);
	if(dst==NULL || src==NULL || src->data==NULL)
		return 0;

	// Get and check header word
	u32 header= read32le(src->data);
	if((header&255) != CPRS_LZ77_TAG)
		return 0;

	u32 flags;
	int ii, jj, dstS= header>>8;
	u8 *srcL= src->data+4, *dstD= (BYTE*)malloc(dstS);

	for(ii=0, jj=-1; ii<dstS; jj--)
	{
		if(jj<0)				// Get block flags
		{
			flags= *srcL++;
			jj= 7;
		}
		
		if(flags>>jj & 1)		// Compressed stint
		{
			int count= (srcL[0]>>4)+THRESHOLD+1;
			int ofs=  ((srcL[0]&15)<<8 | srcL[1])+1;
			srcL += 2;
			while(count--)
			{
				dstD[ii]= dstD[ii-ofs];
				ii++;
			}
		}
		else					// Single byte from source
			dstD[ii++]= *srcL++;
	}

	rec_attach(dst, dstD, 1, dstS);
	return dstS;
}


/* InitTree() **************************
   Initialize a binary search tree.

   For i = 0 to RING_MAX - 1, rson[i] and lson[i] will be the right and
   left children of node i.  These nodes need not be initialized.
   Also, dad[i] is the parent of node i.  These are initialized
   to NIL (= RING_MAX), which stands for 'not used.'
   For i = 0 to 255, rson[RING_MAX + i + 1] is the root of the tree
   for strings that begin with character i.  These are
   initialized to NIL.  Note there are 256 trees.
*/
void InitTree(void)
{
	int  i;
	for(i= RING_MAX+1; i <= RING_MAX+256; i++)
		rson[i]= NIL;
	for(i=0; i < RING_MAX; i++)
		dad[i]= NIL;
}

/* InsertNode() ************************
   Inserts string of length FRAME_MAX, text_buf[r..r+FRAME_MAX-1], into one of the
   trees (text_buf[r]'th tree) and returns the longest-match position
   and length via the global variables match_position and match_length.
   If match_length = FRAME_MAX, then removes the old node in favor of the new
   one, because the old one will be deleted sooner.
   Note r plays double role, as tree node and position in buffer.
*/
void InsertNode(int r)
{
	int  i, p, cmp, prev_length;
	BYTE *key;

	cmp= 1;  key= &text_buf[r];  p= RING_MAX + 1 + key[0];
	rson[r]= lson[r]= NIL;  
	prev_length= match_length= 0;
	for( ; ; )
	{
		if(cmp >= 0)
		{
			if(rson[p] != NIL)
				p= rson[p];
			else
			{
				rson[p]= r;
				dad[r]= p;
				return;
			}
		}
		else
		{
			if(lson[p] != NIL)
				p= lson[p];
			else
			{
				lson[p]= r;
				dad[r]= p;
				return;
			}

		}
		for(i=1; i < FRAME_MAX; i++)
			if((cmp = key[i] - text_buf[p + i]) != 0)
				break;

		if(i > match_length)
		{
			// VRAM safety:
			// match_length= i ONLY if the matched position 
			// isn't the previous one (r-1)
			// for normal case, remove the if.
			// That's _IT_?!? Yup, that's it.
			if(p != ((r-1)&NMASK) )
			{
				match_length= i;
				match_position= p;
			}
			if(match_length >= FRAME_MAX)
				break;
		}
	}

	// Full length match, remove old node in favor of this one
	dad[r]= dad[p];
	lson[r]= lson[p];
	rson[r]= rson[p];
	dad[lson[p]]= r;
	dad[rson[p]]= r;
	if(rson[dad[p]] == p)
		rson[dad[p]]= r;
	else
		lson[dad[p]]= r;
	dad[p]= NIL;
}


/* DeleteNode() ************************
   Deletes node p from the tree.
*/
void DeleteNode(int p)  
{
	int  q;

	if(dad[p] == NIL)
		return;  /* not in tree */
	if(rson[p] == NIL)
		q = lson[p];
	else if(lson[p] == NIL)
		q = rson[p];
	else
	{
		q = lson[p];
		if(rson[q] != NIL)
		{
			do {
				q = rson[q];
			} while(rson[q] != NIL);

			rson[dad[q]] = lson[q];
			dad[lson[q]] = dad[q];
			lson[q] = lson[p];
			dad[lson[p]] = q;
		}
		rson[q] = rson[p];
		dad[rson[p]] = q;
	}

	dad[q] = dad[p];

	if(rson[dad[p]] == p)
		rson[dad[p]] = q;
	else
		lson[dad[p]] = q;

	dad[p] = NIL;
}


/* CompressLZ77() **********************
   Compress InBuffer to OutBuffer.
*/

void CompressLZ77(void)
{
	int  i, c, len, r, s, last_match_length, code_buf_ptr;
	unsigned char  code_buf[17];
	unsigned short mask;
	BYTE *FileSize;
	unsigned int curmatch;		// PONDER: doesn't this do what r does?
	unsigned int savematch;

	OutSize=4;  // skip the compression type and file size
	InOffset=0;
	match_position= curmatch= RING_MAX-FRAME_MAX;

	InitTree();  // initialize trees
	code_buf[0] = 0;  /* code_buf[1..16] saves eight units of code, and
	code_buf[0] works as eight flags, "0" representing that the unit
	is an unencoded letter (1 byte), "1" a position-and-length pair
	(2 bytes).  Thus, eight units require at most 16 bytes of code. */
	code_buf_ptr = 1;
	s = 0;  r = RING_MAX - FRAME_MAX;

	// Clear the buffer
	for(i = s; i < r; i++)
		text_buf[i] = TEXT_BUF_CLEAR;
	// Read FRAME_MAX bytes into the last FRAME_MAX bytes of the buffer
	for(len = 0; len < FRAME_MAX && (c = InChar()) != -1; len++)
		text_buf[r + len] = c;  
	if(len == 0)
		return;

	/* Insert the F strings, each of which begins with one or more 
	// 'space' characters.  Note the order in which these strings are 
	// inserted.  This way, degenerate trees will be less likely to occur. 
	*/
	// Perhaps. 
	// However, the strings you create here have no relation to 
	// the actual data and are therefore completely bogus. Removed!
	//for (i = 1; i <= FRAME_MAX; i++)
	//	InsertNode(r - i);

	// Create the first node, sets match_length to 0
	InsertNode(r);

	// GBA LZSS masks are big-endian
	mask = 0x80;
	do
	{
		if(match_length > len) 
			match_length = len;  

		// match too short: add one unencoded byte
		if(match_length <= THRESHOLD)
		{
			match_length = 1;
			code_buf[code_buf_ptr++] = text_buf[r];
		} 
		else	// Long enough: add position and length pair.
		{
			code_buf[0] |= mask;	// set match flag

			// 0 byte is 4:length and 4:top 4 bits of match_position
			savematch= ((curmatch-match_position)&NMASK)-1;
			code_buf[code_buf_ptr++] = ((BYTE)((savematch>>8)&0xf))
				| ((match_length - (THRESHOLD + 1))<<4);

			code_buf[code_buf_ptr++] = (BYTE)savematch;
		}
		curmatch += match_length;
		curmatch &= NMASK;

		// if mask is empty, the buffer's full; write it out the code buffer
		// at end of source, code_buf_ptr will be <17
		if((mask >>= 1) == 0) 
		{  
			for(i=0; i < code_buf_ptr; i++)
				OutBuf[OutSize++]= code_buf[i];

			codesize += code_buf_ptr;
			code_buf[0] = 0;  
			code_buf_ptr = 1;
			mask = 0x80;
		}

		// Inserts nodes for this match. The last_match_length is 
		// required because InsertNode changes match_length.
		last_match_length = match_length;
		for(i=0; i < last_match_length && (c = InChar()) != -1; i++) 
		{
			DeleteNode(s);      // Delete string beforelook-ahead
			text_buf[s] = c;    // place new bytes
			// text_buf[N..RING_MAX+FRAME_MAX> is a double for text_buf[0..FRAME_MAX>
			// for easier string comparison
			if(s < FRAME_MAX-1)
				text_buf[s + RING_MAX] = c;

			// add and wrap around the buffer
			s = (s + 1) & NMASK;
			r = (r + 1) & NMASK;

			// Register the string in text_buf[r..r+FRAME_MAX-1]
			InsertNode(r);
		}

		while(i++ < last_match_length) 
		{    
			// After the end of text
			DeleteNode(s);            // no need to read, but
			s = (s + 1) & NMASK;
			r = (r + 1) & NMASK;
			if(--len)
				InsertNode(r);        // buffer may not be empty
		}
	} while(len > 0);    // until length of string to be processed is zero

	if(code_buf_ptr > 1) 
	{     
		// Send remaining code.
		for(i=0; i < code_buf_ptr; i++) 
			OutBuf[OutSize++]=code_buf[i]; 

		codesize += code_buf_ptr;
	}

	FileSize= (BYTE*)OutBuf;
	FileSize[0]= CPRS_LZ77_TAG;
	FileSize[1]= ((InSize>>0)&0xFF);
	FileSize[2]= ((InSize>>8)&0xFF);
	FileSize[3]= ((InSize>>16)&0xFF);
}

/* InChar() ****************************
   Get the next character from the input stream, or -1 for end of file.
*/
int InChar()
{
	return (InOffset < InSize) ? InBuf[InOffset++] : -1;
}

// EOF
