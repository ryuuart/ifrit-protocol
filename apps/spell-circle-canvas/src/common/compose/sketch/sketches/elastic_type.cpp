// elastic_type.cpp — PATTERN: rubber type. Two published keyframe tables,
// transcribed number for number and run per glyph.
// =============================================================================
// THE PATTERN, AND ITS SOURCE
//
// Animate.css (Daniel Eden, 2013) is the CSS animation library that put a
// vocabulary of named motions into everyday web work, and two of its
// entries are the whole elastic-lettering genre:
//
//   rubberBand — a squash-and-stretch on the two scale axes, overshooting
//                and settling in six keyframes.
//   jello      — a decaying shear: the same skew, halved and reversed at
//                each step, eight times.
//
// They are the web's restatement of the animator's first principle. SQUASH
// AND STRETCH is the first of the twelve in Thomas and Johnston's THE
// ILLUSION OF LIFE (1981): a body deforms under acceleration and preserves
// its volume while it does, which is why the stretched frame is narrow and
// the squashed frame is wide. `rubberBand`'s table obeys that — every pair
// multiplies out near 1 — and `jello`'s does not, because a shear is not a
// squash.
//
// -----------------------------------------------------------------------------
// THE TABLES, VERBATIM
//
//   rubberBand      scaleX  scaleY          jello        skewX
//     0%             1.00    1.00             0.0%        0
//    30%             1.25    0.75            11.1%        0
//    40%             0.75    1.25            22.2%      -12.5°
//    50%             1.15    0.85            33.3%       +6.25°
//    65%             0.95    1.05            44.4%       -3.125°
//    75%             1.05    0.95            55.5%       +1.5625°
//   100%             1.00    1.00            66.6%       -0.78125°
//                                            77.7%       +0.390625°
//                                            88.8%       -0.1953125°
//                                           100.0%        0
//
// WHAT IS NOT REPRODUCED, and it matters twice:
//
//  * `jello` shears on BOTH axes — the published rule is
//    `skewX(a) skewY(a)`, the same angle on each. A `GlyphMod` carries
//    `skewXDeg` and no Y counterpart, so only half the shear is here. The
//    difference is visible: the real thing rocks on a diagonal, this rocks
//    horizontally.
//  * CSS interpolates each keyframe segment with its own timing function,
//    `ease` by default. The segments below are LINEAR between the published
//    numbers, so the corners of the curve are sharper than a browser's. The
//    graphs at the bottom draw exactly what the effects evaluate, which is
//    the point of drawing them.
//
// -----------------------------------------------------------------------------
// WHAT THIS PUTS UNDER LOAD
//
// A non-uniform scale and a shear are the ONE deviation an RSXform cannot
// carry: that transform encodes a rotation, one scale and no shear at all.
// A glyph whose composed deviation uses `scaleX`, `scaleY` or `skewXDeg`
// therefore leaves the shared transform array and draws under its own
// matrix — same passes, same paint, one canvas concat — while its
// neighbours stay batched. Both rows here are a whole line of such glyphs,
// which is the worst case for that split and the reason it is worth having
// a study of.
//
// EDIT THESE FIRST
//   kEachMs   — start-to-start per letter. At 0 the whole word deforms as
//               one body, which is what the CSS class actually does to an
//               element; per letter is what a lettering artist does to a
//               word.
//   kDurMs    — one letter's whole table, start to finish.
//
// Run:
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/elastic_type.cpp \
//       --frame /tmp/elastic_type.png --at 1.15

#include <include/core/SkCanvas.h>
#include <include/core/SkPathBuilder.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Studio.h>
#include <sigilcompose/TextFx.h>
#include <sigilsketch/Sketch.h>

#include <array>
#include <cmath>
#include <string>

using namespace sigil::compose;
using namespace sigil::compose::util;

