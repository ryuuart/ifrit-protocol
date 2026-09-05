// stroke_atlas.cpp — the SPECIMEN PLATE for the SigilCompose stroke,
// border and corner vocabulary. Every rule on this sheet is labelled with
// the exact call that made it.
//
// WHY IT IS A PLATE AND NOT A GRID.
//
// The reference is the printer's rule specimen: the pages at the back of a
// type foundry's catalogue where the brass rule is shown, not described.
// Stephenson Blake's *Printing Types, Borders and Ornaments* (Sheffield),
// the Caslon and Miller & Richard specimen books, and the Letraset /
// Chartpak dry-transfer tape sheets all share one convention that a grid
// of swatches destroys:
//
//   A rule is shown on the SHAPE IT WILL BE USED ON.
//
// So a foundry sheet gives you the same double rule as a straight run, as
// a box corner, as an oval, and as a swash — because a rule that only
// looks right horizontally is a lie. Half the styles in this library are
// built by offsetting a contour, and an offset contour is exactly where
// curvature bites: the outer rail of a parallel pair is longer than the
// inner one, so dashes shear; a wave whose wavelength is snapped per
// contour changes frequency when the contour does; a corner tile has to
// know which way is out.
//
// This plate therefore samples every style FIVE ways:
//
//   the fan      straight runs at twenty angles out of one origin
//   the serpent  an S-curve with one gentle bend and one tight one
//   the rings    concentric circles from r=160 down to r=42 — the same
//                style at five curvatures, which is the honest test
//   the torture  a spiral and a hairpin, where offsets self-intersect
//   the frames   closed rectangles, for the corner and border language
//
// The engraver's other convention is kept too: everything is stated in a
// second colour where a rule needs a second colour to be read at all
// (parallel pairs, phase-registered dashes), and nothing is captioned in
// prose — the caption IS the call.
//
// WHAT THIS SHEET IS FOR. It is easy to reach for a dash, a straight line
// and the occasional double rule and stop there, because those are the
// calls one already knows, while the library ships a great deal more. That
// is a DISCOVERABILITY problem, not a capability one, and a list of names
// in a header does not fix it — you have to see the thing. Point a reader
// here.
//
// BUILT FROM (the library, not by hand):
//   lines::Line          parallels/gap/coreWidthFactor, wave + zigzag,
//                        railway ties, terminal + mid caps, dash with
//                        phase-registered rails, alongStops
//   lines::hatch/crosshatch/radialHatch/concentric
//   sigil::geometry::shapers::wave/square/zigzag/rounded/jitter/offset
//                        as Brush pipelines, through .shaped()
//   Brush                .shaped() pipeline + multi-layer; the per-layer
//                        suffix takes shapers too
//   brush::presets::filament/circuit/rope/pulse            LayeredBrush
//   stacks brush::Ribbon      taper and calligraphic nib
//   brush::Scatter/Pattern/Art/restyle
//   PathFormat           align Inner/Outer, cap/join, stampPath, trim
//   shapes::onEdges/inset/parametric/spiral/star/polygon
//   decorations::wash

#include <include/core/SkFontStyle.h>
#include <include/core/SkPathBuilder.h>
#include <sigilcompose/brush/Adaptors.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/Hatches.h>
#include <sigilcompose/brush/Lines.h>
#include <sigilcompose/brush/Rails.h>
#include <sigilcompose/kit/Layouts.h>
#include <sigilcompose/kit/Strokes.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Shapers.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace mskia = sigil::material::skia;
namespace field = sigil::material::field;
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

/** The three faces the sheet is set in. Resolved on first use and never
 *  written again — walking the system font list per call is what the
 *  fallback chain is memoised against, and a value nothing mutates is a
 *  memo rather than state two sessions of this sketch could share. */
struct Faces {
  sk_sp<SkTypeface> mono, roman, romanBold;
};
const Faces& faces() {
  static const Faces f{
      .mono = weave::ports::face({"Menlo", "Courier New"}),
      .roman = weave::ports::face({"Palatino", "Georgia"}),
      .romanBold = weave::ports::face({"Palatino", "Georgia"},
                                      SkFontStyle::kBold_Weight),
  };
  return f;
}

/** The caption IS the call: monospaced, small, and set in the same ink as
 *  the body unless a caller asks for a lighter one. */
