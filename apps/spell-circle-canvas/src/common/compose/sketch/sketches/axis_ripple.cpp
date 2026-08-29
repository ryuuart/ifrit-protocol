// axis_ripple.cpp — PATTERN: the weight wave. The demo every variable
// typeface ships with, and the one constraint every version of it hides.
// =============================================================================
// THE PATTERN
//
// OpenType Font Variations arrived in OpenType 1.8 (2016), announced jointly
// by Adobe, Apple, Google and Microsoft at ATypI Warsaw. Within months the
// specimen page had settled into a form that has barely changed since: one
// headline, set large, with a WAVE OF WEIGHT travelling along it — each
// letter riding the same sine a fixed phase behind its neighbour, so the
// line appears to inhale from left to right. It is on v-fonts.com, it is
// what the Codrops variable-font demos animate, it is what a Font of the
// Month specimen does on load, and it is the first thing anyone builds after
// installing their first variable face.
//
// The word set here is HAMBURGEFONTSIV, the type designer's own proofing
// string — the letters chosen because between them they carry most of the
// shapes a Latin face has to get right.
//
// -----------------------------------------------------------------------------
// THE CONSTRAINT THE WEB VERSION HIDES
//
// The web demo animates `wght`, and `wght` MOVES ADVANCES. A heavier letter
// is a wider letter, so every frame of that animation is a fresh line
// layout: the letters slide horizontally as the wave passes, the line's
// width breathes with it, and whatever sits after the headline moves. In a
// browser that is invisible because the browser re-lays the line anyway.
//
// This engine draws a driven axis at DRAW TIME, over glyphs that were shaped
// once — which is what makes a per-letter axis cost one batched draw instead
// of one layout per frame — and that is sound only where the axis does not
// move an advance. So it PROBES the face and refuses the ones that do.
// `GRAD` is the axis that exists for exactly this: a grade is weight without
// width, drawn heavier on the same skeleton, and the letters stand still.
//
// The bottom half of this sheet is the proof rather than the claim. The same
// word is shaped twice — once at wght 300, once at wght 900 — and the two
// runs are measured with `runPens`, which shapes through the same path a
// text leaf takes. The overhang between them, printed on the canvas, is how
// far every letter after the first would travel during one pass of the wave
// if the drive were allowed. The GRAD row above it is measured the same way
// and comes out to zero, which is the whole reason it is permitted.
//
// -----------------------------------------------------------------------------
// HOW THE RIPPLE IS SPELLED
//
// `fx::waveLoop` is this shape already, on dy. The axis version is the same
// three lines with the sine landing on `GlyphMod::axis` instead — an ad-hoc
// effect under a key, driven by a wrapping phase with `eachMs = 0` so every
// glyph reads ONE master phase and the travelling wave comes from the glyph's
// own index inside the effect body rather than from the cascade.
//
// The track is NOT continuous, and that is worth knowing about a wave this
// large. A driven axis coordinate is snapped before it reaches the draw,
// because each distinct value is its own clone and its own glyph-atlas
// strike — and the ladder it snaps to is cut per rendered size, since one
// step in design units displaces an outline by more pixels the larger the
// glyph is. So the 64 px hero here gets a ladder several times finer than a
// caption's, and the ramp reads smooth off the memoized faces. `continuous`
// is the opt-out for where that still is not enough, and it costs a face
// built and rasterized fresh every frame.
//
// EDIT THESE FIRST
//   kRadPerGlyph — the wavelength. 0 makes the whole line pulse together;
//                  0.55 is about one full wave across HAMBURGEFONTSIV.
//   kGradLo / kGradHi — the ends of the ramp. The axis's own range is
//                  400 to 1000 on this face and the sketch prints it.
//   kPeriod      — seconds per pass.
//
// Run:
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/axis_ripple.cpp \
//       --frame /tmp/axis_ripple.png --at 2.05

#include <include/core/SkCanvas.h>
#include <include/core/SkTypeface.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/TextFx.h>
#include <sigilcompose/Typography.h>
#include <sigilsketch/Sketch.h>

#include <cmath>
#include <memory>
#include <string>
#include <vector>

using namespace sigil::compose;

