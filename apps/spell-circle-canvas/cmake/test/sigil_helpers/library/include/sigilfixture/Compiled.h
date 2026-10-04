#pragma once

#include <concepts>
#include <span>

namespace sigil::fixture {

template <class T>
concept Integer = std::integral<T>;

int sum(std::span<const int> values);

}  // namespace sigil::fixture
