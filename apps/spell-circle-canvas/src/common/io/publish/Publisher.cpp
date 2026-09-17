/** @file
 * The one place that knows what this build can publish over. Everything
 * else in this repository holds a `Publisher` or holds nothing.
 */

#include <sigilio/publish/Publisher.h>

#include <utility>

#if defined(__APPLE__)
#include "SyphonPublisher.h"
#elif defined(SIGIL_PUBLISH_SPOUT)
#include "SpoutPublisher.h"
#endif

namespace sigil::io::publish {

std::unique_ptr<Publisher> createPublisher(std::string name, Backend backend,
                                           void* nativeDevice) {
  if (name.empty() || !nativeDevice) return nullptr;
#if defined(__APPLE__)
  if (backend == Backend::Metal)
    return makeSyphonPublisher(std::move(name), nativeDevice);
#elif defined(SIGIL_PUBLISH_SPOUT)
  if (backend == Backend::Direct3D11)
    return makeSpoutPublisher(std::move(name), nativeDevice);
#endif
  return nullptr;
}

}  // namespace sigil::io::publish