namespace {

constexpr float kW = 1120.0f;
constexpr float kH = 620.0f;

constexpr SkColor4f kPaper = hex(0x0C0C0E);
constexpr SkColor4f kInk = hex(0xF4F1EA);
constexpr SkColor4f kLabel = hex(0x7E8492);
constexpr SkColor4f kFaint = hex(0x3A3F4B);
constexpr SkColor4f kMark = hex(0xE2504B);  // the overhang
constexpr SkColor4f kAxis = hex(0x63B8FF);  // the driven coordinate

const char* kProof = "HAMBURGEFONTSIV";

constexpr float kProofSize = 64.0f;
constexpr float kProofTrack = 1.0f;

// ---- the wave -------------------------------------------------------------
constexpr float kGradLo = 400.0f;
constexpr float kGradHi = 1000.0f;
constexpr float kRadPerGlyph = 0.55f;
constexpr double kPeriod = 2.6;

// ---- the proof ------------------------------------------------------------
constexpr float kWghtLo = 300.0f;
constexpr float kWghtHi = 900.0f;
constexpr float kProofRowSize = 40.0f;

/** The ripple: `fx::waveLoop`'s sine, landing on the axis coordinate.
 *
 *  The phase comes from the glyph's own index rather than from the cascade,
 *  so the track's stagger is one beat for the whole line and the effect
 *  owns the wavelength. That is the shape a LOOP wants: a cascade spreads
 *  START TIMES, which is right for an entrance and wrong for something that
 *  never ends. */
TextEffect gradWave(float lo, float hi, float radPerGlyph) {
  return fx::effect("gradWave",
                    [lo, hi, radPerGlyph](const GlyphInfo& g, float t, Rng&) {
                      const float s =
                          0.5f + 0.5f * std::sin(t * 6.2831853f -
                                                 (float)g.index * radPerGlyph);
                      GlyphMod m;
                      m.axis = sigil::weave::FontVariation("GRAD",
                                                           lo + (hi - lo) * s);
                      return m;
                    },
                    0.0f, {lo, hi, radPerGlyph})
      // The wave is a GRADE, and only an advance-invariant axis is honoured:
      // every letter keeps the pen position shaping gave it for the whole
      // ripple. The phase driving this is bound and never settles, so saying
      // so is what keeps the run on whole-pixel origins and one atlas strike
      // per letter instead of one per phase.
      .displacing(false);
}

}  // namespace

// ===========================================================================

struct AxisRipple : sigil::compose::sketch::Sketch {
  choreograph::Output<float> phase{0};

  sk_sp<SkTypeface> face, faceLabel;
  sigil::weave::TextStyle proof;
  std::vector<float> pens;  // the ripple line's glyph pens
  int glyphs = 0;
  float widthLo = 0, widthHi = 0;          // the wght proof rows
  float gradWidthLo = 0, gradWidthHi = 0;  // the same measure on GRAD
  float axisMin = 0, axisMax = 0;          // what the face itself declares
  bool hasGrad = false;

  [[nodiscard]] sigil::weave::TextStyle small(SkColor4f color,
                                              float size = 11.5f,
                                              float track = 2.4f) const {
    return type(
        {.face = faceLabel, .size = size, .color = color, .track = track});
  }

  /** THE METER: the axis coordinate each letter is being drawn at, as a bar
   *  under that letter.
   *
   *  It restates the effect's own sine, and it is a schedule meter's
   *  opposite rather than a hand-rolled one. `Composer::beatsOf` reports
   *  what a CASCADE scheduled — and this wave is not in the cascade: a
   *  loop reads a wrapping phase that every glyph must see at once, so the
   *  stagger is one flat beat and the wavelength lives in the effect body,
   *  off the glyph's own index. The DEVIATION a track computed goes to the
   *  draw and no further, so the coordinate under each letter can only be
   *  computed again here.
   *
   *  What it does NOT restate is the clock: the program reads the SAME
   *  phase Output the track's progress is bound to, so the bars cannot
   *  drift a frame away from the letters they describe. A second
   *  `motion::phase` off `elapsedSeconds` would have been one line shorter
   *  and wrong.
   *
   *  The pens come from `runPens` on the UNDRIVEN style, which is only
   *  sound because the axis is advance-invariant: that is the same fact the
   *  engine's own gate checks, used here for a second purpose. */
  [[nodiscard]] Element meter(float width) {
    // The pens ride into the program behind a shared pointer rather than as
    // a copied vector: a paint program is copied whenever the element value
    // is, and a copy that can allocate is a copy that can throw.
    auto localPens = std::make_shared<const std::vector<float>>(pens);
    const int n = glyphs;
    const choreograph::Output<float>* clock = &phase;
    return custom("axis-meter",
                  [localPens = std::move(localPens), n, clock](
                      SkCanvas& canvas, const PaintContext& ctx) {
                    const float t = clock->value();
                    const float h = ctx.size.height();
                    SkPaint bed, bar;
                    bed.setColor4f(kFaint, nullptr);
                    bar.setColor4f(kAxis, nullptr);
                    for (int i = 0; i < n; ++i) {
                      const float x = (*localPens)[(size_t)i];
                      const float w = (*localPens)[(size_t)i + 1] - x - 3.0f;
                      if (w <= 0) continue;
                      const float s =
                          0.5f + 0.5f * std::sin(t * 6.2831853f -
                                                 (float)i * kRadPerGlyph);
                      canvas.drawRect(SkRect::MakeXYWH(x, h - 1, w, 1), bed);
                      canvas.drawRect(SkRect::MakeXYWH(x, h - 1 - s * (h - 1),
                                                       w, s * (h - 1) + 1),
                                      bar);
                    }
                  })
        .width(width)
        .height(34)
        .cache(Cache::None);
  }

