/** A prebuilt module exposes its capture canvas and retains glyphs and holes.
 */
#include <include/core/SkCanvas.h>
#include <sigilcompose/core/Core.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/core/Device.h>
#include <sigilsketch/core/Registry.h>

#include <cstdio>

namespace {

struct PluginCapture {
  void setup(sigil::sketch::SketchContext& ctx) {
    std::puts(sigil::sketch::device() ? "PLUGIN_CAPTURE world_device=present"
                                      : "PLUGIN_CAPTURE world_device=absent");
    ctx.canvas(320, 160);
    ctx.oversample(1);
    ctx.captureAt(0.25);
    ctx.background({0.05f, 0.06f, 0.08f, 1});
    describe(0, ctx);
  }

  void update(double elapsed, sigil::sketch::SketchContext& ctx) {
    describe(elapsed, ctx);
  }

  void describe(double elapsed, sigil::sketch::SketchContext& ctx) {
    namespace compose = sigil::compose;
    ctx.composer.render(compose::stack().children(
        {compose::box()
             .rect(16, 16, 96, 96)
             .shape(sigil::geometry::shapes::annulus(0.5f))
             .fill({1, 0.5f, 0, 1}),
         compose::text("GLYPH")
             .rect(128, 20, 160, 56)
             .font({.size = 32})
             .ink({1, 1, 1, 1}),
         compose::box()
             .rect(288, 112, 24, 32)
             .fill(elapsed < 0.5 ? sigil::material::Color{1, 0, 0, 1}
                                 : sigil::material::Color{0, 0, 1, 1}),
         compose::custom(
             "capture-backend",
             [](sigil::draw::Pen& pen, const compose::PaintContext&) {
               if (pen.canvas()->recorder())
                 std::puts("PLUGIN_CAPTURE recorder=Graphite");
             })
             .rect(0, 0, 1, 1)
             .cache(compose::Cache::None)}));
  }
};

}  // namespace

SIGIL_SKETCH(PluginCapture, "Test", "Native module capture coverage")
