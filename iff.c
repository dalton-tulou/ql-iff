/*
 *  iff.c
 *  ql_plugin_test_2
 *
 *  Created by David R on 13-03-09.
 *  Copyright 2013 __MyCompanyName__. All rights reserved.
 *
 */

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFByteOrder.h>
#include <CoreServices/CoreServices.h>
#include <QuickLook/QuickLook.h>

#include <CoreFoundation/CFByteOrder.h>
#include "iff.h"

uint32_t form_getType(form_t *form)
{
	return CFSwapInt32(form->type);
}

header_t *form_getFirstChunk(form_t *form)
{
	return (header_t *)((UInt8 *)form+sizeof(*form));
}

uint32_t header_getID(header_t *h)
{
	return CFSwapInt32(h->id);
}

int header_getSize(header_t *h)
{
	return CFSwapInt32(h->size);
}

void *header_getData(header_t *h)
{
	return (UInt8 *)h+sizeof(*h);
}

header_t *header_getNext(header_t *h)
{
	return header_getData(h) + ((header_getSize(h)+1) & -2);
}

int bmhd_getWidth(bmhd_t *bmhd)
{
	return CFSwapInt16(bmhd->w);
}

int bmhd_getHeight(bmhd_t *bmhd)
{
	return CFSwapInt16(bmhd->h);
}

int bmhd_getDepth(bmhd_t *bmhd)
{
	return bmhd->nPlanes;
}

int bmhd_getCompression(bmhd_t *bmhd)
{
	return bmhd->compression;
}


int iff_mapChunks(form_t *form, chunkMap_t *ckmap)
{
	memset(ckmap, 0, sizeof(*ckmap));

	if (header_getID(&form->header) != 'FORM')
	{
		return 1;
	}
	
	if (form_getType(form) != 'ILBM' && form_getType(form) != 'PBM ')
	{
		return 1;
	}

	ckmap->form = form;

	header_t *header = form_getFirstChunk(form);

	while (header < header_getNext(&form->header))
	{
		switch (header_getID(header))
		{
			case 'BMHD':
				ckmap->bmhd = (bmhd_t *)header;
				break;
				
			case 'CMAP':
				ckmap->cmap = (cmap_t *)header;				
				break;
				
			case 'BODY':
				ckmap->body = (body_t *)header;
				break;
				
			case 'CAMG':
				ckmap->camg = (camg_t *)header;
				break;
		}
		
		header = header_getNext(header);
	}
	
	return 0;
}

int cmap_unpack(chunkMap_t *ckmap, UInt32 *dest)
{
	cmap_t *cmap = ckmap->cmap;
	
	if (!cmap)
	{
		return 1;
	}
	
	int numColors = header_getSize(&cmap->header) / 3;
	UInt8 *src = header_getData(&cmap->header);
	
	UInt8 (*palette)[4] = (void *)dest;

	for (int i=0; i<numColors; i++)
	{
		palette[i][0] = 0;
		palette[i][1] = *src++;
		palette[i][2] = *src++;
		palette[i][3] = *src++;
	}
	
	return 0;
}

/*

 ilbm_body
 
 tst.l	a1	; write bitmap?
 beq.b	.skip
 
 movea.l	a0,a4	; src pointer
 
 move.w	bmhd_width(a3),d2	; width in pixels
 move.w	bmhd_height(a3),d5	; rows
 
 asr.w	#3,d2			; width in bytes
.rows
 move.l	a1,-(sp)
 
 move.b	bmhd_depth(a3),d4	; nr of bitplanes
.planes
 lea	(a1,d2.w),a5	; a5 = row end
.byte
 cmpa.l	a5,a1
 bge.b	.next
 
 move.b	(a4)+,d3
 extb.l	d3
 
 bpl.b	.copy
 
 ; negative means rle
 
 neg.l	d3
 
 move.b	(a4)+,d0
.rle	move.b	d0,(a1)+
 dbf	d3,.rle
 
 bra.b	.byte
 
 .copy	move.b	(a4)+,(a1)+
 dbf	d3,.copy
 
 bra.b	.byte
 
.next	suba.w	d2,a1		; back to row start
 adda.l	d7,a1		; next bitplane
 
 subq.b	#1,d4
 bgt.b	.planes
 
 move.l	(sp)+,a1	; get dest pointer
 adda.l	d6,a1		; next dest row
 
 subq.w	#1,d5
 bgt.b	.rows
.skip
 adda.l	d1,a0
 rts
 
 */
 
