// shape_tour.cpp — NOTHING HERE IS A RECTANGLE.
// =============================================================================
// One subject: the non-rectangular shape and layout vocabulary, and the
// fact that every part of it composes with every other. A box's OUTLINE
// is a value (`shapes::blob`, `star`, `rounded`, `squircle`); WHERE a
// child lands is a value (`layouts::Scatter`, `layouts::Radial`); what
// runs ALONG an outline is a value (`ContourWalk`, `shapes::onEdges`);
// and what joins two of them is a value (`routers::`). None of the four
// knows about the others, which is why they can be stacked in one tree
// without a single special case.
//
//   THE BED     fourteen seeded blobs under `layouts::Scatter`. A seed
//               and a jitter, and the same fourteen land in the same
//               fourteen places on every run.
//   THE FIELD   the SAME layout verb over a different generator — five
//               pointed rosettes from `shapes::star` at a second seed.
//               One scatter, two vocabularies of outline, no code in
//               between placing anything by hand.
//   THE SIGIL   `shapes::rounded(shapes::star(…))` — a shape operator
//               over a shape — carrying a `ContourWalk` that stamps a
//               composed ORNAMENT round its own contour, and holding a
//               `layouts::Radial` ring of runes inside it.
//   THE MOONS   a blob and a squircle, keyed so the routers can find
//               them.
//   THE PLAQUE  `shapes::onEdges` four ways on one box: a dash on the
//               top, a stamped zigzag on the bottom, walked dots down
//               the left, and a bare right edge for the comparison.
//   THE WIRES   `routers::orthogonal` and `routers::arc` between keys.
//               A connector names two keys and a router; it never learns
//               what shape either end is.
//
// The whole tree is DECLARED once and never re-described: the sigil's
// turn and its breath are bound outputs a ticker steps, which is the
// first of the three motion paths and the right one when nothing about
// the DATA changes.
//
// EDIT THESE FIRST
//   the two Scatter seeds — the whole bed and field re-land, and land
//                           the same way on every run afterwards.
//   stampWalk.spacing     — how close the stamped ornaments sit.
//   the routers           — orthogonal(18) vs arc(0.35) on either wire.

#include <sigilcompose/shape/Layouts.h>
#include <sigilcompose/shape/Routers.h>
#include <sigilcompose/shape/Shapes.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/canvas/Sketch.h>

#include <string>
#include <vector>

namespace sketch = sigil::sketch;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1000, 760};

}  // namespace

struct ShapeTour final : sketch::Sketch {
  choreograph::Output<float> spin{0.0f};
  choreograph::Output<float> pulse{1.0f};

