// reserved_word_probe.cpp — a sketch whose device shader fails to compile.
//
// The reproduction vehicle for "a sketch that fails to load leaves the
// window unable to load any other". It is not in the registry and the
// build does not compile it: it is opened BY PATH so the live host
// compiles and dlopens it, the same way a workspace file is.
//
// Its custom leaf runs an SkSL runtime shader that names a variable
// `pos` — a name Graphite reserves — so on the device the pipeline fails
// to compile. Open it in the real window on the device, then open a good
// sketch, and the good one must render:
//
//   Sketchbook src/sketch/book/test/reserved_word_probe.cpp --gpu
//   # then, from the browser, present any other sketch — it must draw.
//
// Headless this cannot be seen: the failure is the window's and not the
// host's, and a host opened for a second file is fresh. What must hold is
// that a failed Graphite frame drops ONE session and keeps the context
// and the other residents, and that every exit re-requests a frame, so
// the window goes on asking and the next selection reopens.

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilsketch/canvas/Sketch.h>

namespace sketch = sigil::sketch;
using namespace sigil::compose;

namespace {

struct ReservedWordProbe final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(640, 400);
    ctx.background({0.02f, 0.02f, 0.04f, 1});
    ctx.captureAt(0.1);
    ctx.composer.render(
        custom("reserved_word_probe.leaf",
               [](SkCanvas& canvas, const PaintContext& pc) {
                 // `pos` is reserved by Graphite: on the device this program
                 // fails to compile, which is the whole point of the probe.
                 static const auto compiled = SkRuntimeEffect::MakeForShader(
                     SkString("half4 main(float2 fragCoord) {"
                              "  float2 pos = fragCoord / 64.0;"
                              "  return half4(half(fract(pos.x)), "
                              "half(fract(pos.y)), 0.4, 1.0);"
                              "}"));
                 if (!compiled.effect) return;
                 SkPaint paint;
                 paint.setShader(compiled.effect->makeShader(nullptr, {}));
                 canvas.drawRect(
                     SkRect::MakeWH(pc.size.width(), pc.size.height()), paint);
               }));
  }
};

}  // namespace

SIGIL_SKETCH(ReservedWordProbe, "Test",
             "a device shader that fails to compile, opened by path")
