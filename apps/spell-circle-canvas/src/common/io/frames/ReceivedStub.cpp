// Where no frames arrive, no arrival is retained or bound.

#include "Received.h"

namespace sigil::io::frames::detail {

std::shared_ptr<void> retainTexture(void*) { return nullptr; }

std::shared_ptr<media::DeviceBinding> bindArrival(std::shared_ptr<void>) {
  return nullptr;
}

}  // namespace sigil::io::frames::detail
