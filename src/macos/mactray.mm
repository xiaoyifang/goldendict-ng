#include "mactray.hh"

#import <AppKit/AppKit.h>
#import <ServiceManagement/SMAppService.h>

void MacTray::setDockIconVisible( bool visible )
{
    NSApplicationActivationPolicy policy =
        visible ? NSApplicationActivationPolicyRegular : NSApplicationActivationPolicyAccessory;
    [NSApp setActivationPolicy:policy];
}

void MacTray::activateApplication()
{
    [NSApp activateIgnoringOtherApps:YES];
}

bool MacTray::setAutoStartEnabled( bool enabled )
{
    if ( @available(macOS 13.0, *) ) {
        NSError *error = nil;
        BOOL result;
        if ( enabled ) {
            result = [SMAppService.mainAppService registerAndReturnError:&error];
        }
        else {
            result = [SMAppService.mainAppService unregisterAndReturnError:&error];
        }
        if ( !result && error ) {
            NSLog( @"GoldenDict-ng: failed to %@ login item: %@",
                   enabled ? @"register" : @"unregister", error );
            return false;
        }
        return true;
    }
    return false;
}
