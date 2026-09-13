// The directory of publications, and the run loop it is heard over.

#import <Syphon/SyphonServerDirectory.h>

#include "Servers.h"

namespace receiver {

namespace {

/** The interval a wait is broken into: short enough that a wait ends on
 *  what it was waiting for rather than on a tick. */
constexpr double kSlice = 0.02;

}  // namespace

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

void hearWhileInactive() { [[NSDistributedNotificationCenter defaultCenter] setSuspended:NO]; }

NSArray<NSDictionary<NSString *, id> *> *publications(double settleSeconds) {
  turnRunLoop(settleSeconds);
  return [[SyphonServerDirectory sharedDirectory] servers];
}

NSDictionary<NSString *, id> *publicationNamed(NSString *name, NSString *app,
                                               double withinSeconds) {
  const NSTimeInterval deadline = [NSDate timeIntervalSinceReferenceDate] + withinSeconds;
  for (;;) {
    NSArray<NSDictionary<NSString *, id> *> *matching =
        [[SyphonServerDirectory sharedDirectory] serversMatchingName:name appName:app];
    if (matching.count > 0) return matching.firstObject;
    if ([NSDate timeIntervalSinceReferenceDate] >= deadline) return nil;
    turnRunLoop(kSlice);
  }
}

NSString *publicationName(NSDictionary<NSString *, id> *publication) {
  NSString *name = publication[SyphonServerDescriptionNameKey];
  return name ? name : @"";
}

NSString *publicationApp(NSDictionary<NSString *, id> *publication) {
  NSString *app = publication[SyphonServerDescriptionAppNameKey];
  return app ? app : @"";
}

NSString *publicationIdentity(NSDictionary<NSString *, id> *publication) {
  return publication[SyphonServerDescriptionUUIDKey];
}

bool publicationIsListed(NSString *identity) {
  // A publisher that named no identity cannot be told from another under
  // the same name, so the question it answers is not asked of it.
  if (!identity) return true;
  for (NSDictionary<NSString *, id> *publication in
       [[SyphonServerDirectory sharedDirectory] servers])
    if ([identity isEqualToString:publicationIdentity(publication)]) return true;
  return false;
}

}  // namespace receiver
