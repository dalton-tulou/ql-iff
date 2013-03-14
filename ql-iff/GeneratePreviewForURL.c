#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <QuickLook/QuickLook.h>
#include "iff.h"

OSStatus GeneratePreviewForURL(void *thisInterface, QLPreviewRequestRef preview, CFURLRef url, CFStringRef contentTypeUTI, CFDictionaryRef options);
void CancelPreviewGeneration(void *thisInterface, QLPreviewRequestRef preview);

/* -----------------------------------------------------------------------------
   Generate a preview for file

   This function's job is to create preview for designated file
   ----------------------------------------------------------------------------- */

OSStatus GeneratePreviewForURL(void *thisInterface, QLPreviewRequestRef preview, CFURLRef url, CFStringRef contentTypeUTI, CFDictionaryRef options)
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

    int width=0, height=0;
    
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
        return noErr;
    }
	
	int width2 = (width+1)&-2;

    UInt32 *picture = malloc(sizeof(UInt32)*width*height);

    if (!picture)
    {
        return noErr;
    }
    
    error = ilbm_render(&ckmap, picture, width, height);

    if (error)
    {
        return noErr;
    }

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

    CFRelease(bitmapContext);

	CGContextRef context = QLPreviewRequestCreateContext(preview, CGSizeMake(width, height), false, NULL);
    
	if (!context)
	{
		return noErr;
	}
    
	CGContextDrawImage(context, CGRectMake(0, 0, width-1, height-1), image);    
    
	QLPreviewRequestFlushContext(preview, context);
	CFRelease(context);
    
	free(picture);
    
    return noErr;
}

void CancelPreviewGeneration(void *thisInterface, QLPreviewRequestRef preview)
{
    // Implement only if supported
}
