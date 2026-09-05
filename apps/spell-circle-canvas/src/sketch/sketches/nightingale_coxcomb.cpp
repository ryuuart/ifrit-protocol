// nightingale_coxcomb.cpp — "Diagram of the Causes of Mortality in the
// Army in the East", Florence Nightingale, engraved 1858.
//
// REFERENCE
//   The two-wheel overlapping-sector polar-area plate ("the wedges";
//   later: coxcomb / rose diagram) from Notes on Matters Affecting the
//   Health … of the British Army (1858), printed as a lithograph in
//   Martineau & Nightingale, England and Her Soldiers (Smith, Elder &
//   Co., 1859). Printer's imprint: "Harrison & Sons, St. Martin's Lane."
//   Physical plate 19 x 35 cm.
//
//   Scan studied: David Rumsey Map Collection / Internet Archive item
//   dr_diagram-of-the-causes-of-mortality-in-the-army-in-the-east-10563002,
//   6996 x 3826 JPEG (507 dpi). Every geometric constant below was
//   re-derived from that scan with a pixel sampler, not eyeballed:
//   the blue-ink bounding boxes of both wheels give two independent
//   least-squares fits for the radius constant k and both centres, and
//   they agree to 5% — which is also the proof that the two wheels share
//   ONE scale (k_left = 61.0, k_right = 62.8 scan px per sqrt(rate)).
//
//   Data: Nightingale's own published figures via HistData::Nightingale
//   (R), rate = 12 * 1000 * deaths / army = "annual rate of mortality
//   per 1000". Radius law r = k*sqrt(rate) — the 1858 innovation: at a
//   fixed 30 deg wedge, only a square-root radius makes AREA
//   proportional to the number of dead.
//
// TWO THINGS THE PLATE DOES THAT REPRODUCTIONS USUALLY GET WRONG,
// settled here off the scan itself:
//   1. Month labels are NOT on a common label ring. Each one hugs its
//      OWN wedge's rim (APRIL 1854 sits ~160 px from the hub, JANUARY
//      1855 sits ~570 px out), with a floor so the tiny spring months
//      clear the black hub. That scalloped label ring is most of what
//      makes the plate read as engraved rather than plotted.
//   2. The lower-half labels are NOT flipped. The engraver used one
//      convention — glyph-up points radially OUTWARD, everywhere — so
//      DECEMBER, JANUARY, FEBRUARY and the left wheel's "1856" all come
//      out genuinely upside down. The left wheel's "1856" is often read
//      as a unique quirk; it is simply that rule, applied consistently.
//   Also: the two campaign annotations (BULGARIA, CRIMEA) are set
//   RADIALLY along their spoke, not tangentially like the months.
//
// BUILT FROM (the library, not by hand):
//   shapes::sector()      all 72 petals — one call each, no path building
//   patterns::speckle()   the litho stipple, per band, over a colour wash
//   field::grain()     plate tone + the ink-density wander inside a band
//   Paint::blend()     wash + stipple + blot + density, one fill value
//   fx::typeOn()          the pen writing the title and the legend
//   spans::upTo / scale / animate  the whole 13.6 s reading order
//   Element::onPath       every label — ONE LEAF EACH, shaped once with
//                         real kerning and placed by arc length. The months
//                         ride a clockwise ring beginning at 12 o'clock, so
//                         a label's bearing over 360 IS the fraction it
//                         sits at; glyph-up points radially outward and
//                         autoFlip is off, so the lower half reads upside
//                         down as it is printed
//   shapes::ticks         the two campaign annotations' baseline: one tick
//                         of a one-division ladder IS a straight spoke,
//                         and a comparable value, so BULGARIA and CRIMEA
//                         run outward along the radius as they are set
//
// Run:
//   ./build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
//       src/sketch/sketches/nightingale_coxcomb.cpp \
//       --frame /tmp/nightingale_coxcomb.png
//
// The 13.6 s mark is the settled plate. Earlier moments show the argument
// being made: 2.2 s is diagram 1 growing clockwise out of July 1854.

#include <include/core/SkFont.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkTypeface.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Kinetic.h>
#include <sigilcompose/kit/Legibility.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Divisions.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Frame.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/Animation.h>
#include <sigilmotion/schedule/Spread.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace field = sigil::material::field;
namespace motion = sigil::motion;
namespace patterns = sigil::material::pattern;
namespace path = sigil::geometry::path;
namespace shapes = sigil::geometry::shapes;
namespace weave = sigil::weave;

namespace skia = sigil::material::skia;

using namespace sigil::compose;
using namespace sigil::motion;
using sigil::material::skia::Paint;
using namespace std::chrono_literals;
namespace ch = choreograph;

