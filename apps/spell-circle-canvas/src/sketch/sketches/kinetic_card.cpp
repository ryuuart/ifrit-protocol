/** @file
 * kinetic_card — the fx preset specimen: every stock text effect on one
 * sheet, each caught mid-cascade with its own schedule drawn under it.
 */

// NINE PRESETS, ONE CLOCK. `kit/Kinetic.h` ships nine comparable
// `TextEffect` values over the kernel's `fx()` seam, and a still of a
// moving effect is worth nothing unless it is caught while the effect is
// moving — so every cell here is driven by ONE wrapping phase, and the
// declared moment falls where every cascade is mid-flight.
//
// The rows, in the order the header names them:
//
//   · RISE, SLIDE, POP, SPIN IN, SCATTER — the five that MOVE glyphs off
//     the pen positions the layout gave them, which is what puts a live
//     run on the subpixel grid.
//   · TYPE ON, AXIS SWEEP, TINT — the three that do not: they touch
//     coverage, an outline coordinate and colour, so their letters keep
//     whole-pixel origins however hard they run.
//   · WAVE LOOP — the one that never lands. It reads the same wrapping
//     phase as a loop rather than as an entrance, which is why its meter
//     never fills.
//
// THE METER UNDER EACH CELL is `kit::trackMeter`: one cell per beat of the
// track, at that beat's own laid-out rect, filled by that beat's local
// time. It is `Composer::beatsOf` drawn with nothing in between, so the
// bars and the letters cannot disagree about the schedule — which is the
// only way to read a cascade off a still.
//
// AXIS SWEEP IS GATED. A driven axis is honoured only where it does not
// move advances: `wght` does, so the runtime would refuse it and draw at
// the shaped face. `GRAD` is the axis that exists for exactly this — a
// grade is weight without width — and the cell is set in a face that
// carries one.
//
// TINT READS BACKWARDS ON PURPOSE. `GlyphMod::colorMul` multiplies, and a
// multiplier only takes a colour toward black, so the element is set in
// the DESTINATION and the effect divides down toward the origin. The
// arguments still read in time order.
//
// EDIT THESE FIRST
//   kPeriod    — seconds per pass of the shared phase. Every cascade's
//                master maps onto exactly one of these.
//   kCascade   — the spread every cell beats on, so the nine differ in
//                their effect and in nothing else.
//   kSpecimen  — the size the specimens are set at.

#include <sigilcompose/kit/Instruments.h>
#include <sigilcompose/kit/Kinetic.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace motion = sigil::motion;
namespace weave = sigil::weave;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kSceneSize{1180, 620};

constexpr float kMargin = 56;
constexpr float kGutter = 30;
constexpr float kCell = (kSceneSize.fWidth - 2 * kMargin - 2 * kGutter) / 3;
constexpr float kSpecimen = 38;
constexpr float kBodyH = 78;

/// Seconds per pass of the shared phase.
constexpr double kPeriod = 3.0;

constexpr SkColor4f kGround{0.043f, 0.043f, 0.058f, 1};
constexpr SkColor4f kBone{0.930f, 0.920f, 0.890f, 1};
constexpr SkColor4f kAsh{0.540f, 0.540f, 0.590f, 1};
constexpr SkColor4f kFaint{0.540f, 0.540f, 0.590f, 0.28f};
constexpr SkColor4f kAccent{0.980f, 0.360f, 0.250f, 1};
/// Where `fx::tint` wipes FROM. The specimen is set in kAccent, its
/// DESTINATION, and the effect multiplies down to this — so every channel
/// here has to be darker than the destination's, since a multiplier
/// cannot brighten and a channel it could not reach simply holds.
constexpr SkColor4f kPale{0.180f, 0.090f, 0.060f, 1};

/** The one cascade every cell beats on, so the nine differ in their
 *  effect and in nothing else. Its span is what the shared phase maps
 *  onto — one wrap is exactly one pass of the schedule. */
const motion::Spread kCascade{.eachMs = 110, .durationMs = 620};

/** The nine cells, in the order the header reads them. `key` is what the
 *  meter under the cell resolves the schedule from. */
struct Row {
  const char* key;
  const char* call;
  const char* note;
  const char* word;
};

