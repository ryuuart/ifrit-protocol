/** @file
 * ink — the glyphs painted with a material mapped to text-metric
 * space, so one ramp authored in the unit square crosses the capitals at
 * any size.
 *
 * A reference example. It stands outside the sketch registry, so it is
 * photographed with `--frame` and never enters the plate sweep.
 */

#include <sigilcompose/core/Core.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace skia = sigil::material::skia;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {620, 260};
constexpr material::Color kGround = hexColor(0x101418);
constexpr material::Color kAsh = hexColor(0x7e8f9c);

/** The chrome ramp, authored once in the unit square: the horizon sits
 *  where the ramp's middle stops meet, and lands on the capitals at
 *  whatever size the word is set. */
skia::Paint chrome() {
  return skia::Paint::linearUnit({0, 0}, {0, 1},
                                 {{0.00f, hexColor(0xf2f6f8)},
                                  {0.46f, hexColor(0x8fa6b4)},
                                  {0.52f, hexColor(0x2b3d4a)},
                                  {0.58f, hexColor(0xcfe0e8)},
                                  {1.00f, hexColor(0x6d8593)}});
}

}  // namespace

struct TextFillVerb {
  void setup(sketch::SketchContext& ctx) {
    ctx.canvas({.size = kCanvas, .background = kGround, .captureSeconds = 0});
    ctx.composer.render(describe());
  }

  Element describe() const {
    return box()
        .column()
        .gap(18)
        .padding(28)
        .justifyContent(Justify::Center)
        .children({
            text("CHROME").font({.size = 64, .track = 2}).ink(chrome()),
            text("SET SMALLER").font({.size = 26, .track = 2}).ink(chrome()),
            text("One ramp, two sizes, the same horizon.")
                .font({.size = 13, .color = kAsh}),
        });
  }
};

SIGIL_SKETCH(TextFillVerb, "Reference · Compose",
             "one unit-square ramp painting the glyphs at two sizes")