namespace {

// ---------------------------------------------------------------------------
// palette — sampled from the scan (patch means + the darkest/lightest
// deciles, which separate "ink dot" from "paper showing through")

constexpr SkColor4f kPaper = hex(0xf2e9d9);  // aged ivory, WARM CREAM
constexpr SkColor4f kInk = hex(0x241c15);    // engraver's warm black-brown
constexpr SkColor4f kInkSoft = hex(0x241c15, 0.55f);
constexpr SkColor4f kFox = hex(0xc9a688, 0.10f);

// per band: the paper-side wash and the ink dot laid over it
// Read off the lithograph rather than off a modern chart of it: the blue
// is a POWDER blue, the pink a pale greyish rose, and both sit on warm
// cream. A saturated teal and a salmon on a pink-lilac ground are the
// same data a stop or two too strong, and at that strength the sheet
// stops looking like a stone-printed diagram.
constexpr SkColor4f kBlueWash = hex(0xccd9e0);
constexpr SkColor4f kBlueInk = hex(0x5d7f8c);
constexpr SkColor4f kRoseWash = hex(0xe6cec6);
constexpr SkColor4f kRoseInk = hex(0xb07a6a);
constexpr SkColor4f kGreyWash = hex(0xd9d0c8);
constexpr SkColor4f kGreyInk = hex(0x241f19);

// ---------------------------------------------------------------------------
// geometry — canvas 1900x1032 keeps the plate's 35:19 ratio

constexpr float kW = 1900.0f;
constexpr float kH = 1032.0f;
constexpr float kK = 17.0f;  // px per sqrt(rate); ONE scale for both wheels
// The paper sliver between two blue wedges, in degrees of the 30 deg pitch.
constexpr float kBlueGapDeg = 0.9f;
// How far a radial runs, as a fraction of the wheel's own rim. On the
// lithograph the faint radials live INSIDE the central cluster and stop
// there; run to the rim they cross the blue, which nothing on the stone
// does.
constexpr float kSpokeReach = 0.24f;
constexpr SkPoint kC1{1397, 386};  // Diagram 1 (right) — Apr 1854..Mar 1855
constexpr SkPoint kC2{430, 384};   // Diagram 2 (left)  — Apr 1855..Mar 1856
constexpr float kR1 = 543.7f;      // kK * sqrt(1022.8), Jan 1855 disease
constexpr float kR2 = 267.5f;      // kK * sqrt(247.6),  Jun 1855 disease

// ---------------------------------------------------------------------------
// the data (HistData::Nightingale). Listed in WHEEL order: the engraver's
// seam is the June/July boundary at 12 o'clock, so the wheel runs
// July..June even though the report year runs April..March.

struct Month {
  const char* label;             // outer label line
  const char* line2;             // inner label line ("" = single line)
  float disease, wounds, other;  // annual rate per 1000
};

// bearing = degrees clockwise from 12 o'clock; month i spans [30i, 30i+30]
const std::array<Month, 12> kD1 = {{
    {"JULY", "", 150.0f, 0.0f, 9.6f},             // Jul 1854
    {"AUGUST", "", 328.5f, 0.4f, 11.9f},          // Aug 1854
    {"SEPTEMBER", "", 312.2f, 32.1f, 27.7f},      // Sep 1854
    {"OCTOBER", "", 197.0f, 51.7f, 50.1f},        // Oct 1854
    {"NOVEMBER", "", 340.6f, 115.8f, 42.8f},      // Nov 1854
    {"DECEMBER", "", 631.5f, 41.7f, 48.0f},       // Dec 1854
    {"JANUARY", "1855", 1022.8f, 30.7f, 120.0f},  // Jan 1855 — the maximum
    {"FEBRUARY", "", 822.8f, 16.3f, 140.1f},      // Feb 1855
    {"MARCH", "1855.", 480.3f, 12.8f, 68.6f},     // Mar 1855
    {"APRIL", "1854", 1.4f, 0.0f, 7.0f},          // Apr 1854
    {"MAY", "", 6.2f, 0.0f, 4.6f},                // May 1854
    {"JUNE", "", 4.7f, 0.0f, 2.5f},               // Jun 1854
}};

const std::array<Month, 12> kD2 = {{
    {"JULY", "", 107.5f, 37.7f, 9.3f},        // Jul 1855
    {"AUGUST", "", 129.9f, 44.1f, 6.7f},      // Aug 1855
    {"SEPTEMBER", "", 47.5f, 69.4f, 5.0f},    // Sep 1855
    {"OCTOBER", "", 32.8f, 13.6f, 4.6f},      // Oct 1855
    {"NOVEMBER", "", 56.4f, 10.5f, 10.1f},    // Nov 1855
    {"DECEMBER", "", 25.3f, 5.0f, 7.8f},      // Dec 1855
    {"JANUARY", "", 11.4f, 0.5f, 13.0f},      // Jan 1856
    {"FEBRUARY", "", 6.6f, 0.0f, 5.2f},       // Feb 1856
    {"MARCH", "", 3.9f, 0.0f, 9.1f},          // Mar 1856
    {"APRIL", "1855", 177.5f, 17.9f, 21.2f},  // Apr 1855
    {"MAY", "", 171.8f, 16.6f, 12.5f},        // May 1855
    {"JUNE", "", 247.6f, 64.5f, 9.6f},        // Jun 1855 — this wheel's maximum
}};

// ---------------------------------------------------------------------------
// the legend, transcribed verbatim off the plate (period spelling
// "Preventible" kept). Continuation lines carry the hanging indent.

struct LegendLine {
  int indent;
  const char* text;
};
const std::array<LegendLine, 12> kLegendText = {{
    {0, "The Areas of the blue, red, & black wedges are each measured from"},
    {1, "the centre as the common vertex."},
    {0,
     "The blue wedges measured from the centre of the circle represent area"},
    {1,
     "for area the deaths from Preventible or Mitigable Zymotic diseases; the"},
    {1, "red wedges measured from the centre the deaths from wounds; & the"},
    {1,
     "black wedges measured from the centre the deaths from all other causes."},
    {0,
     "The black line across the red triangle in Novr. 1854 marks the boundary"},
    {1, "of the deaths from all other causes during the month."},
    {0,
     "In October 1854, & April 1855, the black area coincides with the red;"},
    {1, "in January & February 1855, the blue coincides with the black."},
    {0,
     "The entire areas may be compared by following the blue, the red & the"},
    {1, "black lines enclosing them."},
}};

// ---------------------------------------------------------------------------
// helpers

constexpr float kDeg = 3.14159265358979f / 180.0f;

// The plate's own coordinate convention, as a value: 0 deg is 12 o'clock and
// bearings run clockwise, because the 1858 coxcomb starts its year at the
// top and reads round to the right. `radius` is 1 so px radii can be passed
// straight to `px()`; every wheel makes its own frame with `about()`.
constexpr path::Frame kPlate{.centre = {0, 0},
                            .radius = 1.0f,
                            .zero = path::Zero::North,
                            .sense = path::Sense::CW};

/** Bearing (deg clockwise from 12 o'clock) + radius -> canvas point. */
SkPoint polar(SkPoint c, float radius, float bearingDeg) {
  return kPlate.about(c).px(bearingDeg, radius);
}

/** THE PLATE'S OWN BASELINE: a clockwise ring beginning at 12 o'clock, so
 *  the arc-length fraction a run is placed at IS its bearing over 360 and
 *  no conversion stands between the plate's coordinate and the library's.
 *  Clockwise is what puts glyph-up radially OUTWARD, which is the
 *  engraver's one convention on this sheet — with `autoFlip` off,
 *  DECEMBER, JANUARY, FEBRUARY and the left wheel's 1856 come out
 *  genuinely upside down, as they are printed. */
inline shapes::Circle rimBaseline() {
  return shapes::circle(SkPathDirection::kCW, 0);
}

/** THE SPOKE a campaign annotation is lettered along: one tick of a
 *  one-division ladder at @p bearing, running from @p inner to @p outer of
 *  the node's own radius. A straight baseline AND a comparable value, so
 *  the run prunes.
 *
 *  It is not `Orient::Radial`. That turns each glyph off the tangent while
 *  the run still advances round the arc, which is right for a numeral on a
 *  dial and wrong for a word: BULGARIA laid out that way spreads its eight
 *  letters across the top of the wheel. The plate sets both annotations
 *  ALONG their spoke, so the baseline is the spoke and the orientation is
 *  the ordinary tangent. */
inline shapes::TicksShape spokeBaseline(float bearing, float inner,
                                        float outer) {
  return shapes::ticks(
      {.divisions = 1, .from = bearing, .mark = {inner, outer}}, kPlate);
}

/** A square box of radius @p r centred on @p c, in the parent's space —
 *  the frame every shapes::sector / shapes::arc wedge is inscribed in. */
Element discBox(SkPoint c, float r) {
  return box().rect(path::Frame{.centre = c, .radius = r}.box());
}

/** THE BOX A SECTOR NEEDS, not the box its disc needs. A wedge on its own
 *  bake costs its box's pixels every frame its entrance scales it — the
 *  blit is resampled through the transform — and a 30 degree sector
 *  inscribed in its whole disc is a bake that is nine parts transparent.
 *  So a wedge stands in the sector's own bounds: the centre, the arc's two
 *  ends and whichever axis extremes the arc crosses, grown by a pixel for
 *  the outline. Angles are Skia's canvas angles, 0 = +x, clockwise. */
SkRect sectorBounds(SkPoint c, float r, float startDeg, float sweepDeg) {
  const auto at = [&](float deg) {
    const float a = deg * (float)std::numbers::pi / 180.0f;
    return SkPoint{c.fX + r * std::cos(a), c.fY + r * std::sin(a)};
  };
  SkRect b = SkRect::MakeXYWH(c.fX, c.fY, 0, 0);
  const auto grow = [&](SkPoint p) {
    b.fLeft = std::min(b.fLeft, p.fX);
    b.fTop = std::min(b.fTop, p.fY);
    b.fRight = std::max(b.fRight, p.fX);
    b.fBottom = std::max(b.fBottom, p.fY);
  };
  grow(at(startDeg));
  grow(at(startDeg + sweepDeg));
  const float lo = std::min(startDeg, startDeg + sweepDeg);
  const float hi = std::max(startDeg, startDeg + sweepDeg);
  for (float axis = std::ceil(lo / 90.0f) * 90.0f; axis <= hi; axis += 90.0f)
    grow(at(axis));
  b.outset(1.0f, 1.0f);
  return b;
}

/** The sector itself, drawn in a box that is its bounds rather than its
 *  disc: the same arc `shapes::sector` draws, about a centre that lies
 *  outside the box. */
Element sectorBox(SkPoint c, float r, float startDeg, float sweepDeg) {
  const SkRect bounds = sectorBounds(c, r, startDeg, sweepDeg);
  const SkPoint centre{c.fX - bounds.left(), c.fY - bounds.top()};
  return box()
      .rect(bounds)
      .shape([=](SkSize) {
        SkPathBuilder b;
        b.moveTo(centre);
        b.arcTo(SkRect::MakeXYWH(centre.fX - r, centre.fY - r, 2 * r, 2 * r),
                startDeg, sweepDeg, false);
        b.close();
        return b.detach();
      })
      .transformOriginPx(centre);
}

/** A straight spoke from the box centre out to radiusFraction. One tick of
 *  a one-division ladder: kit::ticks resolves the centre and radius from
 *  the node's own laid-out box, so the caller never computes them. The
 *  spokes stay one node EACH — each carries its own trim reveal off its own
 *  delay, and a single multi-tick path would own one animation for all
 *  twelve and lose the stagger. */
std::function<SkPath(SkSize)> spoke(float radiusFraction, float bearing) {
  return shapes::ticks(
      {.divisions = 1, .from = bearing, .mark = {0.0f, radiusFraction}},
      kPlate);
}

}  // namespace

