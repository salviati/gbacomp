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


/* Copyright (c) 2007, J Vijn

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE. */

//
//! \file grit_rle.cpp
//!   GBA RLE compression
//! \date 20050814 - 20081207
//! \author cearn
//
// === NOTES === 

#include <stdlib.h>
#include <memory.h>
#include <assert.h>

#include "cprs.h"


// --------------------------------------------------------------------
// FUNCTIONS
// --------------------------------------------------------------------


//! Compression routine for GBA RLE
/*!
*/
uint rle8gba_compress(RECORD *dst, const RECORD *src)
{
	if(src==NULL || dst==NULL || src->data == NULL)
		return 0;

	uint ii, rle, non;
	BYTE curr, prev;

	uint srcS= src->width*src->height;
	BYTE *srcD= src->data;

	// Annoyingly enough, rle _can_ end up being larger than
	// the original. A checker-board will do it for example.
	// if srcS is the size of the alternating pattern, then
	// the endresult will be 4 + srcS + (srcS+0x80-1)/0x80.
	uint dstS= 8+2*(srcS);
	BYTE *dstD = (BYTE*)malloc(dstS), *dstL= dstD;
	if(dstD == NULL)
		return 0;

	prev= srcD[0];
	rle= non= 1;

	// NOTE! non will always be 1 more than the actual non-stretch
	// PONDER: why [1,srcS] ?? (to finish up the stretch)
	for(ii=1; ii<=srcS; ii++)
	{
		if(ii != srcS)
			curr= srcD[ii];

		if(rle==0x82 || ii==srcS)	// stop rle
			prev= ~curr;

		if(rle<3 && (non+rle > 0x80 || ii==srcS))	// ** mini non
		{
			non += rle;
			dstL[0]= non-2;	
			memcpy(&dstL[1], &srcD[ii-non+1], non-1);
			dstL += non;
			non= rle= 1;
		}
		else if(curr == prev)		// ** start rle / non on hold
		{
			rle++;
			if( rle==3 && non>1 )	// write non-1 bytes
			{
				dstL[0]= non-2;
				memcpy(&dstL[1], &srcD[ii-non-1], non-1);
				dstL += non;
				non= 1;
			}
		}
		else						// ** rle end / non start
		{
			if(rle>=3)	// write rle
			{
				dstL[0]= 0x80 | (rle-3);
				dstL[1]= srcD[ii-1];
				dstL += 2;
				non= 0;
				rle= 1;
			}
			non += rle;
			rle= 1;
		}
		prev= curr;
	}
	
	dstS= ALIGN4(dstL-dstD)+4;

	dstL= (BYTE*)malloc(dstS);
	if(dstL == NULL)
	{
		free(dstD);
		return 0;
	}

	*(u32*)dstL= cprs_create_header(srcS, CPRS_RLE_TAG);
	memcpy(dstL+4, dstD, dstS-4);
	rec_attach(dst, dstL, 1, dstS);

	free(dstD);

	return dstS;
}


uint rle8gba_decompress(RECORD *dst, const RECORD *src)
{
	assert(dst && src && src->data);
	if(dst==NULL || src==NULL || src->data==NULL)
		return 0;

	// Get and check header word
	u32 header= read32le(src->data);
	if((header&255) != CPRS_RLE_TAG)
		return 0;

	uint ii, dstS= header>>8, size=0;
	u8 *srcL= src->data+4, *dstD= (BYTE*)malloc(dstS);

	for(ii=0; ii<dstS; ii += size)
	{
		// Get header byte
		header= *srcL++;

		if(header&0x80)		// compressed stint
		{
			size= MIN( (header&~0x80)+3, dstS-ii);
			memset(&dstD[ii], *srcL++, size);
		}
		else				// noncompressed stint
		{
			size= MIN(header+1, dstS-ii);
			memcpy(&dstD[ii], srcL, size);
			srcL += size;
		}
	}

	rec_attach(dst, dstD, 1, dstS);
	return dstS;
}

// EOF
