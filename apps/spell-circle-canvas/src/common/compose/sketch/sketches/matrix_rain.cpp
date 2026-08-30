// matrix_rain.cpp — THE DIGITAL RAIN of The Matrix (1999), run as a stress
// field: three overlapping curtains of vertical type over a churning bed,
// thousands of glyphs churning and fading at once.
// =============================================================================
// SUBJECT  The most recognisable piece of kinetic typography ever shipped,
//          and a natural worst case for a text engine: it is nothing BUT
//          glyphs in motion — no layout to hide behind, no hero line to
//          carry the eye, just a field where every character is being
//          substituted, tinted and faded on its own clock, all the time.
//
// -----------------------------------------------------------------------------
// FROM THE RECORD
//
//   * The code was designed by Simon Whiteley at Animal Logic for THE
//     MATRIX (1999). He has said the characters were scanned from his
//     wife's Japanese cookbooks: HALF-WIDTH KATAKANA, MIRRORED left to
//     right, mixed with Latin digits and a few other westerns.
//   * Green on black, and the LEADING glyph of a falling streak is paler —
//     near white — with the tail decaying through green toward dark
//     behind it.
//   * Columns run at DIFFERING rates and phases; no two fall together.
//   * The glyphs CHURN — a character in the field keeps being replaced by
//     another from the set, whether or not a streak is passing through it.
//   * The title-sequence rain and the in-film screens differ: the titles
//     let characters travel down the frame, while the operators' monitors
//     hold the glyphs IN PLACE and run the brightness down the column.
//     This study is the in-film kind — the type never moves, the light
//     does.
//
// THIS STUDY'S OWN, flagged rather than smuggled:
//   * Every colour, size, rate and seed. Four planes — a dim churning bed
//     and three falling depths, one leaf each — are this study's
//     composition; the film's screens are a single plane.
//   * The charset is the record's mix — half-width katakana with the odd
//     bracket, dot and small form, plus digits — but PARTITIONED BY THE
//     SUBSTITUTION GATE: a codepoint swap is honoured only where the
//     replacement has the original's advance, and in the face that carries
//     this field the half-width forms sit at one advance and the digits at
//     another. So the churn runs as TWO tracks, each cell substituting
//     within its own advance class, and every glyph in the field still
//     churns — one mixed charset would silently freeze exactly the cells
//     whose roll crossed the class line. ('4' is left out: this face cuts
//     it a hair wider than its siblings and the gate would refuse it.)
//   * The mirror is PER GLYPH, as the record's is: `scaleX = -1` in a
//     static track flips each half-width form about its own centre while
//     the columns keep their reading order. A per-glyph non-uniform scale
//     routes every mirrored glyph onto its own matrix draw instead of the
//     shared-transform batch — thousands of matrix draws per frame — which
//     is load this stress field takes on deliberately. The digits stand
//     unmirrored, and each glyph also takes its own phosphor lift
//     (`colorScreen`, seeded per glyph), so no two cells burn alike.
//
// -----------------------------------------------------------------------------
// THE MACHINE, and what it puts under load
//
//   * ONE TEXT LEAF PER DEPTH, running down the page: `writingMode`
//     vertical-RL, the kana held UPRIGHT by their style (their default
//     vertical orientation is rotated, which is not what the screens
//     show). A column is a LINE unit there, so "each column on its own
//     clock" is `From::Random` over lines — the seeded scrambled ladder,
//     spread across the loop period by `amountMs`, each field dealing its
//     own scatter from its own `Stagger::seed` — with a nested cluster
//     cascade running the glyphs down inside each column's beat. The
//     whole schedule is declared; nothing steps per-column state by hand,
//     and no table has to agree with the laid-out column count.
//   * THE RAIN NEVER STOPS because the cascade LOOPS: `Stagger::loopMs`
//     re-opens every glyph's beat once per period, phase-offset by its
//     column's scattered start, and the scatter spans the whole period —
//     so at any instant every age of streak is on screen somewhere and
//     each column re-drops forever on its own offset. The master is a wrapping
//     phase whose wall period IS the declared loop, so the drive and the
//     schedule cannot drift. Where a column's ladder outruns the period,
//     successive drops share the column, spaced one period apart — the
//     screens do that too.
//   * THE STREAK is one keyframe table: a glyph flashes in near-white (the
//     table's head), decays through phosphor green, and is gone by local
//     1 — so the head, the tail and the dark between streaks are ONE
//     effect read at different local times, and a whole column costs no
//     more declarations than one glyph. No `fx::hold`: a looping cascade
//     has no waiting units to withhold — between beats a glyph rests at
//     local 1, which this table paints dark — and the bright head IS the
//     arrival.
//   * THE CHURN is `fx::scramble` on two more tracks sharing one wrapping
//     progress — the field partitioned by a selector into its two advance
//     classes, each churning within its own charset — composing with the
//     streak by the track algebra: the substitution from one track, the
//     mirror and lift from another, alpha and tint from the streak.
//   * THE LOAD, deliberately: thousands of glyphs whose alpha and tint
//     vary per glyph per frame — which exercises the quantisation ladder
//     (each distinct value is a batch bucket and an atlas strike), the
//     per-glyph substitution gate, and the batcher's band ordering, since
//     the two bright fields wear a blurred glow UNDERLAY beneath their
//     foreground while per-glyph fades split both passes into fade
//     classes. Every underlay must land beneath every foreground however
//     the buckets split; a halo drawn over a neighbouring glyph's body is
//     this study failing.
//
// EDIT THESE FIRST
//   kFields   — each curtain's size, head rate (eachMs), trail life
//               (durationMs) and loop period. eachMs is the speed the
//               light runs down a column; durationMs / eachMs is the
//               trail's length in glyphs; loopMs is how often a column
//               re-drops.
//   kMeter    — true draws the near field's resolved schedule over the
//               frame (one cell per column beat), which is the instrument
//               for tuning the column scatter.
//
// Run:
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/matrix_rain.cpp \
//       --frame /tmp/matrix_rain.png --at 7.0