  /** The ripple itself, plus the coordinate range the face declares. */
  [[nodiscard]] Element ripplePanel() {
    const float width = pens.back();
    Element panel = box().column().gap(10).width(width);
    panel.child(text(toU8("GRAD \xe2\x80\x94 DRIVEN AT DRAW TIME, "
                          "ONE SHAPING, LETTERS FIXED"),
                     small(kLabel)));
    panel.child(text(toU8(kProof), proof)
                    .key("ripple")
                    .fx({.effect = gradWave(kGradLo, kGradHi, kRadPerGlyph),
                         .stagger = {.eachMs = 0, .durationMs = 400},
                         .progress = &phase}));
    panel.child(meter(width));
    char line[160];
    // One line, deliberately: the panel is the run's own width, so a
    // caption that wraps changes the sheet's height between frames.
    std::snprintf(line, sizeof(line),
                  "GRAD %.0f\xe2\x80\x93%.0f \xc2\xb7 %.2f RAD/GLYPH "
                  "\xc2\xb7 %.1f S PER PASS \xc2\xb7 RUN WIDTH "
                  "\xce\x94 %.2f PX ACROSS THE RAMP",
                  axisMin, axisMax, kRadPerGlyph, kPeriod,
                  gradWidthHi - gradWidthLo);
    panel.child(text(toU8(hasGrad ? line
                                  : "THIS FACE DECLARES NO GRAD AXIS \xc2\xb7 "
                                    "THE DRIVE IS REFUSED AND THE LINE DRAWS "
                                    "AT ITS SHAPED COORDINATES"),
                     small(hasGrad ? kFaint : kMark, 11.0f, 0.6f)));
    return panel;
  }

  /** The proof: the same word shaped at two weights, left edges aligned.
   *
   *  Every letter after the first has moved, and the rule marks where the
   *  light run ended. That distance is what a draw-time drive would have to
   *  pretend was zero, which is why the runtime measures the face and says
   *  no. */
  [[nodiscard]] Element wghtPanel() {
    const auto row = [&](float weight, SkColor4f color, const char* tag,
                         bool marked) {
      Element run = text(toU8(kProof), type({.face = face,
                                             .size = kProofRowSize,
                                             .color = color,
                                             .track = kProofTrack * 0.6f,
                                             .weight = weight}));
      // THE RULE IS ANCHORED TO THE RUN, not fitted to it. An unsliced
      // selector resolves to the union of every glyph's box, so pct(100) of
      // that rect is the last letter's trailing edge — which moves with the
      // label column, the gap and the tracking, none of which the mark has
      // to be told about.
      if (marked)
        run.mark(
            Selector{},
            box().key("rule").left(pct(100)).top(0).width(1).height(112).fill(
                Fill::color(kMark)));
      return box()
          .row()
          .alignItems(Align::Baseline)
          .gap(16)
          .child(text(toU8(tag), small(kLabel, 11.0f, 1.6f)).width(62))
          .child(std::move(run));
    };
    char delta[160];
    std::snprintf(delta, sizeof(delta),
                  "wght %.0f \xe2\x86\x92 %.0f WIDENS THE RUN BY %.2f PX "
                  "(%.1f%%) \xc2\xb7 EVERY LETTER AFTER THE FIRST MOVES, "
                  "SO THE DRIVE IS REFUSED",
                  kWghtLo, kWghtHi, widthHi - widthLo,
                  widthLo > 0 ? 100.0f * (widthHi - widthLo) / widthLo : 0.0f);

    return box()
        .column()
        .gap(12)
        .child(text(toU8("wght \xe2\x80\x94 A SHAPING AXIS: THE SAME WORD, "
                         "TWICE, MEASURED"),
                    small(kLabel)))
        // The rule stands where the LIGHT run ended, so the heavy run's
        // overhang past it is the number below.
        .child(box()
                   .column()
                   .gap(6)
                   .child(row(kWghtLo, kInk, "300", true))
                   .child(row(kWghtHi, kInk, "900", false)))
        .child(text(toU8(delta), small(kMark, 11.0f, 0.6f)));
  }

