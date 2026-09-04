#pragma once

/** @file The pin counts shared by a Hub and the leases issued from it. */

#include <cstddef>
#include <map>
#include <mutex>
#include <string>

namespace sigil::io::detail {

struct Residency {
  std::mutex mutex;
  std::map<std::string, std::size_t, std::less<>> pins;
};

struct Synchronization {
  mutable std::recursive_mutex mutex;
};

}  // namespace sigil::io::detail
