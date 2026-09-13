#pragma once

/** @file
 * WHAT IS BEING OFFERED ON THIS MACHINE. Syphon's directory hears of a
 * publication through a notification between processes, and those arrive
 * on a run loop — so a process that has only just started knows nothing
 * at all until it has let the loop turn. Every function here turns it for
 * as long as it is given before it answers, which is why none of them is
 * a plain read.
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

/** KEEPS HEARING WHILE THIS APPLICATION IS NOT THE ACTIVE ONE. What one
 *  process has to say to another is held back from an application that is
 *  not in front until it comes forward, and a window watching a
 *  publication is watching it from behind whatever the reader is working
 *  in — so it is told to listen anyway. A run with no application of its
 *  own is never held back and never needs this. */
void hearWhileInactive();

/** Every publication the directory has heard of after the run loop has
 *  turned for @p settleSeconds. Each element is the description a
 *  subscription is opened from. */
NSArray<NSDictionary<NSString *, id> *> *publications(double settleSeconds);

/** The first publication announced under @p name — and, when @p app is
 *  not empty, by that application — waiting up to @p withinSeconds for
 *  one to answer. Nil when none did. Zero seconds asks what the directory
 *  knows already, which is what a run whose loop is turning anyway does.
 */
NSDictionary<NSString *, id> *publicationNamed(NSString *name, NSString *app, double withinSeconds);

/** What a publication calls itself, and the application it is drawn in.
 *  Either may be missing from a description and reads as empty. */
NSString *publicationName(NSDictionary<NSString *, id> *publication);
NSString *publicationApp(NSDictionary<NSString *, id> *publication);

/** Which publication this is — the one thing about a description that
 *  tells one publication from another one of the same name in the same
 *  application. Nil when the publisher did not say. */
NSString *publicationIdentity(NSDictionary<NSString *, id> *publication);

/** Whether the publication @p identity names is still one the directory
 *  knows of.
 *
 *  A PUBLISHER THAT DIED SAYS NOTHING, so a subscription to it stays
 *  open onto frames that will never arrive: what a client is told is that
 *  a server retired, and a process that was killed never told anybody.
 *  The directory finds out anyway — it asks everything it knows of to
 *  announce itself whenever any application on this machine starts
 *  looking, and drops whatever does not answer — so a publication that is
 *  no longer listed is one to stop waiting on. */
bool publicationIsListed(NSString *identity);

}  // namespace receiver