// ===========================================================================

struct NightingaleCoxcomb : sketch::Sketch {
  // --- the plate's own reading order, as a clock (seconds) ---
  static constexpr float tTitle1 = 0.0f;
  static constexpr float tTitle2 = 0.9f;
  static constexpr float tCap1 = 1.0f;
  static constexpr float tSpoke1 = 1.15f;
  static constexpr float tWedge1 = 1.35f;  // + 0.115 s per month
  static constexpr float tLabel1 = 3.10f;
  static constexpr float tCap2 = 3.70f;
  static constexpr float tSpoke2 = 3.85f;
  static constexpr float tWedge2 = 4.05f;  // + 0.100 s per month
  static constexpr float tLabel2 = 5.45f;
  static constexpr float tLeader = 6.00f;
  static constexpr float tLegend = 6.40f;  // + 0.20 s per line
  static constexpr float tNeedle1 = 9.40f;
  static constexpr float tNeedle2 = 11.50f;
  static constexpr float tNeedleEnd = 13.10f;

  // bound outputs: the two index needles and the 24 rim flashes they ring
  ch::Output<float> needle1Deg{0}, needle1A{0};
  ch::Output<float> needle2Deg{0}, needle2A{0};
  std::array<ch::Output<float>, 24> flash;

  // held so their identity (and the speckle bake) survives re-describes
  Pattern blueGrain, roseGrain, greyGrain, foxing;
  Paint blueMat, roseMat, greyMat, paperMat;

