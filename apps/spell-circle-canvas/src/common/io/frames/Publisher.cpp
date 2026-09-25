/** @file
 * The one place that knows what this build can publish over. Everything
 * else in this repository holds a `Publisher` or holds nothing.
 */

#include <sigilio/frames/Publisher.h>

#include <utility>

#if defined(__APPLE__)
#include "SyphonPublisher.h"
#elif defined(SIGIL_FRAMES_SPOUT)
#include "SpoutPublisher.h"
#endif

namespace sigil::io::frames {

Publisher createPublisher(std::string name, Backend backend,
                                           void* nativeDevice) {
  if (name.empty() || !nativeDevice) return {};
#if defined(__APPLE__)
  if (backend == Backend::Metal)
    return Publisher(makeSyphonPublisher(std::move(name), nativeDevice));
#elif defined(SIGIL_FRAMES_SPOUT)
  if (backend == Backend::Direct3D11)
    return Publisher(makeSpoutPublisher(std::move(name), nativeDevice));
#endif
  return {};
}

}  // namespace sigil::io::frames
