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
//    paths and only the mask parameters animate. The whole reveal is one
//    declaration — a mount transition per arc with a shared delay — so the
//    scene is described once and never again.
//
// EDIT THESE FIRST
//   runs()        — the measured arc table: inner and outer radius as a
//                   fraction of the poster width, and the two math-convention
//                   angles. Everything the plate draws is in it.
//   kRingBaseMs   — the innermost ring's reveal, which every ring outward
//                   doubles. The progression is the poster's, the base is
//                   the piece's own.
//   kPlateH       — how large the reproduction hangs on the wall; every
//                   fraction below rides it.

#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/shape/Generators.h>
#include <sigilcompose/typography/Type.h>
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

/// The beat of blank paper the reveal starts from.
constexpr std::chrono::milliseconds kRevealDelay{150};
/// The innermost ring's sweep; every ring outward doubles it.
constexpr unsigned kRingBaseMs = 120;

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

}  // namespace beethoven_plate

struct Beethoven final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kSceneSize.fWidth, kSceneSize.fHeight);
    // The whole table is revealed: the outermost ring runs 120 ms doubled
    // five times, so the last sweep lands before four seconds and the plate
    // is the poster rather than a frame of its assembly.
    ctx.captureAt(6.0);
    ctx.background({0, 0, 0, 1});
    ctx.composer.render(describe());
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
  Element arcRun(const beethoven_plate::Run& run, int ring) {
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
            .shape(shapes::arc(canvasStart))
            .stroke(inkStroke);
    // The poster's own progression: reveal duration doubles per ring
    // outward, and each sweep runs linearly. The mount delay is what used
    // to be a first-frame re-describe — a transition that starts from
    // nothing needs a moment of nothing to start from, and that moment is
    // the transition's own.
    const auto duration = std::chrono::milliseconds(
        bp::kRingBaseMs << (unsigned)std::min(ring, 5));
    e.mask(by::spans(spans::upTo(
        animate(from(0.0001f).to(span),
                {duration, &choreograph::easeNone, bp::kRevealDelay}))));
    return e;
  }

  /** The imprint: four groups, each a hanging label against its own block
   *  of entries. The label column is a fixed measure and sets flush RIGHT
   *  against the gutter; the entries set flush left from it, so every entry
   *  in the table starts on one line whatever its label's length. */
  Element imprint() {
    namespace bp = beethoven_plate;
    const auto small = [](float size) {
      return sigil::compose::type({.size = size * bp::kScale,
                                   .color = bp::kInk,
                                   .track = 0.2f * bp::kScale,
                                   .color8 = true});
    };
    struct Group {
      const char* label;
      std::vector<const char*> lines;
    };
    const std::vector<Group> groups = {
        {"tonhalle",
         {"grosser saal", "dienstag, den 22. februar 1955,", "20.15 uhr",
          "4. extrakonzert", "der tonhalle-gesellschaft"}},
        {"leitung", {"carl schuricht"}},
        {"solist", {"wolfgang schneiderhan"}},
        {"beethoven",
         {"ouverture zu \xc2\xab"
          "coriolan\xc2\xbb, op. 62",
          "violinkonzert in d-dur, op. 61",
          "siebente sinfonie in a-dur, op. 92"}},
        {"vorverkauf",
         {"tonhalle-kasse, hug, jecklin,", "kuoni",
          "karten zu fr. 3.50 bis 9.50"}}};

    Element table = box().column().gap(9 * bp::kScale);
    for (size_t g = 0; g < groups.size(); ++g) {
      Element entries = box().column().gap(1 * bp::kScale);
      for (const char* line : groups[g].lines)
        entries.child(text(toU8(line), small(11.5f)));
      table.child(
          box()
              .key("group" + std::to_string(g))
              .row()
              .gap(7 * bp::kScale)
              .child(box()
                         .width(56 * bp::kScale)
                         .row()
                         .justify(Justify::End)
                         .child(text(toU8(groups[g].label), small(11.5f))))
              .child(std::move(entries)));
    }
    return table;
  }

  /** The poster itself, in plate coordinates. */
  Element plate() {
    namespace bp = beethoven_plate;
    auto poster =
        stack()
            .fill(Fill::color(bp::kPaper))
            .background(styles::dropShadow({0, 0, 0, 0.45f}, {0, 8}, 22))
            .clip();
    const auto& table = bp::runs();
    const auto& ringOf = bp::rings();
    for (size_t i = 0; i < table.size(); ++i)
      poster.child(arcRun(table[i], ringOf[i]).key("arc" + std::to_string(i)));

    // THE TYPE BLOCK, as the sheet sets it: "beethoven" alone at the left
    // margin at about 62% depth, in the clear white, and the imprint below
    // it as a TWO-COLUMN TABLE — five hanging labels flush right in a
    // narrow column against their entries flush left. That table is the
    // poster's most characteristic detail and the reason its lower half
    // reads as setting rather than as caption.
    poster.child(text(toU8("beethoven"),
                      sigil::compose::type({.size = 38 * bp::kScale,
                                            .color = bp::kInk,
                                            .track = 0.2f * bp::kScale,
                                            .color8 = true}))
                     .key("title")
                     .absolute()
                     .left(0.055f * bp::kPlateW)
                     .top(0.600f * bp::kPlateH));
    poster.child(imprint()
                     .key("imprint")
                     .absolute()
                     .left(0.175f * bp::kPlateW)
                     .top(0.690f * bp::kPlateH));
    return poster;
  }

  Element describe() {
    namespace bp = beethoven_plate;
    return stack()
        .fill(Fill::color(bp::kWall))
        // The plate, centered on the wall — the letterbox panels are the
        // mat itself.
        .child(plate()
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
                               sigil::compose::type({.size = 14.0f,
                                                     .color = bp::kLabel,
                                                     .track = 0.6f,
                                                     .color8 = true})))
                   .child(text(toU8("beethoven \xe2\x80\x94 tonhalle "
                                    "z\xc3\xbcrich, 1955"),
                               sigil::compose::type({.size = 12.0f,
                                                     .color = bp::kLabel,
                                                     .track = 0.4f,
                                                     .color8 = true})))
                   .child(text(toU8("measured arc table \xc2\xb7 rings "
                                    "double 1:2:4:8:16"),
                               sigil::compose::type({.size = 12.0f,
                                                     .color = bp::kLabel,
                                                     .track = 0.4f,
                                                     .color8 = true})))
                   .key("label"));
  }
};

}  // namespace

SIGIL_SKETCH_AS(Beethoven, "beethoven", "Catalog \xc2\xb7 Type & grid",
                "Brockmann arc table, span reveal")
