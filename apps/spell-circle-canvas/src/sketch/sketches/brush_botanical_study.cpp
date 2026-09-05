// brush_botanical_study.cpp — a field specimen built from natural-media layers.
//
// The branch is pressure-bearing spline geometry. Leaves are reusable polygons
// whose flat body, watercolor bleed, dry mass, hatch and pencil outline remain
// independent choices, so one silhouette can carry several kinds of pigment.

// EDIT THESE FIRST
//   kLeaves   the silhouettes arranged along the stem
//   kGreens   the pigments shared by wet and dry leaves

#include <sigildraw/brush/Brush.h>
#include <sigilsketch/draw/Draw.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace sketch = sigil::sketch;
namespace brush = sigil::draw::brush;
using namespace sigil::draw;

namespace {

struct LeafSpec {
  SkPoint base;
  float length;
  float width;
  float angle;
  float bend;
};

struct Leaf {
  brush::Polygon polygon;
  SkPoint base;
  SkPoint tip;
  SkPoint axis;
  SkPoint normal;
  float length;
  float width;
  float bend;
};

constexpr std::array<LeafSpec, 10> kLeaves{{
    {{286, 626}, 184, 48, -2.45f, -0.10f},
    {{354, 584}, 196, 54, 0.34f, 0.13f},
    {{434, 536}, 205, 57, -2.48f, 0.09f},
    {{503, 487}, 192, 51, 0.12f, -0.11f},
    {{584, 431}, 211, 61, -2.57f, -0.08f},
    {{650, 373}, 185, 47, -0.06f, 0.12f},
    {{720, 310}, 180, 50, -2.62f, 0.10f},
    {{779, 248}, 157, 43, -0.46f, -0.08f},
    {{829, 190}, 139, 39, -2.68f, 0.06f},
    {{865, 147}, 115, 33, -0.63f, -0.06f},
}};

constexpr std::array<SkColor4f, 5> kGreens{{
    {0.10f, 0.31f, 0.22f, 1},
    {0.20f, 0.47f, 0.31f, 1},
    {0.05f, 0.40f, 0.42f, 1},
    {0.47f, 0.53f, 0.13f, 1},
    {0.73f, 0.42f, 0.10f, 1},
}};

Leaf makeLeaf(Pen& pen, const LeafSpec& spec) {
  const SkPoint axis{std::cos(spec.angle), std::sin(spec.angle)};
  const SkPoint normal{-axis.fY, axis.fX};
  std::vector<SkPoint> points;
  constexpr int kSteps = 22;
  points.reserve(kSteps * 2 + 2);

  const auto point = [&](float t, float side) {
    const float envelope = std::pow(std::max(0.0f, std::sin(PI * t)), 0.72f);
    const float asymmetry = 1.0f + side * 0.08f * std::sin(PI * t);
    const float roughness = 1.0f + pen.random(-0.025f, 0.025f);
    const float curve = std::sin(PI * t) * spec.bend * spec.length;
    const float along = spec.length * t;
    const float across =
        curve + side * spec.width * envelope * asymmetry * roughness;
    return SkPoint{spec.base.fX + axis.fX * along + normal.fX * across,
                   spec.base.fY + axis.fY * along + normal.fY * across};
  };

  for (int step = 0; step <= kSteps; ++step)
    points.push_back(point((float)step / (float)kSteps, 1.0f));
  for (int step = kSteps; step >= 0; --step)
    points.push_back(point((float)step / (float)kSteps, -1.0f));

  return {brush::Polygon(std::move(points)),
          spec.base,
          {spec.base.fX + axis.fX * spec.length,
           spec.base.fY + axis.fY * spec.length},
          axis,
          normal,
          spec.length,
          spec.width,
          spec.bend};
}

void paper(Pen& pen) {
  pen.stroke(80, 61, 40, 13);
  for (int fleck = 0; fleck < 2800; ++fleck) {
    pen.strokeWeight(pen.random(0.25f, 0.9f));
    pen.point(pen.random(pen.width), pen.random(pen.height));
  }
}

void vein(Pen& pen, const Leaf& leaf, SkColor4f color) {
  brush::Tool lead = brush::pencil(color, 1.25f);
  lead.opacity = 0.62f;
  lead.scatter = 0.08f;

  const std::array<brush::Sample, 4> midrib{{
      {leaf.base, 0.18f},
      {{leaf.base.fX + leaf.axis.fX * leaf.length * 0.34f +
            leaf.normal.fX * leaf.bend * leaf.length * 0.75f,
        leaf.base.fY + leaf.axis.fY * leaf.length * 0.34f +
            leaf.normal.fY * leaf.bend * leaf.length * 0.75f},
       0.95f},
      {{leaf.base.fX + leaf.axis.fX * leaf.length * 0.72f +
            leaf.normal.fX * leaf.bend * leaf.length * 0.55f,
        leaf.base.fY + leaf.axis.fY * leaf.length * 0.72f +
            leaf.normal.fY * leaf.bend * leaf.length * 0.55f},
       0.58f},
      {leaf.tip, 0.08f},
  }};
  brush::spline(pen, lead, midrib, 0.72f);

  lead.width = 0.72f;
  lead.opacity = 0.38f;
  for (int index = 1; index <= 5; ++index) {
    const float t = 0.14f + (float)index * 0.12f;
    const float envelope = std::pow(std::max(0.0f, std::sin(PI * t)), 0.72f);
    const SkPoint center{
        leaf.base.fX + leaf.axis.fX * leaf.length * t +
            leaf.normal.fX * std::sin(PI * t) * leaf.bend * leaf.length,
        leaf.base.fY + leaf.axis.fY * leaf.length * t +
            leaf.normal.fY * std::sin(PI * t) * leaf.bend * leaf.length,
    };
    for (float side : {-1.0f, 1.0f}) {
      const float reach = leaf.width * envelope * 0.83f;
      const SkPoint edge{
          center.fX + leaf.normal.fX * reach * side +
              leaf.axis.fX * leaf.length * 0.055f,
          center.fY + leaf.normal.fY * reach * side +
              leaf.axis.fY * leaf.length * 0.055f,
      };
      brush::line(pen, lead, center, edge, 0.68f, 0.06f);
    }
  }
}

struct BrushBotanicalStudy final : sketch::DrawSketch {
  brush::Engine brushes;

