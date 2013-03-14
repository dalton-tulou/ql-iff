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


int iff_mapChunks(const UInt8 *bytePtr, long length, chunkMap_t *ckmap)
{
	memset(ckmap, 0, sizeof(*ckmap));

    form_t *form = (form_t *)bytePtr;
    
	if (header_getID(&form->header) != 'FORM')
	{
		return 1;
	}
    
    if (header_getSize(&form->header) + sizeof(form->header) > length)
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
		return -1;
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
	
	return numColors;
}

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
        
        // lame c2p
        
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
			}
			else
			{
				memcpy(dest, src, height*width);
			}
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

int ilbm_render(chunkMap_t *ckmap, UInt32 *picture, int width, int height)
{
    if (ckmap->bmhd && ckmap->body && ckmap->cmap)
    {
        UInt8 *chunky = malloc(width*height);
        UInt32 *palette = malloc(256*4);
        
        body_unpack(ckmap, chunky);
        cmap_unpack(ckmap, palette);
        iblm_makePicture(ckmap, chunky, palette, picture);
        
        free(chunky);
        free(palette);
        
        return 0;
    }
    else if (ckmap->cmap)
    {
        UInt32 *palette = malloc(256*4);
        int numColors = cmap_unpack(ckmap, palette);
        
        if (numColors <= 0)
        {
            return 1;
        }
        
        int rows, cols;
        
        if (numColors <= 1)
        {
            rows = 1;
            cols = 1;
        }
        else if (numColors <= 2)
        {
            rows = 1;
            cols = 2;
        }
        else if (numColors <= 4)
        {
            rows = 2;
            cols = 2;
        }
        else if (numColors <= 8)
        {
            rows = 2;
            cols = 4;
        }
        else if (numColors <= 16)
        {
            rows = 4;
            cols = 4;
        }
        else if (numColors <= 32)
        {
            rows = 4;
            cols = 8;
        }
        else if (numColors <= 64)
        {
            rows = 8;
            cols = 8;
        }
        else if (numColors <= 128)
        {
            rows = 8;
            cols = 16;
        }
        else if (numColors <= 256)
        {
            rows = 16;
            cols = 16;
        }
        else
        {
            return 1;
        }
        
        {
            for (int y=0; y<height; y++)
            {
                for (int x=0; x<width; x++)
                {
                    int i = cols*(y*rows/height) + (x*cols/width);
                    picture[width*y+x] = palette[i];
                }
            }
        }
        
        free(palette);

        return 0;
    }
    
    return 1;
}


CGImageRef iff_getImageRef(CFURLRef url)
{
	CFDataRef data;
	
	int errorCode;
	
	Boolean success = CFURLCreateDataAndPropertiesFromResource(NULL, url, &data, NULL, NULL, &errorCode);
	
	if (!success)
	{
		return NULL;
	}
	
	CFIndex length = CFDataGetLength(data);
    const UInt8 *bytePtr = CFDataGetBytePtr(data);
    
	chunkMap_t ckmap;
	
	errorCode = iff_mapChunks(bytePtr, length, &ckmap);
	
    if (errorCode)
    {
        return NULL;
    }
    
    int width, height;
    
    if (ckmap.bmhd && ckmap.body && ckmap.cmap)
    {
        width = bmhd_getWidth(ckmap.bmhd);
        height = bmhd_getHeight(ckmap.bmhd);
    }
    else if (ckmap.cmap)
    {
        width = 256;
        height = 256;
    }
    else
    {
        return NULL;
    }
    
	int width2 = (width+1)&-2;
	
	UInt32 *picture = malloc(4*width*height);
    
    errorCode = ilbm_render(&ckmap, picture, width, height);
    
    if (errorCode)
    {
        return NULL;
    }
    
	CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
    
	if (!colorSpace)
	{
		return NULL;
	}
	
	CGContextRef bitmapContext =
    CGBitmapContextCreate(picture, width, height, 8, 4*width2, colorSpace, kCGImageAlphaNoneSkipFirst);
    
	if (!bitmapContext)
	{
		return NULL;
	}
	
	CGColorSpaceRelease(colorSpace);
	
	CGImageRef image = CGBitmapContextCreateImage(bitmapContext);
    
    CFRelease(bitmapContext);
	free(picture);

    return image;
}