#include <sigilcompose/core/Material.h>
#include <sigilcompose/kit/Instruments.h>
#include <sigilcompose/typography/TextFx.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/Sketch.h>
#include <sigilweave/style/Style.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace sigil::compose;
namespace ch = choreograph;

namespace {

constexpr float kW = 1280.0f;
constexpr float kH = 780.0f;

/** Draw the near field's schedule as cells over the frame. An instrument,
 *  not part of the picture: it re-describes per frame while on. */
constexpr bool kMeter = false;

// ---- the palette ----------------------------------------------------------
// The type is SET in the head colour and every later moment is a multiplier
// down from it: `colorMul` can only darken, so the brightest state must be
// the one the style owns.
constexpr SkColor4f kVoid = {0.004f, 0.012f, 0.006f, 1};
constexpr SkColor4f kHead = {0.90f, 1.0f, 0.92f, 1};
constexpr SkColor4f kBedInk = {0.055f, 0.17f, 0.075f, 1};
constexpr SkColor4f kLabel = {0.24f, 0.42f, 0.28f, 1};

/** One falling curtain. The three differ in size (depth), rate and period,
 *  so no column of one ever keeps step with a column of another. */
struct FieldSpec {
  const char* key;
  float size;        ///< px — also the depth cue
  float eachMs;      ///< head step, glyph to glyph down the column
  float durationMs;  ///< one glyph's whole life: flash, decay, gone
  float loopMs;      ///< the period every column re-drops on
  float alpha;       ///< element opacity — the depth haze
  float glowSigma;   ///< blurred underlay beneath the foreground; 0 = none
  uint32_t seed;     ///< the field's own text draw AND its column scatter
  double churnSecs;  ///< one full re-roll cycle of the substitution
};
constexpr FieldSpec kFields[] = {
    {"rain-far", 16.0f, 130.0f, 1900.0f, 8400.0f, 0.55f, 0.0f, 0x5157A3B1u,
     9.2},
    {"rain-mid", 23.0f, 105.0f, 1650.0f, 6200.0f, 0.80f, 4.0f, 0xA70F3C55u,
     7.6},
    {"rain-near", 32.0f, 80.0f, 1400.0f, 4600.0f, 1.0f, 7.0f, 0x2F81D9E7u, 6.4},
};
constexpr int kFieldCount = 3;

constexpr float kBedSize = 20.0f;
constexpr double kBedChurnSecs = 11.0;
constexpr uint32_t kBedSeed = 0xC3A5E1u;

/** The two charsets, cut where the substitution gate cuts. The face that
 *  carries this field sets every HALF-WIDTH form — the katakana proper
 *  (U+FF66, U+FF70..U+FF9D), the small forms (U+FF67..U+FF6F) and the
 *  corner brackets and dot (U+FF62, U+FF63, U+FF65) — at one advance, and
 *  the DIGITS plus '#' at another, wider one ('4' alone is cut a hair
 *  wider still, so it stays out). A substitution is honoured only within
 *  one advance class, so each class churns within itself and every glyph
 *  in the field passes the gate. */
std::u32string rainKana() {
  std::u32string set;
  set += U'ｦ';
  for (char32_t c = U'ｱ'; c <= U'ﾝ'; ++c) set += c;
  set += U'ｰ';
  set += U'｢';
  set += U'｣';
  set += U'･';
  for (char32_t c = U'ｧ'; c <= U'ｯ'; ++c) set += c;
  return set;
}
std::u32string rainWest() { return U"012356789#"; }

/** Which cells the western class holds: roughly one in seven, the way the
 *  screens sprinkle digits through the kana rather than dealing them
 *  evenly. */
constexpr uint32_t kWestOneIn = 7;

/** Deterministic and implementation-independent: the field must be the
 *  same field on every run and platform, or a byte-identity sweep reads
 *  the sketch as changed by nothing. */
struct Lcg {
  uint32_t state;
  uint32_t next() {
    state = state * 1664525u + 1013904223u;
    return state >> 8;
  }
  float uniform() { return (float)(next() % 65536u) / 65536.0f; }
};

/// ASCII (the western class) or U+0800..U+FFFF (the half-width forms).
void appendUtf8(std::string& out, char32_t c) {
  if (c < 0x80) {
    out.push_back((char)c);
    return;
  }
  out.push_back((char)(0xE0 | (c >> 12)));
  out.push_back((char)(0x80 | ((c >> 6) & 0x3F)));
  out.push_back((char)(0x80 | (c & 0x3F)));
}

/** THE STREAK, as one keyframe table over local time: the near-white flash
 *  (the table's own first segment, multiplier 1 over the head colour), the
 *  decay to phosphor green, the long dim, and gone by 1. Between beats a
 *  looping glyph rests at local 1, which this table paints dark — so the
 *  dark between streaks is the table's own tail, no hold needed, and the
 *  re-opening flash is its head. */
TextEffect streak() {
  return fx::keys({
      {0.000f, {}},
      {0.090f, {}},
      {0.220f, {.colorMul = {0.27f, 0.96f, 0.42f, 1}}},
      {0.600f, {.colorMul = {0.09f, 0.50f, 0.16f, 1}}},
      {1.000f, {.alpha = 0.0f, .colorMul = {0.02f, 0.20f, 0.06f, 1}}},
  });
}

/** WHICH CELLS ARE WESTERN — the digits-and-'#' class, addressed by the
 *  characters themselves so the partition follows whatever text a seed
 *  dealt. The complement is the half-width class. */
Selector westCells() { return sel::regex(u8"[0-9#]"); }

/** The per-glyph phosphor lift the whole field wears: each cell screens up
 *  by its own seeded amount — most barely, a few hard — so no two cells
 *  burn alike, the way the screens' tubes never sit at one brightness.
 *  Squaring the draw skews the field dim with sparse hot cells. Stable per
 *  glyph, so the field caches between churn steps. */
GlyphMod phosphorLift(Rng& rng) {
  GlyphMod m;
  const float lift = rng.unit() * rng.unit();
  m.colorScreen = {0.10f * lift, 0.45f * lift, 0.16f * lift, 0.0f};
  return m;
}

/** THE RECORD'S MIRROR, per glyph: every half-width form flips about its
 *  own centre (`scaleX = -1` rides the matrix lane — one matrix draw per
 *  mirrored glyph, this field's own deliberate load), digits stand
 *  unmirrored, and both classes take the phosphor lift. */
TextEffect mirrorLift() {
  return fx::effect(
      "rain-mirror-lift",
      [](const GlyphInfo&, float, Rng& rng) {
        GlyphMod m = phosphorLift(rng);
        m.scaleX = -1.0f;
        return m;
      },
      0.0f);
}
TextEffect westLift() {
  return fx::effect(
      "rain-west-lift",
      [](const GlyphInfo&, float, Rng& rng) { return phosphorLift(rng); },
      0.0f);
}

}  // namespace