  [[nodiscard]] Element describe() {
    return box()
        .column()
        .padding(52, 44)
        .gap(30)
        .fill(Material::linear(
            {0, 0}, {0, kH},
            {{0.0f, kPaper}, {0.6f, hex(0x111116)}, {1.0f, kPaper}}))
        .child(
            box()
                .row()
                .alignItems(Align::End)
                .child(text(toU8("THE AXIS RIPPLE"), small(kInk, 12.5f, 3.4f))
                           .grow(1))
                .child(text(toU8("OPENTYPE FONT VARIATIONS \xc2\xb7 2016"),
                            small(kFaint))))
        .child(box().height(1).fill(Fill::color(kFaint)))
        .child(ripplePanel())
        .child(box().height(6))
        .child(wghtPanel())
        .child(box().grow(1))
        .child(text(toU8("A GRADE IS WEIGHT WITHOUT WIDTH \xc2\xb7 IT IS THE "
                         "ONE AXIS A DRAW-TIME DRIVE CAN HONOUR, AND THE "
                         "REASON THE RIPPLE COSTS ONE SHAPING RATHER THAN "
                         "ONE PER FRAME"),
                    small(kFaint, 11.0f, 0.6f)));
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kW, kH);
    ctx.background(kPaper);
    // A quarter-pass in: the wave's crest is inside the word rather than at
    // either end, so both the ramp up and the ramp down are on the page.
    ctx.captureAt(kPeriod * 0.79);
    if (!ctx.fonts) return;

    // The system grotesque is the face here because it is the one installed
    // face that carries BOTH axes this sheet needs — a grade to drive and a
    // weight to measure against it.
    face = pickFace({".SF NS", "SF Pro", "Helvetica Neue"}, 700);
    faceLabel = pickFace({".SF NS", "SF Pro", "Helvetica Neue"}, 500);
    proof = type({.face = face,
                  .size = kProofSize,
                  .color = kInk,
                  .track = kProofTrack});
    pens = runPens(toU8(kProof), proof, *ctx.fonts);
    glyphs = (int)pens.size() - 1;

    // What the face itself declares, read off the face rather than assumed.
    if (face) {
      const int count = face->getVariationDesignParameters({});
      if (count > 0) {
        std::vector<SkFontParameters::Variation::Axis> axes((size_t)count);
        face->getVariationDesignParameters(SkSpan(axes.data(), (size_t)count));
        for (const auto& a : axes)
          if (a.tag == SkSetFourByteTag('G', 'R', 'A', 'D')) {
            hasGrad = true;
            axisMin = a.min;
            axisMax = a.max;
          }
      }
    }

    // The measurements, taken through the same shaping path a text leaf
    // takes. Both pairs are shaped at the ROW size, so the printed px are
    // the px on the page.
    const auto widthAt = [&](const char (&tag)[5], float value, float size) {
      sigil::weave::TextStyle s =
          type({.face = face,
                .size = size,
                .color = kInk,
                .track = kProofTrack * (size == kProofSize ? 1.0f : 0.6f)});
      s.variation(tag, value);
      return runPens(toU8(kProof), s, *ctx.fonts).back();
    };
    widthLo = widthAt("wght", kWghtLo, kProofRowSize);
    widthHi = widthAt("wght", kWghtHi, kProofRowSize);
    gradWidthLo = widthAt("GRAD", kGradLo, kProofSize);
    gradWidthHi = widthAt("GRAD", kGradHi, kProofSize);

    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      phase = motion::phase(t, kPeriod);
      return true;
    });

    ctx.composer.render(describe());
  }
};

SIGIL_SKETCH(AxisRipple)
