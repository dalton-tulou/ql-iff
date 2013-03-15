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
    CGImageRef image = iff_createImage(url);
    
    if (!image)
    {
        return noErr;
    }
    
    size_t width = CGImageGetWidth(image);
    size_t height = CGImageGetHeight(image);
    
	CGContextRef context = QLPreviewRequestCreateContext(preview, CGSizeMake(width, height), false, NULL);
    
	if (!context)
	{
		return noErr;
	}
    
	CGContextDrawImage(context, CGRectMake(0, 0, width, height), image);    
    
    CGImageRelease(image);
    
	QLPreviewRequestFlushContext(preview, context);
	CFRelease(context);
    
    return noErr;
}

void CancelPreviewGeneration(void *thisInterface, QLPreviewRequestRef preview)
{
    // Implement only if supported
}