  sk_sp<SkTypeface> faceDisplay, faceGrotesque, faceLabel, faceScript;

  // ------------------------------------------------------------------
  /** ONE LABEL ON A RING. The run is shaped ONCE — real kerning, real
   *  ligatures, real advances — and every glyph is placed by arc length
   *  and turned to its bearing through one batched RSXform draw. The
   *  baseline resolves against the TEXT NODE'S OWN box, which is why the
   *  leaf carries the ring's diameter and stands at the wheel's centre;
   *  wrapping it in a sized parent collapses the whole run onto a point.
   *
   *  `Tangent` is the months' running lettering, glyph-up outward.
   *  `Radial` runs the baseline along the radius instead, which is how the
   *  two campaign annotations are set — along their spoke, and the one
   *  thing on this plate that genuinely radiates. */
  Element ringRun(const weave::TextStyle& style, SkPoint centre,
                  const std::string& content, float bearingDeg, float radius,
                  float delayMs, const std::string& key) {
    return text(std::u8string(content.begin(), content.end()), style)
        .key(key)
        .width(Dim(2 * radius))
        .height(Dim(2 * radius))
        .centerAt(centre)
        .onPath(TextPath{.path = rimBaseline(),
                         .at = bearingDeg / 360.0f,
                         .align = TextPath::Align::Center,
                         .autoFlip = false,
                         .orient = TextPath::Orient::Tangent})
        .opacity(animate(from(0.0f).to(1.0f), ramp(delayMs, 260.0f)));
  }

  /** A CAMPAIGN ANNOTATION, set along its spoke: the same one-leaf run on
   *  a straight radial baseline, centred at @p radius from the hub. */
  Element spokeRun(const weave::TextStyle& style, SkPoint centre,
                   const std::string& content, float bearingDeg, float radius,
                   float delayMs, const std::string& key) {
    const float half = 120.0f;  // half the reach the run is given, px
    const float box = radius + half;
    return text(std::u8string(content.begin(), content.end()), style)
        .key(key)
        .width(Dim(2 * box))
        .height(Dim(2 * box))
        .centerAt(centre)
        .onPath(TextPath{.path = spokeBaseline(bearingDeg,
                                               (radius - half) / box,
                                               (radius + half) / box),
                         .at = 0.5f,
                         .align = TextPath::Align::Center,
                         .autoFlip = false,
                         .orient = TextPath::Orient::Tangent})
        .opacity(animate(from(0.0f).to(1.0f), ramp(delayMs, 260.0f)));
  }

