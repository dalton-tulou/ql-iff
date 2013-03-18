/*
 *  iff.c
 *  ql_plugin_test_2
 *
 *  Created by David R on 13-03-09.
 *  Copyright 2013 __MyCompanyName__. All rights reserved.
 *
 */

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <QuickLook/QuickLook.h>

#include <CoreFoundation/CFUtilities.h>
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

int camg_getEHB(camg_t *camg)
{
    return !!(CFSwapInt32(camg->viewMode) & 0x0080);
}

int camg_getHAM(camg_t *camg)
{
    return !!(CFSwapInt32(camg->viewMode) & 0x0800);
}

int camg_getLace(camg_t *camg)
{    
    return !!(CFSwapInt32(camg->viewMode) & 0x0004);
}

int camg_getHires(camg_t *camg)
{
    return !!(CFSwapInt32(camg->viewMode) & 0x8000);
}

int camg_getSuper(camg_t *camg)
{
    return (CFSwapInt32(camg->viewMode) & 0x8020) == 0x8020;
}

int camg_getSDbl(camg_t *camg)
{
    return !!(CFSwapInt32(camg->viewMode) & 0x0008);
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
    
    if (ckmap->camg && camg_getEHB(ckmap->camg)
        && ckmap->bmhd && bmhd_getDepth(ckmap->bmhd) == 6
        && numColors <= 32)
    {
        for (int i=0; i<numColors; i++)
        {
            for (int j=0; j<4; j++)
            {
                palette[i+numColors][j] = palette[i][j]/2;
            }
        }
    }
	
	return numColors;
}

