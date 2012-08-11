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

#ifndef CPRS_H
# define CPRS_H

#include <stdint.h>
#include <stdlib.h>

typedef uint32_t u32;
typedef uint8_t u8;
typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef unsigned int uint;

typedef struct RECORD
{
	int width;		//!< Width of \a data / datatype
	int height;		//!< Height of data / Length of data
	unsigned char *data;		//!< Binary data.
} RECORD;

#define INLINE static inline

//! Return full size of \a rec in bytes.
INLINE int rec_size(const RECORD *rec)
{ return rec->width*rec->height;	}

//! Shallow (semisafe) copy from \a src to \a dst
/*! \note \a dst's data is freed, but the data still points to 
	  src's data. Yes, this can be a good thing.
*/
INLINE RECORD *rec_alias(RECORD *dst, const RECORD *src)
{	free(dst->data); *dst= *src; return dst;	}

//! Attach new data to the current record.
INLINE void rec_attach(RECORD *dst, const void *data, int width, int height)
{
	free(dst->data);
	dst->width= width;
	dst->height= height;
	dst->data= (BYTE*)data;
}

//! Read a little-endian 32bit number.
INLINE DWORD read32le(const BYTE *src)
{	return src[0] | src[1]<<8 | src[2]<<16 | src[3]<<24;			}

//! Write a little-endian 16bit number.
INLINE void write16le(BYTE *dst, WORD src)
{	dst[0]=(BYTE)(src);	dst[1]=(BYTE)(src>>8);						}

//! Write a little-endian 32bit number.
INLINE void write32le(BYTE *dst, DWORD src)
{	write16le(dst, (WORD)(src));	write16le(dst+2, (WORD)(src>>16));	}


#define MIN(a, b) (((a) < (b)) ? (a) : (b))

// --------------------------------------------------------------------
// CONSTANTS
// --------------------------------------------------------------------


//! Compression type tags.
enum ECprsTag
{
	CPRS_FAKE_TAG	= 0x00,		//<! No compression.
	CPRS_LZ77_TAG	= 0x10,		//<! GBA LZ77 compression.
	CPRS_HUFF_TAG	= 0x20, 
	CPRS_HUFF4_TAG	= 0x24,		//<! GBA Huffman, 4bit.
	CPRS_HUFF8_TAG	= 0x28,		//<! GBA Huffman, 8bit.
	CPRS_RLE_TAG	= 0x30,		//<! GBA RLE compression.
//	CPRS_DIFF8_TAG	= 0x81,		//<! GBA Diff-filter, 8bit.
//	CPRS_DIFF16_TAG	= 0x82,		//<! GBA Diff-filter, 16bit.
};

#define ALIGN4(nn) ( ((nn)+3)&~3 )


// --------------------------------------------------------------------
// PROTOTYPES 
// --------------------------------------------------------------------

u32	cprs_create_header(uint size, u8 tag); 

uint lz77gba_compress(RECORD *dst, const RECORD *src);
uint lz77gba_decompress(RECORD *dst, const RECORD *src);

uint huffman_encode(RECORD *dst, const RECORD *src, int data_size);

uint rle8gba_compress(RECORD *dst, const RECORD *src);
uint rle8gba_decompress(RECORD *dst, const RECORD *src);
uint huffman_decode    (RECORD *dst, const RECORD *src);
uint huffman_decode_vba(RECORD *dst, const RECORD *src);

#endif
