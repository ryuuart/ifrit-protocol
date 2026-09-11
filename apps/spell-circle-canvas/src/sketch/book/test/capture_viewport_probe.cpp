/** Every warm-up and capture paints the complete declared viewport. */
#include <include/core/SkCanvas.h>
#include <sigilsketch/canvas/Sketch.h>

#include <cstdio>
#include <cstdlib>

namespace {
struct CaptureViewport final : sigil::sketch::Sketch {
  void setup(sigil::sketch::SketchContext& ctx) override {
    using namespace sigil::compose;
    ctx.canvas(640, 400);
    ctx.composer.render(
        custom("viewport",
               [](SkCanvas& canvas, const PaintContext&) {
                 const SkRect clip = canvas.getLocalClipBounds();
                 if (!clip.contains(SkRect::MakeXYWH(1, 1, 638, 398))) {
                   std::fprintf(
                       stderr,
                       "capture warm-up clipped the declared viewport\n");
                   std::exit(1);
                 }
                 canvas.clear(SK_ColorGREEN);
               })
            .inset(0)
            .cache(Cache::None));
  }
};
}  // namespace

SIGIL_SKETCH(CaptureViewport, "Test",
             "the viewport survives warm-up and capture")
