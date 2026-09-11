#pragma once

#include <include/core/SkFontStyle.h>
#include <include/core/SkPathBuilder.h>
#include <sigilcompose/brush/Adaptors.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/Hatches.h>
#include <sigilcompose/brush/Lines.h>
#include <sigilcompose/brush/Rails.h>
#include <sigilcompose/brush/Ribbons.h>
#include <sigilcompose/brush/Stamps.h>
#include <sigilcompose/kit/Layouts.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/kit/Strokes.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Shapers.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/kit/Theme.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace mskia = sigil::material::skia;
namespace field = sigil::material::field;
namespace arrange = sigil::geometry::arrange;
namespace shapes = sigil::geometry::shapes;
namespace weave = sigil::weave;

using namespace sigil::compose;

namespace {

// ---------------------------------------------------------------------------
// the plate's ink

constexpr SkColor4f kPaper = {0.918f, 0.902f, 0.859f, 1};  // #EAE6DB
constexpr SkColor4f kInk = {0.114f, 0.106f, 0.098f, 1};    // #1D1B19
constexpr SkColor4f kInkSoft = {0.114f, 0.106f, 0.098f, 0.55f};
constexpr SkColor4f kRed = {0.663f, 0.157f, 0.125f, 1};    // #A92820
constexpr SkColor4f kBlue = {0.157f, 0.278f, 0.435f, 1};   // #28476F
constexpr SkColor4f kGreen = {0.243f, 0.373f, 0.243f, 1};  // #3E5F3E

Fill ink() { return Fill::color(kInk); }
Fill red() { return Fill::color(kRed); }
Fill blue() { return Fill::color(kBlue); }
Fill soft() { return Fill::color(kInkSoft); }

// ---------------------------------------------------------------------------
// type

/** The three faces the sheet is set in. `ports::face` is the held
 *  resolution — it walks the system font list once for the process and
 *  answers the same face afterwards — so each of these is the ASK and
 *  nothing here holds the answer. A memo in front of it would hold one
 *  answer per site in a dylib that a reload unloads, where the host
 *  already holds one for the whole process. */
sk_sp<SkTypeface> monoFace() {
  return sketch::kit::houseFace(sketch::kit::Voice::Terminal);
}
sk_sp<SkTypeface> romanFace() {
  return weave::ports::face({"Palatino", "Georgia"});
}
sk_sp<SkTypeface> romanBoldFace() {
  return weave::ports::face({"Palatino", "Georgia"}, SkFontStyle::kBold_Weight);
}

/** The caption IS the call: monospaced, small, and set in the same ink as
 *  the body unless a caller asks for a lighter one. */
Element call(const char* words, float size = 9.5f, SkColor4f c = kInk) {
  return text(
      toU8(words),
      weave::textStyle(
          {.face = monoFace(), .size = size, .color = c, .track = 0.1f}));
}
Element roman(const char* words, float size, SkColor4f c = kInk,
              float tracking = 0) {
  return text(
      toU8(words),
      weave::textStyle(
          {.face = romanFace(), .size = size, .color = c, .track = tracking}));
}
Element romanBold(const char* words, float size, SkColor4f c = kInk,
                  float tracking = 0) {
  return text(toU8(words), weave::textStyle({.face = romanBoldFace(),
                                             .size = size,
                                             .color = c,
                                             .track = tracking}));
}

// ---------------------------------------------------------------------------
// the geometry the specimens ride on
//
// Each is an OutlineFn: a path in the node's own laid-out box. Keeping
// them as generators (rather than baked paths) is what lets the SAME
// decoration value be dropped onto a straight run, an S, a ring and a
// spiral without restating it.

/** A straight run down the middle of the box. */
shapes::OutlineFn hline() {
  return [](SkSize s) {
    SkPathBuilder b;
    b.moveTo(0, s.height() * 0.5f);
    b.lineTo(s.width(), s.height() * 0.5f);
    return b.detach();
  };
}

/** The serpent: one gentle bend, then a tight one. This is the shape that
 *  exposes an offset-contour bug — the tight lobe's inner rail is much
 *  shorter than its outer one. */
shapes::OutlineFn serpent() {
  return [](SkSize s) {
    const float w = s.width(), h = s.height();
    SkPathBuilder b;
    b.moveTo(0, h * 0.5f);
    b.cubicTo(w * 0.30f, h * 0.5f, w * 0.28f, h * 0.06f, w * 0.50f, h * 0.10f);
    b.cubicTo(w * 0.66f, h * 0.13f, w * 0.60f, h * 0.96f, w * 0.78f, h * 0.90f);
    b.cubicTo(w * 0.88f, h * 0.86f, w * 0.90f, h * 0.5f, w, h * 0.5f);
    return b.detach();
  };
}

/** A hairpin: a 180° turn at a radius small enough that a 6 px casing
 *  self-intersects on the inside. If a style survives this it survives
 *  anything. */
shapes::OutlineFn hairpin() {
  return [](SkSize s) {
    const float w = s.width(), h = s.height();
    const float r = h * 0.30f;
    SkPathBuilder b;
    b.moveTo(0, h * 0.5f - r);
    b.lineTo(w - r * 1.6f, h * 0.5f - r);
    b.arcTo(SkRect::MakeLTRB(w - r * 2.6f, h * 0.5f - r, w - r * 0.6f,
                             h * 0.5f + r),
            -90, 180, false);
    b.lineTo(0, h * 0.5f + r);
    return b.detach();
  };
}

/** A plain rectangle inset by `pad` — the frame specimens' carrier. */
shapes::OutlineFn frameRect(float pad) {
  return [pad](SkSize s) {
    SkPathBuilder b;
    b.addRect(SkRect::MakeLTRB(pad, pad, s.width() - pad, s.height() - pad));
    return b.detach();
  };
}

// ---------------------------------------------------------------------------
// specimen placement

/** One labelled specimen: a node carrying `shape` as its outline with
 *  `dec` stroked along it, and the call set beneath it.
 *
 *  Everything on this plate is placed absolutely. There is not one row or
 *  column on the sheet — a specimen book positions by eye against the
 *  shape being shown, and a flex row would put every rule on the same
 *  baseline, which is exactly the reading this sheet exists to refute. */
Element specimen(float x, float y, float w, float h, shapes::OutlineFn shape,
                 Decoration dec, const char* label, float labelDy = 6) {
  return box()
      .absolute()
      .left(x)
      .top(y)
      .width(w)
      .height(h)
      .shape(std::move(shape))
      .stroke(std::move(dec))
      .child(call(label).absolute().left(0).top(h + labelDy));
}

/** The same, with no caption (for the rings, which are captioned outside
 *  the circle). */
Element bare(float x, float y, float w, float h, shapes::OutlineFn shape,
             Decoration dec) {
  return box()
      .absolute()
      .left(x)
      .top(y)
      .width(w)
      .height(h)
      .shape(std::move(shape))
      .stroke(std::move(dec));
}

Element rule(float x, float y, float w, Decoration dec) {
  return box()
      .absolute()
      .left(x)
      .top(y)
      .width(w)
      .height(10)
      .shape(hline())
      .stroke(std::move(dec));
}

/** The numeral and the name, set as a ROW rather than as two absolutely
 *  placed children a fixed distance apart. A fixed offset has to be chosen
 *  wide enough for the widest numeral on the sheet — "VIII" is wider than
 *  "VII" by more than a word space in this face — and gets it wrong for
 *  every other one. A baseline-aligned row cannot collide for any numeral,
 *  in any face. */
Element sectionTitle(float x, float y, const char* n, const char* name) {
  return box()
      .absolute()
      .left(x)
      .top(y)
      .row()
      .gap(11)
      .alignItems(Align::Baseline)
      .child(romanBold(n, 15, kRed, 1.0f))
      .child(romanBold(name, 12, kInk, 2.2f));
}

// ---------------------------------------------------------------------------
// the styles themselves
//
// Grouped so a reader can find "the one that looks like X". Each entry is
// the decoration and the source text of the call that built it — kept
// adjacent so they cannot drift apart.

struct Style {
  const char* label;
  Decoration dec;
};

/** THE RAILS — parallel casings. `cased` is two, `triple` is three with a
 *  weighted spine; `parallels` takes any count. */
std::vector<Style> railStyles() {
  lines::Line quad;
  quad.width = 1.6f;
  quad.fill = ink();
  quad.parallels = 4;
  quad.gap = 4.0f;

  lines::Line heavyHair = lines::presets::triple(1.0f, ink(), 5.0f, 4.0f);

  lines::Line dashedPair = lines::presets::cased(2.0f, red(), 7.0f);
  dashedPair.dashIntervals = {9, 7};

  lines::Line offsetLine;
  offsetLine.width = 2.0f;
  offsetLine.fill = blue();
  offsetLine.across = -7.0f;

  // …and the same family through lines::Rails, where each rail is its own
  // line. A `parallels` count cannot reach these: it spaces N identical
  // rails evenly, so per-rail width, per-rail fill, per-rail dash phase and
  // unequal gaps all need the rails spelled out one at a time.
  lines::Rails inkRedInk = lines::rails({
      {.across = 5, .width = 2.4f, .fill = ink()},
      {.across = 0, .width = 0.7f, .fill = red()},
      {.across = -5, .width = 2.4f, .fill = ink()},
  });
  lines::Rails counterDashed = lines::rails({
      {.across = 4, .width = 2.0f, .fill = ink(), .dash = {9, 7}},
      {.across = -4,
       .width = 2.0f,
       .fill = ink(),
       .dash = {9, 7},
       .dashPhase = 8},
  });
  lines::Rails unequal = lines::rails({
      {.across = 9, .width = 1.0f, .fill = soft()},
      {.across = 3, .width = 3.0f, .fill = ink()},
      {.across = -4, .width = 1.6f, .fill = ink()},
      {.across = -8, .width = 0.8f, .fill = soft()},
  });

  return {
      {"lines::Line{.width=2}", lines::Line{.width = 2, .fill = ink()}},
      {"lines::presets::cased(2, ink, 6)",
       lines::presets::cased(2.0f, ink(), 6.0f)},
      {"lines::presets::triple(1.6, ink, 5, 1.8)",
       lines::presets::triple(1.6f, ink(), 5.0f, 1.8f)},
      {"{.parallels=4, .gap=4}", quad},
      {"triple(1, ink, 5, coreFactor=4)  heavy/hair/heavy", heavyHair},
      {"cased(2,red,7) + dash{9,7}  rails stay in phase", dashedPair},
      {"{.across=-7}  positive across is LEFT of travel", offsetLine},
      {"lines::presets::quad(1.4, ink, 5)",
       lines::presets::quad(1.4f, ink(), 5.0f)},
      {"brush::presets::heavyHairHeavy(3, 0.6, ink, 6)",
       brush::presets::heavyHairHeavy(3.0f, 0.6f, ink(), 6.0f)},
      {"brush::presets::dottedCore(2, 1.4, ink, 7, 6)",
       brush::presets::dottedCore(2.0f, 1.4f, ink(), 7.0f, 6.0f)},
      {"Rails{{-5,2.4,ink},{0,0.7,RED},{5,2.4,ink}}  per-rail fill", inkRedInk},
      {"Rails{...dashPhase=8}  counter-dashed strands", counterDashed},
      {"Rails{-9,-3,+4,+8}  unequal gaps and widths", unequal},
  };
}

/** THE DISPLACED — the run itself is bent before it is stroked. */
std::vector<Style> displacedStyles() {
  lines::Line zig = lines::presets::wavy(1.8f, ink(), 5.0f, 20.0f);
  zig.zigzag = true;

  Brush square;
  square
      .shaped(
          sigil::geometry::shapers::Square{.amplitude = 5, .wavelength = 26})
      .layer(lines::Line{.width = 1.6f, .fill = ink()});

  Brush sketch2;
  sketch2
      .layer(lines::Line{.width = 1.3f, .fill = soft()},
             {sigil::geometry::shapers::Jitter{
                 .segLength = 9, .deviation = 2.0f, .seed = 7}})
      .layer(lines::Line{.width = 1.3f, .fill = soft()},
             {sigil::geometry::shapers::Jitter{
                 .segLength = 9, .deviation = 1.0f, .seed = 41}});

  Brush waveOnCased;
  waveOnCased
      .shaped(
          sigil::geometry::shapers::Wave{.amplitude = 3.5f, .wavelength = 30})
      .layer(lines::presets::cased(1.6f, red(), 5.0f));

  return {
      {"lines::presets::wavy(1.8, ink, 4, 18)",
       lines::presets::wavy(1.8f, ink(), 4.0f, 18.0f)},
      {"wavy(...) then .zigzag = true", zig},
      {"Brush{}.shaped(sigil::geometry::shapers::Square{5,26})  battlement",
       square},
      {"two shapers::Jitter layers, seeds 7 + 41  (rough.js)", sketch2},
      {"shaped(shapers::Wave{3.5,30}).layer(cased(1.6,red,5))", waveOnCased},
  };
}

/** THE FURNISHED — ties, terminals, repeated glyphs along the run. */
std::vector<Style> furnishedStyles() {
  lines::Line chevrons;
  chevrons.width = 1.4f;
  chevrons.fill = soft();
  chevrons.midCap = lines::Cap::Arrow;
  chevrons.midSpacing = 26.0f;
  chevrons.capSize = 8.0f;

  lines::Line terminals;
  terminals.width = 1.8f;
  terminals.fill = ink();
  terminals.startCap = lines::Cap::Dot;
  terminals.endCap = lines::Cap::Bar;
  terminals.capSize = 11.0f;

  lines::Line gradient;
  gradient.width = 3.0f;
  gradient.alongStops = {
      {0.0f, kRed}, {0.5f, {0.85f, 0.66f, 0.16f, 1}}, {1.0f, kBlue}};

  PathFormat dotted = stroke(2.6f, ink());
  dotted.cap = SkPaint::kRound_Cap;
  dotted.dashIntervals = {0.01f, 8.0f};

  PathFormat morse = stroke(1.8f, ink());
  morse.cap = SkPaint::kButt_Cap;
  morse.dashIntervals = {14, 5, 3, 5, 3, 12};

  return {
      {"lines::presets::railway(1.6, ink, 12, 10)",
       lines::presets::railway(1.6f, ink(), 12.0f, 10.0f)},
      {"lines::presets::arrow(1.8, ink, 12)",
       lines::presets::arrow(1.8f, ink(), 12.0f)},
      {"{.midCap=Arrow, .midSpacing=26}", chevrons},
      {"{.startCap=Dot, .endCap=Bar, .capSize=11}", terminals},
      {"{.alongStops={red, gold, blue}}  arc gradient", gradient},
      {"PathFormat{cap=Round, dash{0.01, 8}}  dotted", dotted},
      {"PathFormat{dash{14,5,3,5,3,12}}  morse rule", morse},
  };
}

/** THE STACKS — LayeredBrush: several passes of one outline. */
std::vector<Style> stackStyles() {
  return {
      {"brush::presets::circuit(teal, tier=2)",
       brush::presets::circuit({0.208f, 0.478f, 0.424f, 1}, 2)},
      {"brush::presets::filament(...)  4-pass additive glow",
       brush::presets::filament({0.20f, 0.42f, 0.66f, 1},
                                {0.10f, 0.12f, 0.16f, 1}, 0.55f)},
      {"brush::presets::rope(state=2, scale=0.5)",
       brush::presets::rope(2, 0.5f)},
      {"brush::presets::pulse(...)  trim a window and march it",
       brush::presets::pulse({0.66f, 0.16f, 0.13f, 0.45f},
                             {0.15f, 0.13f, 0.11f, 0.9f}, 0.6f)},
  };
}

/** THE BANDS — a filled band whose width varies along the run. */
std::vector<Style> bandStyles() {
  return {
      {"brush::presets::taper(9, 0.6, ink)",
       brush::presets::taper(9.0f, 0.6f, ink())},
      {"brush::presets::calligraphic(38, 11, ink, 0.10)",
       brush::presets::calligraphic(38.0f, 11.0f, ink(), 0.10f)},
      {"brush::presets::calligraphic(-15, 9, red, 0.22)",
       brush::presets::calligraphic(-15.0f, 9.0f, red(), 0.22f)},
  };
}

/** THE STAMPED — an element instanced along the run. */
Element tick(SkColor4f c, float w, float h) {
  return box().width(w).height(h).fill(c);
}
Element lozenge(SkColor4f c, float r) {
  return box()
      .width(r * 2)
      .height(r * 2)
      .shape(shapes::polygon(4, 45.0f))
      .fill(c);
}

std::vector<Style> stampedStyles() {
  brush::Scatter seeds;
  seeds.art = lozenge(kInk, 2.6f);
  seeds.spacing = 15.0f;
  seeds.seed = 11;
  seeds.jitterNormal = 3.4f;
  seeds.jitterScale = 0.5f;
  seeds.jitterRotateDeg = 40.0f;
  seeds.bleedPx = 14.0f;

  brush::Scatter ladder;
  ladder.art = tick(kRed, 1.6f, 11.0f);
  ladder.spacing = 9.0f;
  ladder.bleedPx = 10.0f;

  brush::Pattern chain;
  chain.side = box()
                   .width(14)
                   .height(10)
                   .shape(shapes::circle())
                   .foreground(stroke(1.4f, ink()));
  chain.advance = 11.0f;
  chain.bleedPx = 12.0f;

  PathFormat vine = stroke(1.2f, ink());
  {
    SkPathBuilder leaf;
    leaf.moveTo(0, 0);
    leaf.quadTo(3.5f, -4.5f, 8, 0);
    leaf.quadTo(3.5f, 4.5f, 0, 0);
    leaf.close();
    vine.stampPath = leaf.detach();
    vine.stampAdvance = 13.0f;
  }

  return {
      {"brush::Scatter{lozenge, 15px, seed 11, jitter}", seeds},
      {"brush::Scatter{1.6x11 tick, 9px}  tick ladder", ladder},
      {"brush::Pattern{side = ringed cell, advance 11}", chain},
      {"PathFormat{stampPath = leaf, stampAdvance 13}", vine},
  };
}

}  // namespace

// ===========================================================================
