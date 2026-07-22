// The registry behind SIGIL_SKETCH's static mode (see Sketch.h). Sketch
// files compiled into a host register here from a namespace-scope
// initializer, so a host that links them gets its scene list from the
// files themselves rather than from a table someone has to remember to
// update.

#include "sigilsketch/Sketch.h"

namespace sigil::compose::sketch {

std::vector<StaticSketch> &staticSketches() {
  // Function-local: registrations run during static initialization, and a
  // namespace-scope vector could be constructed after the first of them.
  static std::vector<StaticSketch> registry;
  return registry;
}

bool registerStaticSketch(const char *key, SketchFactory factory) {
  if (key && factory)
    staticSketches().push_back({key, factory});
  return true;
}

SketchFactory findStaticSketch(std::string_view key) {
  for (const StaticSketch &entry : staticSketches())
    if (key == entry.key)
      return entry.factory;
  return nullptr;
}

} // namespace sigil::compose::sketch
