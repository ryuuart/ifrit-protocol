/** @file
 * The one place that knows what this build can subscribe over.
 * Everything else in this repository holds a `Subscription` or holds
 * nothing.
 */

#include <sigilsketch/publish/Subscription.h>

#include <utility>

#if defined(__APPLE__)
#include "SyphonSubscription.h"
#endif

namespace sigil::sketch {

std::unique_ptr<Subscription> subscribe(std::string name,
                                        std::string application,
                                        void* mtlDevice) {
  if (name.empty() || !mtlDevice) return nullptr;
#if defined(__APPLE__)
  return makeSyphonSubscription(std::move(name), std::move(application),
                                mtlDevice);
#else
  return nullptr;
#endif
}

void* defaultMetalDevice() {
#if defined(__APPLE__)
  return metalDeviceOfThisMachine();
#else
  return nullptr;
#endif
}

}  // namespace sigil::sketch