sk_sp<SkTypeface> display() {
  static sk_sp<SkTypeface> face = weave::ports::pickTypeface(
      {"Helvetica Neue", "Inter", "Helvetica", "Arial"}, SkFontStyle::Bold());
  return face;
}
/** The face the axis cell is set in: San Francisco carries a GRAD axis,
 *  which is advance-invariant and therefore the one a draw-time drive is
 *  allowed to move. */
sk_sp<SkTypeface> graded() {
  static sk_sp<SkTypeface> face =
      weave::ports::pickTypeface({".SF NS", "SF Pro", "Helvetica Neue"}, 500);
  return face;
}

weave::TextStyle label(float size, SkColor4f colour, float track = 0) {
  return weave::textStyle({.size = size, .color = colour, .track = track});
}
weave::TextStyle specimen(SkColor4f colour, sk_sp<SkTypeface> face) {
  return weave::textStyle({.face = std::move(face),
                           .size = kSpecimen,
                           .color = colour,
                           .track = 1.5f});
}

/** The one voice every cell is captioned in: the call over the specimen,
 *  what it deviates under it. */
kit::Caption voice() {
  return {.where = kit::Caption::Where::Split,
          .label = label(12, kBone, 0.8f),
          .note = label(10.5f, kAsh, 0.2f),
          .gap = 8,
          .noteMeasure = kCell};
}

}  // namespace

