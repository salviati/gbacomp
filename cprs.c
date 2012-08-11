#include "cprs.h"

//! Create the compression header word (little endian)
u32	cprs_create_header(uint size, u8 tag)
{
	u8 data[4];

	data[0]= tag;
	data[1]= (size    ) & 255;
	data[2]= (size>> 8) & 255;
	data[3]= (size>>16) & 255;
	return *(u32*)data;
}

