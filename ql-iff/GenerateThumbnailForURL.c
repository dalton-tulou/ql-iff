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
    const UInt8 *bytePtr = CFDataGetBytePtr(data);
    
	chunkMap_t ckmap;
	
    int error;
    
	error = iff_mapChunks(bytePtr, length, &ckmap);
	
    if (error)
    {
        return noErr;
    }
    
	if (!ckmap.bmhd || !ckmap.cmap || !ckmap.body)
	{
		return noErr;
	}
	
	int width = bmhd_getWidth(ckmap.bmhd);
	int height = bmhd_getHeight(ckmap.bmhd);
	
	int width2 = (width+1)&-2;
	
	UInt32 *picture = malloc(4*width*height);

    error = ilbm_render(&ckmap, picture);
    
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
    
	if (!bitmapContext)
	{
		return noErr;
	}
	
	CGColorSpaceRelease(colorSpace);
	
	CGImageRef image = CGBitmapContextCreateImage(bitmapContext);
    
    if (width > maxSize.width)
    {
        height = height*maxSize.width/width;
        width = maxSize.width;
    }
    
    if (height > maxSize.height)
    {
        width = width*maxSize.height/height;
        height = maxSize.height;
    }

    CGContextRef context = QLThumbnailRequestCreateContext(thumbnail, CGSizeMake(width, height), false, NULL);
    
	if (!context)
	{
		return noErr;
	}

	CGContextDrawImage(context, CGRectMake(0, 0, width-1, height-1), image);
    
	CFRelease(bitmapContext);
    
	QLThumbnailRequestFlushContext(thumbnail, context);
	CFRelease(context);
    
	free(picture);
    
    return noErr;
}

void CancelThumbnailGeneration(void *thisInterface, QLThumbnailRequestRef thumbnail)
{
    // Implement only if supported
}
