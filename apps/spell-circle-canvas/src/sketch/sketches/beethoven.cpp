/** @file
 * beethoven — a Brockmann arc table: a Swiss grid of measured runs
 * with a span reveal running across it.
 */

// Josef Müller-Brockmann's "beethoven" concert poster (Tonhalle Zürich,
// 1955), reconstructed rather than traced. Every number below comes from
// pixel analysis of an archival scan: arc centre at (0.2693W, 0.7156H), ring
// widths doubling 1:2:4:8:16, and every arc end landing on an exact multiple
// of the poster's 11.25° angular module.
//
// The construction is pure composition. Each arc run is a full circle
// OUTLINE whose path starts at that run's canvas start angle, stroked at the
// ring's width and then revealed with a span mask — so the mask parameter
// maps directly onto the measured sweep. The reveal moves in the poster's
// own units: ring durations double from the inside out and sweeps run
// linearly, so the motion is a geometric progression rather than a
// decorative ease.
//
// Two things to know before editing:
//  - The poster is a Weltformat portrait (the √2 rectangle). This canvas is
//    landscape, so it hangs as a PLATE REPRODUCTION — scaled to the canvas
//    height, centred, matted on a museum wall with a label panel to the
//    right. Every arc-table fraction is a fraction of the PLATE box, not the
//    canvas, which is what lets the measured geometry survive the rescale.
//  - The ring geometry is static and stays cacheable: the outlines are fixed
//    paths and only the mask parameters animate. The reveal is a single
//    discrete re-describe gated in update(), not a per-frame one.

#include <include/core/SkPathBuilder.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilsketch/canvas/Sketch.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
using sigil::compose::toU8;
using namespace std::chrono_literals;

namespace {
/** The canvas this piece was drawn against, which is also the default a
 *  sketch gets when it declares none. */
constexpr SkSize kSceneSize = {900, 640};

namespace beethoven_plate {

constexpr float kW = kSceneSize.fWidth, kH = kSceneSize.fHeight;

// The plate: poster-space is 720 x (720·1.4144); reproduce at canvas
// height minus a mat margin, centered horizontally.
constexpr float kPosterW = 720.0f;
constexpr float kPosterH = kPosterW * 1.4144f;  // the Weltformat √2 rectangle
constexpr float kPlateH = 600.0f;
constexpr float kPlateW = kPlateH * (kPosterW / kPosterH);  // ≈ 424.4
constexpr float kScale = kPlateW / kPosterW;  // poster units → plate px
constexpr float kPlateX = (kW - kPlateW) * 0.5f;
constexpr float kPlateY = (kH - kPlateH) * 0.5f;

constexpr SkColor4f kWall{0.235f, 0.230f, 0.222f, 1};   // the museum wall
constexpr SkColor4f kPaper{0.961f, 0.953f, 0.933f, 1};  // #F5F3EE
constexpr SkColor4f kInk{0.066f, 0.062f, 0.058f, 1};
constexpr SkColor4f kLabel{0.760f, 0.745f, 0.715f, 1};

// The arc table measured off the poster. Angles are in MATH convention
// (0° = +x, counter-clockwise positive, y up); the canvas is y-down and
// clockwise-positive, so canvas angle = −math angle.
struct Run {
  float rInner, rOuter;    // × poster W
  float startDeg, endDeg;  // math convention
};
inline const std::vector<Run>& runs() {
  static const std::vector<Run> table = {
      {0.3480f, 0.3607f, -33.75f, 22.50f},
      {0.3480f, 0.3607f, -146.25f, -135.00f},
      {0.3640f, 0.3913f, -22.50f, 45.00f},
      {0.3640f, 0.3913f, -157.50f, -90.00f},
      {0.3940f, 0.4473f, 0.00f, 78.75f},
      {0.3940f, 0.4473f, -135.00f, -45.00f},
      {0.4500f, 0.5560f, -150.00f, 90.00f},
      {0.5600f, 0.7770f, -150.00f, 45.00f},
      {0.7820f, 1.2070f, -150.00f, 101.25f},
  };
  return table;
}
// Ring index per run (for the doubling reveal durations).
inline const std::vector<int>& rings() {
  static const std::vector<int> table = {0, 0, 1, 1, 2, 2, 3, 4, 5};
  return table;
}

/** The plate's type colour reaches the paint as 8-bit sRGB, so a tint
 *  computed here lands on the same 256-step ladder as the sampled inks.
 *  `compose::type` carries the float through instead, and the device
 *  raster resolves the two differently. */
inline sigil::weave::TextStyle type(float size, SkColor4f color = kInk,
                                    float tracking = 0) {
  sigil::weave::TextStyle s;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = tracking;
  s.paint.foreground.setColor(color.toSkColor());
  s.paint.foreground.setAntiAlias(true);
  return s;
}

}  // namespace beethoven_plate

struct Beethoven final : sketch::Sketch {
  bool revealed = false;

  void setup(sketch::SketchContext& ctx) override {
    ctx.background({0, 0, 0, 1});
    Composer& composer = ctx.composer;
    revealed = false;
    composer.render(describe(0));  // every arc masked to nothing
  }

