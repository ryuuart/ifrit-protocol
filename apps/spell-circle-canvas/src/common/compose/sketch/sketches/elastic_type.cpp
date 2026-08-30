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
//   rubberBand      scaleX  scaleY          jello     skewX = skewY
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
// The table is ALL this file states. `fx::keys` takes the entries and does
// the sampling, so the numbers above and the numbers below are the same
// list read twice rather than a transcription and a sampler that have to
// agree — and the graphs at the bottom plot the EFFECT, not the table, so
// the picture draws whatever the motion is actually doing.
//
// TWO THINGS THE TRANSCRIPTION GETS RIGHT, both of which take a browser to
// notice and both of which change the shape of the motion:
//
//  * `jello` shears on BOTH axes — the published rule is
//    `skewX(a) skewY(a)`, the same angle on each — so the word rocks on a
//    diagonal. The two angles ride one `GlyphMod` and reach the glyph as a
//    single shear pair.
//  * CSS crosses EACH KEYFRAME SEGMENT with its own timing function, `ease`
//    by default, rather than running one curve across the whole list. That
//    is what `fx::keys` means by a per-segment curve, and `cssEase` below is
//    the curve itself: every segment eases in and out of its own endpoints,
//    which rounds the corners a linear reading leaves sharp.
//
// -----------------------------------------------------------------------------
// WHAT THIS PUTS UNDER LOAD
//
// A non-uniform scale and a shear are the ONE deviation an RSXform cannot
// carry: that transform encodes a rotation, one scale and no shear at all.
// A glyph whose composed deviation uses `scaleX`, `scaleY`, `skewXDeg` or
// `skewYDeg` therefore leaves the shared transform array and draws under its
// own matrix — same passes, same paint, one canvas concat — while its
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
#include <sigilcompose/core/Material.h>
#include <sigilcompose/kit/Instruments.h>
#include <sigilcompose/typography/TextFx.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/Sketch.h>

#include <string>
#include <utility>
#include <vector>

using namespace sigil::compose;

