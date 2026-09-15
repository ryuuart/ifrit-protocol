#pragma once

/** @file
 * WHAT IS BEING OFFERED ON THIS MACHINE, and the run loop it is heard
 * over. Syphon's directory hears of a publication through a notification
 * between processes, and those arrive on a run loop — so a process that
 * has only just started knows nothing at all until it has let the loop
 * turn, and a listing is a wait rather than a plain read.
 */

#import <Foundation/Foundation.h>

namespace receiver {

/** How long a listing collects announcements before it answers. A
 *  publication answers a request to announce itself as soon as it hears
 *  it, so what this covers is one round trip between two processes and
 *  not a publisher's own work. */
constexpr double kAnnounceSeconds = 0.5;

/** Lets the run loop turn for @p seconds, which is how anything another
 *  process has to say reaches this one. It returns when the time is up,
 *  however many times something arrived on the way. */
void turnRunLoop(double seconds);

/** Every publication the directory has heard of after the run loop has
 *  turned for @p settleSeconds. Each element is the description a
 *  subscription is opened from. */
NSArray<NSDictionary<NSString *, id> *> *publications(double settleSeconds);

/** What a publication calls itself, and the application it is drawn in.
 *  Either may be missing from a description and reads as empty. */
NSString *publicationName(NSDictionary<NSString *, id> *publication);
NSString *publicationApp(NSDictionary<NSString *, id> *publication);

}  // namespace receiver
