// brush_custom.cpp — a brush built out of pictures, the way a painting
// program means the word.
//
// One shape source and one grain source, both drawn here rather than read
// from disk, carried through the three things an imported brush states: the
// spacing and scatter a stamp is placed by, the grain standing still on the
// surface or riding each dab, and a curve the pressure drives.

#include <include/core/SkBitmap.h>
#include <sigildraw/brush/Brush.h>
#include <sigilsketch/draw/Draw.h>

#include <array>
#include <cmath>

namespace sketch = sigil::sketch;
namespace brush = sigil::draw::brush;
using namespace sigil::draw;

namespace {

constexpr SkColor4f kInk{0.09f, 0.10f, 0.14f, 1.0f};
constexpr SkColor4f kRust{0.72f, 0.28f, 0.14f, 1.0f};
constexpr SkColor4f kSea{0.10f, 0.36f, 0.44f, 1.0f};

/** The shape source: a chisel with a soft edge, dark on white, which is
 *  the artwork a tip is usually drawn as. */
sk_sp<SkImage> chiselTip() {
  constexpr int kSide = 96;
  SkBitmap artwork;
  artwork.allocN32Pixels(kSide, kSide, true);
  artwork.eraseColor(SK_ColorWHITE);
  for (int y = 0; y < kSide; ++y)
    for (int x = 0; x < kSide; ++x) {
      const float across = ((float)x - 47.5f) / 46.0f;
      const float along = ((float)y - 47.5f) / 30.0f;
      const float edge = std::hypot(across, along);
      const float ink = std::clamp(1.6f - edge * 1.6f, 0.0f, 1.0f);
      const uint32_t level = (uint32_t)((1.0f - ink * ink) * 255.0f);
      *artwork.getAddr32(x, y) = SkPreMultiplyARGB(255, level, level, level);
    }
  artwork.setImmutable();
  return SkImages::RasterFromBitmap(artwork);
}

/** The grain source: a tile of soft speckle, its luminance the coverage
 *  a mark keeps. */
sk_sp<SkImage> paperGrain() {
  constexpr int kSide = 128;
  SkBitmap texture;
  texture.allocN32Pixels(kSide, kSide, true);
  uint32_t state = 0x2545f491u;
  for (int y = 0; y < kSide; ++y)
    for (int x = 0; x < kSide; ++x) {
      state = state * 1664525u + 1013904223u;
      const uint32_t speckle = 110u + (state >> 24) % 146u;
      *texture.getAddr32(x, y) =
          SkPreMultiplyARGB(255, speckle, speckle, speckle);
    }
  texture.setImmutable();
  return SkImages::RasterFromBitmap(texture);
}

/** A brush whose whole look is its two pictures. */
brush::Tool imported(sk_sp<SkImage> shape, SkColor4f color, float width) {
  brush::Tool tool = brush::marker(color, width);
  tool.tip = brush::Tip::Image;
  tool.opacity = 0.75f;
  tool.markerTip = false;
  tool.rotation = brush::Rotation::Natural;
  tool.shape = brush::Shape{.image = std::move(shape), .spacing = 0.09f};
  return tool;
}

struct BrushCustom final : sketch::DrawSketch {
  sk_sp<SkImage> shape;
  sk_sp<SkImage> grain;

  void setup(sketch::DrawContext& context) override {
    context.canvas(1000, 760);
    context.pen.randomSeed(0xC5A17u);
    context.pen.noiseSeed(0xC5A17u);
    shape = chiselTip();
    grain = paperGrain();
  }

  /** One arc of the same centreline, so every row differs only by what
   *  the brush says. */
  static brush::Stroke sweep(float y) {
    std::array<brush::Sample, 4> controls{{
        {{70, y + 26}, 0.35f},
        {{330, y - 30}, 1.0f},
        {{640, y + 34}, 0.9f},
        {{930, y - 18}, 0.3f},
    }};
    return brush::spline(controls, 2.0f, 0.7f);
  }

  void draw(sketch::DrawContext& context) override {
    Pen& pen = context.pen;
    pen.background(246, 243, 236);

    // The shape alone: spacing and scatter stated against the stamp.
    brush::Tool plain = imported(shape, kInk, 46.0f);
    brush::paint(pen, plain, sweep(120));

    brush::Tool thrown = imported(shape, kInk, 40.0f);
    thrown.shape->scatter = 0.22f;
    thrown.shape->angleJitter = 0.55f;
    thrown.shape->spacing = 0.16f;
    brush::paint(pen, thrown, sweep(280));

    // The grain standing still on the surface: two marks crossing one
    // place meet one texture.
    brush::Tool onPaper = imported(shape, kSea, 52.0f);
    onPaper.grain = brush::Grain{.image = grain,
                                 .space = brush::GrainSpace::Stroke,
                                 .scale = 1.4f,
                                 .depth = 0.85f};
    brush::paint(pen, onPaper, sweep(440));
    brush::line(pen, onPaper, {200, 380}, {430, 520});

    // The grain riding each stamp, and the size following the pressure.
    brush::Tool loaded = imported(shape, kRust, 52.0f);
    loaded.grain = brush::Grain{.image = grain,
                                .space = brush::GrainSpace::Dab,
                                .scale = 0.7f,
                                .depth = 0.9f};
    loaded.dynamics.size =
        brush::Response{.drive = brush::Drive::Pressure,
                        .curve = {.minimum = 0.22f, .maximum = 1.0f,
                                  .bend = 1.6f}};
    brush::paint(pen, loaded, sweep(610));

    pen.noLoop();
  }
};

}  // namespace

SIGIL_SKETCH(BrushCustom, "Draw · Procedural",
             "A brush built from a shape image and a grain image, with the "
             "spacing, scatter and pressure curve an import carries.")
