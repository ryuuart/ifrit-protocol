/** @file
 * The one place that knows what this build can publish over. Everything
 * else in this repository holds a `Publisher` or holds nothing.
 */

#include <sigilsketch/publish/Publisher.h>

#include <utility>

#if defined(__APPLE__)
#include "SyphonPublisher.h"
#endif

namespace sigil::sketch {

std::unique_ptr<Publisher> createPublisher(std::string name, void* mtlDevice) {
  if (name.empty() || !mtlDevice) return nullptr;
#if defined(__APPLE__)
  return makeSyphonPublisher(std::move(name), mtlDevice);
#else
  return nullptr;
#endif
}

}  // namespace sigil::sketch