  void setup(sketch::DrawContext& context) override {
    context.canvas(1100, 780);
    context.captureAt(0.25);
    context.pen.randomSeed(0xB07A11CAu);
    context.pen.noiseSeed(0xB07A11CAu);
    brushes.scaleBrushes(1.15f);
  }

  void draw(Pen& pen) override {
    pen.background(247, 241, 222);
    paper(pen);

    brush::Tool stem = brush::charcoal({0.27f, 0.20f, 0.12f, 1}, 6.4f);
    stem.opacity = 0.48f;
    stem.bristles = 17;
    const std::array<brush::Sample, 6> branch{{
        {{145, 718}, 0.24f},
        {{292, 624}, 1.0f},
        {{452, 526}, 0.72f},
        {{624, 407}, 0.96f},
        {{778, 258}, 0.54f},
        {{906, 102}, 0.10f},
    }};
    brush::spline(pen, stem, branch, 0.84f);

    brush::Tool twig = brush::pencil({0.30f, 0.22f, 0.13f, 1}, 2.1f);
    twig.opacity = 0.66f;
    for (const LeafSpec& spec : kLeaves) {
      const SkPoint join{spec.base.fX + std::cos(spec.angle) * 24.0f,
                         spec.base.fY + std::sin(spec.angle) * 24.0f};
      brush::line(pen, twig, spec.base, join, 0.88f, 0.18f);
    }

    for (size_t index = 0; index < kLeaves.size(); ++index) {
      Leaf leaf = makeLeaf(pen, kLeaves[index]);
      const SkColor4f pigment = kGreens[index % kGreens.size()];
      const SkColor4f edge{pigment.fR * 0.47f, pigment.fG * 0.48f,
                           pigment.fB * 0.43f, 1};

      brushes.push();
      brushes.set("HB", edge, 0.78f);
      brushes.wash(pigment, 0.055f);
      if (index % 3 == 1) {
        brushes.noFill();
        brushes.noHatch();
        brushes.mass("pastel", pigment,
                     {.precision = 0.56f,
                      .strength = 0.42f,
                      .gradient = index % 2 == 0 ? 0.24f : -0.18f,
                      .outline = false});
      } else {
        brushes.noMass();
        brushes.fill(pigment, 0.13f);
        brushes.fillBleed(0.14f, brush::BleedDirection::Out,
                          kLeaves[index].angle);
        brushes.fillTexture(0.58f, 0.52f, true);
        brushes.hatchStyle("2H", edge, 0.42f);
        brushes.hatch({.spacing = 12.0f + (float)(index % 3) * 2.0f,
                       .angle = kLeaves[index].angle,
                       .jitter = 0.12f,
                       .gradient = index % 2 == 0 ? 0.16f : -0.12f});
      }
      leaf.polygon.show(pen, brushes);
      brushes.pop();
      vein(pen, leaf, edge);
    }

    const std::array<SkPoint, 7> fruit{{
        {903, 111},
        {936, 128},
        {918, 151},
        {957, 160},
        {888, 166},
        {934, 190},
        {971, 119},
    }};
    brush::Tool fruitStem = brush::pencil({0.30f, 0.23f, 0.13f, 1}, 1.2f);
    fruitStem.opacity = 0.62f;
    for (SkPoint berry : fruit)
      brush::line(pen, fruitStem, {902, 111}, berry, 0.72f, 0.12f);

    brushes.push();
    brushes.set("2B", {0.42f, 0.08f, 0.07f, 1}, 0.65f);
    brushes.fill({0.74f, 0.12f, 0.10f, 1}, 0.19f);
    brushes.fillBleed(0.18f);
    brushes.fillTexture(0.72f, 0.62f, true);
    brushes.noWash();
    brushes.noHatch();
    brushes.noMass();
    for (size_t index = 0; index < fruit.size(); ++index)
      brushes.circle(pen, fruit[index].fX, fruit[index].fY,
                     12.0f + (float)(index % 3) * 2.5f, 0.28f);
    brushes.pop();

    pen.noStroke();
    pen.fill(55, 48, 38, 210);
    pen.textAlign(LEFT, CENTER);
    pen.textSize(15);
    pen.text("FIELD STUDY  /  WET + DRY", 58, 50);
    pen.noLoop();
  }
};

}  // namespace

SIGIL_SKETCH(BrushBotanicalStudy, "Draw · Procedural",
             "An ink-and-wash branch composed from reusable leaf surfaces.")
