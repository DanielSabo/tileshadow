#include "machelpers.h"
#import <AppKit/NSWindow.h>

void disableAutoTabBar()
{
#if defined(MAC_OS_X_VERSION_10_12) && MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_12
    // Disable the "View" -> "Show Tab Bar" menu entry
    if ([NSWindow respondsToSelector:@selector(setAllowsAutomaticWindowTabbing:)])
        [NSWindow setAllowsAutomaticWindowTabbing: NO];
#endif
}
