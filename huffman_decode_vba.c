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


// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2004 Forgotten and the VBA development team

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or(at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

#include "cprs.h"
#include <stdlib.h>
#include <string.h>
//#include <stdio.h>

uint huffman_decode_vba(RECORD *rec_dst, const RECORD *rec_src)
{
	u8 *src, *dst, treeSize, *treeStart, rootNode, currentNode;
	u8 writeData;
	u32 len;
	u32 data;
	u32 pos;
	u32 mask = 0x80000000;
	u32 writeValue = 0;
	
	u8 *in = rec_src->data;

	u32 insize = rec_src->width * rec_src->height;
	u32 leftover = insize & 3;
	if (leftover) {
		in = calloc(insize + (4-leftover),1);
		memcpy(in, rec_src->data, insize);
	}

	src = in;

	if(src[0] != CPRS_HUFF8_TAG && src[0] != CPRS_HUFF4_TAG)
	{
		rec_dst->width = 1;
		rec_dst->height = 0;
		return 0;
	}

	len = src[1] + (src[2]<<8) + (src[3]<<16);
	src += 4;
	
	u8 *out;
	dst = out = rec_dst->data = malloc(len);
	if (out == NULL) return 0;

	treeSize = *src++;
	treeStart = src;
	src += (treeSize<<1) + 1;

	data = read32le(src);
	src += 4;

	pos = 0;
	rootNode = *treeStart;
	currentNode = rootNode;
	writeData = 0;
	int byteShift = 0;
	int byteCount = 0;

	if(rec_src->data[0] == CPRS_HUFF8_TAG)
	{
		while(len > 0)
		{
			/* take left */
			if(pos == 0)
				pos++;
			else
				pos += ((currentNode & 0x3f)+1)<<1;

			if(data & mask)
			{
				/* right */
				if(currentNode & 0x40)
					writeData = 1;
				currentNode = *(treeStart+pos+1);
			}
			else
			{
				/* left */
				if(currentNode & 0x80)
					writeData = 1;
				currentNode = *(treeStart+pos);
			}
			
			if(writeData) {
					writeValue |= (currentNode << byteShift);
					byteCount++;
					byteShift += 8;

					pos = 0;
					currentNode = rootNode;
					writeData = 0;

					if(byteCount == 4) {
					byteCount = 0;
					byteShift = 0;
					write32le(dst,writeValue);
					writeValue = 0;
					dst += 4;
					len -= 4;
					}
				}
				mask >>= 1;
			if(mask == 0)
			{
				if (src-in >= insize)
				{
					//fprintf(stderr, "%s:%d: force break\n", __FILE__, __LINE__);
					break;
				}

				mask = 0x80000000;
				data = read32le(src);

				src += 4;
			}
      
		}
	}
	else if(rec_src->data[0] == CPRS_HUFF4_TAG)
	{
		int halfLen = 0;
		int value = 0;
		while(len > 0)
		{
			// take left
			if(pos == 0)
				pos++;
			else
				pos += (((currentNode & 0x3f)+1)<<1);

			if(data & mask)
			{
				// right
				if(currentNode & 0x40)
				writeData = 1;
				currentNode = *(treeStart+pos+1);
			}
			else
			{
				// left
				if(currentNode & 0x80)
					writeData = 1;
				currentNode = *(treeStart+pos);
			}
			
			if(writeData) {
				if(halfLen == 0)
					value |= currentNode;
				else
					value |= (currentNode<<4);

				halfLen += 4;
				if(halfLen == 8)
				{
					writeValue |= (value << byteShift);
					byteCount++;
					byteShift += 8;
					
					halfLen = 0;
					value = 0;

					if(byteCount == 4)
					{
						byteCount = 0;
						byteShift = 0;
						write32le(dst, writeValue);
						dst += 4;
						writeValue = 0;
						len -= 4;
					}
				}
				pos = 0;
				currentNode = rootNode;
				writeData = 0;
			}
			mask >>= 1;
			if(mask == 0)
			{
				if (src-in > insize)
				{
					//fprintf(stderr, "%s:%d: force break\n", __FILE__, __LINE__);
					break;
				}
				
				mask = 0x80000000;
				data = read32le(src);

				src += 4;
			}
		}
    }
	rec_dst->width = 1;
	rec_dst->height = dst - out;

	return dst - out;
}