namespace {

struct KineticCard final : sketch::Sketch {
  /// The one clock: a wrapping [0,1) every cascade's master reads.
  choreograph::Output<float> phase{0};

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kSceneSize.fWidth, kSceneSize.fHeight);
    ctx.background(kGround);
    // MID-CASCADE. The master maps onto each track's OWN span, so one
    // fraction of the period is the same fraction of every schedule
    // however many letters a word has. At half way the head of every word
    // has landed and its tail is still in flight — the ladder a meter
    // exists to show, and late enough that each word is legible as the
    // word it is.
    ctx.captureAt(kPeriod * 0.5);
    phase = 0;
    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      phase = motion::phase(t, kPeriod);
      return true;
    });
    // THE METERS ARE A READ-BACK: they resolve from the layout the last
    // draw left standing, so the page is described once for the specimens
    // and again for the instruments beside them.
    ctx.composer.render(describe(ctx));
    ctx.composer.render(describe(ctx));
  }

  void update(double, sketch::SketchContext& ctx) override {
    // A meter that moves with its cascade is re-read every frame; that is
    // the whole cost of an instrument, and it is why one is never in the
    // paint loop of anything that ships.
    ctx.composer.render(describe(ctx));
  }

  /** One cell: the specimen wearing one track, captioned with the call
   *  that made it. The meter is not here — it is drawn over the whole
   *  composition, because `beatsOf` answers in the composer's space. */
  Element cell(const Row& row, Track track, SkColor4f ink,
               sk_sp<SkTypeface> face) {
    track.progress = &phase;
    track.stagger = kCascade;
    return kit::cell(
        voice(), toU8(row.call), toU8(row.note),
        box()
            .width(Dim(kCell))
            .height(Dim(kBodyH))
            .child(text(toU8(row.word), specimen(ink, std::move(face)))
                       .key(row.key)
                       .width(Dim(kCell))
                       .fx(std::move(track))));
  }

  Element describe(sketch::SketchContext& ctx) {
    const Composer& composer = ctx.composer;

    static const Row kRows[9] = {
        {"rise", "fx::rise(26)",
         "up from below, fading in over the first "
         "third of its beat",
         "RISE"},
        {"slide", "fx::slide(-32)",
         "in from the side; negative is from the "
         "left",
         "SLIDE"},
        {"pop", "fx::pop(0.35, 1.70158)",
         "scale overshoot \xe2\x80\x94 "
         "back.out(1.7)",
         "POP"},
        {"spin", "fx::spinIn(70, 14)",
         "a tumble: rotation and a rise, "
         "eased out together",
         "SPIN IN"},
        {"scatter", "fx::scatter(40, 24)",
         "each glyph from its own seeded "
         "offset and lean",
         "SCATTER"},
        {"typeon", "fx::typeOn()",
         "absent, then simply there \xe2\x80\x94 "
         "coverage only, no displacement",
         "TYPE ON"},
        {"axis", "fx::variableAxisSweep(\"GRAD\", 400, 1000)",
         "a grade swept at draw time; advance-invariant, so nothing moves",
         "AXIS SWEEP"},
        {"tint", "fx::tint(pale, accent)",
         "the element is set in the destination and the effect multiplies "
         "down to the origin",
         "TINT"},
        {"wave", "fx::waveLoop(0.10, 0.5)",
         "the one that never lands: a loop on the same wrapping phase, so "
         "its meter never fills",
         "WAVE LOOP"},
    };

    std::vector<Element> cells;
    cells.push_back(cell(kRows[0], {.effect = fx::rise(26)}, kBone, display()));
    cells.push_back(
        cell(kRows[1], {.effect = fx::slide(-32)}, kBone, display()));
    cells.push_back(
        cell(kRows[2], {.effect = fx::pop(0.35f, 1.70158f)}, kBone, display()));
    cells.push_back(
        cell(kRows[3], {.effect = fx::spinIn(70, 14)}, kBone, display()));
    cells.push_back(
        cell(kRows[4], {.effect = fx::scatter(40, 24)}, kBone, display()));
    cells.push_back(cell(kRows[5], {.effect = fx::typeOn()}, kBone, display()));
    cells.push_back(cell(kRows[6],
                         {.effect = fx::variableAxisSweep("GRAD", 400, 1000)},
                         kBone, graded()));
    cells.push_back(cell(kRows[7], {.effect = fx::tint(kPale, kAccent)},
                         kAccent, display()));
    // The loop reads the master as a phase rather than as an entrance, so
    // every glyph takes the SAME master and the travelling wave comes from
    // the glyph's own index inside the effect.
    cells.push_back(cell(kRows[8], {.effect = fx::waveLoop(0.10f, 0.5f)}, kBone,
                         display()));

    std::vector<Element> shelves;
    for (int r = 0; r < 3; ++r) {
      std::vector<Element> run;
      for (int c = 0; c < 3; ++c) run.push_back(cells[(size_t)(r * 3 + c)]);
      shelves.push_back(kit::cells({.cells = std::move(run), .gap = kGutter}));
    }

    Element sheet =
        kit::sheet(
            {.title = u8"THE STOCK TEXT EFFECTS",
             .subtitle = u8"nine presets, one cascade, one wrapping "
                         u8"phase \xe2\x80\x94 and each one's own "
                         u8"schedule drawn under it",
             .footer = u8"rise \xc2\xb7 slide \xc2\xb7 pop \xc2\xb7 "
                       u8"spinIn \xc2\xb7 scatter move their glyphs; "
                       u8"typeOn \xc2\xb7 variableAxisSweep \xc2\xb7 "
                       u8"tint touch coverage, an outline and colour "
                       u8"and leave every pen position alone",
             .titleStyle = label(13, kBone, 3.6f),
             .subtitleStyle = label(10.5f, kAsh, 0.3f),
             .footerStyle = label(10, kAsh, 0.2f),
             .marginX = kMargin,
             .marginTop = kMargin - 12,
             .marginBottom = 30,
             .ground = Fill::color(kGround),
             .rule = Fill::color(kFaint)},
            kit::cells(
                {.cells = std::move(shelves), .column = true, .gap = 34}))
            .absolute()
            .inset(0);

    Element root = stack().fill(Fill::color(kGround)).child(std::move(sheet));
    // One meter per cell, over the whole composition: the rects are in the
    // composer's space, so the bars land on the letters wherever the sheet
    // put them.
    for (const Row& row : kRows)
      root.child(kit::trackMeter(composer, row.key, 0, kAccent,
                                 {kAccent.fR, kAccent.fG, kAccent.fB, 0.14f},
                                 {.where = kit::MeterPlacement::Where::Under,
                                  .thickness = 3.0f,
                                  .gap = 7.0f,
                                  .trim = 1.5f})
                     .absolute()
                     .inset(0));
    return root;
  }
};

}  // namespace

SIGIL_SKETCH_AS(KineticCard, "kinetic_card", "Specimen",
                "every stock text effect, caught mid-cascade with its "
                "schedule under it")
