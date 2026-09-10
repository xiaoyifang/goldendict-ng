#include "fullscreen_aux.hh"
#include <QWindow>
#import <AppKit/AppKit.h>

static bool ConfigurePanel( QWindow * window, NSWindow ** outWindow )
{
  NSView * view = window ? (NSView *) window->winId() : nil;
  NSWindow * nsWindow = [ view window ];
  if ( !window || !view || !nsWindow ) {
    return false;
  }
  [ nsWindow setLevel: NSScreenSaverWindowLevel ];
  // Non-activating panels are the only windows macOS lets float above another
  // app's fullscreen space without disturbing it. A non-activating panel can
  // still receive keyboard input after a click, so the popup's search box
  // stays usable.
  NSUInteger auxMask = nsWindow.styleMask;
  auxMask |= NSWindowStyleMaskNonactivatingPanel;
  [ nsWindow setStyleMask: auxMask ];
  [ nsWindow setCollectionBehavior:
      NSWindowCollectionBehaviorCanJoinAllSpaces
      | NSWindowCollectionBehaviorFullScreenAuxiliary
      | NSWindowCollectionBehaviorStationary ];
  [ nsWindow setHidesOnDeactivate: NO ];
  if ( outWindow ) {
    *outWindow = nsWindow;
  }
  return true;
}

void MakeWindowFullscreenAuxiliary( QWindow * window )
{
  NSWindow * nsWindow = nil;
  if ( !ConfigurePanel( window, &nsWindow ) ) {
    return;
  }
  // orderFrontRegardless makes the panel appear above other apps' fullscreen
  // content while the app stays inactive; without this the window stays on
  // the normal space even with fullScreenAuxiliary set.
  [ nsWindow orderFrontRegardless ];
}

bool IsFrontmostAppFullscreen()
{
  NSRunningApplication * front = [ NSWorkspace sharedWorkspace ].frontmostApplication;
  if ( !front ) {
    return false;
  }
  NSArray<NSDictionary *> * windows = (__bridge_transfer NSArray *)CGWindowListCopyWindowInfo(
      kCGWindowListOptionOnScreenOnly, kCGNullWindowID );
  for ( NSDictionary * info in windows ) {
    NSNumber * pid = info[ (NSString *)kCGWindowOwnerPID ];
    if ( !pid || pid.intValue != front.processIdentifier ) {
      continue;
    }
    NSNumber * layer = info[ (NSString *)kCGWindowLayer ];
    if ( layer.intValue != 0 ) {
      continue;
    }
    NSDictionary * bounds = info[ (NSString *)kCGWindowBounds ];
    if ( !bounds ) {
      continue;
    }
    CGRect rect = CGRectZero;
    CGRectMakeWithDictionaryRepresentation( (CFDictionaryRef)bounds, &rect );
    if ( CGRectIsEmpty( rect ) ) {
      continue;
    }
    for ( NSScreen * screen in [ NSScreen screens ] ) {
      // Treat a window as fullscreen when it covers at least 95% of a screen
      // (exact equality can fail by a pixel on some displays).
      CGRect intersection = CGRectIntersection( rect, screen.frame );
      CGFloat screenArea = screen.frame.size.width * screen.frame.size.height;
      CGFloat covered = intersection.size.width * intersection.size.height;
      CGFloat ratio = screenArea > 0 ? covered / screenArea : 0.0;
      if ( ratio >= 0.95 ) {
        return true;
      }
    }
  }
  return false;
}

void ConfigureWindowAsAuxiliaryPanel( QWindow * window )
{
  ( void )ConfigurePanel( window, nil );
}
