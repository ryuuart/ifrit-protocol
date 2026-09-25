/** @file
 * The one place that knows what this build can subscribe over.
 * Everything else in this repository holds a `Subscription` or holds
 * nothing.
 */

#include <sigilio/frames/Subscription.h>

#include <utility>

#if defined(__APPLE__)
#include "SyphonSubscription.h"
#endif

namespace sigil::io::frames {

std::vector<Publication> publications() {
#if defined(__APPLE__)
  return syphonPublications();
#else
  return {};
#endif
}

Subscription subscribe(std::string name,
                                        std::string application,
                                        void* metalDevice) {
  if (name.empty() || !metalDevice) return {};
#if defined(__APPLE__)
  return Subscription(makeSyphonSubscription(
      std::move(name), std::move(application), metalDevice));
#else
  return {};
#endif
}

void* defaultMetalDevice() {
#if defined(__APPLE__)
  return metalDeviceOfThisMachine();
#else
  return {};
#endif
}

}  // namespace sigil::io::frames
