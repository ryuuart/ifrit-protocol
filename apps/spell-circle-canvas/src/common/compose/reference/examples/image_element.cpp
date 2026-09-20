/** @file
 * image — a picture as a leaf, and the three ways it meets its box.
 *
 * The picture is drawn here rather than loaded, so the example depends on
 * no asset: a four-by-four checker, sampled nearest, whose proportions
 * differ from the cells it is shown in.
 *
 * A reference example. It stands outside the sketch registry, so it is
 * photographed with `--frame` and never enters the plate sweep.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/Core.h>
#include <sigilsketch/canvas/Sketch.h>

namespace sketch = sigil::sketch;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {620, 260};
constexpr SkColor4f kGround = hexColor(0x14181d);
constexpr SkColor4f kCell = hexColor(0x1c232a);
constexpr SkColor4f kAsh = hexColor(0x8ea0ad);

/** Eight by four pixels of checker, so a square cell shows what each fit
 *  does with the proportions. */
sk_sp<SkImage> checker() {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(8, 4));
  SkCanvas& canvas = *surface->getCanvas();
  canvas.clear(hexColor(0x25303a).toSkColor());
  SkPaint paint;
  paint.setColor4f(hexColor(0x6fb3a6), nullptr);
  for (int y = 0; y < 4; ++y)
    for (int x = 0; x < 8; ++x)
      if ((x + y) % 2 == 0)
        canvas.drawRect(SkRect::MakeXYWH((float)x, (float)y, 1, 1), paint);
  return surface->makeImageSnapshot();
}

Element cell(const char* caption, Element leaf) {
  return box()
      .column()
      .gap(8)
      .basis(0)
      .grow(1)
      .alignItems(Align::Center)
      .children({box()
                     .height(140)
                     .width(pct(100))
                     .corners({8})
                     .fill(kCell)
                     .clip()
                     .children({std::move(leaf)}),
                 text(caption).font({.size = 12, .color = kAsh})});
}

}  // namespace

struct ImageElement {
  void setup(sketch::SketchContext& ctx) {
    ctx.canvas({.size = kCanvas, .background = kGround, .captureSeconds = 0});
    ctx.composer.render(describe());
  }

  Element describe() const {
    const sk_sp<SkImage> picture = checker();
    // Nearest sampling on the container reaches every image leaf under
    // it, which is what a grid of pixels wants and a photograph does not.
    return box()
        .row()
        .gap(16)
        .padding(22)
        .sampling(SkSamplingOptions(SkFilterMode::kNearest))
        .children({cell("Fit::Contain", image(picture, Fit::Contain).cover()),
                   cell("Fit::Cover", image(picture, Fit::Cover).cover()),
                   cell("Fit::Stretch", image(picture, Fit::Stretch).cover())});
  }
};

SIGIL_SKETCH(ImageElement, "Reference · Compose",
             "the image leaf under each of the three fits")