int body_unpack(chunkMap_t *ckmap, UInt8 *chunky)
{
	bmhd_t *bmhd = ckmap->bmhd;
	body_t *body = ckmap->body;
	
	if (!bmhd || !body)
	{
		return -1;
	}

	int comp = bmhd_getCompression(bmhd);
	
	if (comp != 0 && comp != 1)
	{
		return -1;
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

CGSize ilbm_render(chunkMap_t *ckmap, UInt32 *picture)
{
    if (ckmap->bmhd && ckmap->body && ckmap->cmap)
    {
        int width = bmhd_getWidth(ckmap->bmhd);
        int height = bmhd_getHeight(ckmap->bmhd);
        
        UInt8 *chunky = malloc(width*height*8);

        if (!chunky)
        {
            return CGSizeMake(0, 0);
        }
        
        if (body_unpack(ckmap, chunky) < 0)
        {
            free(chunky);
            return CGSizeMake(0, 0);
        }
        
        UInt32 *palette = malloc(256*4);
        
        if (!palette)
        {
            free(chunky);
            return CGSizeMake(0, 0);
        }
        
        if (cmap_unpack(ckmap, palette) < 0)
        {
            free(chunky);
            free(palette);
            return CGSizeMake(0, 0);
        }

        if (ckmap->camg && camg_getHAM(ckmap->camg) && bmhd_getDepth(ckmap->bmhd) == 6)
        {
            // HAM6
            
            int r=0, g=0, b=0;
            
            for (int i=0; i<width*height; i++)
            {
                int h = chunky[i] >> 4;
                int l = chunky[i] & 15;
                
                switch (h & 3)
                {
                    case 0:
                        
                        r = (palette[l] >>  8) & 255;
                        g = (palette[l] >> 16) & 255;
                        b = (palette[l] >> 24) & 255;
                        
                        break;
                        
                    case 2:
                        
                        r = l << 4;
                        
                        break;
                        
                    case 3:
                        
                        g = l << 4;
                        
                        break;
                        
                    case 1:
                        
                        b = l << 4;
                        
                        break;
                }
                
                picture[i] = (b<<24)+(g<<16)+(r<<8);
            }
        }
        else
        {
            for (int i=0; i<width*height; i++)
            {
                picture[i] = palette[chunky[i]];
            }
        }
        
        free(chunky);
        free(palette);

        int aspect = 0;
        
        if (ckmap->camg)
        {
            aspect += camg_getHires(ckmap->camg);
            aspect += camg_getSuper(ckmap->camg);
            aspect += camg_getSDbl(ckmap->camg);
            aspect -= camg_getLace(ckmap->camg);
        }
        
        if (aspect < 0)
        {
            aspect = -aspect;
            
            for (int y=height-1; y>=0; y--)
            {
                for (int x=width-1; x>=0; x--)
                {
                    for (int z=0; z<(1<<aspect); z++)
                    {
                        picture[((y*width+x)<<aspect)+z] = picture[y*width+x];
                    }
                }
            }
            width <<= aspect;
        }
        else if (aspect > 0)
        {
            for (int x=width-1; x>=0; x--)
            {
                for (int y=height-1; y>=0; y--)
                {
                    for (int z=0; z<(1<<aspect); z++)
                    {
                        picture[((y<<aspect)+z)*width+x] = picture[y*width+x];
                    }
                }
            }
            height <<= aspect;
        }
/*
        int doubleWidth = (ckmap->camg && camg_getLace(ckmap->camg) && !camg_getHires(ckmap->camg));
        int doubleHeight = (ckmap->camg && !camg_getLace(ckmap->camg) && camg_getHires(ckmap->camg));
 
        if (doubleWidth)
        {
            for (int y=height-1; y>=0; y--)
            {
                for (int x=width-1; x>=0; x--)
                {
                    picture[y*width*2+x*2+0] = picture[y*width+x];
                    picture[y*width*2+x*2+1] = picture[y*width+x];
                }
            }
            width *= 2;
        }
        else if (doubleHeight)
        {
            for (int x=width-1; x>=0; x--)
            {
                for (int y=height-1; y>=0; y--)
                {
                    picture[(y*2+0)*width+x] = picture[y*width+x];
                    picture[(y*2+1)*width+x] = picture[y*width+x];
                }
            }
            height *= 2;
        }
*/
        return CGSizeMake(width, height);
    }
    else if (ckmap->cmap)
    {
        int width = 256;
        int height = 256;
        
        UInt32 *palette = malloc(256*4);

        if (!palette)
        {
            return CGSizeMake(0, 0);
        }
        
        int numColors = cmap_unpack(ckmap, palette);
        
        if (numColors <= 0 || numColors > 256)
        {
            free(palette);
            return CGSizeMake(0, 0);
        }
        
        int n = 8*sizeof(numColors)-1-__builtin_clz(numColors); // n = log2(numColors)

        int rows = 1<<(n/2);
        int cols = 1<<(n-n/2);
        
        for (int y=0; y<height; y++)
        {
            for (int x=0; x<width; x++)
            {
                int i = cols*(y*rows/height) + (x*cols/width);
                picture[width*y+x] = palette[i];
            }
        }
        
        free(palette);

        return CGSizeMake(width, height);
    }
    
    return CGSizeMake(0, 0);
}

CGSize ilbm_getFinalSize(chunkMap_t *ckmap)
{
    CGSize size = CGSizeMake(0, 0);
    
    if (ckmap->bmhd && ckmap->body && ckmap->cmap)
    {
        size.width = bmhd_getWidth(ckmap->bmhd);
        size.height = bmhd_getHeight(ckmap->bmhd);
    }
    else if (ckmap->cmap)
    {
        size.width = 256;
        size.height = 256;
    }
    
    if (ckmap->camg)
    {
        int lace = camg_getLace(ckmap->camg);
        int hires = camg_getHires(ckmap->camg);
        
        if (hires && !lace)
        {
            size.height *= 2;
        }
        else if (lace && !hires)
        {
            size.width *= 2;
        }
    }
    
    return size;
}


CGImageRef iff_createImage(CFURLRef url)
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
    
    CGSize size = ilbm_getFinalSize(&ckmap);
    
	UInt32 *picture = malloc(4*size.width*size.height);
    
    CGSize size2 = ilbm_render(&ckmap, picture);
    
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
        CGBitmapContextCreate(picture, size2.width, size2.height, 8, 4*size2.width, colorSpace, kCGImageAlphaNoneSkipFirst);
    
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