namespace {

constexpr float kW = 1080.0f;
constexpr float kH = 620.0f;

constexpr SkColor4f kPaper = studio::hex(0x101014);
constexpr SkColor4f kInk = studio::hex(0xF6F2E9);
constexpr SkColor4f kLabel = studio::hex(0x848B99);
constexpr SkColor4f kFaint = studio::hex(0x2E3440);
constexpr SkColor4f kX = studio::hex(0xFF7A59);  // scaleX / skewX
constexpr SkColor4f kY = studio::hex(0x5AC8F5);  // scaleY
constexpr SkColor4f kRest = studio::hex(0x4A5262);

constexpr float kWordSize = 68.0f;
constexpr float kEachMs = 62.0f;
constexpr float kDurMs = 900.0f;
constexpr double kLoop = 3.4;  // one pass of both rows, then a rest

// ---------------------------------------------------------------------------
// The tables. One definition each, read by the EFFECT and by the GRAPH, so
// the picture cannot disagree with the motion.

struct Key {
  float at;  // local time, 0..1
  float x;   // scaleX, or skewX in degrees
  float y;   // scaleY
};

constexpr std::array<Key, 7> kRubber = {{{0.00f, 1.00f, 1.00f},
                                         {0.30f, 1.25f, 0.75f},
                                         {0.40f, 0.75f, 1.25f},
                                         {0.50f, 1.15f, 0.85f},
                                         {0.65f, 0.95f, 1.05f},
                                         {0.75f, 1.05f, 0.95f},
                                         {1.00f, 1.00f, 1.00f}}};

constexpr std::array<Key, 10> kJello = {{{0.000f, 0.0f, 0},
                                         {0.111f, 0.0f, 0},
                                         {0.222f, -12.5f, 0},
                                         {0.333f, 6.25f, 0},
                                         {0.444f, -3.125f, 0},
                                         {0.555f, 1.5625f, 0},
                                         {0.666f, -0.78125f, 0},
                                         {0.777f, 0.390625f, 0},
                                         {0.888f, -0.1953125f, 0},
                                         {1.000f, 0.0f, 0}}};

/** Linear interpolation across a keyframe table — what a browser does with
 *  `animation-timing-function: linear`, and not what it does by default. */
template <size_t N>
Key sample(const std::array<Key, N>& table, float t) {
  t = t < 0 ? 0 : (t > 1 ? 1 : t);
  for (size_t i = 1; i < N; ++i) {
    if (t > table[i].at) continue;
    const Key& a = table[i - 1];
    const Key& b = table[i];
    const float span = b.at - a.at;
    const float u = span > 0 ? (t - a.at) / span : 0.0f;
    return {t, a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u};
  }
  return table[N - 1];
}

/** rubberBand, per glyph. The two scales are independent, so the glyph
 *  leaves the batched RSXform array and draws under its own matrix. */
TextEffect rubberBand() {
  return fx::effect(
      "animateCss.rubberBand",
      [](const GlyphInfo&, float t, Rng&) {
        const Key k = sample(kRubber, t);
        GlyphMod m;
        m.scaleX = k.x;
        m.scaleY = k.y;
        return m;
      },
      // The widest keyframe grows a glyph by a quarter about its own pivot.
      kWordSize * 0.5f);
}

/** jello, per glyph — the X half of it. The published rule shears on both
 *  axes by the same angle; there is no `skewYDeg` on a GlyphMod, so this is
 *  the horizontal component alone and the motion rocks rather than wobbles. */
TextEffect jello() {
  return fx::effect(
      "animateCss.jello",
      [](const GlyphInfo&, float t, Rng&) {
        GlyphMod m;
        m.skewXDeg = sample(kJello, t).x;
        return m;
      },
      kWordSize * 0.5f);
}

// ---------------------------------------------------------------------------
// The graphs — the same tables, plotted. Pure functions of local time, so
// they are static leaves and nothing here reads a clock.

void strokePath(SkCanvas& canvas, const SkPath& path, SkColor4f color,
                float width) {
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(width);
  paint.setColor4f(color, nullptr);
  canvas.drawPath(path, paint);
}

/** One lane of a table, over local time, with a dot at every published
 *  keyframe. @p lo and @p hi are the value range the plot's height spans. */
template <size_t N>
Element graph(const char* key, const std::array<Key, N>& table, bool useX,
              SkColor4f color, float lo, float hi, float rest) {
  return custom(key,
                [&table, useX, color, lo, hi, rest](SkCanvas& canvas,
                                                    const PaintContext& ctx) {
                  const float w = ctx.size.width(), h = ctx.size.height();
                  const auto toY = [&](float v) {
                    return h - (v - lo) / (hi - lo) * h;
                  };
                  SkPaint rule;
                  rule.setStyle(SkPaint::kStroke_Style);
                  rule.setStrokeWidth(1);
                  rule.setColor4f(kRest, nullptr);
                  canvas.drawLine(0, toY(rest), w, toY(rest), rule);

                  SkPathBuilder trace;
                  for (int i = 0; i <= 240; ++i) {
                    const float t = (float)i / 240.0f;
                    const Key k = sample(table, t);
                    const float v = useX ? k.x : k.y;
                    const SkPoint at{w * t, toY(v)};
                    i == 0 ? (void)trace.moveTo(at) : (void)trace.lineTo(at);
                  }
                  strokePath(canvas, trace.detach(), color, 1.6f);

                  SkPaint dot;
                  dot.setAntiAlias(true);
                  dot.setColor4f(color, nullptr);
                  for (const Key& k : table)
                    canvas.drawCircle(w * k.at, toY(useX ? k.x : k.y), 2.6f,
                                      dot);
                })
      .width(pct(100))
      .height(pct(100));
}

}  // namespace

// ===========================================================================

struct ElasticType : sigil::compose::sketch::Sketch {
  choreograph::Output<float> pass{0};  // one wrapping 0→1 over kLoop

  sk_sp<SkTypeface> face, faceLabel;

