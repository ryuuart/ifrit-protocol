#include <sigilfixture/Shaded.h>
#include <sigilshaders/FixtureShaded.h>

namespace sigil::fixture {

int shaderLength() {
  return static_cast<int>(shaderSource("Fixture.sksl").size());
}

}  // namespace sigil::fixture
