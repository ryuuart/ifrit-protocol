/** Every warm-up and capture paints the complete declared viewport. */
#include <include/core/SkCanvas.h>
#include <sigilcompose/draw/Draw.h>
#include <sigildraw/Pen.h>
#include <sigilsketch/scene2d/Scene2D.h>

#include <cstdio>
#include <cstdlib>

namespace {
struct CaptureViewport {
  void setup(sigil::sketch::Scene2DContext& ctx) {
    using namespace sigil::compose;
    ctx.canvas(640, 400);
    ctx.composer.render(
        pen("viewport",
            [](sigil::draw::Pen& pen, const PaintContext&) {
              const SkRect clip = pen.canvas()->getLocalClipBounds();
              if (!clip.contains(SkRect::MakeXYWH(1, 1, 638, 398))) {
                std::fprintf(stderr,
                             "capture warm-up clipped the declared viewport\n");
                std::exit(1);
              }
              pen.background(0, 255, 0);
            })
            .inset(0)
            .cache(Cache::None));
  }
};
}  // namespace

SIGIL_SKETCH(CaptureViewport, "Test",
             "the viewport survives warm-up and capture")
