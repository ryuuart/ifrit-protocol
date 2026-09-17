#pragma once

/** @file
 * The Syphon client behind the seam, reached only by the factory beside
 * it: Objective-C stays inside the one translation unit that talks to
 * the framework.
 */

#include <sigilio/publish/Subscription.h>

#include <memory>
#include <string>

namespace sigil::io::publish {

std::vector<Publication> syphonPublications();

/** A subscription to the publication @p name — and, where
 *  @p application is not empty, that application's — receiving on
 *  @p mtlDevice. It stands whether or not anything is publishing yet,
 *  and is null only when a client could not be stood up at all. */
std::unique_ptr<Subscription> makeSyphonSubscription(std::string name,
                                                     std::string application,
                                                     void* mtlDevice);

/** This machine's own Metal device, as an `id<MTLDevice>` bridged to
 *  `void*`; null where there is none. */
void* metalDeviceOfThisMachine();

}  // namespace sigil::io::publish
