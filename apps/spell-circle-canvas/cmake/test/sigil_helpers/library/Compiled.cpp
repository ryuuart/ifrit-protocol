#include <sigilfixture/Compiled.h>

namespace sigil::fixture {

int sum(std::span<const int> values) {
  int total = 0;
  for (const int value : values) total += value;
  return total;
}

}  // namespace sigil::fixture