  // ------------------------------------------------------------------
  Element wheel(sketch::SketchContext& ctx, const std::array<Month, 12>& data,
                SkPoint centre, float rMax, float startSec, float stepSec,
                float spokeSec, int flashBase, const char* tag) {
    (void)ctx;
    const SkPoint local{rMax, rMax};  // the wheel box's own centre
    auto wheelBox = discBox(centre, rMax);

    // The 12 hairline spokes. On the plate a radial line only exists where
    // BOTH neighbouring months have ink, so each spoke runs out to the
    // smaller of its two months' rims — otherwise stray hairlines shoot
    // across the empty spring quadrant.
    auto rimOf = [&](int m) {
      const Month& d = data[(m % 12 + 12) % 12];
      return kK * std::sqrt(std::max({d.disease, d.wounds, d.other, 0.1f}));
    };
    for (int i = 0; i < 12; ++i) {
      const float len =
          std::min({rimOf(i - 1), rimOf(i), rMax * kSpokeReach}) * 0.98f;
      if (len < 4.0f) continue;
      wheelBox.child(
          box()
              .inset(0)
              .key(std::string(tag) + "spoke" + std::to_string(i))
              .shape(spoke(len / rMax, (float)i * 30.0f))
              .stroke(spans::upTo(animate(
                          from(0.0f).to(1.0f),
                          ramp(spokeSec * 1000.0f + (float)i * 16.0f, 220.0f))),
                      stroke(0.7f, Fill::color(kInkSoft))));
    }

    for (int m = 0; m < 12; ++m) {
      const Month& mo = data[m];
      // bearing (0 = 12 o'clock, clockwise) -> Skia canvas angle (0 = +x)
      const float skia0 = (float)m * 30.0f - 90.0f;
      // Painter's algorithm by magnitude: biggest first, so every band
      // shows its own colour with no stacking arithmetic anywhere.
      struct Band {
        float rate;
        const Paint* mat;
        const char* name;
      };
      std::array<Band, 3> bands = {{{mo.disease, &blueMat, "b"},
                                    {mo.wounds, &roseMat, "r"},
                                    {mo.other, &greyMat, "k"}}};
      std::sort(bands.begin(), bands.end(),
                [](const Band& a, const Band& b) { return a.rate > b.rate; });

      const float delay = (startSec + stepSec * (float)m) * 1000.0f;
      for (const Band& band : bands) {
        if (band.rate <= 0.0f) continue;
        const float r = kK * std::sqrt(band.rate);
        // THE BLUE WEDGES CARRY NO BORDER. On the stone the tint simply
        // stops, and adjacent blue wedges are parted by a sliver of paper
        // at their outer ends — an ANGULAR gap, so it opens toward the
        // rim. Only the inner black and pink wedges are outlined, and a
        // hairline round every wedge is the one decision that makes the
        // sheet read as a modern vector chart.
        const bool outer = band.mat == &blueMat;
        const float gap = outer ? kBlueGapDeg : 0.0f;
        Element wedge =
            sectorBox(local, r, skia0 + gap * 0.5f, 30.0f - gap)
                .key(std::string(tag) + band.name + std::to_string(m))
                .fill(*band.mat);
        if (!outer) wedge.stroke(stroke(1.0f, Fill::color(kInk)));
        // The entrance scales the wedge about the WHEEL's centre, which
        // sectorBox set as the pivot: the sector grows out of the hub.
        wheelBox.child(
            std::move(wedge)
                .scale(animate(from(0.002f).to(1.0f),
                               ramp(delay, 620.0f, ch::easeOutExpo)))
                // Each band's litho fill is a Paint::blend of four
                // shaders (wash + speckle + blot + grain) over an area that
                // never changes. Uncached, every one of those shaders re-runs
                // on every frame for all ~24 bands. The content is static —
                // only the entrance SCALE animates — and the cache captures
                // NODE-LOCAL content, so the transform rides the blit and the
                // texture is baked once.
                //
                // The trade is resampling: the scale transform now samples a
                // baked texture rather than re-rasterising, so sector edges
                // are texture-filtered and the stipple (noise generated in
                // node-local space) shifts by a fraction of a pixel. On a
                // data plate that is invisible; if pixel-exact sector edges
                // matter more than the shader cost, drop this cache.
                .cache(Cache::Texture));
      }

      // the flash the index needle rings out of each month's rim
      const float rim =
          kK * std::sqrt(std::max({mo.disease, mo.wounds, mo.other, 1.0f})) +
          10.0f;
      wheelBox.child(discBox(local, rim)
                         .key(std::string(tag) + "flash" + std::to_string(m))
                         .shape(shapes::arc(skia0 + 1.0f, 28.0f))
                         .stroke(stroke(2.4f, Fill::color(hex(0xc8a24a, 0.9f))))
                         .opacity(&flash[flashBase + m]));
    }
    return wheelBox;
  }

  // ------------------------------------------------------------------
  Element needle(SkPoint centre, float rMax, const ch::Output<float>* deg,
                 const ch::Output<float>* alpha, const char* key) {
    return discBox(centre, rMax)
        .key(key)
        .shape(spoke(1.0f, 0.0f))
        .stroke(stroke(1.4f, Fill::color(hex(0xd8b45c))))
        .background(shadow(hex(0xd8b45c, 0.5f), {0, 0}, 9))
        .transformOrigin(0.5f, 0.5f)
        .rotate(deg)
        .opacity(alpha)
        .cache(Cache::None);
  }