int body_unpack(chunkMap_t *ckmap, UInt8 *chunky)
{
	bmhd_t *bmhd = ckmap->bmhd;
	body_t *body = ckmap->body;
	
	if (!bmhd || !body)
	{
		return 1;
	}

	int comp = bmhd_getCompression(bmhd);
	
	if (comp != 0 && comp != 1)
	{
		return 1;
	}

	int height = bmhd_getHeight(bmhd);
	int width = bmhd_getWidth(bmhd);
	int depth = bmhd_getDepth(bmhd);

	UInt32 type = form_getType(ckmap->form);

	int cols = ((width+15) & -16) >> 3;
	
	UInt8 *planar = malloc(height*cols*8);
	
	if (type == 'ILBM')
	{
		UInt8 *dest = planar;
		
		SInt8 *src = header_getData(&body->header);	
		
		for (int y=0; y<height; y++)
		{
			for (int z=0; z<depth; z++)
			{
				if (comp)
				{
					UInt8 *rowEnd = dest+cols;
					
					while (dest < rowEnd)
					{
						int x = *src++;
						
						if (x >= 0)
						{
							while (x-- >= 0)
							{
								*dest++ = *src++;
							}
						}
						else if (x != -128) // rle
						{
							int y = *src++;
							
							while (x++ <= 0)
							{
								*dest++ = y;
							}
						}
					}					
					assert(dest == rowEnd);
				}
				else
				{
					memcpy(dest, src, cols);
					src += cols;
					dest += cols;
				}
			}
		}
	}
	else if (type == 'PBM ')
	{
		UInt8 *dest = chunky;
		SInt8 *src = header_getData(&body->header);	
		
			if (comp)
			{
				UInt8 *dataEnd = dest+height*width;
				
				while (dest < dataEnd)
				{
					int x = *src++;
					
					if (x >= 0)
					{
						while (x-- >= 0)
						{
							*dest++ = *src++;
						}
					}
					else if (x != 128) // rle
					{
						int y = *src++;
						
						while (x++ <= 0)
						{
							*dest++ = y;
						}
					}
				}
				//assert(dest == dataEnd);
			}
			else
			{
				memcpy(dest, src, height*width);
			}
	}
	
	if (type == 'ILBM')
	{
		for (int y=0; y<height; y++)
		{
			for (int x=0; x<width; x++)
			{
				int c = 0;
				
				for (int z=depth-1; z>=0; z--)
				{
					int pos = (y*depth+z)*cols + (x>>3);
					int bit = 7-(x & 7);
					
					c += c + ((planar[pos] >> bit) & 1);
				}
				
				chunky[y*width+x] = c;
			}
		}
	}
	else if (type == 'PBM ')
	{
		/*for (int y=0; y<height; y++)
		{
			for (int x=0; x<width; x++)
			{
				int c = 0;
				
				for (int z=0; z<depth; z++)
				{
					int pos = (z*cols*height) + (y*cols) + (x>>3);
					int bit = 7-(x & 7);
					
					c += c + ((planar[pos] >> bit) & 1);
				}
				
				chunky[y*width+x] = c;
			}
		}*/
		//memcpy(chunky, planar, width*height);
	}

	free(planar);
	
	return 0;
}

int iblm_makePicture(chunkMap_t *ckmap, UInt8 *chunky, UInt32 *palette, UInt32 *dest)
{
	bmhd_t *bmhd = ckmap->bmhd;
	
	if (!bmhd)
	{
		return 1;
	}
	
	int height = bmhd_getHeight(bmhd);
	int width = bmhd_getWidth(bmhd);

	UInt8 *src = chunky;
	
	for (int i=0; i<width*height; i++)
	{
		*dest++ = palette[*src++];
	}
	
	return 0;
}