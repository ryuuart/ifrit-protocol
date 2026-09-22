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
// TINT READS BACKWARDS ON PURPOSE. `GlyphModifier::colorMultiplier` multiplies,
// and a multiplier only takes a colour toward black, so the element is set in
// the DESTINATION and the effect divides down toward the origin. The
// arguments still read in time order.
//
// EDIT THESE FIRST
//   kPeriod    — seconds per pass of the shared phase. Every cascade's
//                master maps onto exactly one of these.
//   kCascade   — the spread every cell beats on, so the nine differ in
//                their effect and in nothing else.
//   kSpecimen  — the size the specimens are set at.

// TAGS: Typography/Effects, Motion/Transitions

#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Instruments.h>
#include <sigilcompose/kit/Kinetic.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <string>
#include <utility>
#include <vector>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace motion = sigil::motion;
namespace weave = sigil::weave;

using namespace sigil::compose;

namespace {

constexpr SkSize kSceneSize{1180, 930};

constexpr float kMargin = 56;
constexpr float kSpecimen = 29;
constexpr float kBodyH = 100;

/// Seconds per pass of the shared phase.
constexpr double kPeriod = 3.0;

constexpr material::Color kGround{0.043f, 0.043f, 0.058f, 1};
constexpr material::Color kBone{0.930f, 0.920f, 0.890f, 1};
constexpr material::Color kFaint{0.540f, 0.540f, 0.590f, 0.28f};
constexpr material::Color kAccent{0.980f, 0.360f, 0.250f, 1};
/// Where `fx::tint` wipes FROM. The specimen is set in kAccent, its
/// DESTINATION, and the effect multiplies down to this — so every channel
/// here has to be darker than the destination's, since a multiplier
/// cannot brighten and a channel it could not reach simply holds.
constexpr material::Color kPale{0.180f, 0.090f, 0.060f, 1};

/** The one cascade every cell beats on, so the nine differ in their
 *  effect and in nothing else. Its span is what the shared phase maps
 *  onto — one wrap is exactly one pass of the schedule. */
const motion::Spread kCascade{.eachMs = 110, .durationMs = 620};

/** The nine cells, in the order the header reads them. `key` is what the
 *  meter under the cell resolves the schedule from; `over` is what this
 *  cell's specimen changes about the register — a face, a colour — and
 *  nothing for most. */
struct Row {
  const char* key;
  const char* call;
  const char* note;
  const char* word;
  TextEffect effect;
  weave::Type over;
};

sk_sp<SkTypeface> display() {
  return weave::ports::face({"Helvetica Neue", "Inter", "Helvetica", "Arial"},
                            SkFontStyle::Bold());
}
/** The face the axis cell is set in: San Francisco carries a GRAD axis,
 *  which is advance-invariant and therefore the one a draw-time drive is
 *  allowed to move. */
sk_sp<SkTypeface> graded() {
  return sketch::kit::houseFace(sketch::kit::Voice::Interface, 500);
}

/** This card's look: bone on near-black, every line set in the font
 *  context's own face, and one voice for every cell — the call over the
 *  specimen, what it deviates under it. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.palette.ground = kGround;
  look.palette.ink = kBone;
  look.palette.rule = kFaint;
  look.type.captionLabel = {.size = 12, .track = 0.8f};
  look.spacing.marginX = kMargin;
  look.spacing.marginTop = kMargin - 12;
  look.spacing.marginBottom = 30;
  look.spacing.captionGap = 8;
  return look;
}

/** The theme's registers as classes, and the specimen's own: the display
 *  face at kSpecimen, tracked, in whatever ink is in force. */
weave::StyleSheet sheetClasses() {
  weave::StyleSheet classes = sheetTheme().styleSheet();
  classes.set("specimen",
              {.face = display(), .size = kSpecimen, .track = 1.5f});
  return classes;
}

}  // namespace

namespace {

struct KineticCard {
  /// The one clock: a wrapping [0,1) every cascade's master reads.
  choreograph::Output<float> phase{0};

  void setup(sketch::SketchContext& ctx) {
    sketch::kit::stage(ctx, {.size = kSceneSize, .captureAt = kPeriod * 0.5});
    // MID-CASCADE. The master maps onto each track's OWN span, so one
    // fraction of the period is the same fraction of every schedule
    // however many letters a word has. At half way the head of every word
    // has landed and its tail is still in flight — the ladder a meter
    // exists to show, and late enough that each word is legible as the
    // word it is.
    phase = 0;
    ctx.ticker.add([this, &ticker = ctx.ticker] {
      const double t = ticker.elapsed();
      phase = motion::phase(t, kPeriod);
    });
    // THE METERS ARE A READ-BACK: they resolve from the layout the last
    // draw left standing, so the page is described once for the specimens
    // and again for the instruments beside them.
    ctx.composer.render(describe(ctx));
    ctx.composer.render(describe(ctx));
  }

  void update(double, sketch::SketchContext& ctx) {
    // A meter that moves with its cascade is re-read every frame; that is
    // the whole cost of an instrument, and it is why one is never in the
    // paint loop of anything that ships.
    ctx.composer.render(describe(ctx));
  }