// ===========================================================================

struct MatrixRain : sigil::compose::sketch::Sketch {
  sk_sp<SkTypeface> faceKana, faceLabel;

  // One text per curtain, built once against measured metrics; the column
  // count rides along only to spread the seeded scatter across the loop.
  std::string fieldText[kFieldCount];
  int fieldCols[kFieldCount] = {};
  std::string bedText;
  int totalGlyphs = 0;

  ch::Output<float> fall[kFieldCount];
  ch::Output<float> churn[kFieldCount];
  ch::Output<float> bedChurn{0.0f};

  /** The vertical style all four fields share the shape of: the kana held
   *  UPRIGHT — their default vertical orientation is rotated, and the
   *  screens show them standing. */
  [[nodiscard]] sigil::weave::TextStyle kanaStyle(float size, SkColor4f color,
                                                  float glowSigma) const {
    sigil::weave::TextStyle style =
        type({.face = faceKana, .size = size, .color = color});
    style.shaping.verticalForm = sigil::weave::VerticalForm::kUpright;
    if (glowSigma > 0)
      style.paint.addUnderlay(
          sigil::weave::PaintLayer::glow(0x8030FF60, glowSigma, 1.2f));
    return style;
  }

  /** One field's text, sized off measured metrics. A column is ended by
   *  an explicit newline every `rows` glyphs, with exactly as many columns
   *  as fit the width. Every cell shares one vertical advance in this
   *  face whichever class it draws from, so the mixed text keeps the
   *  grid. `outCols` reports the column count, which the curtain uses to
   *  spread its seeded scatter across the whole loop period — every age
   *  of streak on screen at once: rain, not surges. */
  void buildField(sigil::compose::sketch::SketchContext& ctx,
                  const std::u32string& kana, const std::u32string& west,
                  float size, uint32_t seed, std::string& outText,
                  int* outCols) {
    // An 8-glyph probe of one column: height/8 is the step down the
    // column, width is the column pitch the next one advances left by.
    std::string probe;
    for (int i = 0; i < 8; ++i) appendUtf8(probe, U'ｱ');
    const SkSize one =
        ctx.measure(text(toU8(probe), kanaStyle(size, kHead, 0.0f))
                        .writingMode(sigil::weave::WritingMode::kVerticalRL));
    const float step = std::max(1.0f, one.height() / 8.0f);
    const float pitch = std::max(1.0f, one.width());
    const int rows = std::max(1, (int)std::floor(kH / step));
    const int cols = std::max(1, (int)std::floor(kW / pitch));

    Lcg rng{seed};
    outText.clear();
    for (int c = 0; c < cols; ++c) {
      if (c > 0) outText.push_back('\n');
      for (int r = 0; r < rows; ++r) {
        const bool western = rng.next() % kWestOneIn == 0;
        const std::u32string& set = western ? west : kana;
        appendUtf8(outText, set[rng.next() % set.size()]);
      }
    }
    if (outCols) *outCols = cols;
    totalGlyphs += rows * cols;
  }

