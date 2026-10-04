#include <sigilcompose/core/Core.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/core/Registry.h>
#ifdef SIGIL_TEST_PLUGIN_USE_KIT
#include <sigilcompose/kit/Frame.h>
#endif

namespace {
struct Example {
  void setup(sigil::sketch::SketchContext& ctx) {
    ctx.canvas(37, 19);
#ifdef SIGIL_TEST_PLUGIN_USE_KIT
    ctx.composer.render(sigil::compose::kit::at(0, 0, 37, 19));
#else
    ctx.composer.render(sigil::compose::box().width(37).height(19));
#endif
  }
};
}  // namespace
SIGIL_SKETCH(Example, "Example", "External toolchain plugin")
