// brush_engine_atlas.cpp — one plate through the complete brush engine seam.
//
// Stock tools, a caller-defined tip, field-warped surfaces and an even-odd
// mass share one instance-owned engine and one deterministic pigment stream.
// Below them, a brush made of two pictures, the way a painting program means
// the word: one shape image and one grain image, both drawn here on an
// offscreen canvas rather than read from disk, carried through what an
// imported brush states — the spacing and scatter a stamp is placed by, the
// grain standing still on the paper or riding each dab, and a size the
// pressure drives.
//
// EDIT THESE FIRST
//   kInk          the six pigments the stock rows cycle through
//   chiselTip     the shape image: a soft-edged chisel, dark on white
//   paperGrain    the grain image: a tile of the material grain field

// TAGS: Drawing/Brushes

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigildraw/Constants.h>
#include <sigildraw/Graphics.h>
#include <sigildraw/Pen.h>
#include <sigildraw/brush/Brush.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilsketch/scene2d/Scene2D.h>

#include <array>
#include <cmath>
#include <memory>
#include <utility>

namespace field = sigil::material::field;
namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace compose = sigil::compose;
namespace brush = sigil::draw::brush;
using namespace sigil::draw;

namespace {

constexpr std::array<material::Color, 6> kInk{{
    {0.10f, 0.12f, 0.16f, 1.0f},
    {0.12f, 0.32f, 0.42f, 1.0f},
    {0.12f, 0.48f, 0.38f, 1.0f},
    {0.82f, 0.26f, 0.18f, 1.0f},
    {0.55f, 0.32f, 0.04f, 1.0f},
    {0.48f, 0.22f, 0.48f, 1.0f},
}};

/** Where the image-tip section begins, below the engine's rows. */
constexpr float kImageTips = 860.0f;

/** THE SHAPE IMAGE: a chisel with a soft edge, dark on white, which is the
 *  artwork a tip is usually drawn as. A radial gradient fitted to the
 *  ellipse holds the middle at full ink and lets the edge fall to paper. */
void chiselTip(Pen& pen) {
  pen.background(255);
  pen.noStroke();
  pen.fill(material::radialGradient({0.5f, 0.5f}, 0.71f,
                                    {{0.0f, {0, 0, 0, 1}},
                                     {0.40f, {0.08f, 0.08f, 0.08f, 1}},
                                     {1.0f, {1, 1, 1, 1}}}),
           SHAPE);
  pen.ellipse(48, 48, 92, 60);
}

/** THE GRAIN IMAGE: a tile of the material grain field, its luminance the
 *  coverage a mark keeps. */
void paperGrain(Pen& pen) {
  pen.background(255);
  pen.noStroke();
  pen.fill(field::grain(0.18f, 2, 7.0f, 1.3f));
  pen.rect(0, 0, 128, 128);
}

/** A brush whose whole look is its pictures. */
brush::Tool imported(sk_sp<SkImage> shape, material::Color color, float width) {
  brush::Tool tool = brush::marker(color, width);
  tool.tip = brush::Tip::Image;
  tool.opacity = 0.75f;
  tool.markerTip = false;
  tool.rotation = brush::Rotation::Natural;
  tool.shape = brush::Shape{.image = std::move(shape), .spacing = 0.09f};
  return tool;
}

/** One arc of the same centreline, so every image-tip row differs only by
 *  what the brush says. */
brush::Stroke sweep(float y) {
  const std::array<brush::Sample, 4> controls{{
      {{70, y + 26}, 0.35f},
      {{330, y - 30}, 1.0f},
      {{640, y + 34}, 0.9f},
      {{930, y - 18}, 0.3f},
  }};
  return brush::spline(controls, 2.0f, 0.7f);
}

struct BrushEngineAtlas {
  brush::Engine brushes;
  /** The two pictures the image tips are made of, drawn once on canvases
   *  of their own the first time the sheet is drawn. */
  std::unique_ptr<Graphics> shape, grain;