  /** One falling curtain: the streak on the declared looping schedule, the
   *  churn on its own wrapping clock split across the two advance classes,
   *  the record's mirror and the phosphor lift per glyph. */
  [[nodiscard]] Element curtain(int j) {
    const FieldSpec& f = kFields[j];
    // Each column starts at its own seeded rank of the scrambled even
    // ladder, spread across the whole loop period ((cols−1)/cols of it, so
    // the last rank does not fold onto the first); inside that beat the
    // cluster cascade runs the glyphs top to bottom at the field's own
    // rate; and the whole schedule re-opens every loopMs, each glyph on
    // its own fold of that period. The outer beat's length is not stated
    // anywhere — a column's beat lasts exactly as long as its own glyphs
    // need.
    const float cols = (float)std::max(fieldCols[j], 1);
    Stagger cascade =
        stagger(unit::Line, {.amountMs = f.loopMs * (cols - 1.0f) / cols,
                             .from = Stagger::From::Random});
    cascade.seed = f.seed;
    cascade.then(unit::Cluster,
                 {.eachMs = f.eachMs, .durationMs = f.durationMs});
    cascade.loopMs = f.loopMs;
    return text(toU8(fieldText[j]), kanaStyle(f.size, kHead, f.glowSigma))
        .key(f.key)
        .left(0)
        .top(0)
        .width(kW)
        .height(kH)
        .clip()
        .writingMode(sigil::weave::WritingMode::kVerticalRL)
        .opacity(f.alpha)
        .fx({.effect = streak(), .stagger = cascade, .progress = &fall[j]})
        .fx({.where = !westCells(), .effect = mirrorLift()})
        .fx({.where = westCells(), .effect = westLift()})
        .fx({.where = !westCells(),
             .effect = fx::scramble(rainKana(), 20),
             .progress = &churn[j]})
        .fx({.where = westCells(),
             .effect = fx::scramble(rainWest(), 20),
             .progress = &churn[j]});
  }