  Element describe() {
    const float w = kCanvas.width(), h = kCanvas.height();

    // THE BED — seeded blobs, scatter-laid, no two runs different.
    auto bed =
        layout(layouts::Scatter{.seed = 77, .jitter = 0.9f}).inset(0).zIndex(0);
    for (int i = 0; i < 14; ++i) {
      const float hue = (float)i / 14.0f;
      const SkColor4f tint{0.25f + 0.5f * hue, 0.2f, 0.45f - 0.25f * hue, 0.5f};
      bed.child(box()
                    .width(70 + (float)(i % 5) * 26)
                    .height(60 + (float)(i % 4) * 24)
                    .shape(shapes::blob((uint32_t)(100 + i), 0.35f, 5 + i % 6))
                    .fill(Fill::color(tint))
                    .blend(SkBlendMode::kPlus));
    }

    // THE FIELD — the same verb, a different generator. Nothing places
    // these by hand either; only the outline changed.
    auto field = layout(layouts::Scatter{.seed = 313, .jitter = 0.75f})
                     .inset(0, h * 0.62f, 0, 0)
                     .zIndex(1);
    for (int i = 0; i < 22; ++i) {
      const float size = 20.0f + (float)(i % 5) * 7.0f;
      const SkColor4f petal = i % 3 == 0 ? SkColor4f{1.0f, 0.54f, 0.71f, 0.9f}
                              : i % 3 == 1
                                  ? SkColor4f{0.60f, 0.86f, 0.94f, 0.9f}
                                  : SkColor4f{1.0f, 0.85f, 0.63f, 0.9f};
      field.child(box()
                      .width(size)
                      .height(size)
                      .shape(shapes::star(5, 0.42f))
                      .fill(Fill::color(petal))
                      .foreground(stroke(1.0f, Fill::color(hex(0x6a4a38)))));
    }

    // THE SIGIL — a shape operator over a shape, wearing a stamped
    // ornament and holding a radial ring.
    ContourWalk stampWalk;
    stampWalk.spacing = 46.0f;
    stampWalk.stamp =
        box()
            .width(18)
            .height(18)
            .shape(shapes::star(4, 0.4f))
            .fill(Fill::color({1.0f, 0.71f, 0.42f, 0.9f}))
            .foreground(stroke(1.2f, Fill::color({1, 0.9f, 0.75f, 1})));
    auto sigil =
        box()
            .key("sigil")
            .width(300)
            .height(300)
            .inset((w - 300) / 2, 130, (w - 300) / 2, h - 430)
            .zIndex(3)
            .shape(shapes::rounded(shapes::star(7, 0.62f), 14))
            .fill(Fill::color({0.96f, 0.42f, 0.29f, 0.92f}))
            .rotate(&spin)
            .scale(&pulse)
            .foreground(stampWalk)
            .child(layout(layouts::Radial{.radiusFraction = 0.52f})
                       .inset(0)
                       .children([] {
                         std::vector<Element> runes;
                         const char8_t* glyphs[] = {u8"ᚠ", u8"ᚢ", u8"ᚦ",
                                                    u8"ᚨ", u8"ᚱ", u8"ᚲ",
                                                    u8"ᚷ", u8"ᚹ", u8"ᚺ"};
                         runes.reserve(9);
                         for (int i = 0; i < 9; ++i)
                           runes.push_back(text(
                               std::u8string(glyphs[(size_t)i]),
                               type({.size = 22, .color = hex(0x2a101c)})));
                         return runes;
                       }()));

    // THE MOONS — one blob, one squircle, so the routers have two ends
    // of different shapes to reach and neither router knows which.
    auto moonA = box()
                     .key("moon-a")
                     .width(90)
                     .height(90)
                     .inset(80, 90, w - 170, h - 180)
                     .zIndex(2)
                     .shape(shapes::blob(21, 0.28f, 7))
                     .fill(Fill::color({0.36f, 0.62f, 0.66f, 0.95f}))
                     .foreground(stroke(2, Fill::color({0.8f, 1, 1, 0.6f})));
    auto moonB =
        box()
            .key("moon-b")
            .width(90)
            .height(90)
            .inset(w - 180, 120, 90, h - 210)
            .zIndex(2)
            .shape(shapes::squircle(4.0f))
            .fill(Fill::color({0.52f, 0.55f, 0.82f, 0.95f}))
            .foreground(stroke(2, Fill::color({0.9f, 0.92f, 1, 0.6f})));

    // THE PLAQUE — four edges, four answers.
    PathFormat topDash;
    topDash.width = 5;
    topDash.strokeFill = Fill::color({1.0f, 0.71f, 0.42f, 1});
    topDash.dashIntervals = {14, 6};
    PathFormat bottomStamp;
    bottomStamp.width = 3;
    bottomStamp.strokeFill = Fill::color({0.62f, 0.83f, 1.0f, 1});
    {
      SkPathBuilder zig;
      zig.moveTo(0, 3);
      zig.lineTo(4, -3);
      zig.lineTo(8, 3);
      bottomStamp.stampPath = zig.detach();
      bottomStamp.stampAdvance = 9;
    }
    ContourWalk leftDots;
    leftDots.spacing = 12.0f;
    leftDots.draw = [](SkCanvas& c, const PathSample&, const PaintContext&) {
      SkPaint p;
      p.setAntiAlias(true);
      p.setColor(0xffffd9a0);
      c.drawCircle(0, 0, 2.4f, p);
    };
    auto plaque =
        box()
            .key("plaque")
            .width(250)
            .height(120)
            .inset(40, h - 170, w - 290, 50)
            .zIndex(4)
            .corners({0, 26, 0, 26})
            .fill(Fill::color({0.11f, 0.12f, 0.2f, 0.96f}))
            .foreground(shapes::onEdges(shapes::Edge::Top, topDash))
            .foreground(shapes::onEdges(shapes::Edge::Bottom, bottomStamp))
            .foreground(shapes::onEdges(shapes::Edge::Left, leftDots))
            .padding(18, 16)
            .child(text(u8"per-edge chrome:",
                        type({.size = 15, .color = hex(0x9aa4bb)})))
            .child(text(u8"dash / zigzag / dots / bare",
                        type({.size = 17, .color = hex(0xe8d9c2)})));

    // THE WIRES — two keys and a router apiece.
    PathFormat wire;
    wire.width = 3;
    wire.strokeFill = Fill::color({1.0f, 0.84f, 0.5f, 0.85f});
    wire.dashIntervals = {10, 7};
    PathFormat bow;
    bow.width = 2.4f;
    bow.strokeFill = Fill::color({0.62f, 0.9f, 0.9f, 0.8f});

    return stack()
        .fill(linearGradient(
            {0, 0}, {w, h},
            {{0.10f, 0.06f, 0.18f, 1}, {0.26f, 0.08f, 0.17f, 1}}, {0.0f, 1.0f}))
        .child(std::move(bed))
        .child(std::move(field))
        .child(std::move(sigil))
        .child(std::move(moonA))
        .child(std::move(moonB))
        .child(std::move(plaque))
        .child(connector("plaque", "sigil", routers::orthogonal(18))
                   .inset(0)
                   .foreground(wire)
                   .zIndex(2))
        .child(connector("moon-a", "moon-b", routers::arc(0.35f))
                   .inset(0)
                   .foreground(bow)
                   .zIndex(2));
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background({0, 0, 0, 1});
    ctx.captureAt(6.0);
    // Declared motion, not a redraw: the sigil's turn and breath are
    // bound outputs the runtime resolves every frame, and the tree above
    // is described exactly once.
    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      spin = (float)std::fmod(t * 9.0, 360.0);
      pulse = 1.0f + 0.05f * (float)std::sin(t * 2.2);
      return true;
    });
    ctx.composer.render(describe());
  }
};

SIGIL_SKETCH(ShapeTour, "Kit \xc2\xb7 API",
             "the non-rectangular vocabulary, composed \xe2\x80\x94 blob, "
             "star, rounded and squircle laid by Scatter and Radial, "
             "dressed by onEdges and joined by routers")
