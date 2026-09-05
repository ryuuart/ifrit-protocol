// brushwork_currents.cpp — four procedural tools crossing two fields.
//
// Long watercolor fibers ride a wave, charcoal turns around a vortex, pencil
// splines pin the composition together and a light spray settles at crossings.
// The sketch chooses paths, colours and fields; the pressure, dry gaps, bristle
// bundles and pigment deposition belong to SigilDrawBrush.
//
// EDIT THESE FIRST
//   kStreams      how many long wet strokes cross the paper
//   kOrbitMarks   how many charcoal gestures turn around the centre
//   kPalette      the pigments assigned to successive strokes

#include <sigildraw/brush/Brush.h>
#include <sigilsketch/draw/Draw.h>

#include <array>
#include <vector>

namespace sketch = sigil::sketch;
namespace brush = sigil::draw::brush;
using namespace sigil::draw;

namespace {

constexpr int kStreams = 34;
constexpr int kOrbitMarks = 13;
constexpr std::array<SkColor4f, 5> kPalette{{
    {0.05f, 0.30f, 0.38f, 1.0f},
    {0.11f, 0.48f, 0.52f, 1.0f},
    {0.82f, 0.24f, 0.15f, 1.0f},
    {0.92f, 0.55f, 0.12f, 1.0f},
    {0.38f, 0.16f, 0.34f, 1.0f},
}};

struct BrushworkCurrents final : sketch::DrawSketch {
  void setup(sketch::DrawContext& ctx) override {
    ctx.canvas(1080, 760);
    ctx.background(241, 234, 215);
    ctx.captureAt(0.25);
    ctx.pen.randomSeed(0xB405u);
    ctx.pen.noFill();
  }

  void draw(Pen& pen) override {
    pen.background(241, 234, 215);

    pen.stroke(72, 56, 43, 16);
    for (int fleck = 0; fleck < 2600; ++fleck) {
      pen.strokeWeight(pen.random(0.25f, 1.0f));
      pen.point(pen.random(pen.width), pen.random(pen.height));
    }

    brush::Wave current{.direction = 0.0f,
                                .amplitude = 0.48f,
                                .wavelength = 118.0f,
                                .speed = 0.0f};
    for (int stream = 0; stream < kStreams; ++stream) {
      brush::Tool wet = brush::watercolor(
          kPalette[(size_t)stream % kPalette.size()], pen.random(15, 29));
      wet.opacity *= pen.random(0.72f, 1.16f);
      wet.pressure = {pen.random(0.16f, 0.48f), pen.random(0.75f, 1.18f),
                      pen.random(0.08f, 0.38f)};
      const SkPoint start{-45.0f,
                          34.0f + (float)stream * 20.5f + pen.random(-13, 13)};
      brush::flowLine(pen, wet, start, pen.random(780, 1180), pen.random(-2, 2),
                      current);
    }

    const brush::Vortex orbit{
        .center = {pen.width * 0.58f, pen.height * 0.52f},
        .direction = -1.0f,
        .pull = 0.12f};
    for (int mark = 0; mark < kOrbitMarks; ++mark) {
      brush::Tool dry = brush::charcoal(
          kPalette[(size_t)(mark + 2) % kPalette.size()], pen.random(6, 13));
      dry.opacity *= pen.random(0.65f, 1.1f);
      const SkPoint start{pen.width * 0.58f + pen.random(100, 310),
                          pen.height * 0.52f + pen.random(-120, 145)};
      brush::flowLine(pen, dry, start, pen.random(180, 490), 0, orbit);
    }

    brush::Tool lead = brush::pencil({0.12f, 0.10f, 0.13f, 1}, 2.1f);
    lead.opacity = 0.55f;
    for (int ribbon = 0; ribbon < 6; ++ribbon) {
      const float y = 116.0f + (float)ribbon * 104.0f;
      const std::array<brush::Sample, 5> controls{{
          {{58, y + pen.random(-25, 25)}, 0.2f},
          {{260, y + pen.random(-75, 75)}, 1.0f},
          {{500, y + pen.random(-55, 55)}, 0.65f},
          {{760, y + pen.random(-90, 90)}, 1.15f},
          {{1020, y + pen.random(-35, 35)}, 0.18f},
      }};
      brush::spline(pen, lead, controls, 0.74f);
    }

    brush::Tool mist = brush::spray({0.72f, 0.18f, 0.12f, 1}, 34.0f);
    mist.opacity = 0.15f;
    for (int cloud = 0; cloud < 18; ++cloud) {
      const SkPoint center{pen.random(90, pen.width - 90),
                           pen.random(80, pen.height - 80)};
      brush::line(
          pen, mist, center,
          {center.fX + pen.random(-18, 18), center.fY + pen.random(-18, 18)});
    }

    pen.noLoop();
  }
};

}  // namespace

SIGIL_SKETCH(BrushworkCurrents, "Draw · Procedural",
             "Watercolor, charcoal, pencil and spray marks crossing wave and "
             "vortex fields.")