  [[nodiscard]] Element describe(sigil::compose::sketch::SketchContext& ctx) {
    Element root = stack().fill(Fill::color(kVoid));

    // The bed: the whole screen faintly alive. No streak track — these
    // glyphs are never bright and never absent, they only churn, mirrored
    // and lifted like the curtains above them.
    root.child(text(toU8(bedText), kanaStyle(kBedSize, kBedInk, 0.0f))
                   .key("rain-bed")
                   .left(0)
                   .top(0)
                   .width(kW)
                   .height(kH)
                   .clip()
                   .writingMode(sigil::weave::WritingMode::kVerticalRL)
                   .fx({.where = !westCells(), .effect = mirrorLift()})
                   .fx({.where = westCells(), .effect = westLift()})
                   .fx({.where = !westCells(),
                        .effect = fx::scramble(rainKana(), 20),
                        .progress = &bedChurn})
                   .fx({.where = westCells(),
                        .effect = fx::scramble(rainWest(), 20),
                        .progress = &bedChurn}));

    for (int j = 0; j < kFieldCount; ++j) root.child(curtain(j));

    // The monitor's falloff: a radial wash from clear centre to dark
    // edges, over everything. Static, so it caches.
    root.child(box()
                   .key("vignette")
                   .left(0)
                   .top(0)
                   .width(kW)
                   .height(kH)
                   .hitTestable(false)
                   .fill(Material::glowUnit(
                       {0.5f, 0.44f}, 1.05f,
                       {{0.0f, {0, 0, 0, 0}},
                        {0.60f, {0, 0, 0, 0.04f}},
                        {1.0f, {0.002f, 0.008f, 0.004f, 0.45f}}})));

    root.child(
        text(toU8("SIMON WHITELEY'S DIGITAL RAIN \xc2\xb7 " +
                  std::to_string(totalGlyphs) +
                  " GLYPHS IN FOUR PLANES \xc2\xb7 KATAKANA AND DIGITS, "
                  "MIRRORED PER GLYPH, HELD UPRIGHT \xc2\xb7 THE LIGHT FALLS, "
                  "THE TYPE STANDS STILL"),
             type({.face = faceLabel,
                   .size = 10.5f,
                   .color = kLabel,
                   .track = 2.2f}))
            .key("caption")
            .left(26)
            .top(kH - 30));

    if (kMeter)
      root.child(kit::trackMeter(ctx.composer, "rain-near", 0,
                                 {0.2f, 0.9f, 0.4f, 0.5f})
                     .absolute()
                     .inset(0)
                     .hitTestable(false));
    return root;
  }

  void setup(sigil::compose::sketch::SketchContext& ctx) override {
    ctx.canvas(kW, kH);
    ctx.background(kVoid);
    // Deep into the steady state: streaks at every age at once — fresh
    // heads, long tails, and columns resting dark between drops.
    ctx.captureAt(7.0);

    faceKana =
        pickFace({"Hiragino Kaku Gothic ProN", "Hiragino Sans", "Osaka"}, 400);
    faceLabel = pickFace({"Helvetica Neue", "Arial"}, 500);

    const std::u32string kana = rainKana();
    const std::u32string west = rainWest();
    totalGlyphs = 0;
    for (int j = 0; j < kFieldCount; ++j) {
      const FieldSpec& f = kFields[j];
      buildField(ctx, kana, west, f.size, f.seed, fieldText[j], &fieldCols[j]);
    }
    buildField(ctx, kana, west, kBedSize, kBedSeed, bedText, nullptr);

    ctx.composer.render(describe(ctx));
  }

  void update(double elapsed,
              sigil::compose::sketch::SketchContext& ctx) override {
    for (int j = 0; j < kFieldCount; ++j) {
      const FieldSpec& f = kFields[j];
      // The master is a wrapping phase whose wall period is the cascade's
      // own declared loop, so one wrap is one cycle of every beat and the
      // schedule runs at its authored ms — nothing is read back, nothing
      // can drift.
      fall[j] = motion::phase(elapsed, (double)f.loopMs / 1000.0);
      churn[j] = motion::phase(elapsed + (double)j * 1.7, f.churnSecs);
    }
    bedChurn = motion::phase(elapsed, kBedChurnSecs);
    // The picture animates through bound outputs alone; only the
    // instrument, which reads the schedule at describe time, needs the
    // tree re-described.
    if (kMeter) ctx.composer.render(describe(ctx));
  }
};

SIGIL_SKETCH(MatrixRain)