  // ------------------------------------------------------------------
  Element describe(sketch::SketchContext& ctx) {
    auto root = stack().fill(Fill::color(kPaper));

    // ---- paper: fractal mottle, sparse foxing, a soft vignette ------
    // The paper base is three static but expensive layers: a procedural
    // fractal under a full-canvas softLight composite, a speckle foxing
    // material, and a vignette. They are wrapped in ONE opaque box whose own
    // fill is kPaper — the exact backdrop the softLight would otherwise
    // blend against as separate root children, so folding them changes no
    // pixels — and cached together. The softLight then resolves once at bake
    // time and each frame blits a single opaque texture instead of running
    // three shaders across the whole canvas.
    //
    // The group boundary sits exactly here: everything inside is the static
    // base, and the wedges and titles above it animate, so they stay outside
    // where the cache cannot be invalidated by them.
    root.child(stack()
                   .inset(0)
                   .fill(Fill::color(kPaper))
                   .child(box().inset(0).fill(paperMat).opacity(0.17f).blend(
                       SkBlendMode::kSoftLight))
                   .child(box().inset(0).fill(foxing.material()))
                   .child(box().inset(0).fill(
                       radialGradient({kW * 0.5f, kH * 0.5f}, kW * 0.72f,
                                      {hex(0x000000, 0.0f), hex(0x000000, 0.0f),
                                       hex(0x6b4a33, 0.085f)},
                                      {0.0f, 0.70f, 1.0f})))
                   .cache(Cache::Texture));

    // ---- the reverse page showing through (custom leaf, raw Skia) ----
    root.child(custom([this](SkCanvas& canvas, const PaintContext&) {
                 if (!faceDisplay) return;
                 SkFont f(faceDisplay, 46);
                 SkPaint p;
                 p.setAntiAlias(true);
                 p.setColor4f(hex(0x241c15, 0.055f), nullptr);
                 canvas.save();
                 canvas.translate(760, 118);  // mirrored: the verso title
                 canvas.scale(-1, 1);
                 canvas.drawString("ENGLAND", 0, 0, f, p);
                 canvas.restore();
               }).inset(0));

    // ---- the plate mark: the physical impression of the copper ------
    root.child(box()
                   .inset(26)
                   .fill(Fill::none())
                   .stroke(stroke(1.0f, Fill::color(hex(0x8a7060, 0.20f)))));
    root.child(box()
                   .inset(28)
                   .fill(Fill::none())
                   .stroke(stroke(1.0f, Fill::color(hex(0xffffff, 0.35f)))));

    // ---- the spine fold at the sheet's centre -----------------------
    root.child(box().left(938).top(0).width(24).height(kH).fill(
        linearGradient({0, 0}, {24, 0},
                       {hex(0x3a2a20, 0.0f), hex(0x3a2a20, 0.06f),
                        hex(0xffffff, 0.09f), hex(0x3a2a20, 0.0f)},
                       {0.0f, 0.42f, 0.60f, 1.0f})));

    // ---- title block -------------------------------------------------
    const auto title1 = kit::emboldened(
        weave::textStyle({.face = faceDisplay, .size = 39, .color = kInk, .track = 0.8f}),
        2.0f, kInk);
    const auto title2 = kit::emboldened(
        weave::textStyle({.face = faceGrotesque, .size = 27, .color = kInk, .track = 0.4f}),
        0.9f, kInk);

    Track t1{.effect = fx::typeOn(),
             .stagger = {.eachMs = 0, .amountMs = 620, .durationMs = 40},
             .progress = animate(from(0.0f).to(1.0f),
                                 ramp(tTitle1 * 1000, 700, ch::easeNone))};
    root.child(text(toU8("DIAGRAM of the CAUSES of MORTALITY"), title1)
                   .key("title1")
                   .fx(std::move(t1))
                   .echo({0.8f, 0.5f}, hex(0x241c15, 0.8f))
                   .centerAt({968, 38}));

    Track t2{.effect = fx::typeOn(),
             .stagger = {.eachMs = 0, .amountMs = 340, .durationMs = 40},
             .progress = animate(from(0.0f).to(1.0f),
                                 ramp(tTitle2 * 1000, 400, ch::easeNone))};
    root.child(text(toU8("in the ARMY in the EAST."), title2)
                   .key("title2")
                   .fx(std::move(t2))
                   .echo({0.6f, 0.4f}, hex(0x241c15, 0.7f))
                   .centerAt({945, 84}));

    // the double hairline under the title
    for (int i = 0; i < 2; ++i)
      root.child(box()
                     .left(775)
                     .top(108.0f + (float)i * 4.0f)
                     .width(368)
                     .height(1)
                     .fill(Fill::color(kInk))
                     .transformOrigin(0.0f, 0.5f)
                     .scale(animate(from(0.0f).to(1.0f),
                                    ramp(tTitle2 * 1000 + 220 + (float)i * 60,
                                         420, ch::easeOutQuint))));

    // ---- the two diagram captions -----------------------------------
    const auto capNum =
        weave::textStyle({.face = faceGrotesque, .size = 24, .color = kInk});
    const auto capText =
        weave::textStyle({.face = faceGrotesque, .size = 21, .color = kInk, .track = 0.4f});
    auto caption = [&](const char* num, const char* label, float cx, float numX,
                       float startSec, const char* key) {
      root.child(text(toU8(num), capNum)
                     .key(std::string(key) + "n")
                     .centerAt({numX, 40})
                     .opacity(animate(from(0.0f).to(1.0f),
                                      ramp(startSec * 1000, 320))));
      root.child(text(toU8(label), capText)
                     .key(std::string(key) + "t")
                     .centerAt({cx, 78})
                     .opacity(animate(from(0.0f).to(1.0f),
                                      ramp(startSec * 1000 + 90, 320))));
      root.child(box()
                     .left(cx - 140)
                     .top(94)
                     .width(280)
                     .height(1)
                     .fill(Fill::color(kInkSoft))
                     .transformOrigin(0.0f, 0.5f)
                     .scale(animate(
                         from(0.0f).to(1.0f),
                         ramp(startSec * 1000 + 180, 380, ch::easeOutQuint))));
    };
    caption("1.", "APRIL 1854 to MARCH 1855.", 1320, 1489, tCap1, "cap1");
    caption("2.", "APRIL 1855 to MARCH 1856.", 413, 394, tCap2, "cap2");

    // ---- the wheels --------------------------------------------------
    root.child(wheel(ctx, kD1, kC1, kR1, tWedge1, 0.115f, tSpoke1, 0, "a"));
    root.child(wheel(ctx, kD2, kC2, kR2, tWedge2, 0.100f, tSpoke2, 12, "b"));

    // ---- the ring labels: each hugging its own wedge's rim ----------
    const auto labelStyle = kit::emboldened(
        weave::textStyle({.face = faceLabel, .size = 20, .color = kInk, .track = 0.4f}),
        0.35f, kInk);
    const auto smallLabel =
        weave::textStyle({.face = faceLabel, .size = 12, .color = kInk, .track = 0.0f});
    // The two campaign annotations are tracked wider than the months.
    // A run on a path is shaped once, so tracking is part of the shaping
    // and belongs to the style rather than to the call.
    const auto campaign = weave::textStyle(
        {.face = faceLabel, .size = 16, .color = kInk, .track = 1.9f});
    std::vector<Element> labels;

    // The floor is not decoration: twelve labels must fit the circumference
    // they sit on, so the ring cannot close tighter than 12 * (widest label)
    // / 2pi. That is why the plate sets the small left wheel in a smaller
    // face — the same constraint, solved the same way.
    auto ringLabels = [&](const std::array<Month, 12>& data, SkPoint centre,
                          float floorR, const weave::TextStyle& style,
                          float gap, float step, float startSec,
                          const char* tag) {
      for (int m = 0; m < 12; ++m) {
        const Month& mo = data[m];
        const float rim =
            kK * std::sqrt(std::max({mo.disease, mo.wounds, mo.other, 0.5f}));
        const float base = std::max(rim + gap, floorR);
        const float bearing = (float)m * 30.0f + 15.0f;
        const float delay = (startSec + (float)m * 0.028f) * 1000.0f;
        const bool twoLines = mo.line2[0] != '\0';
        labels.push_back(ringRun(style, centre, mo.label, bearing,
                                 base + (twoLines ? step : 0.0f), delay,
                                 std::string(tag) + "L" + std::to_string(m) +
                                     "a"));
        if (twoLines)
          labels.push_back(ringRun(style, centre, mo.line2, bearing, base,
                                   delay + 60.0f,
                                   std::string(tag) + "L" +
                                       std::to_string(m) + "b"));
      }
    };
    ringLabels(kD1, kC1, 172.0f, labelStyle, 26.0f, 24.0f, tLabel1, "a");
    ringLabels(kD2, kC2, 160.0f, smallLabel, 14.0f, 14.0f, tLabel2, "b");

    // the campaign annotations — set RADIALLY along their spoke
    labels.push_back(spokeRun(campaign, kC1, "BULGARIA", 358.0f, 150.0f,
                              tLabel1 * 1000 + 380, "bulg"));
    labels.push_back(spokeRun(campaign, kC1, "CRIMEA", 94.0f, 268.0f,
                              tLabel1 * 1000 + 460, "crim"));
    // the left wheel's year marker, upside down at 6 o'clock — which is
    // simply the outward-up rule arriving at the bottom of the circle
    labels.push_back(ringRun(smallLabel, kC2, "1856", 180.0f, 134.0f,
                             tLabel2 * 1000 + 300, "y1856"));

    for (Element& e : labels) root.child(std::move(e));

    // ---- the dashed leader between the two wheels -------------------
    PathFormat dash = stroke(1.1f, Fill::color(kInk));
    dash.dashIntervals = {7.0f, 5.0f};
    root.child(box()
                   .inset(0)
                   .key("leader")
                   .fill(Fill::none())
                   .shape([](SkSize) {
                     SkPathBuilder p;
                     p.moveTo(202, 398);
                     p.lineTo(614, 522);
                     p.lineTo(1024, 374);
                     return p.detach();
                   })
                   .stroke(spans::upTo(animate(
                               from(0.0f).to(1.0f),
                               ramp(tLeader * 1000, 620, ch::easeOutQuad))),
                           dash));

    // ---- the engraved-hand legend -----------------------------------
    // Twelve hand-placed lines, one node each: every line sits at the
    // engraving's own indent and leading, which no single paragraph's
    // layout can reproduce — so the lines stay separate nodes and only
    // the SCHEDULE rides the engine. The container's staggerChildren is
    // the 200 ms per-line ladder, and each line's pen runs for exactly
    // its cascade's span, so the writing speed is the cascade's own.
    const auto script = weave::textStyle({.face = faceScript, .size = 27, .color = kInk});
    const motion::Spread penStagger{.amountMs = 620, .durationMs = 30};
    Element legend = stack().inset(0).staggerChildren(200ms);
    for (size_t i = 0; i < kLegendText.size(); ++i) {
      Track pen{.effect = fx::typeOn(),
                .stagger = penStagger,
                .progress = animate(
                    from(0.0f).to(1.0f),
                    ramp(tLegend * 1000, penStagger.spanMs(2), ch::easeNone))};
      legend.child(text(toU8(kLegendText[i].text), script)
                       .key("leg" + std::to_string(i))
                       .fx(std::move(pen))
                       .left(171.0f + (float)kLegendText[i].indent * 22.0f)
                       .top(628.0f + (float)i * 30.7f));
    }
    root.child(std::move(legend));

    // ---- printer's imprint ------------------------------------------
    root.child(text(toU8("Harrison & Sons, St. Martin's Lane."),
                    weave::textStyle({.face = faceScript, .size = 20, .color = kInkSoft}))
                   .key("imprint")
                   .centerAt({1712, 1004})
                   .opacity(animate(from(0.0f).to(1.0f),
                                    ramp(tLegend * 1000 + 2500, 600))));

    // ---- the index needles ------------------------------------------
    root.child(needle(kC1, kR1, &needle1Deg, &needle1A, "needle1"));
    root.child(needle(kC2, kR2, &needle2Deg, &needle2A, "needle2"));

    return root;
  }

