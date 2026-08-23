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
//   * Every colour, size, rate and cue. Four planes — a dim churning bed
//     and three falling depths, one leaf each — are this study's
//     composition; the film's screens are a single plane.
//   * The charset keeps to the half-width kana and leaves the record's
//     digits out, on purpose: a substitution is honoured only where the
//     replacement has the original's advance, and the kana block is
//     advance-uniform in the CJK faces that carry it — so the whole
//     charset passes that gate and every glyph in the field churns. Mixing
//     in digits of another width would freeze exactly the glyphs the gate
//     refuses, silently.
//   * The mirror is ONE flip of each field (`scaleX(-1)` on the leaf), not
//     a per-glyph deviation. The record mirrors every glyph the same way,
//     so one turn of the whole curtain states it — where a per-glyph
//     mirror is a non-uniform scale, which forces every glyph out of the
//     shared-transform batch into its own matrix draw.
//
// -----------------------------------------------------------------------------
// THE MACHINE, and what it puts under load
//
//   * ONE TEXT LEAF PER DEPTH, running down the page: `writingMode`
//     vertical-RL, the kana held UPRIGHT by their style (their default
//     vertical orientation is rotated, which is not what the screens
//     show). A column is a LINE unit there, so "each column on its own
//     clock" is a CUE TABLE over lines — one seeded start per column —
//     with a nested cluster cascade running the glyphs down inside each
//     column's beat. The whole schedule is declared; nothing steps
//     per-column state by hand.
//   * THE RAIN NEVER STOPS because the cascade LOOPS: `Stagger::loopMs`
//     re-opens every glyph's beat once per period, phase-offset by its
//     column's cue, and the cues are drawn across the whole period — so at
//     any instant every age of streak is on screen somewhere and each
//     column re-drops forever on its own offset. The master is a wrapping
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
//   * THE CHURN is `fx::scramble` on a second track with its own wrapping
//     progress, composing with the streak by the track algebra — the
//     substitution from one track, alpha and tint from the other.
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
//               for tuning the cue table.
//
// Run:
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/matrix_rain.cpp \
//       --frame /tmp/matrix_rain.png --at 7.0

#include <sigilcompose/Debug.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Studio.h>
#include <sigilcompose/TextFx.h>
#include <sigilsketch/Sketch.h>
#include <sigilweave/Style.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace sigil::compose;
using namespace sigil::compose::util;
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
  uint32_t seed;     ///< the field's own text and cue draw
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

/** The charset, and the whole point of its bounds: half-width katakana
 *  (U+FF66, U+FF70, U+FF71..U+FF9D) are one advance in the faces that
 *  carry them, so every substitution the churn asks for passes the
 *  equal-advance gate. */
std::u32string rainSet() {
  std::u32string set;
  set += U'ｦ';
  set += U'ｰ';
  for (char32_t c = U'ｱ'; c <= U'ﾝ'; ++c) set += c;
  return set;
}

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

/// Every code point in the set is three UTF-8 bytes (U+0800..U+FFFF).
void appendUtf8(std::string& out, char32_t c) {
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

}  // namespace

// ===========================================================================

struct MatrixRain : sigil::compose::sketch::Sketch {
  sk_sp<SkTypeface> faceKana, faceLabel;