  /** One cell: the specimen wearing its own effect on the shared
   *  cascade, captioned with the call that made it. The meter is not here
   *  — it is drawn over the whole composition, because `beatsOf` answers
   *  in the composer's space. */
  sketch::kit::ComparisonCase cell(const Row& row, float width) {
    return {
        .title = row.word,
        .control = row.call,
        .figure = box()
                      .width(width)
                      .height(kBodyH)
                      .padding(22, 4)
                      .children({text(row.word)
                                     .styleClass("specimen")
                                     .font(row.over)
                                     .key(row.key)
                                     .width(width - 8)
                                     .fx({.effect = row.effect,
                                          .stagger = kCascade,
                                          .progress = &phase})}),
        .note = row.note};
  }

  Element describe(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    const Composer& composer = ctx.composer;

    static const Row kRows[9] = {
        {"rise", "fx::rise(26)",
         "up from below, fading in over the first "
         "third of its beat",
         "RISE", fx::rise(26)},
        {"slide", "fx::slide(-32)",
         "in from the side; negative is from the "
         "left",
         "SLIDE", fx::slide(-32)},
        {"pop", "fx::pop(0.35, 1.70158)",
         "scale overshoot — "
         "back.out(1.7)",
         "POP", fx::pop(0.35f, 1.70158f)},
        {"spin", "fx::spinIn(70, 14)",
         "a tumble: rotation and a rise, "
         "eased out together",
         "SPIN IN", fx::spinIn(70, 14)},
        {"scatter", "fx::scatter(40, 24)",
         "each glyph from its own seeded "
         "offset and lean",
         "SCATTER", fx::scatter(40, 24)},
        {"typeon", "fx::typeOn()",
         "absent, then simply there — "
         "coverage only, no displacement",
         "TYPE ON", fx::typeOn()},
        {"axis",
         "GRAD axis · 400 → 1000",
         "a grade swept at draw time; advance-invariant, so nothing moves",
         "AXIS SWEEP",
         fx::variableAxisSweep("GRAD", 400, 1000),
         {.face = graded()}},
        {"tint",
         "fx::tint(pale, accent)",
         "the element is set in the destination and the effect multiplies "
         "down to the origin",
         "TINT",
         fx::tint(kPale, kAccent),
         {.color = material::skia::toSkColor(kAccent)}},
        {"wave", "fx::waveLoop(0.10, 0.5)",
         "the one that never lands: a loop on the same wrapping phase, so "
         "its meter never fills",
         // The loop reads the master as a phase rather than as an
         // entrance, so every glyph takes the SAME master and the
         // travelling wave comes from the glyph's own index inside the
         // effect.
         "WAVE LOOP", fx::waveLoop(0.10f, 0.5f)},
    };

    std::vector<sketch::kit::ComparisonCase> moving;
    for (int i = 0; i < 5; ++i) moving.push_back(cell(kRows[i], 200));
    std::vector<sketch::kit::ComparisonCase> stationary;
    for (int i = 5; i < 8; ++i) stationary.push_back(cell(kRows[i], 340));
    Element sheet = sketch::kit::page(
        {.title = "A vocabulary of moving type",
         .subtitle = "Nine presets on one three-second clock · each meter "
                     "reads the glyph's actual share of the cascade",
         .footer = "Each entrance spans its own word, so half a cycle means "
                   "half a cascade. The grade axis changes outlines without "
                   "changing advances."},
        box().column().gap(26).children(
            {document::h2("01 / MOVE THE GLYPHS"),
             sketch::kit::comparison(
                 {.cases = std::move(moving), .measure = 1068, .gap = 17}),
             document::h2("02 / KEEP THE PEN POSITIONS"),
             sketch::kit::comparison(
                 {.cases = std::move(stationary), .measure = 1068, .gap = 24}),
             document::h2("03 / KEEP MOVING"),
             box().row().gap(28).children(
                 {sketch::kit::comparison(
                      {.cases = {cell(kRows[8], 340)}, .measure = 340}),
                  box().width(480).column().gap(16).children(
                      {document::h2("READING THE CASCADE"),
                       document::caption(
                           "A full meter means that glyph has completed its "
                           "beat. A partial meter marks a glyph in motion; an "
                           "empty one is still waiting.")
                           .width(440),
                       document::caption(
                           "The wave reads the shared clock as a loop. It "
                           "changes neither the entrance schedule nor the "
                           "positions chosen by paragraph layout.")
                           .width(440)})})}));

    Element root = stack()
                       .styleSheet(sheetClasses())
                       .fill(Fill::color(kGround))
                       .children({std::move(sheet)});
    // One meter per cell, over the whole composition: the rects are in the
    // composer's space, so the bars land on the letters wherever the sheet
    // put them.
    for (const Row& row : kRows)
      root.children(
          {kit::trackMeter(composer, row.key, 0, kAccent,
                           {kAccent.r, kAccent.g, kAccent.b, 0.14f},
                           {.where = kit::MeterPlacement::Where::Under,
                            .thickness = 3.0f,
                            .gap = 7.0f,
                            .trim = 1.5f})
               .inset(0)});
    return root;
  }
};

}  // namespace

SIGIL_SKETCH_AS(KineticCard, "kinetic_card", "Specimen",
                "every stock text effect, caught mid-cascade with its "
                "schedule under it")
