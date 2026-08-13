#include "mac_app_activation.hh"

#import <AppKit/NSApplication.h>

namespace MacAppActivation {

void setDockIconVisible( bool visible )
{
  const NSApplicationActivationPolicy policy =
    visible ? NSApplicationActivationPolicyRegular : NSApplicationActivationPolicyAccessory;

  if ( NSApp.activationPolicy != policy ) {
    [ NSApp setActivationPolicy:policy ];
  }
}

void activate()
{
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 140000
  if ( @available( macOS 14.0, * ) ) {
    [ NSApp activate ];
  }
  else {
    [ NSApp activateIgnoringOtherApps:YES ];
  }
#else
  [ NSApp activateIgnoringOtherApps:YES ];
#endif
}

} // namespace MacAppActivation
