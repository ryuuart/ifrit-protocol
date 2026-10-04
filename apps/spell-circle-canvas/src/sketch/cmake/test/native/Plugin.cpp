#include <Boundary.h>
#include <sigilcompose/core/Core.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/core/Registry.h>

#include <cstdlib>
#include <fstream>

namespace {
struct NativeExample {
  NativeExample() {
    if (const char* marker = std::getenv("SIGIL_NATIVE_FACTORY_MARKER"))
      std::ofstream(marker) << "opened";
  }
  void setup(sigil::sketch::SketchContext& ctx) {
    ctx.canvas(BoundaryValue{}.width, 19);
    ctx.composer.render(sigil::compose::box().width(37).height(19));
  }
};
}  // namespace
SIGIL_SKETCH(NativeExample, "Example", "Native toolchain plugin")
