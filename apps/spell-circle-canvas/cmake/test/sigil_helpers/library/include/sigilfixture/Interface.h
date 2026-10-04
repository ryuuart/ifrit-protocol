#pragma once

#include <concepts>
#include <span>

namespace sigil::fixture {

template <std::integral T>
T first(std::span<const T> values) {
  return values.front();
}

}  // namespace sigil::fixture