  void setup(sketch::Scene2DContext& context) {
    context.canvas(1000, 1500);
    context.captureAt(0.25);
    brushes.scaleBrushes(3.5f);

    brush::Tool diamond = brush::marker(SkColors::kBlack, 15.0f);
    diamond.tip = brush::Tip::Custom;
    diamond.spacing = 5.5f;
    diamond.scatter = 1.2f;
    diamond.opacity = 0.22f;
    diamond.rotation = brush::Rotation::Natural;
    diamond.markerTip = false;
    diamond.pressure.curve = [](float t) {
      return 0.28f + std::sin(t * PI) * 0.9f;
    };
    diamond.customTip = [](Pen& pen) {
      pen.noStroke();
      pen.rectMode(CENTER);
      pen.rotate(QUARTER_PI);
      pen.rect(0, 0, 0.72f, 0.72f, 0.08f);
    };
    brushes.add("diamond", diamond);

    context.composer.render(compose::graphics("brush_engine_atlas.sheet",
                                              [this](Pen& pen) { draw(pen); }));
  }

  /** The picture @p body draws on a @p side square canvas of its own. */
  static std::unique_ptr<Graphics> picture(Pen& host, float side,
                                           void (*body)(Pen&)) {
    auto canvas = std::make_unique<Graphics>(side, side);
    body(canvas->begin(host));
    canvas->end();
    return canvas;
  }

  void heading(Pen& pen, const char* words, float x, float y) {
    pen.noStroke();
    pen.fill(40, 34, 31);
    pen.textAlign(LEFT, BASELINE);
    pen.textSize(13);
    pen.text(words, x, y);
  }

  void engineRows(Pen& pen) {
    pen.noStroke();
    pen.fill(40, 34, 31);
    pen.textSize(26);
    pen.text("PIGMENT / PRESSURE / PAPER", 48, 40);
    // The three columns' heads, each over the run it names.
    for (const auto& [x, head] : std::array<std::pair<float, const char*>, 3>{
             {{72, "ONE PASS"},
              {250, "SIX PASSES"},
              {610, "FIELD-WARPED WASH"}}})
      heading(pen, head, x, 78);

    const std::array<const char*, 13> names{{
        "2B",
        "HB",
        "2H",
        "cpencil",
        "pen",
        "rotring",
        "spray",
        "marker",
        "marker2",
        "charcoal",
        "pastel",
        "crayon",
        "diamond",
    }};
    for (size_t i = 0; i < names.size(); ++i) {
      const float y = 112.0f + (float)i * 52.0f;
      brushes.set(names[i], kInk[i % kInk.size()], i < 6 ? 4.0f : 1.6f);
      brushes.line(pen, {72, y}, {218, y - 6}, 0.75f, 1.0f);
      for (int pass = 0; pass < 6; ++pass)
        brushes.line(pen, {250, y + pen.random(-2, 2)},
                     {414, y - 6 + pen.random(-2, 2)}, 0.75f, 1.0f);
      pen.noStroke();
      pen.fill(42, 38, 34, 205);
      pen.textAlign(LEFT, CENTER);
      pen.textSize(17);
      pen.text(names[i], 455, y);
    }

    brushes.set("HB", {0.08f, 0.25f, 0.31f, 1}, 1.6f);
    brushes.fill({0.12f, 0.48f, 0.44f, 1}, 0.22f);
    brushes.fillBleed(0.32f, brush::BleedDirection::Out, -0.35f);
    brushes.fillTexture(0.5f, 0.45f, true);
    brushes.field("waves");
    brushes.circle(pen, 750, 232, 122.5f, 0.8f);
    brushes.noField();
    brushes.noFill();

    const std::array<brush::Polygon, 2> seal{{
        brush::Polygon({{570, 410}, {920, 390}, {892, 742}, {548, 718}}),
        brush::Polygon({{666, 500}, {816, 492}, {808, 648}, {650, 636}}),
    }};
    brushes.noStroke();
    brushes.mass("crayon", {0.80f, 0.24f, 0.18f, 1},
                 {.precision = 0.72f,
                  .strength = 0.72f,
                  .gradient = 0.3f,
                  .outline = true});
    brushes.massArray(pen, seal);
    brushes.noMass();
    brushes.hatchStyle("rotring", {0.37f, 0.17f, 0.31f, 1}, 1.6f);
    brushes.hatch({.pattern = {.spacing = 13.0f,
                               .angle = -0.62f,
                               .taper = brush::gradientTaper(-0.25f)},
                   .jitter = 0.08f,
                   .continuous = true});
    brushes.hatchArray(pen, seal);
    heading(pen, "EVEN-ODD MASS / HATCH OVERLAY", 570, 790);
  }