  // ------------------------------------------------------------------
  void setup(sketch::SketchContext& ctx) override {
    // The still frame this sketch photographs itself at: the first clean
    // instant after the second needle sweep has faded out (tNeedleEnd plus
    // its 0.45 s fade), by which point every entrance has finished and the
    // plate holds unchanged. Capturing earlier catches the legend half
    // written and the dashed leader mid-draw.
    sketch::kit::stage(ctx, {.size = SkSize::Make(kW, kH),
                             .captureAt = 13.6,
                             .background = kPaper});

    // The plate's title face is an ornamental Victorian INLINE Roman —
    // dark stems carrying a white hairline. "Academy Engraved LET" is the
    // only installed face of that genre; it draws lighter than the plate,
    // so the title carries a glyph-level stroke underlay to thicken the
    // ink ribbon (Bodoni 72 Bold matches the WEIGHT but loses the genre).
    // ONE FALLBACK CHAIN PER LETTERING SYSTEM, resolved through the
    // library's own walk: the first installed family wins, and a machine
    // with none of them gets the default face AT THE WEIGHT ASKED FOR
    // rather than silently at Normal.
    faceDisplay =
        weave::ports::face({"Academy Engraved LET"}, SkFontStyle::Normal());
    if (!faceDisplay)
      faceDisplay =
          weave::ports::face({"Bodoni 72"}, SkFontStyle::kBold_Weight);
    faceGrotesque = weave::ports::face({"Copperplate", "Helvetica Neue"},
                                       SkFontStyle::kBold_Weight);
    faceLabel = weave::ports::face({"Copperplate", "Helvetica Neue"});
    faceScript = weave::ports::face({"Snell Roundhand", "Apple Chancery"});

    // The litho tint: a paper-side wash with the ink dot field over it.
    // Two speckle layers per band — a fine one for the tint itself and a
    // coarse sparse one so the ink density visibly wanders, which is what
    // separates a stone-printed tint from a flat vector fill.
    auto band = [](SkColor4f wash, SkColor4f ink, int fine, int coarse,
                   uint32_t seed, Pattern& grainOut) {
      // THE TILE HAS TO BE BIGGER THAN THE EYE'S PATCH. A forty-pixel
      // stipple repeats a dozen times across a wheel, and at that pitch
      // the tile's own little clusters read as an ordered motif — which
      // an aquatint stipple never is. At this tile it repeats three or
      // four times across the same wheel and the repeat stops being
      // findable; the mark count rises with the area so the density is
      // the density it was.
      grainOut = patterns::speckle(128, fine * 10, 0.25f, 0.66f, {skia::toColor(ink)});
      grainOut.seed(seed);
      Pattern blot =
          patterns::speckle(320, coarse * 8, 1.8f, 5.0f,
                            {skia::toColor(
                                SkColor4f{ink.fR, ink.fG, ink.fB, 0.12f})});
      blot.seed(seed * 7 + 3);
      return Paint::blend(
          {{Paint::solid(wash), SkBlendMode::kSrc},
           {grainOut.material(), SkBlendMode::kSrcOver},
           {blot.material(), SkBlendMode::kSrcOver},
           // ink density wanders across the stone: LUMINANCE noise, so it
           // reads as light on the tint instead of hue-shifting it
           {Paint::recipe(field::grain(0.010f, 3, (float)seed)),
            SkBlendMode::kSoftLight}});
    };
    blueMat = band(kBlueWash, kBlueInk, 1150, 14, 11, blueGrain);
    roseMat = band(kRoseWash, kRoseInk, 900, 10, 23, roseGrain);
    greyMat = band(kGreyWash, kGreyInk, 900, 18, 37, greyGrain);

    paperMat = Paint::recipe(field::grain(0.011f, 4, 5.0f));
    foxing = patterns::speckle(190, 4, 1.5f, 6.5f, {skia::toColor(kFox)});
    foxing.seed(91);

    // The needles and the rim flashes they ring.
    ctx.ticker.add([this, &ticker = ctx.ticker](double) {
      const double t = ticker.elapsed();
      const float s = (float)t;
      auto sweep = [&](float t0, float t1, ch::Output<float>& deg,
                       ch::Output<float>& alpha, int base) {
        if (s < t0 || s > t1 + 0.45f) {
          alpha = 0.0f;
          return;
        }
        const float u = std::clamp((s - t0) / (t1 - t0), 0.0f, 1.0f);
        deg = u * 360.0f;
        alpha = s > t1 ? std::max(0.0f, 1.0f - (s - t1) / 0.45f)
                       : std::min(1.0f, (s - t0) / 0.15f);
        for (int m = 0; m < 12; ++m) {
          const float centreB = (float)m * 30.0f + 15.0f;
          float d = std::fabs(deg.value() - centreB);
          if (d > 180.0f) d = 360.0f - d;
          flash[base + m] = alpha.value() * std::max(0.0f, 1.0f - d / 13.0f);
        }
      };
      sweep(tNeedle1, tNeedle2 - 0.1f, needle1Deg, needle1A, 0);
      sweep(tNeedle2, tNeedleEnd, needle2Deg, needle2A, 12);
      return true;
    });

    ctx.composer.render(describe(ctx));
  }

  void update(double, sketch::SketchContext&) override {}
};

SIGIL_SKETCH(
    NightingaleCoxcomb, "Study \xc2\xb7 Science",
    "Nightingale's 1858 coxcomb \xe2\x80\x94 polar-area wedges from the real "
    "mortality table")