  [[nodiscard]] sigil::weave::TextStyle small(SkColor4f color,
                                              float size = 11.5f,
                                              float track = 2.4f) const {
    return studio::type(
        {.face = faceLabel, .size = size, .color = color, .track = track});
  }

  /** One specimen row: the word, deformed letter by letter.
   *
   *  The cascade is per CLUSTER rather than per glyph, which for this word
   *  is the same thing and stays right for text where it is not — a letter
   *  and its marks are one body and squash together. */
  [[nodiscard]] Element row(const char* word, const char* caption,
                            TextEffect effect) {
    const sigil::weave::TextStyle set = studio::type(
        {.face = face, .size = kWordSize, .color = kInk, .track = 3.0f});
    sigil::weave::TextStyle ghostStyle = set;
    ghostStyle.paint.foreground.setColor4f(kRest, nullptr);

    // THE GHOST: the same word, same style, no track — the rest position
    // the deviation is measured against. It is a sibling under a stack
    // rather than a decoration, because a track's deviation is per glyph
    // and nothing draws the undeformed glyph beside the deformed one.
    return box()
        .column()
        .gap(8)
        .child(text(toU8(caption), small(kLabel)))
        .child(box()
                   .child(text(toU8(word), ghostStyle)
                              .key(std::string(word) + "-rest")
                              .left(0)
                              .top(0))
                   .child(text(toU8(word), set)
                              .key(word)
                              .fx({.effect = std::move(effect),
                                   .stagger = {.eachMs = kEachMs,
                                               .durationMs = kDurMs},
                                   .progress = &pass})));
  }

  [[nodiscard]] Element plot(const char* title, Element inner) {
    return box()
        .column()
        .grow(1)
        .gap(7)
        .child(box()
                   .width(pct(100))
                   .height(112)
                   .stroke(stroke(1.0f, Fill::color(kFaint)))
                   .child(std::move(inner).inset(0)))
        .child(text(toU8(title), small(kLabel, 11.0f, 0.8f)));
  }

  [[nodiscard]] Element describe() {
    return box()
        .column()
        .padding(48, 42)
        .gap(26)
        .fill(Material::linear(
            {0, 0}, {0, kH},
            {{0.0f, kPaper}, {0.55f, studio::hex(0x15151B)}, {1.0f, kPaper}}))
        .child(box()
                   .row()
                   .alignItems(Align::End)
                   .child(text(toU8("ELASTIC TYPE"), small(kInk, 12.5f, 3.4f))
                              .grow(1))
                   .child(text(toU8("ANIMATE.CSS 2013 \xc2\xb7 SQUASH AND "
                                    "STRETCH 1981"),
                               small(kFaint))))
        .child(box().height(1).fill(Fill::color(kFaint)))
        .child(row("RUBBERBAND",
                   "rubberBand \xc2\xb7 SIX KEYFRAMES ON TWO SCALE AXES",
                   rubberBand()))
        .child(row("JELLO",
                   "jello \xc2\xb7 A HALVING, ALTERNATING SHEAR \xc2\xb7 "
                   "X ONLY",
                   jello()))
        .child(box().grow(1))
        .child(box()
                   .row()
                   .gap(28)
                   .height(146)
                   .child(plot(
                       "rubberBand \xe2\x80\x94 scaleX 0.75 TO 1.25",
                       graph("g-rx", kRubber, true, kX, 0.62f, 1.38f, 1.0f)))
                   .child(plot(
                       "rubberBand \xe2\x80\x94 scaleY 0.75 TO 1.25",
                       graph("g-ry", kRubber, false, kY, 0.62f, 1.38f, 1.0f)))
                   .child(plot(
                       "jello \xe2\x80\x94 skewX \xc2\xb1"
                       "12.5\xc2\xb0, HALVING",
                       graph("g-j", kJello, true, kX, -14.0f, 14.0f, 0.0f))))
        .child(text(toU8("A NON-UNIFORM SCALE AND A SHEAR ARE THE ONE "
                         "DEVIATION AN RSXFORM CANNOT CARRY \xc2\xb7 EVERY "
                         "GLYPH ON THESE TWO LINES DRAWS UNDER ITS OWN "
                         "MATRIX"),
                    small(kFaint, 11.0f, 0.6f)));
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kW, kH);
    ctx.background(kPaper);
    // Early in the pass: the head of each word is past its overshoot and
    // settling while the tail is still at rest, so one frame shows the whole
    // table laid out along the line.
    ctx.captureAt(1.15);

    face = studio::pickFace({"Avenir Next", "Futura", "Helvetica Neue"}, 700);
    faceLabel = studio::pickFace({".SF NS", "SF Pro", "Helvetica Neue"}, 500);

    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      pass = studio::phase(t, kLoop);
      return true;
    });

    ctx.composer.render(describe());
  }
};

SIGIL_SKETCH(ElasticType)
