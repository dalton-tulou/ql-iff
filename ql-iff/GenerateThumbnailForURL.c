#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <QuickLook/QuickLook.h>
#include "iff.h"

OSStatus GenerateThumbnailForURL(void *thisInterface, QLThumbnailRequestRef thumbnail, CFURLRef url, CFStringRef contentTypeUTI, CFDictionaryRef options, CGSize maxSize);
void CancelThumbnailGeneration(void *thisInterface, QLThumbnailRequestRef thumbnail);

/* -----------------------------------------------------------------------------
    Generate a thumbnail for file

   This function's job is to create thumbnail for designated file as fast as possible
   ----------------------------------------------------------------------------- */

OSStatus GenerateThumbnailForURL(void *thisInterface, QLThumbnailRequestRef thumbnail, CFURLRef url, CFStringRef contentTypeUTI, CFDictionaryRef options, CGSize maxSize)
{
	CFDataRef data;
	
	int errorCode;
	
	Boolean success = CFURLCreateDataAndPropertiesFromResource(NULL, url, &data, NULL, NULL, &errorCode);
	
	if (!success)
	{
		return errorCode;
	}
	
	CFIndex length = CFDataGetLength(data);
    
	form_t *form = (void *)CFDataGetBytePtr(data);
	
	chunkMap_t ckmap;
	
	iff_mapChunks(form, &ckmap);
	
	if (!ckmap.bmhd || !ckmap.cmap || !ckmap.body)
	{
		return noErr;
	}
	
	int width = bmhd_getWidth(ckmap.bmhd);
	int height = bmhd_getHeight(ckmap.bmhd);
	
	int width2 = (width+1)&-2;
	
	CGContextRef context = QLThumbnailRequestCreateContext(thumbnail, CGSizeMake(width, height), false, NULL);
    
	if (!context)
	{
		return noErr;
	}
    
	UInt8 *chunky = malloc(width*height);
	UInt32 *palette = malloc(256*4);
	UInt32 *picture = malloc(4*width*height);
	
	body_unpack(&ckmap, chunky);
	cmap_unpack(&ckmap, palette);
	iblm_makePicture(&ckmap, chunky, palette, picture);
    
	/*{
     int numColors = 1 << bmhd_getDepth(ckmap.bmhd);
     
     for (int y=0; y<height/16; y++)
     {
     for (int x=0; x<width; x++)
     {
     int i = x*numColors/width;
     picture[width*y+x] = palette[i];
     }
     }
     }*/
	
	CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
    
	if (!colorSpace)
	{
		return noErr;
	}
	
	CGContextRef bitmapContext =
    CGBitmapContextCreate(picture, width, height, 8, 4*width2, colorSpace, kCGImageAlphaNoneSkipFirst);
    
	if (!context)
	{
		return noErr;
	}
	
	CGColorSpaceRelease(colorSpace);
	
	CGImageRef image = CGBitmapContextCreateImage(bitmapContext);
    
	CGContextDrawImage(context, CGRectMake(0, 0, width-1, height-1), image);
    
	CFRelease(bitmapContext);
    
	QLThumbnailRequestFlushContext(thumbnail, context);
	CFRelease(context);
    
	free(chunky);
	free(palette);
	free(picture);
    
    return noErr;
}

void CancelThumbnailGeneration(void *thisInterface, QLThumbnailRequestRef thumbnail)
{
    // Implement only if supported
}