Element call(const char* words, float size = 9.5f, SkColor4f c = kInk) {
  return text(
      toU8(words),
      weave::textStyle({.face = faces().mono, .size = size, .color = c, .track = 0.1f}));
}
Element roman(const char* words, float size, SkColor4f c = kInk,
              float tracking = 0) {
  return text(toU8(words), weave::textStyle({.face = faces().roman,
                                 .size = size,
                                 .color = c,
                                 .track = tracking}));
}
Element romanBold(const char* words, float size, SkColor4f c = kInk,
                  float tracking = 0) {
  return text(toU8(words), weave::textStyle({.face = faces().romanBold,
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

/** A ring of the given radius, centred in the box. */
shapes::OutlineFn ringOf(float radius) {
  return [radius](SkSize s) {
    SkPathBuilder b;
    b.addCircle(s.width() * 0.5f, s.height() * 0.5f, radius);
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
      {"lines::presets::cased(2, ink, 6)", lines::presets::cased(2.0f, ink(), 6.0f)},
      {"lines::presets::triple(1.6, ink, 5, 1.8)",
       lines::presets::triple(1.6f, ink(), 5.0f, 1.8f)},
      {"{.parallels=4, .gap=4}", quad},
      {"triple(1, ink, 5, coreFactor=4)  heavy/hair/heavy", heavyHair},
      {"cased(2,red,7) + dash{9,7}  rails stay in phase", dashedPair},
      {"{.across=-7}  positive across is LEFT of travel", offsetLine},
      {"lines::presets::quad(1.4, ink, 5)", lines::presets::quad(1.4f, ink(), 5.0f)},
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
  square.shaped(sigil::geometry::shapers::Square{.amplitude = 5, .wavelength = 26})
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
      .shaped(sigil::geometry::shapers::Wave{.amplitude = 3.5f, .wavelength = 30})
      .layer(lines::presets::cased(1.6f, red(), 5.0f));

  return {
      {"lines::presets::wavy(1.8, ink, 4, 18)", lines::presets::wavy(1.8f, ink(), 4.0f, 18.0f)},
      {"wavy(...) then .zigzag = true", zig},
      {"Brush{}.shaped(sigil::geometry::shapers::Square{5,26})  battlement", square},
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
      {"lines::presets::arrow(1.8, ink, 12)", lines::presets::arrow(1.8f, ink(), 12.0f)},
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
      {"brush::presets::taper(9, 0.6, ink)", brush::presets::taper(9.0f, 0.6f, ink())},
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
  seeds.reach = 14.0f;

  brush::Scatter ladder;
  ladder.art = tick(kRed, 1.6f, 11.0f);
  ladder.spacing = 9.0f;
  ladder.reach = 10.0f;

  brush::Pattern chain;
  chain.side = box()
                   .width(14)
                   .height(10)
                   .shape(shapes::circle())
                   .foreground(stroke(1.4f, ink()));
  chain.advance = 11.0f;
  chain.reach = 12.0f;

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

struct StrokeAtlasSketch : sketch::Sketch {
  choreograph::Output<float> march{0};

  Element describe(sketch::SketchContext& ctx) {
    Element plate = stack().fill(mskia::Paint::solid(kPaper));

    // ---- masthead --------------------------------------------------------
    plate.child(romanBold("THE STROKE ATLAS", 26, kInk, 6.0f)
                    .absolute()
                    .left(56)
                    .top(34));
    plate.child(roman("a specimen plate of the SigilCompose line, border and "
                      "corner vocabulary \xe2\x80\x94 every rule captioned "
                      "with the call that made it",
                      11.5f, kInk)
                    .absolute()
                    .left(58)
                    .top(70));
    plate.child(call("sigilcompose/brush/{Lines,Brushes,Hatches,Rails}.h + "
                     "shape/Shapes.h",
                     9.0f, kRed)
                    .absolute()
                    .left(58)
                    .top(88));
    plate.child(call("PLATE I", 9.0f, kInkSoft).absolute().left(1470).top(88));
    plate.child(
        rule(56, 112, 1488, brush::presets::heavyHairHeavy(1.2f, 0.5f, ink(), 3.0f)));

    // ---- I. THE FAN ------------------------------------------------------
    // Straight is the easy case and a specimen book still starts there: the
    // angles exist so cap geometry, tie spacing and anti-aliasing can be
    // compared against the pixel grid at more than one slope.
    plate.child(sectionTitle(56, 140, "I", "THE FAN \xc2\xb7 STRAIGHT RUNS"));
    plate.child(call("twenty rules out of one origin \xc2\xb7 numbered to the "
                     "key",
                     9.0f, kInkSoft)
                    .absolute()
                    .left(56)
                    .top(160));
    {
      std::vector<Style> fan = railStyles();
      for (Style& s : furnishedStyles()) fan.push_back(std::move(s));
      const float originX = 66, originY = 510;
      const float length = 232;
      const int n = (int)fan.size();
      for (int i = 0; i < n; ++i) {
        const float t = n > 1 ? (float)i / (float)(n - 1) : 0.5f;
        const float deg = -54.0f + t * 108.0f;
        // The spoke carries only a NUMERAL, and it sits at the OUTER end
        // where the twenty rules are maximally separated. Captions along
        // the spokes converge on the pivot — which is exactly where they are
        // closest together — so no amount of nudging saves them. A numbered
        // key is what a specimen sheet does with a crowded figure, and it
        // lets the calls be set at a readable size in reading order, which
        // is the actual payload: a reader is here to learn what to type.
        plate.child(box()
                        .absolute()
                        .left(originX)
                        .top(originY - 19)
                        .width(length)
                        .height(38)
                        .transformOrigin(0, 0.5f)
                        .rotate(deg)
                        .shape(hline())
                        .stroke(fan[(size_t)i].dec));
        const float rad = deg * 0.0174532925f;
        const float ex = originX + std::cos(rad) * (length + 9);
        const float ey = originY + std::sin(rad) * (length + 9);
        char numeral[8];
        std::snprintf(numeral, sizeof numeral, "%d", i + 1);
        plate.child(call(numeral, 8.5f, kRed).absolute().left(ex).top(ey - 6));
      }
      // The pivot, drawn as a registration mark.
      plate.child(box()
                      .absolute()
                      .left(originX - 8)
                      .top(originY - 8)
                      .width(16)
                      .height(16)
                      .shape(shapes::circle())
                      .foreground(stroke(1.0f, red())));
      plate.child(box()
                      .absolute()
                      .left(originX - 13)
                      .top(originY - 0.5f)
                      .width(26)
                      .height(1)
                      .fill(kRed));
      plate.child(box()
                      .absolute()
                      .left(originX - 0.5f)
                      .top(originY - 13)
                      .width(1)
                      .height(26)
                      .fill(kRed));

      // The key.
      const float keyX = 330, keyTop = 236;
      plate.child(
          call("KEY", 8.5f, kInk).absolute().left(keyX).top(keyTop - 16));
      for (int i = 0; i < n; ++i) {
        char numeral[8];
        std::snprintf(numeral, sizeof numeral, "%2d", i + 1);
        plate.child(call(numeral, 8.5f, kRed)
                        .absolute()
                        .left(keyX)
                        .top(keyTop + (float)i * 13.6f));
        plate.child(call(fan[(size_t)i].label, 7.8f, kInkSoft)
                        .absolute()
                        .left(keyX + 19)
                        .top(keyTop + (float)i * 13.6f));
      }
    }

    // ---- II. THE SERPENT -------------------------------------------------
    plate.child(
        sectionTitle(636, 140, "II", "THE SERPENT \xc2\xb7 ON A CURVE"));
    plate.child(call("one gentle bend and one tight one \xe2\x80\x94 a rule "
                     "that only reads straight is a lie",
                     9.0f, kInkSoft)
                    .absolute()
                    .left(636)
                    .top(160));
    {
      std::vector<Style> column = displacedStyles();
      for (Style& s : bandStyles()) column.push_back(std::move(s));
      for (Style& s : stampedStyles()) column.push_back(std::move(s));

      float y = 184;
      int i = 0;
      for (Style& s : column) {
        // A slight alternating stagger: the sheet must not read as a column
        // of cells, and the offset also shows the styles are size-relative
        // rather than pinned to an x.
        const float x = 636 + (i % 2 ? 22.0f : 0.0f);
        plate.child(
            specimen(x, y, 356, 46, serpent(), std::move(s.dec), s.label, 0));
        y += 58;
        ++i;
      }
    }

    // ---- III. THE RINGS --------------------------------------------------
    // The same style at five curvatures. Concentric so the eye reads "this
    // style, tighter" rather than five unrelated circles.
    plate.child(sectionTitle(1064, 140, "III", "THE RINGS \xc2\xb7 CURVATURE"));
    plate.child(call("r = 160 \xe2\x86\x92 42 \xc2\xb7 where offset contours "
                     "shear",
                     9.0f, kInkSoft)
                    .absolute()
                    .left(1064)
                    .top(160));
    {
      const float cx = 1178, cy = 430;
      struct Ring {
        float r;
        float angle;  // rad; fanned so the captions stack instead of collide
        const char* label;
        Decoration dec;
      };
      lines::Line chev;
      chev.width = 1.3f;
      chev.fill = soft();
      chev.midCap = lines::Cap::Arrow;
      chev.midSpacing = 30.0f;
      chev.capSize = 8.0f;
      Brush wavyRing;
      wavyRing
          .shaped(sigil::geometry::shapers::Wave{.amplitude = 4, .wavelength = 26})
          .layer(lines::Line{.width = 1.4f, .fill = red()});
      lines::Rails registered = lines::rails({
          {.across = 5, .width = 1.6f, .fill = ink(), .dash = {10, 8}},
          {.across = -5, .width = 1.6f, .fill = ink(), .dash = {10, 8}},
      });

      const std::vector<Ring> rings = {
          {160, -1.20f, "Rails{-5,+5, dash{10,8}} in register", registered},
          {130, -1.00f, "shaped(shapers::Wave{4,26}).layer(1.4 red)", wavyRing},
          {100, -0.80f, "lines::presets::railway(1.4, ink, 13, 9)",
           lines::presets::railway(1.4f, ink(), 13.0f, 9.0f)},
          {70, -0.60f, "{.midCap=Arrow, .midSpacing=30}", chev},
          {42, -0.40f, "lines::presets::triple(1.4, ink, 4.5, 2)",
           lines::presets::triple(1.4f, ink(), 4.5f, 2.0f)},
      };
      const float span = 380;
      for (const Ring& r : rings) {
        plate.child(bare(cx - span * 0.5f, cy - span * 0.5f, span, span,
                         ringOf(r.r), r.dec));
        const float lx = cx + std::cos(r.angle) * r.r;
        const float ly = cy + std::sin(r.angle) * r.r;
        // The leader runs OUT of the cluster to a caption column clear of
        // every ring. A caption set just off its own ring lands on top of
        // the rings outside it, which is the one thing a plate of concentric
        // rules must not do.
        const float capX = 1352;
        plate.child(box()
                        .absolute()
                        .left(lx)
                        .top(ly - 0.5f)
                        .width(capX - 6 - lx)
                        .height(1)
                        .fill(kInkSoft));
        plate.child(
            call(r.label, 8.0f, kInkSoft).absolute().left(capX).top(ly - 5));
      }
      plate.child(box()
                      .absolute()
                      .left(cx - 3)
                      .top(cy - 3)
                      .width(6)
                      .height(6)
                      .shape(shapes::circle())
                      .fill(kRed));
    }

    // ---- IV. THE REVERSE -------------------------------------------------
    // Additive glow brushes are built for dark UI and wash out on paper.
    // Printing a black patch to show a rule reversed is what a real specimen
    // sheet does, so the plate does it too.
    plate.child(
        sectionTitle(1064, 690, "IV", "THE REVERSE \xc2\xb7 LAYERED STACKS"));
    plate.child(call("additive stacks, shown on the black patch they are for",
                     9.0f, kInkSoft)
                    .absolute()
                    .left(1064)
                    .top(710));
    {
      plate.child(
          box().absolute().left(1058).top(732).width(486).height(300).fill(
              SkColor4f{0.055f, 0.055f, 0.068f, 1}));
      float y = 748;
      for (Style& s : stackStyles()) {
        plate.child(box()
                        .absolute()
                        .left(1078)
                        .top(y)
                        .width(330)
                        .height(44)
                        .shape(serpent())
                        .stroke(std::move(s.dec))
                        .child(call(s.label, 8.0f, {0.72f, 0.74f, 0.78f, 1})
                                   .absolute()
                                   .left(0)
                                   .top(46)));
        y += 72;
      }
    }

    // ---- V. THE TORTURE --------------------------------------------------
    plate.child(
        sectionTitle(56, 880, "V", "THE TORTURE \xc2\xb7 SPIRAL & HAIRPIN"));
    plate.child(call("where offset contours self-intersect", 9.0f, kInkSoft)
                    .absolute()
                    .left(56)
                    .top(900));
    {
      plate.child(specimen(56, 926, 180, 180,
                           shapes::spiral(3.2f, false, 0.10f),
                           lines::presets::cased(1.6f, ink(), 5.0f),
                           "cased(1.6,ink,5) on shapes::spiral(3.2)"));
      plate.child(specimen(258, 926, 180, 180,
                           shapes::spiral(3.2f, false, 0.10f),
                           lines::presets::railway(1.2f, red(), 11.0f, 8.0f),
                           "railway(1.2,red,11,8), same spiral"));
      plate.child(specimen(460, 926, 150, 74, hairpin(),
                           brush::presets::heavyHairHeavy(2.2f, 0.6f, ink(), 5.0f),
                           "heavyHairHeavy round a hairpin"));
      Brush hairSketch;
      hairSketch.layer(lines::Line{.width = 1.3f, .fill = soft()},
                       {sigil::geometry::shapers::Jitter{
                           .segLength = 7, .deviation = 2.0f, .seed = 3}});
      plate.child(specimen(460, 1032, 150, 74, hairpin(), hairSketch,
                           "shapers::Jitter on a hairpin"));
    }

    // ---- VI. THE FIELDS --------------------------------------------------
    plate.child(sectionTitle(640, 1062, "VI", "THE FIELDS \xc2\xb7 HATCHING"));
    plate.child(
        call("a rule repeated and clipped to a silhouette", 9.0f, kInkSoft)
            .absolute()
            .left(640)
            .top(1082));
    {
      auto field = [&](float x, float dy, const char* label,
                       shapes::OutlineFn shape, Decoration dec) {
        return box()
            .absolute()
            .left(x)
            .top(1108 + dy)
            .width(124)
            .height(124)
            .shape(std::move(shape))
            .background(std::move(dec))
            .foreground(stroke(1.0f, ink()))
            .child(call(label, 8.0f, kInkSoft).absolute().left(0).top(130));
      };
      // Staggered, not ruled: the shapes differ, so their baselines should.
      plate.child(field(640, 0, "lines::presets::hatch(ink, 5, 0.9, 45)",
                        shapes::star(6, 0.52f),
                        lines::presets::hatch(ink(), 5.0f, 0.9f, 45.0f)));
      plate.child(field(786, 18, "lines::presets::crosshatch(ink, 7, 0.8, 20)",
                        shapes::blob(4, 0.16f, 7),
                        lines::presets::crosshatch(ink(), 7.0f, 0.8f, 20.0f)));
      plate.child(field(932, -8, "lines::presets::radialHatch(ink, 72, 0.8)",
                        shapes::polygon(6, 90.0f),
                        lines::presets::radialHatch(ink(), 72, 0.8f)));
      plate.child(field(1078, 22, "lines::presets::concentric(red, 14, 0.8)",
                        shapes::squircle(4.0f),
                        lines::presets::concentric(red(), 14, 0.8f)));
      plate.child(
          field(1224, 2, "decorations::wash(halftoneRamp)", shapes::circle(),
                decorations::wash(mskia::Paint::recipe(field::halftoneRamp(
                                      8, 1.0f, 3.2f, mskia::toColor(kInk))),
                                  SkBlendMode::kSrcOver, 0.95f)));
      plate.child(field(1370, 26, "hatch on shapes::chamfered(22)",
                        shapes::chamfered(22.0f),
                        lines::presets::hatch(soft(), 6.0f, 0.8f, -45.0f)));
    }

    // ---- VII. THE FRAMES -------------------------------------------------
    // The headline. A frame is not a 1 px rounded rect.
    plate.child(
        sectionTitle(56, 1300, "VII", "THE FRAMES \xc2\xb7 BORDERS & CORNERS"));
    plate.child(
        call("decorations::Border \xc2\xb7 shapes::chamfered/notched "
             "\xc2\xb7 brush::Pattern corner tiles \xe2\x80\x94 a frame "
             "is not a 1 px rounded rect",
             9.0f, kInkSoft)
            .absolute()
            .left(56)
            .top(1320));
    {
      struct Frame {
        const char* label;
        shapes::OutlineFn shape;
        Decoration dec;
        float rot = 0;
        std::optional<LayerStyle> style;  // set instead of dec for stacks
        std::optional<Spans> where;       // set to stroke only part of the
                                          // outline instead of all of it
      };
      // Laid out in two staggered rows of seven and eight.
      std::vector<Frame> frames;
      auto add = [&](const char* label, shapes::OutlineFn shape, Decoration dec,
                     float rot = 0) {
        frames.push_back(Frame{label, std::move(shape), std::move(dec), rot,
                               std::nullopt, std::nullopt});
      };
      auto addStyle = [&](const char* label, shapes::OutlineFn shape,
                          LayerStyle style, float rot = 0) {
        frames.push_back(Frame{label, std::move(shape), PathFormat{.width = 0},
                               rot, std::move(style), std::nullopt});
      };
      // Corner brackets and open-corner rules are spelled as a SPAN claim on
      // the frame's own outline — stroke(spans::corners(n), …) — rather than
      // as a dedicated decoration that draws its own rectangle. The ink then
      // follows whatever shape the node actually has, so the same call gives
      // four brackets on a rect and eight on a chamfer.
      auto addSpans = [&](const char* label, shapes::OutlineFn shape,
                          Spans where, Decoration dec, float rot = 0) {
        frames.push_back(Frame{label, std::move(shape), std::move(dec), rot,
                               std::nullopt, std::move(where)});
      };

      add("decorations::border(1.4, ink)", frameRect(8),
          decorations::border(1.4f, ink()), -1.1f);
      addSpans("stroke(spans::corners(26), solid(2, ink))", frameRect(8),
               spans::corners(26.0f), brush::solid(2.0f, ink()), 0.8f);
      addSpans("stroke(spans::edges(22), solid(1.4, ink))", frameRect(8),
               spans::edges(22.0f), brush::solid(1.4f, ink()));
      add("decorations::weightedCorners(1, 3.4, ink, 24)", frameRect(8),
          decorations::weightedCorners(1.0f, 3.4f, ink(), 24.0f), -0.7f);
      addSpans("corners(12) on shapes::chamfered(18)  8 corners",
               shapes::chamfered(18), spans::corners(12.0f),
               brush::solid(2.0f, red()), 1.2f);
      add("border on shapes::notched(26, 9)",
          shapes::notched(26.0f, 9.0f, shapes::Corner::Diagonal),
          decorations::border(1.6f, ink()));
      add("shapes::chamfered(14, Corner::AntiDiagonal)",
          shapes::chamfered(14.0f, shapes::Corner::AntiDiagonal),
          decorations::border(1.6f, ink()), 0.6f);
      // Corner tiles: brush::Pattern's real corner art.
      {
        brush::Pattern tiled;
        tiled.side = box().width(11).height(7).shape(hline()).stroke(
            lines::Line{.width = 1.2f, .fill = ink()});
        // Bisector, and NOT merely because it is what the ART wants. This
        // specimen's caption says "lozenge", and a lozenge is only a
        // lozenge on the bisector: a rectangle's outgoing legs are axis
        // aligned, so under Outgoing the same polygon(4, 45) comes back
        // as four upright SQUARES and the label stops being true of the
        // picture beside it.
        tiled.corner = brush::CornerArt{box()
                                            .width(15)
                                            .height(15)
                                            .shape(shapes::polygon(4, 45.0f))
                                            .foreground(stroke(1.3f, red())),
                                        brush::CornerAlign::Bisector};
        tiled.advance = 11.0f;
        tiled.reach = 16.0f;
        add("brush::Pattern{side, corner = lozenge}", frameRect(8), tiled,
            0.9f);
      }
      {
        ContourWalk walk;
        walk.spacing = 15.0f;
        walk.stamp = box()
                         .width(7)
                         .height(7)
                         .shape(shapes::polygon(3, -90.0f))
                         .fill(kInk);
        add("ContourWalk{spacing=15, stamp=triangle}", frameRect(8), walk);
      }
      {
        PathFormat ants = stroke(1.6f, ink());
        ants.dashIntervals = {8, 6};
        ants.dashPhaseBinding = &march;
        add("PathFormat{dashPhaseBinding}  it marches", frameRect(8), ants,
            -0.8f);
      }
      {
        Brush scalloped;
        scalloped
            .shaped(
                sigil::geometry::shapers::Wave{.amplitude = 3.5f, .wavelength = 22})
            .layer(lines::Line{.width = 1.4f, .fill = ink()});
        add("shaped(shapers::Wave{3.5,22}) on a closed rect", frameRect(8),
            scalloped, 1.4f);
      }
      {
        Brush drawn;
        drawn
            .layer(lines::Line{.width = 1.3f, .fill = ink()},
                   {sigil::geometry::shapers::Jitter{
                       .segLength = 9, .deviation = 2.2f, .seed = 5}})
            .layer(lines::Line{.width = 1.1f, .fill = soft()},
                   {sigil::geometry::shapers::Jitter{
                       .segLength = 9, .deviation = 1.1f, .seed = 23}});
        add("two shapers::Jitter layers on a rect", frameRect(8), drawn, -1.8f);
      }
      add("onEdges(Top|Bottom, stroke(2))", frameRect(8),
          onEdges(sigil::geometry::path::Edge::Top |
                      sigil::geometry::path::Edge::Bottom,
                          stroke(2.0f, ink())),
          -1.2f);
      add("lines::Rails as a border (ink/red/ink)", frameRect(10),
          lines::rails({{.across = 3, .width = 1.6f, .fill = ink()},
                        {.across = 0, .width = 0.6f, .fill = red()},
                        {.across = -3, .width = 1.6f, .fill = ink()}}),
          0.5f);
      addStyle("doubleBorder(solid, dotted @7)", frameRect(8),
               decorations::doubleBorder(decorations::border(1.6f, ink()),
                                         Border{.width = 1.2f,
                                                .fill = ink(),
                                                .inset = 7.0f,
                                                .dash = {0.01f, 5.0f},
                                                .cap = SkPaint::kRound_Cap}),
               -0.9f);

      const float pitch = 212.0f;
      const size_t perRow = 7;
      for (size_t i = 0; i < frames.size(); ++i) {
        const bool second = i >= perRow;
        const size_t col = second ? i - perRow : i;
        const float x = (second ? 162.0f : 56.0f) + pitch * (float)col;
        const float y = (second ? 1546.0f : 1356.0f) + ((i % 3 == 1)   ? 12.0f
                                                        : (i % 3 == 2) ? -8.0f
                                                                       : 0.0f);
        Element frame = box()
                            .absolute()
                            .left(x)
                            .top(y)
                            .width(150)
                            .height(100)
                            .transformOrigin(0.5f, 0.5f)
                            .rotate(frames[i].rot)
                            .shape(frames[i].shape);
        const Frame& spec = frames[i];
        if (spec.style.has_value())
          frame.style(spec.style.value());
        else if (spec.where.has_value())
          frame.stroke(spec.where.value(), spec.dec);
        else
          frame.stroke(spec.dec);
        plate.child(frame.child(
            call(frames[i].label, 7.5f, kInkSoft).absolute().left(0).top(106)));
      }
    }

    // ---- VIII. WHICH WAY A CORNER FACES ----------------------------------
    // The corner tile is the one piece of brush art whose ROTATION is a
    // design decision rather than a consequence of the path, which is why
    // brush::CornerArt makes the caller state it. Taking the outgoing
    // tangent and taking the corner bisector agree nowhere on a rectangle:
    // they differ by half the turn at every vertex.
    //
    // The art below is a CHEVRON pointing along its own local +x, so the
    // difference is unmissable: on the bisector it points out of each
    // corner diagonally; on the outgoing tangent it reads as flow, four
    // arrows chasing each other round the frame.
    plate.child(sectionTitle(56, 1700, "VIII",
                             "THE CORNER \xc2\xb7 WHICH WAY IT FACES"));
    plate.child(call("brush::CornerArt{art, align} \xe2\x80\x94 the same "
                     "art, the same rect, one word different",
                     9.0f, kInkSoft)
                    .absolute()
                    .left(56)
                    .top(1720));
    {
      // A chevron pointing along local +x: two strokes meeting at the tip.
      auto chevron = [] {
        return box()
            .width(17)
            .height(17)
            .shape([](SkSize s) {
              SkPathBuilder b;
              b.moveTo(s.width() * 0.15f, s.height() * 0.12f);
              b.lineTo(s.width() * 0.88f, s.height() * 0.5f);
              b.lineTo(s.width() * 0.15f, s.height() * 0.88f);
              return b.detach();
            })
            .stroke(lines::Line{.width = 1.6f, .fill = red()});
      };
      auto tick = [] {
        return box().width(12).height(7).shape(hline()).stroke(
            lines::Line{.width = 1.1f, .fill = ink()});
      };
      struct Corner {
        const char* label;
        brush::CornerAlign align;
      };
      const Corner variants[] = {
          {"CornerArt{art, Bisector}  (an ornament)",
           brush::CornerAlign::Bisector},
          {"CornerArt{art, Outgoing}  (a marker that keeps going)",
           brush::CornerAlign::Outgoing},
      };
      for (int i = 0; i < 2; ++i) {
        brush::Pattern pb;
        pb.side = tick();
        pb.corner = brush::CornerArt{chevron(), variants[i].align};
        pb.advance = 12.0f;
        pb.cornerLength = 20.0f;
        pb.reach = 20.0f;
        plate.child(box()
                        .absolute()
                        .left(56.0f + 360.0f * (float)i)
                        .top(1762)
                        .width(230)
                        .height(120)
                        .shape(frameRect(10))
                        .stroke(std::move(pb))
                        .child(call(variants[i].label, 7.5f, kInkSoft)
                                   .absolute()
                                   .left(0)
                                   .top(126)));
      }
      // And the placement itself: the corner art sits ON the vertex. A
      // chamfer has EIGHT vertices at short intervals, which is the case
      // where a tile landing half a step off along the contour is obvious
      // rather than looking like a rounding difference.
      brush::Pattern octo;
      octo.side = tick();
      // Bisector, and it is load-bearing for what this specimen is FOR.
      // The point here is PLACEMENT — that a tile lands exactly on the
      // vertex — and bisector alignment gives eight identically oriented
      // lozenges, so a tile that sits wrong breaks an obvious rhythm.
      // Under Outgoing they alternate square/diamond, because a chamfer's
      // legs alternate axis-aligned and 45 degrees, and that alternation
      // is a confound: you cannot tell a misplaced tile from a merely
      // differently-rotated one.
      octo.corner = brush::CornerArt{box()
                                         .width(13)
                                         .height(13)
                                         .shape(shapes::polygon(4, 45.0f))
                                         .foreground(stroke(1.3f, red())),
                                     brush::CornerAlign::Bisector};
      octo.advance = 12.0f;
      octo.cornerLength = 16.0f;
      octo.reach = 18.0f;
      plate.child(box()
                      .absolute()
                      .left(776)
                      .top(1762)
                      .width(230)
                      .height(120)
                      .shape(shapes::chamfered(20.0f))
                      .stroke(std::move(octo))
                      .child(call("on shapes::chamfered(20) \xe2\x80\x94 eight "
                                  "vertices, eight tiles",
                                  7.5f, kInkSoft)
                                 .absolute()
                                 .left(0)
                                 .top(126)));
    }

    // ---- colophon --------------------------------------------------------
    plate.child(rule(56, 1940, 1488, lines::presets::cased(0.8f, soft(), 3.0f)));
    plate.child(
        call("SigilCompose \xc2\xb7 stroke_atlas.cpp \xc2\xb7 render it "
             "yourself: Sketchbook stroke_atlas.cpp --frame out.png",
             8.5f, kInkSoft)
            .absolute()
            .left(56)
            .top(1950));
    return plate;
  }

  void setup(sketch::SketchContext& ctx) override {
    sketch::kit::stage(
        ctx, {.size = {1600, 1990}, .captureAt = 6.0, .background = kPaper});

    // The one moving thing on the sheet: the marching-ants frame. A specimen
    // plate should still prove that a rule can be alive.
    ctx.ticker.add([this, &ticker = ctx.ticker](double) {
      const double t = ticker.elapsed();
      march = std::fmod((float)t * 22.0f, 14.0f);
      return true;
    });

    ctx.composer.render(describe(ctx));
  }
};

SIGIL_SKETCH(
    StrokeAtlasSketch, "Specimen",
    "The line, border and corner specimen plate \xe2\x80\x94 every rule "
    "captioned with the call that made it")