  /** One arc run: a box centred on the poster's arc centre, sized to the
   *  run's mid-radius circle, whose outline STARTS at the run's canvas start
   *  angle. That start angle is what makes the mask parameter meaningful —
   *  masking the outline from 0 to sweep/360 then reveals exactly the
   *  measured run and nothing else.
   *
   *  All fractions here are of the PLATE box, which is the parent of these
   *  absolute children, so the whole measured geometry rescales as one
   *  unit. */
  Element arcRun(const beethoven_plate::Run& run, int ring, int phase) {
    namespace bp = beethoven_plate;
    const SkPoint C{0.2693f * bp::kPlateW, 0.7156f * bp::kPlateH};
    const float rMid = (run.rInner + run.rOuter) * 0.5f * bp::kPlateW;
    const float width = (run.rOuter - run.rInner) * bp::kPlateW;
    const float canvasStart = -run.endDeg;  // y-down mapping
    const float sweep = run.endDeg - run.startDeg;
    const float span = std::min(sweep / 360.0f, 0.9995f);

    PathFormat inkStroke;
    inkStroke.width = width;
    inkStroke.strokeFill = Fill::color(bp::kInk);

    Element e =
        box()
            .width(2 * rMid)
            .height(2 * rMid)
            .inset(C.x() - rMid, C.y() - rMid, bp::kPlateW - C.x() - rMid,
                   bp::kPlateH - C.y() - rMid)
            .shape([canvasStart](SkSize s) {
              SkPathBuilder b;
              b.addArc(SkRect::MakeWH(s.width(), s.height()), canvasStart,
                       359.9f);
              return b.detach();
            })
            .stroke(inkStroke);
    if (phase == 0) {
      e.mask(by::spans(spans::upTo(0.0001f)));
    } else {
      // The poster's own progression: reveal duration doubles per ring
      // outward, and each sweep runs linearly.
      const auto duration =
          std::chrono::milliseconds(120u << (unsigned)std::min(ring, 5));
      e.mask(by::spans(
          spans::upTo(animate(to(span), {duration, &choreograph::easeNone}))));
    }
    return e;
  }

  /** The poster itself, in plate coordinates. */
  Element plate(int phase) {
    namespace bp = beethoven_plate;
    auto poster =
        stack()
            .fill(Fill::color(bp::kPaper))
            .background(styles::dropShadow({0, 0, 0, 0.45f}, {0, 8}, 22))
            .clip();
    const auto& table = bp::runs();
    const auto& ringOf = bp::rings();
    for (size_t i = 0; i < table.size(); ++i)
      poster.child(
          arcRun(table[i], ringOf[i], phase).key("arc" + std::to_string(i)));

    // The type block: flush-left grotesque, lowercase-forward, seated LOW in
    // the paper bowl to the left of the arc centre. Not at the original's
    // top-left, because the arc runs measured above sweep straight through
    // that zone and would cover the title; the bowl below the centre line is
    // the plate's one clear field. Sizes and margins ride the plate scale so
    // the whole block rescales with the reproduction.
    poster.child(
        box()
            .column()
            .inset(0.07f * bp::kPlateW, 0.54f * bp::kPlateH,
                   0.45f * bp::kPlateW, 0.16f * bp::kPlateH)
            .zIndex(2)
            .child(text(toU8("beethoven"),
                        bp::type(58 * bp::kScale, bp::kInk, 0.5f * bp::kScale))
                       .key("title"))
            .child(text(toU8("tonhalle grosser saal"),
                        bp::type(15 * bp::kScale, bp::kInk, 0.4f * bp::kScale))
                       .margin(0, 14 * bp::kScale, 0, 0))
            .child(text(toU8("donnerstag, den 27. januar 1955"),
                        bp::type(15 * bp::kScale, bp::kInk, 0.4f * bp::kScale))
                       .margin(0, 7 * bp::kScale, 0, 0))
            .child(text(toU8("erich schmid \xc2\xb7 g\xc3\xa9za anda"),
                        bp::type(15 * bp::kScale, bp::kInk, 0.4f * bp::kScale))
                       .margin(0, 7 * bp::kScale, 0, 0)));
    return poster;
  }

  Element describe(int phase) {
    namespace bp = beethoven_plate;
    return stack()
        .fill(Fill::color(bp::kWall))
        // The plate, centered on the wall — the letterbox panels are the
        // mat itself.
        .child(plate(phase)
                   .inset(bp::kPlateX, bp::kPlateY,
                          bp::kW - bp::kPlateX - bp::kPlateW,
                          bp::kH - bp::kPlateY - bp::kPlateH)
                   .key("plate"))
        // The museum label, right panel, at hanging height.
        .child(box()
                   .column()
                   .gap(4)
                   .inset(bp::kPlateX + bp::kPlateW + 32, bp::kH - 150, 24, 64)
                   .child(text(toU8("josef m\xc3\xbcller-brockmann"),
                               bp::type(14, bp::kLabel, 0.6f)))
                   .child(text(toU8("beethoven \xe2\x80\x94 tonhalle "
                                    "z\xc3\xbcrich, 1955"),
                               bp::type(12, bp::kLabel, 0.4f)))
                   .child(text(toU8("measured arc table \xc2\xb7 rings "
                                    "double 1:2:4:8:16"),
                               bp::type(12, bp::kLabel, 0.4f)))
                   .key("label"));
  }

  void update(double elapsed, sketch::SketchContext& ctx) override {
    Composer& composer = ctx.composer;
    if (!revealed && elapsed > 0.15) {
      revealed = true;
      composer.render(describe(1));  // the doubling-duration reveal
    }
  }
};

}  // namespace

SIGIL_SKETCH_AS(Beethoven, "beethoven", "Catalog \xc2\xb7 Type & grid",
                "Brockmann arc table, span reveal")
