#import <Foundation/Foundation.h>

#include "AppNap.h"

namespace {
// Retained for the lifetime of the process to keep the activity active.
id<NSObject> g_activityToken = nil;
} // namespace

namespace AppNap {

void disable() {
  if (g_activityToken)
    return;

  g_activityToken = [[NSProcessInfo processInfo]
      beginActivityWithOptions:NSActivityUserInitiated |
                                NSActivityLatencyCritical
                        reason:@"Continuous Syphon publishing and UDP "
                               @"scene updates require full-rate execution"];
}

} // namespace AppNap
