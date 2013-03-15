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
    CGImageRef image = iff_createImage(url);
    
    if (!image)
    {
        return noErr;
    }
    
    size_t width = CGImageGetWidth(image);
    size_t height = CGImageGetHeight(image);
    
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

	CGContextDrawImage(context, CGRectMake(0, 0, width, height), image);

    CGImageRelease(image);
    
	QLThumbnailRequestFlushContext(thumbnail, context);
	CFRelease(context);
    
    return noErr;
}

void CancelThumbnailGeneration(void *thisInterface, QLThumbnailRequestRef thumbnail)
{
    // Implement only if supported
}