  // One text and one cue table per curtain, built once against measured
  // metrics so the cue table and the laid-out columns agree entry for
  // entry.
  std::string fieldText[kFieldCount];
  std::vector<float> fieldCues[kFieldCount];
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
        studio::type({.face = faceKana, .size = size, .color = color});
    style.shaping.verticalForm = sigil::weave::VerticalForm::kUpright;
    if (glowSigma > 0)
      style.paint.addUnderlay(
          sigil::weave::PaintLayer::glow(0x8030FF60, glowSigma, 1.2f));
    return style;
  }

  /** One field's text and cue table, sized off measured metrics. A column
   *  is ended by an explicit newline every `rows` glyphs, with exactly as
   *  many columns as fit the width — so the landed lines and the cue
   *  table match by construction, and the cascade warns about neither a
   *  short table nor unread entries. Cues are drawn across the WHOLE loop
   *  period: the fold phase-offsets each column's re-drop by its cue, so a
   *  full-period spread is what keeps every age of streak on screen at
   *  once — rain, not surges. */
  void buildField(sigil::compose::sketch::SketchContext& ctx,
                  const std::u32string& set, float size, uint32_t seed,
                  float loopMs, std::string& outText,
                  std::vector<float>* outCues) {
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
      for (int r = 0; r < rows; ++r)
        appendUtf8(outText, set[rng.next() % set.size()]);
    }
    if (outCues) {
      outCues->clear();
      outCues->reserve((size_t)cols);
      for (int c = 0; c < cols; ++c) outCues->push_back(rng.uniform() * loopMs);
    }
    totalGlyphs += rows * cols;
  }

  /** One falling curtain: the streak on the declared looping schedule, the
   *  churn on its own wrapping clock, the record's mirror as one flip of
   *  the leaf. */
  [[nodiscard]] Element curtain(int j) {
    const FieldSpec& f = kFields[j];
    // Column k starts at its cue; inside that beat the cluster cascade
    // runs the glyphs top to bottom at the field's own rate; and the whole
    // schedule re-opens every loopMs, each glyph on its own fold of that
    // period. The outer beat's length is not stated anywhere — a column's
    // beat lasts exactly as long as its own glyphs need.
    Stagger cascade = stagger(unit::Line, cues(fieldCues[j]));
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
        .scaleX(-1.0f)
        .opacity(f.alpha)
        .fx({.effect = streak(), .stagger = cascade, .progress = &fall[j]})
        .fx({.effect = fx::scramble(rainSet(), 20), .progress = &churn[j]});
  }

  [[nodiscard]] Element describe(sigil::compose::sketch::SketchContext& ctx) {
    Element root = stack().fill(Fill::color(kVoid));

    // The bed: the whole screen faintly alive. No streak track — these
    // glyphs are never bright and never absent, they only churn.
    root.child(text(toU8(bedText), kanaStyle(kBedSize, kBedInk, 0.0f))
                   .key("rain-bed")
                   .left(0)
                   .top(0)
                   .width(kW)
                   .height(kH)
                   .clip()
                   .writingMode(sigil::weave::WritingMode::kVerticalRL)
                   .scaleX(-1.0f)
                   .fx({.effect = fx::scramble(rainSet(), 20),
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
                  " GLYPHS IN FOUR PLANES \xc2\xb7 HALF-WIDTH KATAKANA, "
                  "MIRRORED, HELD UPRIGHT \xc2\xb7 THE LIGHT FALLS, THE TYPE "
                  "STANDS STILL"),
             studio::type({.face = faceLabel,
                           .size = 10.5f,
                           .color = kLabel,
                           .track = 2.2f}))
            .key("caption")
            .left(26)
            .top(kH - 30));

    if (kMeter)
      root.child(debug::trackMeter(ctx.composer, "rain-near", 0,
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

    faceKana = studio::pickFace(
        {"Hiragino Kaku Gothic ProN", "Hiragino Sans", "Osaka"}, 400);
    faceLabel = studio::pickFace({"Helvetica Neue", "Arial"}, 500);

    const std::u32string set = rainSet();
    totalGlyphs = 0;
    for (int j = 0; j < kFieldCount; ++j) {
      const FieldSpec& f = kFields[j];
      buildField(ctx, set, f.size, f.seed, f.loopMs, fieldText[j],
                 &fieldCues[j]);
    }
    buildField(ctx, set, kBedSize, kBedSeed, 0.0f, bedText, nullptr);

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
      fall[j] = studio::phase(elapsed, (double)f.loopMs / 1000.0);
      churn[j] = studio::phase(elapsed + (double)j * 1.7, f.churnSecs);
    }
    bedChurn = studio::phase(elapsed, kBedChurnSecs);
    // The picture animates through bound outputs alone; only the
    // instrument, which reads the schedule at describe time, needs the
    // tree re-described.
    if (kMeter) ctx.composer.render(describe(ctx));
  }
};

SIGIL_SKETCH(MatrixRain)