  /** The four image-tip rows: one shape image and one grain image, and the
   *  same arc under each, so a row shows only what its brush says. */
  void imageTipRows(Pen& pen) {
    constexpr material::Color kSoot{0.09f, 0.10f, 0.14f, 1.0f};
    constexpr material::Color kRust{0.72f, 0.28f, 0.14f, 1.0f};
    constexpr material::Color kSea{0.10f, 0.36f, 0.44f, 1.0f};
    const sk_sp<SkImage> chisel = shape->image();
    const sk_sp<SkImage> speckle = grain->image();

    pen.noStroke();
    pen.fill(40, 34, 31);
    pen.textAlign(LEFT, BASELINE);
    pen.textSize(22);
    pen.text("A BRUSH MADE OF TWO IMAGES", 48, kImageTips);
    pen.image(chisel, 760, kImageTips - 40, 72, 72);
    pen.image(speckle, 856, kImageTips - 40, 72, 72);
    heading(pen, "SHAPE", 760, kImageTips + 48);
    heading(pen, "GRAIN", 856, kImageTips + 48);

    const float top = kImageTips + 70.0f;
    heading(pen, "01 / SHAPE ALONE", 70, top);
    brush::paint(pen, imported(chisel, kSoot, 46.0f), sweep(top + 50));

    const float scattered = top + 125.0f;
    heading(pen, "02 / SPACING, SCATTER AND ROTATION", 70, scattered);
    brush::Tool thrown = imported(chisel, kSoot, 40.0f);
    thrown.shape->scatter = 0.22f;
    thrown.shape->angleJitter = 0.55f;
    thrown.shape->spacing = 0.16f;
    brush::paint(pen, thrown, sweep(scattered + 50));

    // The grain standing still on the surface: two marks crossing one
    // place meet one texture.
    const float onPaper = top + 250.0f;
    heading(pen, "03 / GRAIN FIXED TO THE PAPER", 70, onPaper);
    brush::Tool fixed = imported(chisel, kSea, 52.0f);
    fixed.grain = brush::Grain{.image = speckle,
                               .space = brush::GrainSpace::Stroke,
                               .scale = 0.7f,
                               .depth = 0.85f};
    brush::paint(pen, fixed, sweep(onPaper + 50));
    brush::line(pen, fixed, {200, onPaper + 10}, {430, onPaper + 96});

    // The grain riding each stamp, and the size following the pressure.
    const float onDab = top + 375.0f;
    heading(pen, "04 / GRAIN ON EACH DAB, SIZE FROM PRESSURE", 70, onDab);
    brush::Tool loaded = imported(chisel, kRust, 52.0f);
    loaded.grain = brush::Grain{.image = speckle,
                                .space = brush::GrainSpace::Dab,
                                .scale = 0.35f,
                                .depth = 0.9f};
    loaded.dynamics.size = brush::Response{
        .drive = brush::Drive::Pressure,
        .curve = {.minimum = 0.22f, .maximum = 1.0f, .bend = 1.6f}};
    brush::paint(pen, loaded, sweep(onDab + 50));
  }

  void draw(Pen& pen) {
    if (!shape) shape = picture(pen, 96, chiselTip);
    if (!grain) grain = picture(pen, 128, paperGrain);
    pen.randomSeed(0xB2A55u);
    pen.noiseSeed(0xB2A55u);
    pen.background(246, 239, 222);

    engineRows(pen);
    imageTipRows(pen);

    pen.noStroke();
    pen.fill(40, 34, 31, 220);
    pen.textAlign(LEFT, BASELINE);
    pen.textSize(14);
    pen.text("Pressure tapers each stroke. Repeated passes build pigment.", 48,
             1452);
    pen.text(
        "One engine: stock tools, custom stamps, image tips, washes, hatches "
        "and masses.",
        48, 1475);
    pen.noLoop();
  }
};

}  // namespace

SIGIL_SKETCH(BrushEngineAtlas, "Draw · Procedural",
             "Stock and custom tips, watercolor fields and even-odd massing, "
             "then a brush made of a shape image and a grain image with the "
             "spacing, scatter, grain space and pressure curve an import "
             "carries.")