namespace {

constexpr float kW = 1080.0f;
constexpr float kH = 620.0f;

constexpr SkColor4f kPaper = hex(0x101014);
constexpr SkColor4f kInk = hex(0xF6F2E9);
constexpr SkColor4f kLabel = hex(0x848B99);
constexpr SkColor4f kFaint = hex(0x2E3440);
constexpr SkColor4f kX = hex(0xFF7A59);  // scaleX / skewX
constexpr SkColor4f kY = hex(0x5AC8F5);  // scaleY
constexpr SkColor4f kRest = hex(0x4A5262);

constexpr float kWordSize = 68.0f;
constexpr float kEachMs = 62.0f;
constexpr float kDurMs = 900.0f;
constexpr double kLoop = 3.4;  // one pass of both rows, then a rest

// ---------------------------------------------------------------------------
// The tables, and the curve every segment of one is crossed with.

using Table = std::vector<fx::Key>;

/** CSS's default `animation-timing-function`: cubic-bezier(0.25, 0.1, 0.25,
 *  1). A keyframe list that names no timing function is crossed with this
 *  ONE SEGMENT AT A TIME, which is why it belongs beside the tables rather
 *  than over them.
 *
 *  A plain function and not a lambda, so the two effects below compare by
 *  their tables and their curve and prune like any other value.
 *
 *  x is monotonic in the parameter for control points inside the unit
 *  square, so bisecting on x lands on the one parameter whose x is the time
 *  asked for, and y at that parameter is the answer. */
float cssEase(float t) {
  if (t <= 0.0f) return 0.0f;
  if (t >= 1.0f) return 1.0f;
  const auto bezier = [](float u, float a, float b) {
    const float v = 1.0f - u;
    return 3.0f * v * v * u * a + 3.0f * v * u * u * b + u * u * u;
  };
  float lo = 0.0f, hi = 1.0f, u = t;
  for (int i = 0; i < 24; ++i) {
    u = 0.5f * (lo + hi);
    (bezier(u, 0.25f, 0.25f) < t ? lo : hi) = u;
  }
  return bezier(u, 0.1f, 1.0f);
}

/** rubberBand: a squash and a stretch on the two scale axes. They are
 *  independent, so every glyph leaves the batched RSXform array and draws
 *  under its own matrix. */
Table rubberTable() {
  return {{0.00f, {}},
          {0.30f, {.scaleX = 1.25f, .scaleY = 0.75f}},
          {0.40f, {.scaleX = 0.75f, .scaleY = 1.25f}},
          {0.50f, {.scaleX = 1.15f, .scaleY = 0.85f}},
          {0.65f, {.scaleX = 0.95f, .scaleY = 1.05f}},
          {0.75f, {.scaleX = 1.05f, .scaleY = 0.95f}},
          {1.00f, {}}};
}

/** jello: a halving, alternating shear, THE SAME ANGLE ON BOTH AXES, which
 *  is what makes the word rock on a diagonal rather than side to side. */
Table jelloTable() {
  const auto shear = [](float at, float deg) {
    return fx::Key{at, {.skewXDeg = deg, .skewYDeg = deg}};
  };
  return {shear(0.000f, 0.0f),        shear(0.111f, 0.0f),
          shear(0.222f, -12.5f),      shear(0.333f, 6.25f),
          shear(0.444f, -3.125f),     shear(0.555f, 1.5625f),
          shear(0.666f, -0.78125f),   shear(0.777f, 0.390625f),
          shear(0.888f, -0.1953125f), shear(1.000f, 0.0f)};
}

/** The deviation at one moment, for the graphs: the effects are pure
 *  functions of local time here, so the picture is read out of the same
 *  value the glyphs are drawn from. */
GlyphMod at(const TextEffect& effect, float t) {
  GlyphInfo glyph;
  Rng rng(1);
  return effect(glyph, t, rng);
}

// ---------------------------------------------------------------------------
// The graphs — the EFFECTS, plotted. Pure functions of local time, so they
// are static leaves and nothing here reads a clock.

void strokePath(SkCanvas& canvas, const SkPath& path, SkColor4f color,
                float width) {
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(width);
  paint.setColor4f(color, nullptr);
  canvas.drawPath(path, paint);
}

/** One lane of one effect, over local time, with a dot at every published
 *  keyframe. `lane` picks the field out of the deviation; @p lo and @p hi
 *  are the value range the plot's height spans. */
Element graph(const char* key, TextEffect effect, Table table,
              float (*lane)(const GlyphMod&), SkColor4f color, float lo,
              float hi, float rest) {
  return custom(
             key,
             [effect = std::move(effect), table = std::move(table), lane, color,
              lo, hi, rest](SkCanvas& canvas, const PaintContext& ctx) {
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
                 const SkPoint point{w * t, toY(lane(at(effect, t)))};
                 i == 0 ? (void)trace.moveTo(point) : (void)trace.lineTo(point);
               }
               strokePath(canvas, trace.detach(), color, 1.6f);

               SkPaint dot;
               dot.setAntiAlias(true);
               dot.setColor4f(color, nullptr);
               for (const fx::Key& key : table)
                 canvas.drawCircle(w * key.at, toY(lane(at(effect, key.at))),
                                   2.6f, dot);
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
    return type(
        {.face = faceLabel, .size = size, .color = color, .track = track});
  }

  /** One specimen row: the word, deformed letter by letter.
   *
   *  The cascade is per CLUSTER rather than per glyph, which for this word
   *  is the same thing and stays right for text where it is not — a letter
   *  and its marks are one body and squash together. */
  [[nodiscard]] Element row(const char* word, const char* caption,
                            TextEffect effect) {
    const sigil::weave::TextStyle set =
        type({.face = face, .size = kWordSize, .color = kInk, .track = 3.0f});

    // THE GHOST: the same word, same style, no track — the rest position
    // the deviation is measured against. A track's deviation is per glyph
    // and lives only in the draw, so nothing else on the sheet says where
    // the undeformed letter was.
    return box()
        .column()
        .gap(8)
        .child(text(toU8(caption), small(kLabel)))
        .child(kit::restGhost(
            text(toU8(word), set)
                .key(word)
                .fx({.effect = std::move(effect),
                     .stagger = {.eachMs = kEachMs, .durationMs = kDurMs},
                     .progress = &pass}),
            kRest));
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
            {{0.0f, kPaper}, {0.55f, hex(0x15151B)}, {1.0f, kPaper}}))
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
                   fx::keys(rubberTable(), &cssEase)))
        .child(row("JELLO",
                   "jello \xc2\xb7 A HALVING, ALTERNATING SHEAR \xc2\xb7 "
                   "BOTH AXES",
                   fx::keys(jelloTable(), &cssEase)))
        .child(box().grow(1))
        .child(box()
                   .row()
                   .gap(28)
                   .height(146)
                   .child(plot("rubberBand \xe2\x80\x94 scaleX 0.75 TO 1.25",
                               graph(
                                   "g-rx", fx::keys(rubberTable(), &cssEase),
                                   rubberTable(),
                                   [](const GlyphMod& m) { return m.scaleX; },
                                   kX, 0.62f, 1.38f, 1.0f)))
                   .child(plot("rubberBand \xe2\x80\x94 scaleY 0.75 TO 1.25",
                               graph(
                                   "g-ry", fx::keys(rubberTable(), &cssEase),
                                   rubberTable(),
                                   [](const GlyphMod& m) { return m.scaleY; },
                                   kY, 0.62f, 1.38f, 1.0f)))
                   .child(plot("jello \xe2\x80\x94 skewX = skewY \xc2\xb1"
                               "12.5\xc2\xb0, HALVING",
                               graph(
                                   "g-j", fx::keys(jelloTable(), &cssEase),
                                   jelloTable(),
                                   [](const GlyphMod& m) { return m.skewXDeg; },
                                   kX, -14.0f, 14.0f, 0.0f))))
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

    face = pickFace({"Avenir Next", "Futura", "Helvetica Neue"}, 700);
    faceLabel = pickFace({".SF NS", "SF Pro", "Helvetica Neue"}, 500);

    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      pass = motion::phase(t, kLoop);
      return true;
    });

    ctx.composer.render(describe());
  }
};

SIGIL_SKETCH(ElasticType)
