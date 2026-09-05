// brush_engine_atlas.cpp — one plate through the complete brush engine seam.
//
// Stock tools, a caller-defined tip, field-warped surfaces and an even-odd
// mass share one instance-owned engine and one deterministic pigment stream.

#include <include/core/SkBitmap.h>
#include <sigildraw/brush/Brush.h>
#include <sigilsketch/draw/Draw.h>

#include <array>

namespace sketch = sigil::sketch;
namespace brush = sigil::draw::brush;
using namespace sigil::draw;

namespace {

constexpr std::array<SkColor4f, 6> kInk{{
    {0.10f, 0.12f, 0.16f, 1.0f},
    {0.12f, 0.32f, 0.42f, 1.0f},
    {0.12f, 0.48f, 0.38f, 1.0f},
    {0.82f, 0.26f, 0.18f, 1.0f},
    {0.91f, 0.55f, 0.08f, 1.0f},
    {0.48f, 0.22f, 0.48f, 1.0f},
}};

struct BrushEngineAtlas final : sketch::DrawSketch {
  brush::Engine brushes;

  void setup(sketch::DrawContext& context) override {
    context.canvas(1000, 820);
    context.captureAt(0.25);
    context.pen.randomSeed(0xB2A55u);
    context.pen.noiseSeed(0xB2A55u);
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
    diamond.customTip = [](Pen& pen, const brush::Dab&) {
      pen.noStroke();
      pen.rectMode(CENTER);
      pen.rotate(QUARTER_PI);
      pen.rect(0, 0, 0.72f, 0.72f, 0.08f);
    };
    brushes.add("diamond", diamond);

    SkBitmap mask;
    mask.allocN32Pixels(64, 64, true);
    mask.eraseColor(SK_ColorWHITE);
    for (int y = 0; y < 64; ++y) {
      for (int x = 0; x < 64; ++x) {
        const float dx = ((float)x - 31.5f) / 28.0f;
        const float dy = ((float)y - 31.5f) / 19.0f;
        const float radius = dx * dx + dy * dy;
        if (radius >= 1.0f) continue;
        const float fiber = 0.5f + 0.5f * std::sin((float)x * 1.7f + y * 0.23f);
        const int shade = (int)(35.0f + 105.0f * radius + 45.0f * fiber);
        *mask.getAddr32(x, y) = SkColorSetRGB(shade, shade, shade);
      }
    }
    brush::Tool paperTip = brush::marker(SkColors::kBlack, 22.0f);
    paperTip.tip = brush::Tip::Image;
    paperTip.imageTip = SkImages::RasterFromBitmap(mask);
    paperTip.spacing = 4.2f;
    paperTip.scatter = 1.8f;
    paperTip.opacity = 0.18f;
    paperTip.rotation = brush::Rotation::Natural;
    paperTip.markerTip = false;
    brushes.add("paper-tip", std::move(paperTip));
  }

  void draw(sketch::DrawContext& context) override {
    Pen& pen = context.pen;
    pen.background(246, 239, 222);

    const std::array<const char*, 14> names{{
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
        "paper-tip",
    }};
    for (size_t i = 0; i < names.size(); ++i) {
      const float y = 58.0f + (float)i * 48.0f;
      brushes.set(names[i], kInk[i % kInk.size()], i < 6 ? 2.0f : 0.8f);
      brushes.line(pen, {72, y}, {430, y + pen.random(-12, 12)}, 0.3f, 0.72f);
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
    brushes.circle(pen, 720, 190, 122.5f, 0.8f);
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
    brushes.hatch({.spacing = 13.0f,
                   .angle = -0.62f,
                   .jitter = 0.08f,
                   .gradient = -0.25f,
                   .continuous = true});
    brushes.hatchArray(pen, seal);

    pen.noStroke();
    pen.fill(40, 34, 31, 220);
    pen.textAlign(CENTER, CENTER);
    pen.textSize(24);
    pen.text("ONE DAB ENGINE", 730, 570);
    pen.noLoop();
  }
};

}  // namespace

SIGIL_SKETCH(BrushEngineAtlas, "Draw · Procedural",
             "Stock and custom tips, watercolor fields and even-odd massing.")
