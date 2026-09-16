// The directory of publications, and the run loop it is heard over.

#import <Syphon/SyphonServerDirectory.h>

#include "Servers.h"

namespace receiver {

void turnRunLoop(double seconds) {
  NSDate *until = [NSDate dateWithTimeIntervalSinceNow:seconds];
  while ([until timeIntervalSinceNow] > 0) {
    // A run loop with nothing attached to it answers straight away, so
    // the rest of the wait is slept through rather than spun on.
    if (![[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:until]) {
      [NSThread sleepUntilDate:until];
      return;
    }
  }
}

NSArray<NSDictionary<NSString *, id> *> *publications(double settleSeconds) {
  turnRunLoop(settleSeconds);
  return [[SyphonServerDirectory sharedDirectory] servers];
}

NSString *publicationName(NSDictionary<NSString *, id> *publication) {
  NSString *name = publication[SyphonServerDescriptionNameKey];
  return name ? name : @"";
}

NSString *publicationApp(NSDictionary<NSString *, id> *publication) {
  NSString *app = publication[SyphonServerDescriptionAppNameKey];
  return app ? app : @"";
}

}  // namespace receiver
