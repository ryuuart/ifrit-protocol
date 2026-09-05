// eva_magi_defense.cpp — THE END OF EVANGELION, THE MAGI DEFENSE PLATE
// =============================================================================
// Gainax / Production I.G, 1997. Episode 25' "Air", the SEELE hacking sequence
// (~00:05-00:08, before the JSSDF assault): the full-screen tactical plate
// NERV's bridge shows while five external MAGI installations are driven against
// Tokyo-3's MAGI 01.
//
//   Shigeru:   "Data entry from all external nodes... They're hacking the
//   MAGI!" Fuyutsuki: "Hacking verified from Germany, China, the U.S. ..."
//   Maya:      "A Danang Type-B defense screen has been deployed."
//                                        — script, evaotaku.com/html/air.html
//
// -----------------------------------------------------------------------------
// SOURCES — read and MEASURED, not remembered
//
//  * The frame itself: static.wikia.nocookie.net/evangelion/images/f/f6/
//    Magi_(EoE).png, 1920x1080. Every number below was measured off it with
//    PIL (scanline run-length + colour-masked flood fill), and this sketch is
//    built at exactly that size so the capture diffs against the plate.
//  * github.com/TheGreatGildo/nerv-ui — components/crt-effects.css: the CRT
//    stack (2px scanline interval, ~4% black, the 70%/70% vignette ellipse to
//    40%, the phosphor-flicker keyframes). Taken verbatim. Its PALETTE is fan-
//    authored and is NOT used: every colour here is sampled off the frame.
//  * github.com/itorr/magi — document.less, a MAGI deliberation-screen
//    generator: cross-checked the knocked-dark-label-on-plate convention and
//    the pill stroke-to-height ratio.
//  * fontsinuse.com/uses/28760 — NERV control panels are Helvetica /
//    Helvetica Condensed. This plate is entirely Latin, so: Helvetica Bold,
//    all caps, condensed to the measured widths. The idiosyncratic English is
//    reproduced exactly ("1st. DEFENSE LINE" with the period, "U.S.A" without).
//
// -----------------------------------------------------------------------------
// THE ONE RULE THAT GENERATES THE PLATE, AND THE PROOF THAT IT DOES
//
// Six installations, ONE component, six rotations. The component is a
// T-trefoil: a 343x176 bar over a 128x104 stem, three 88x150 black cells
// (1 Melchior, 2 Balthasar, 3 Casper) and a two-line MAGI 0n knocked dark into
// the plate. Every outer site is that trefoil rotated by
//
//     theta = snap45( bearing(site -> hub) - 90 deg )
//
// so its STEM points at Tokyo-3; and Tokyo-3 itself obeys the same rule with
// the hub replaced by the CENTROID of the five attackers, which lands on
// 180 deg — its stem points back up at the network. Get one sign wrong and it
// still looks plausible, so setup() ASSERTS it: for each site it rotates the
// local stem vector (0,+1) by theta, takes atan2 to the target, and requires
// (a) the declared rotation to equal the snapped bearing and (b) the stem's
// world direction to lie within the 22.5 deg snap half-window. The table
// prints to stdout on every load; ANY failure paints a magenta banner with the
// failing rows across the frame, so a broken construction cannot ship quietly.
//
// TYPE DOES NOT ROTATE. The plate geometry turns; every numeral and every
// MAGI 0n stays upright (counter-rotated by -theta), exactly as the cel does.
//
// -----------------------------------------------------------------------------
// WHAT THE FRAME SAYS ABOUT THE PLATE'S GEOMETRY
//
//  1. THERE IS NO SINGLE DIAGONAL. Least-squares over six rows of the outer
//     wall's left edge (642 at y=660 down to 494 at y=860) gives dx:dy = 0.7386
//     in the FRAME, 53.6 deg from horizontal. Nowhere near 45. But the left V
//     chevron's OUTER arm fits 0.6955 (55.2 deg) and its INNER arm 0.5346
//     (61.9 deg), and the inner chevron's shoulder 1.1875 (40.1 deg). Four
//     fits, each mirrored to within 2 px on the other side of the plate — so
//     the plate carries three or four deliberate diagonals, not one, and no
//     single angle serves the whole construction. (A fit that includes a point
//     inside the bend reads 0.7004; the wall's honest ratio is 0.7386, and the
//     sketch authors 0.7526 so the camera roll lands it on 0.7386.)
//  2. THE COLOUR FIELD IS VERTICAL, NOT RADIAL. A tactical plate converging on
//     one city invites a radial ramp centred on Tokyo-3, and this is not one:
//     the main barrier band is a uniform #7EB42B across its whole 570 px span,
//     while a radial field would redden its ends. Sampled down the plate the
//     ramp is a pure function of y (#217642 at y=20 -> #7EB42B at 480 ->
//     #A19732 at 700 -> #A55123 at 900 -> #A0250F at 1060). See THE SHARED
//     COLOUR FIELD below for what that buys.
//  3. THE CELLS ARE NEVER CAPTURED. In the reference every cell of every site
//     is black. The capture state is carried by the PLATE: five red hostiles,
//     one cyan friendly. So the falls animate the plate, rim and label colour,
//     not the cells — and by t=2.3 s the plate equals the reference exactly.
//  4. THE FRAME IS NOT SQUARE. The top support band's bottom edge runs y=15 at
//     x=400 and y=7 at x=1400; the main barrier band 471 -> 468 over the same
//     span. That is a -0.45 deg roll on a photographed CRT, applied here as
//     one camera transform over square-authored geometry.
//
// -----------------------------------------------------------------------------
// THE TUBE
//
// Geometry stays hard-edged. Every mark — a site, a pill — stands on a bake
// of its own that carries the phosphor bloom: thresholded once, its bright
// pass spread to three radii with the hue turning down the decay tail, the
// way a phosphor's halo runs from yellow through orange to red and from blue
// through cyan to green. The bake is the size of the mark plus the halo's
// reach, so a plate that is mostly empty pays for its marks and not for its
// canvas; the halo is paid once per change, not once per frame.
//
// The ribbons glow too, and their glow is the one thing the bloom cannot be
// asked for cheaply: the hue field climbs the plate for fourteen seconds, so
// anything bloomed over the ribbons would be re-bloomed every frame. Their
// halo is a MASK — the funnel's silhouette feathered once in setup, the
// silhouette itself cut back out — coloured by the same field, and the field
// pans under both: one image sample per pixel, no kernel.
//
// -----------------------------------------------------------------------------
// THE SHARED COLOUR FIELD, AND WHY IT NEEDS NO WORLD-SPACE MATERIAL
//
// Sixteen ribbons must sample ONE continuous field. Paint::linear is
// node-local pixels and radialUnit is the node's unit square; neither spans
// siblings. The answer here is better than per-node endpoint arithmetic: the
// entire funnel is ONE node the size of the canvas whose outline() is the
// stroked union of every ribbon polyline, so the node's local space IS canvas
// space and one Paint::linear({0,0},{0,1080}) serves all of it in a single
// draw. That works because the field is VERTICAL and every ribbon is
// absolutely placed; the moment a ribbon needs its own transform, or the field
// is radial about a moving centre, this construction stops being available and
// the ribbons need a field that spans siblings.
//
// -----------------------------------------------------------------------------
// BUILT FROM (the library, not by hand)
//   Element::rotate + counter-rotate   six sites, one component, six thetas
//   Element::outline                   the T-trefoil silhouette; a second
//                                      generator with PER-CORNER CHAMFER FLAGS
//                                      for the two FINAL DEFENSE ZONE pills
//   skpathutils::FillPathWithPaint     44 px mitred ribbons from polylines,
//                                      vertices pre-placed on measured coords
//                                      (no router search to notch the bends)
//   Paint::image + offset(&front)      the hue field: twenty stops baked once
//                                      into a strip, panned by a bound
//                                      Output — the front advances with no
//                                      re-describe at all
//   Composer::renderSlot               the funnel in a slot of its own, so
//                                      a fall's re-describe never reaches it
//   Effect::phosphorBloom              the spectral bright-pass, hue turning
//                                      down its tail, baked with each mark
//   Cache::Texture, PER MARK           one bake per mark, sized to the mark
//                                      plus its halo, not one bake of a
//                                      mostly-empty canvas
//   ctx.measure()                      every label's point size is SOLVED from
//                                      the width measured off the reference
//   feed::TextRing                     the rotation audit, printed as it runs
//
// -----------------------------------------------------------------------------
// Run:
//   ./build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
//       src/sketch/sketches/eva_magi_defense.cpp \
//       --frame /tmp/eva_magi_defense.png
//
//   2.5 s  THE REFERENCE MOMENT: all five outer MAGI have fallen, the front
//          has not moved, and the frame diffs against the plate directly
//   0.0 s  the network cold — six cyan installations, nothing taken
//   0.3 -> 2.3 s  Beijing, Berlin, Massachusetts, Hamburg, Matsushiro fall,
//          in the order the script names them, 180 ms snap 450 ms apart
//   3.0 -> 17 s   the front advances: the whole hue field climbs the plate
//
// EDIT THESE FIRST
//   kRoll    — the photographed tube is not square, and every measured
//              coordinate carries this roll. Zero it and the plate stands
//              upright, and stops agreeing with the frame.
//   kDiag    — the funnel wall's measured dx:dy. The trunks are authored
//              square and the camera puts the roll back, so this is the
//              ratio the drawn wall lands on rather than the one typed.
//   kSites   — the six installations: where each stands, which way its
//              stem points, and the second it falls.
//   the front's 3 s / 14 s window in update() — when the hue field starts
//              climbing and how long it takes.
// =============================================================================

#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathUtils.h>
#include <include/core/SkSurface.h>
#include <include/core/SkTypeface.h>
#include <shared/EvangelionUi.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/core/Feed.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/kit/Strokes.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/path/Edges.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/bind/Bind.h>
#include <sigilmotion/values/Keyframes.h>
#include <sigilmotion/values/Time.h>
#include <sigilmotion/values/Transition.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace mskia = sigil::material::skia;
namespace motion = sigil::motion;
namespace path = sigil::geometry::path;
namespace shapes = sigil::geometry::shapes;
namespace weave = sigil::weave;

using namespace sigil::compose;
using namespace std::chrono_literals;
namespace ch = choreograph;

namespace eva {

// ---------------------------------------------------------------------------
// THE FRAME. 1920x1080, the reference's own size, so the capture diffs.

constexpr float kW = 1920.0f, kH = 1080.0f;
constexpr SkPoint kHub{964.4f, 935.6f};  // MAGI 01's bbox centre, un-rolled
constexpr float kAxis = 971.3f;          // the axis of bilateral symmetry:
                                         // the barrier band's un-rolled span
                                         // 686.4..1256.4 and the two trunks
                                         // 708.7/1233.8 both give 971.3
constexpr float kDiag = 0.7386f;         // dx:dy the funnel wall MEASURES in
                                         // the frame — least-squares over six
                                         // rows; asserted in runAudit()
// The outer wall's two centreline vertices, authored PRE-roll and shared by
// the path builder and the audit so the published angle cannot drift from the
// geometry that draws it.
constexpr SkPoint kWallTop{708.7f, 606.0f};
constexpr SkPoint kWallBend{511.9f, 867.5f};
constexpr float kBand = 45.0f;   // ribbon width, measured 44-46
constexpr float kRoll = -0.45f;  // the photographed CRT is not square

inline float mirrorX(float x) { return 2.0f * kAxis - x; }
inline SkPoint mirrorP(SkPoint p) { return {mirrorX(p.fX), p.fY}; }

/** Measured -> authored. Every coordinate in this file was read off a frame
 *  that carries the tube's -0.45 deg roll, and the camera puts that roll BACK
 *  at draw time — so a measurement used raw lands twice-rolled. unroll() is
 *  the inverse rotation about the same pivot, applied to point measurements
 *  (site and label centres). The funnel's polylines are not unrolled
 *  point-by-point: they are authored SQUARE from un-rolled anchors, which is
 *  the whole reason the camera transform is worth having — a vertical trunk
 *  stays one number and comes out with the frame's own 3.5 px drift. */
inline SkPoint unroll(SkPoint m) {
  constexpr float k = 0.007854f;  // tan(0.45 deg)
  const float dx = m.fX - kW * 0.5f, dy = m.fY - kH * 0.5f;
  return {m.fX - dy * k, m.fY + dx * k};
}

// ---------------------------------------------------------------------------
// PALETTE. Percentiles over the actual frame, classified by HSV.

const SkColor4f kGround = hex(0x050A01);   // 51% of the frame; green-cast black
const SkColor4f kHostile = hex(0xEE2C26);  // a captured MAGI (measured core)
const SkColor4f kFriendly = hex(0x8BF0FE);  // MAGI 01, and every site pre-fall
const SkColor4f kRim = hex(0xFF9456);       // 2 px core, blooms
const SkColor4f kRimFriendly = hex(0xD6FBEA);
const SkColor4f kNumeral = hex(0xFDA114);  // yellower and hotter than the rims
const SkColor4f kInkHostile = hex(0x990000);  // knocked DARK into the plate
const SkColor4f kInkFriendly = hex(0x29985E);
const SkColor4f kAlarm = hex(0xFF4740);  // COLLAPSING — pure red, never orange
const SkColor4f kCell = hex(0x060200);   // the cells are not quite black

/** THE FIELD, sampled down the reference plate.
 *
 *  Twenty medians of a 17x7 patch taken INSIDE a ribbon at twenty heights,
 *  following the trunk to the barrier and then the wall down to the rail.
 *  Read it as a hue sweep at near-constant luminance: 150 deg at the top,
 *  through 78 deg at the barrier and 35 deg at the flank, to 5 deg at the
 *  rail. Nothing here is chosen; the only editorial act is stopping at
 *  twenty. */
struct RampStop {
  float t;
  uint32_t rgb;
};
constexpr RampStop kRamp[] = {
    {0.000f, 0x2D964F}, {0.028f, 0x2C954D}, {0.074f, 0x2B933A},
    {0.139f, 0x2D9129}, {0.222f, 0x3C9C25}, {0.306f, 0x50A225},
    {0.389f, 0x65AB28}, {0.456f, 0x84B328}, {0.519f, 0x8FAF2B},
    {0.556f, 0x9FAD30}, {0.602f, 0xAEA834}, {0.639f, 0xACA337},
    {0.667f, 0xA69431}, {0.704f, 0xA2892E}, {0.741f, 0xA6812E},
    {0.778f, 0xA66E2B}, {0.815f, 0xAC5E28}, {0.852f, 0xB04E21},
    {0.889f, 0xB73C1B}, {0.926f, 0xB82B15}, {0.980f, 0xB41B14},
    {1.000f, 0xB41B14},
};
constexpr int kRampN = (int)(sizeof(kRamp) / sizeof(kRamp[0]));

// ---------------------------------------------------------------------------
// TYPE. One condensed grotesque, with sizes selected by semantic role.

inline const sk_sp<SkTypeface>& boldFace() {
  return evangelion::condensedBold();
}

// The terminal's one register, over the library's designated-init `type()`:
// every mark on this plate is the same bold grotesque, condensed.
inline weave::TextStyle type(float size, SkColor4f color, float condense = 1.0f,
                             float track = 0.0f) {
  return evangelion::type(boldFace(), size, color, condense, track);
}

// ---------------------------------------------------------------------------
// EMISSIVE MARKS. The strokes and glyphs remain crisp here. The completed
// display is thresholded and bloomed once in describe(), after its routing,
// overlap and clipping have been resolved.
inline LayeredBrush rimStroke(float core, SkColor4f c) {
  return LayeredBrush{{
      {core, c, 0.0f, {}, 0, SkBlendMode::kSrcOver, false},
  }};
}

inline Element displayText(std::u8string s, float size, SkColor4f c,
                           float condense = 1.0f) {
  return text(std::move(s), type(size, c, condense));
}

// ---------------------------------------------------------------------------
// RIBBONS. A polyline stroked to a mitred outline — every vertex pre-placed on
// a measured coordinate, because a router that searches its own corners
// notches a 45 px band at a 55 deg bend.

inline SkPath ribbon(const std::vector<SkPoint>& pts, float width) {
  if (pts.size() < 2) return SkPath();
  SkPathBuilder b;
  b.moveTo(pts.front());
  for (size_t i = 1; i < pts.size(); ++i) b.lineTo(pts[i]);
  SkPaint p;
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeWidth(width);
  p.setStrokeJoin(SkPaint::kMiter_Join);
  p.setStrokeMiter(24.0f);
  p.setStrokeCap(SkPaint::kButt_Cap);
  return skpathutils::FillPathWithPaint(b.detach(), p);
}

/** The whole funnel as ONE path, in canvas coordinates. Nested chevrons
 *  converging on the hub; no rectangle anywhere in it. */
inline SkPath funnelPath() {
  SkPath out;
  std::vector<SkPath> parts;
  auto add = [&](const std::vector<SkPoint>& pts, float w) {
    parts.push_back(ribbon(pts, w));
  };
  auto pair = [&](const std::vector<SkPoint>& pts, float w) {
    add(pts, w);
    std::vector<SkPoint> m;
    m.reserve(pts.size());
    for (SkPoint p : pts) m.push_back(mirrorP(p));
    add(m, w);
  };

  // 1. the support band along the very top edge, cut off by the frame
  add({{354, -12}, {mirrorX(354), -12}}, 44);

  // 2. the two SUPPORT LINE chevrons. Fitted through seven rows each, then
  //    un-rolled: OUTER arm 0.6858 : 1, INNER arm 0.5402 : 1. They are not
  //    the same angle and they are not the wall's angle either — three
  //    diagonals on one plate, each mirrored exactly left/right, which is
  //    what says they were drawn rather than derived.
  pair({{-71.9f, -100.0f}, {209.4f, 310.3f}, {431.1f, -100.0f}}, 24);

  // 3. trunk -> outer wall -> the chamfered bottom rail.
  // Authored PRE-roll: the camera takes 0.45 deg back out, so a wall that
  // must MEASURE 0.7386 in the frame is cut at 0.7526 here.
  pair({{kWallTop.fX, -100},
        kWallTop,
        kWallBend,
        {kWallBend.fX, 1011},
        {466, 1054},
        {-80, 1054}},
       kBand);

  // 4. the MAIN BARRIER band: a flat-topped chevron between the trunks
  add({{686.4f, 492}, {mirrorX(686.4f), 492}}, kBand);

  // 5. the inner chevron — the flat-bottomed trapezoid hanging under it.
  //    Shoulder 1.1875 : 1 from the trunk junction to the roof corner.
  add({{721.5f, 605},
       {839.7f, 705},
       {mirrorX(839.7f), 705},
       {mirrorX(721.5f), 605}},
      kBand);

  // 6. from each roof corner, the wall that drops to the frame edge
  pair({{839.7f, 705}, {686.9f, 906}, {686.9f, 1120}}, kBand);
  // (the 37 px ribbon 6 px outboard of it is NOT a ribbon: a cross-section at
  //  y=1000 reads solid #BE2B16 to x=713, six px of #480000, then a bright
  //  rim and a DARK interior carrying bright type — it is a long unfilled
  //  pill. It is built with the labels, which is the doubled rule.)

  SkPathBuilder joined;
  joined.setFillType(SkPathFillType::kWinding);  // overlapping bands union
  for (const SkPath& p : parts) joined.addPath(p);
  out = joined.detach();
  return out;
}

// ---------------------------------------------------------------------------
// THE T-TREFOIL. One component; every site is this, rotated.

namespace tre {
inline constexpr evangelion::MagiModule kModule{};
}  // namespace tre

struct Site {
  const char* name;  // "01" .. "06"
  SkPoint centre;    // measured bbox centre
  float rotation;    // declared, snapped to 45
  double fallAt;     // seconds; < 0 = never (MAGI 01)
};

// Centres from a colour-masked flood fill of the reference; rotations
// DECLARED here and asserted against atan2 in runAudit().
constexpr Site kSites[] = {
    {"06", {357.5f, 378.0f}, -45.0f, 0.30},  // CHINA / BEIJING
    {"03", {1582.5f, 374.0f}, 45.0f, 0.75},  // GERMANY / BERLIN
    {"04", {193.0f, 816.5f}, -90.0f, 1.20},  // U.S.A / MASSACHUSETTS
    {"05", {1748.5f, 813.5f}, 90.0f, 1.65},  // GERMANY / HAMBURG
    {"02", {966.5f, 297.0f}, 0.0f, 2.10},    // MATSUSHIRO
    {"01", {967.5f, 935.5f}, 180.0f, -1.0},  // TOKYO-3
};
constexpr int kSiteN = (int)(sizeof(kSites) / sizeof(kSites[0]));

// ---------------------------------------------------------------------------
// PILLS. Unfilled: black interior, stroked rim, text inside. The label role
// chooses a stable type register; measuring only shrinks unusually long runs
// enough to fit their capsule.

enum class LabelRole : uint8_t {
  Support,
  Place,
  Country,
  Defense,
  Barrier,
  Hub,
  Zone,
  Flank,
  Side,
  Alarm,
};

struct LabelRegister {
  float size;
  float insetX;
  float insetY;
  float lineGap;
};

inline LabelRegister labelRegister(LabelRole role) {
  switch (role) {
    case LabelRole::Support:
      return {48.0f, 10.0f, 2.0f, 1.0f};
    case LabelRole::Place:
      return {46.0f, 8.0f, 2.0f, 1.0f};
    case LabelRole::Country:
      return {42.0f, 0.0f, 0.0f, 1.0f};
    case LabelRole::Defense:
      return {43.0f, 8.0f, 1.0f, 1.0f};
    case LabelRole::Barrier:
      return {58.0f, 0.0f, 0.0f, 1.0f};
    case LabelRole::Hub:
      return {68.0f, 10.0f, 2.0f, 1.0f};
    case LabelRole::Zone:
      return {38.0f, 10.0f, 5.0f, 1.0f};
    case LabelRole::Flank:
      return {37.0f, 8.0f, 5.0f, 2.0f};
    case LabelRole::Side:
      return {28.0f, 8.0f, 1.0f, 1.0f};
    case LabelRole::Alarm:
      return {44.0f, 10.0f, 2.0f, 1.0f};
  }
  return {34.0f, 8.0f, 2.0f, 1.0f};
}

/** Every label on the plate. `w` and `h` define a layout slot rather than a
 *  target that each string must be stretched to fill. */
struct Label {
  const char* lines[3];
  SkPoint centre;
  float w, h;
  float rotate;
  uint8_t cuts;
  bool pill;
  bool alarm;
  LabelRole role;
};

const Label kLabels[] = {
    // the four SUPPORT LINE pills — two of them bleed off the side edges
    {{"SUPPORT LINE"},
     {522, 45},
     280,
     45,
     0,
     evangelion::CutNone,
     true,
     false,
     LabelRole::Support},
    {{"SUPPORT LINE"},
     {1409, 43},
     280,
     45,
     0,
     evangelion::CutNone,
     true,
     false,
     LabelRole::Support},
    {{"SUPPORT LINE"},
     {34, 358},
     280,
     45,
     0,
     evangelion::CutNone,
     true,
     false,
     LabelRole::Support},
    {{"SUPPORT LINE"},
     {1913, 355},
     280,
     45,
     0,
     evangelion::CutNone,
     true,
     false,
     LabelRole::Support},
    // site names
    {{"MATSUSHIRO"},
     {966, 120},
     242,
     47,
     0,
     evangelion::CutNone,
     true,
     false,
     LabelRole::Place},
    {{"CHINA"},
     {541, 155},
     168,
     52,
     0,
     evangelion::CutNone,
     false,
     false,
     LabelRole::Country},
    {{"BEIJING"},
     {541, 207},
     168,
     44,
     0,
     evangelion::CutNone,
     true,
     false,
     LabelRole::Place},
    {{"GERMANY"},
     {1377, 171},
     154,
     52,
     0,
     evangelion::CutNone,
     false,
     false,
     LabelRole::Country},
    {{"BERLIN"},
     {1377, 223},
     154,
     44,
     0,
     evangelion::CutNone,
     true,
     false,
     LabelRole::Place},
    {{"U.S.A"},
     {210, 563},
     331,
     52,
     0,
     evangelion::CutNone,
     false,
     false,
     LabelRole::Country},
    {{"MASSACHUSETTS"},
     {210, 610},
     331,
     44,
     0,
     evangelion::CutNone,
     true,
     false,
     LabelRole::Place},
    {{"GERMANY"},
     {1849, 560},
     190,
     52,
     0,
     evangelion::CutNone,
     false,
     false,
     LabelRole::Country},
    {{"HAMBURG"},
     {1849, 610},
     190,
     44,
     0,
     evangelion::CutNone,
     true,
     false,
     LabelRole::Place},
    // the defense lines
    {{"1st. DEFENSE LINE"},
     {972, 543},
     318,
     40,
     0,
     evangelion::CutNone,
     true,
     false,
     LabelRole::Defense},
    {{"MAIN BARRIER"},
     {973, 599},
     385,
     64,
     0,
     evangelion::CutNone,
     false,
     false,
     LabelRole::Barrier},
    {{"2nd. DEFENSE LINE"},
     {979, 655},
     316,
     40,
     0,
     evangelion::CutNone,
     true,
     false,
     LabelRole::Defense},
    {{"3rd. DEFENSE LINE"},
     {668, 757},
     222,
     38,
     -55,
     evangelion::CutNone,
     true,
     false,
     LabelRole::Defense},
    {{"3rd. DEFENSE LINE"},
     {1278, 757},
     222,
     38,
     55,
     evangelion::CutNone,
     true,
     false,
     LabelRole::Defense},
    // the hub
    {{"TOKYO-3"},
     {969, 757},
     274,
     57,
     0,
     evangelion::CutNone,
     true,
     false,
     LabelRole::Hub},
    {{"FINAL", "DEFENSE", "ZONE"},
     {834, 842},
     126,
     92,
     0,
     evangelion::CutTopLeft,
     true,
     false,
     LabelRole::Zone},
    {{"FINAL", "DEFENSE", "ZONE"},
     {1104, 842},
     126,
     92,
     0,
     evangelion::CutTopRight,
     true,
     false,
     LabelRole::Zone},
    {{"LEFT", "FLANK"},
     {602, 962},
     114,
     82,
     0,
     evangelion::CutNone,
     true,
     false,
     LabelRole::Flank},
    {{"RIGHT", "FLANK"},
     {1343, 962},
     118,
     82,
     0,
     evangelion::CutNone,
     true,
     false,
     LabelRole::Flank},
    {{"LEFT SIDE BARRIER"},
     {737, 988},
     196,
     33,
     -90,
     evangelion::CutNone,
     true,
     false,
     LabelRole::Side},
    {{"RIGHT SIDE BARRIER"},
     {1203, 988},
     205,
     33,
     -90,
     evangelion::CutNone,
     true,
     false,
     LabelRole::Side},
};
constexpr int kLabelN = (int)(sizeof(kLabels) / sizeof(kLabels[0]));

// the two COLLAPSING pills blink, so they are their own (volatile) layer
const Label kCollapsing[] = {
    {{"COLLAPSING"},
     {528, 617},
     214,
     46,
     0,
     evangelion::CutNone,
     true,
     true,
     LabelRole::Alarm},
    {{"COLLAPSING"},
     {1416, 617},
     214,
     46,
     0,
     evangelion::CutNone,
     true,
     true,
     LabelRole::Alarm},
};

// ---------------------------------------------------------------------------
// THE FRONT. The hue field is a function of y alone, and the front advancing
// is that function sliding up the plate. So the field is baked ONCE into a
// strip — the twenty stops down the canvas's height, then the last stop held
// for as far as the front travels — and the front is a bound pan on the
// strip's material: a whole-pixel translate of an image, which re-describes
// nothing and re-rasterizes nothing. A ramp with the front as a bound uniform
// would be live per pixel; a ramp re-described per step would dirty every
// recording above it. The pan is neither.

constexpr float kFrontTravel = 0.42f;  // of kH: how far the field climbs
// The pan moves in steps of this many px: each step remakes the funnel's
// bake and the halo's, and between steps both blit. Six px of a smooth
// ramp is below what the eye reads as a step.
constexpr float kFrontStep = 6.0f;

/** A stop's colour with its hue turned by @p degrees, saturation and value
 *  kept. Negative is the phosphor direction: yellow toward red, blue toward
 *  green. */
inline uint32_t turnHue(uint32_t rgb, float degrees) {
  float hsv[3];
  SkRGBToHSV((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF, hsv);
  hsv[0] = std::fmod(hsv[0] + degrees + 720.0f, 360.0f);
  const SkColor c = SkHSVToColor(hsv);
  return (uint32_t)(c & 0x00FFFFFF);
}

/** The field as a strip: row y is the ramp at y / kH, clamped to the last
 *  stop past the canvas, so a pan of up to kFrontTravel * kH never runs off
 *  the image. Four pixels wide and repeated across the plate. */
inline sk_sp<SkImage> fieldStrip(float hueTurn) {
  const int rows = (int)std::ceil(kH * (1.0f + kFrontTravel));
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(4, rows));
  if (!surface) return nullptr;
  // The same gradient the ribbons were once filled with directly, so a
  // row of the strip is the row that gradient painted.
  std::vector<mskia::Stop> stops;
  stops.reserve((size_t)kRampN);
  for (const auto& stop : kRamp)
    stops.push_back({stop.t, hex(turnHue(stop.rgb, hueTurn))});
  SkPaint paint;
  paint.setShader(
      mskia::Paint::linear({0, 0}, {0, kH}, std::move(stops)).asShader());
  surface->getCanvas()->drawPaint(paint);
  return surface->makeImageSnapshot();
}

// ---------------------------------------------------------------------------
// THE BLOOM. Baked with each mark, so its cost is paid when a mark changes
// and never per frame. The halo's reach is the node's box, so a mark's bake
// is inset by kHaloReach on every side.

constexpr float kBloomRadius = 14.0f;
constexpr float kBloomHueDrift = -38.0f;  // degrees at the outer radius
constexpr float kHaloReach = kBloomRadius * 2.0f + 8.0f;

inline mskia::Effect tubeBloom() {
  return mskia::Effect::phosphorBloom(kBloomRadius, 0.52f, 0.78f, 0.86f,
                                      kBloomHueDrift, 0.35f);
}

/** The ribbons' halo mask: the funnel's silhouette feathered twice — a
 *  tight pass and a wide one, the tail — with the silhouette itself cut
 *  back out, so the glow stands beside the ribbons and never over them. */
constexpr float kRibbonHaloNear = 6.0f;
constexpr float kRibbonHaloFar = 18.0f;
constexpr float kRibbonHueTurn = -22.0f;  // the tail's hue, one turn for all

inline sk_sp<SkImage> ribbonHaloMask(const SkPath& funnel) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul((int)kW, (int)kH));
  if (!surface) return nullptr;
  SkCanvas* canvas = surface->getCanvas();
  canvas->clear(SK_ColorTRANSPARENT);
  const auto feather = [&](float sigma, float alpha) {
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor4f({1, 1, 1, alpha});
    paint.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, sigma));
    canvas->drawPath(funnel, paint);
  };
  feather(kRibbonHaloNear, 0.55f);
  feather(kRibbonHaloFar, 0.50f);
  SkPaint cut;
  cut.setAntiAlias(true);
  cut.setBlendMode(SkBlendMode::kDstOut);
  canvas->drawPath(funnel, cut);
  return surface->makeImageSnapshot();
}

/** The axis-aligned box a w x h mark turned by @p degrees about its centre
 *  stands in — what its bake must cover. */
inline SkRect turnedBounds(SkPoint centre, float w, float h, float degrees) {
  const float a = degrees * 0.01745329252f;
  const float c = std::fabs(std::cos(a)), sn = std::fabs(std::sin(a));
  const float hw = (w * c + h * sn) * 0.5f, hh = (w * sn + h * c) * 0.5f;
  return SkRect::MakeLTRB(centre.fX - hw, centre.fY - hh, centre.fX + hw,
                          centre.fY + hh);
}

// ---------------------------------------------------------------------------
// THE CRT. nerv-ui/components/crt-effects.css, transcribed: 2 px scanlines at
// 4% black, a 70%/70% vignette ellipse reaching 40%. Baked once into a texture
// and crept by a bound translateY — no per-frame shader anywhere.

/** THE SILHOUETTES AS COMPARABLE VALUES. A raw outline callable compares
 *  equal to nothing, so a node carrying one is patched on every describe
 *  and its bake — here a bloom — is remade with it. A scheme with an
 *  equality prunes. */
struct SiteSilhouette {
  bool operator==(const SiteSilhouette&) const = default;
  SkPath path(SkSize s) const { return tre::kModule.outline()(s); }
};
struct PillSilhouette {
  float radius = 10.0f;
  SkVector cut{26.0f, 26.0f};
  uint8_t cutMask = evangelion::CutNone;
  bool operator==(const PillSilhouette& o) const {
    return radius == o.radius && cut == o.cut && cutMask == o.cutMask;
  }
  SkPath path(SkSize s) const {
    return evangelion::panel(
        {.radius = radius, .cut = cut, .cutMask = cutMask})(s);
  }
};

inline float wrap180(float d) {
  while (d > 180.0f) d -= 360.0f;
  while (d <= -180.0f) d += 360.0f;
  return d;
}
inline float snap45(float deg) { return std::round(deg / 45.0f) * 45.0f; }
inline float deg(float rad) { return rad * 57.29577951f; }
inline float rad(float d) { return d * 0.01745329252f; }

}  // namespace eva

// =============================================================================

struct EvaMagiDefense : sketch::Sketch {
  using Sketch::Sketch;

  ch::Output<float> creep{0.0f};    // scanline creep
  ch::Output<float> flicker{0.0f};  // phosphor dip (alpha of a black plane)
  ch::Output<float> blink{1.0f};    // COLLAPSING, hard on/off

  // A FALL IS A BOUND OPACITY. Every site carries both its states as two
  // bakes from the first frame, the hostile one over the friendly at this
  // rest alpha — drawn, so it is baked at load rather than on the frame
  // the site falls — and the fall is the ticker easing that alpha to one.
  // Nothing re-describes for a fall, and no bloom is ever painted live.
  static constexpr float kFallRest = 1.0f / 255.0f;
  ch::Output<float> fallAlpha[eva::kSiteN] = {{kFallRest}, {kFallRest},
                                              {kFallRest}, {kFallRest},
                                              {kFallRest}, {kFallRest}};
  // The front: the field's pan in whole px, negative as it climbs. Bound on
  // the funnel's material and on the ribbons' halo, so nothing re-describes.
  ch::Output<float> front{0.0f};
  sk_sp<SkImage> fieldStrip, haloStrip, ribbonHalo;  // baked once in setup
  std::vector<float> labelSize;  // role size, reduced only when it must fit
  std::vector<float> siteNameSize;
  float numeralSize = 96.0f;
  sigil::compose::feed::TextRing audit{48};
  std::vector<std::u8string> failures;
  SkPath funnel;

  // --- the construction rule, asserted ---------------------------------------
  void runAudit() {
    using namespace eva;
    failures.clear();
    // MAGI 01's target is the CENTROID of the five attackers; everyone else's
    // is the hub. Same rule, one substitution.
    SkPoint centroid{0, 0};
    for (const Site& s : kSites)
      if (s.fallAt >= 0) {
        centroid.fX += unroll(s.centre).fX / 5.0f;
        centroid.fY += unroll(s.centre).fY / 5.0f;
      }
    audit.append({u8"ROTATION RULE  theta = snap45(bearing(site->target) - 90)",
                  "heading"});
    char line[160];
    std::printf("\n  MAGI defense plate — rotation audit\n");
    std::printf(
        "  site   centre        target        bearing   want    "
        "declared  stem_dir  err\n");
    for (const Site& s : kSites) {
      const bool hub = s.fallAt >= 0;
      const SkPoint tgt = hub ? kHub : centroid;
      const SkPoint at = unroll(s.centre);
      const float bearing = deg(std::atan2(tgt.fY - at.fY, tgt.fX - at.fX));
      const float want = snap45(bearing - 90.0f);
      // the component's stem runs local +y; rotate it by the declared theta
      const float th = rad(s.rotation);
      const SkVector stem{-std::sin(th), std::cos(th)};
      const float stemDeg = deg(std::atan2(stem.fY, stem.fX));
      const float err = std::fabs(wrap180(stemDeg - bearing));
      const bool ok =
          std::fabs(wrap180(want - s.rotation)) < 0.5f && err < 22.5f;
      std::snprintf(line, sizeof(line),
                    "  MAGI %s (%4.0f,%4.0f)  (%4.0f,%4.0f)  %7.2f  %+5.0f  "
                    "%+7.0f  %+7.1f  %5.2f  %s",
                    s.name, (double)at.fX, (double)at.fY, (double)tgt.fX,
                    (double)tgt.fY, (double)bearing, (double)want,
                    (double)s.rotation, (double)stemDeg, (double)err,
                    ok ? "PASS" : "*** FAIL ***");
      std::printf("%s\n", line);
      audit.append({toU8(line + 2), ok ? "pass" : "fail"});
      if (!ok) failures.push_back(toU8(line + 2));
    }
    // ...and the plate's other published number: the wall angle. The
    // polyline is authored pre-roll, so the check is "does the CAMERA put it
    // back on the 0.7386 the frame measures", which is what makes the roll
    // an honest transform rather than a decoration.
    {
      const float ax = kWallTop.fX - kWallBend.fX;  // authored run (leftward)
      const float ay = kWallBend.fY - kWallTop.fY;  // authored rise
      const float c = std::cos(rad(kRoll)), sn = std::sin(rad(kRoll));
      const float rx = -ax * c - ay * sn;  // rotated by the camera
      const float ry = -ax * sn + ay * c;
      const float rendered = std::fabs(rx / ry);
      const bool ok = std::fabs(rendered - kDiag) < 0.01f;
      std::snprintf(line, sizeof(line),
                    "  WALL  authored %.4f  + roll %.2f deg -> %.4f   "
                    "frame measures %.4f  %s",
                    (double)(ax / ay), (double)kRoll, (double)rendered,
                    (double)kDiag, ok ? "PASS" : "*** FAIL ***");
      std::printf("%s\n", line);
      audit.append({toU8(line + 2), ok ? "pass" : "fail"});
      if (!ok) failures.push_back(toU8(line + 2));
    }
    std::printf(
        "  %d/%d sites obey the rule; stem half-window is 22.5 deg.\n\n",
        kSiteN - (int)failures.size(), kSiteN);
  }

  // --- one installation ------------------------------------------------------
  /** The site in one STATE, placed in canvas coordinates less @p origin —
   *  the top-left of the bake it stands on. A fall is not a change to this
   *  node: the hostile state is a second bake crossfaded over the friendly
   *  one, so neither is ever painted live with its bloom. */
  Element installation(int index, SkPoint origin, bool friendly) const {
    using namespace eva;
    const Site& s = kSites[index];
    const SkColor4f plateFill = friendly ? kFriendly : kHostile;
    const SkColor4f rim = friendly ? kRimFriendly : kRim;
    const SkColor4f ink = friendly ? kInkFriendly : kInkHostile;
    const auto& module = tre::kModule;

    const SkPoint at = unroll(s.centre) - origin;
    auto plate = box()
                     .left(at.fX - module.barWidth * 0.5f)
                     .top(at.fY - module.totalHeight() * 0.5f)
                     .width(module.barWidth)
                     .height(module.totalHeight())
                     .shape(SiteSilhouette{})
                     .rotate(s.rotation)
                     .fill(Fill::color(plateFill))
                     .foreground(rimStroke(2.4f, rim))
                     .key(std::string("site#") + s.name +
                          (friendly ? "" : "#fallen"));

    // three cells: black, hard orange keyline, and the keyline blooms
    for (int n : {1, 2, 3}) {
      const SkRect r = module.cell(n);
      auto cell = box()
                      .left(r.left())
                      .top(r.top())
                      .width(r.width())
                      .height(r.height())
                      .corners({module.cellRadius})
                      .fill(Fill::color(kCell))
                      .foreground(rimStroke(3.2f, rim));
      // The numeral is centred in the cell's TOP SQUARE, not in the cell:
      // measured, the glyph's centre sits at 30% of a 150 px cell, which is
      // 44 px — the middle of the 88 px width. And it stays UPRIGHT while
      // the plate turns.
      cell.child(displayText(toU8(std::string(1, (char)('0' + n))), numeralSize,
                             kNumeral, 0.88f)
                     .centerAt({r.width() * 0.5f, r.width() * 0.5f + 3})
                     .rotate(-s.rotation));
      plate.child(std::move(cell));
    }

    // MAGI 0n knocked dark into the plate: two lines, two SIZES (cap 26 over
    // cap 36, measured), upright, no glow — it is a hole, not a light.
    //
    // THE TWO LINES ARE NOT CONDENSED THE SAME. Measured off MAGI 02 in the
    // reference at cap height 26.6: the word is 83 px wide and "02" is 61.
    // "0n" at 50/0.95 renders 62.5 — right. "MAGI" at 36/0.95 renders 92 —
    // 11% wide, and the excess is not cosmetic: the label is centred at local
    // (173.5, 66) and the stem cell's top edge is at local y 110, so on the
    // two sites rotated +-90 (MAGI 04 and 05) the word's HALF-WIDTH is what
    // has to clear that 44 px, upright, in world x. A half-width over 44 px
    // puts MAGI 04's I under cell 2's left edge and MAGI 05's M on it.
    // Helvetica Bold "MAGI" is 2.667 em, so 83 px at 36 pt is scaleX 0.86 — the
    // word is Helvetica CONDENSED on the plate and the numerals are not, which
    // is what fontsinuse lists for the panels.
    plate.child(box()
                    .centerAt(module.labelCentre())
                    .column()
                    .alignItems(Align::Center)
                    .gap(-6)
                    .rotate(-s.rotation)
                    .child(text(u8"MAGI", type(36, ink, 0.86f)))
                    .child(text(toU8(s.name), type(50, ink, 0.95f))));
    return plate;
  }

  // --- a pill ----------------------------------------------------------------
  Element pillOf(const eva::Label& L, float size, int keyIndex,
                 const char* keyTag, SkPoint origin) const {
    using namespace eva;
    const LabelRegister labelStyle = labelRegister(L.role);
    const SkColor4f ink = L.alarm ? kAlarm : kRim;
    const SkPoint at = unroll(L.centre) - origin;
    auto node = box()
                    .left(at.fX - L.w * 0.5f)
                    .top(at.fY - L.h * 0.5f)
                    .width(L.w)
                    .height(L.h)
                    .rotate(L.rotate)
                    .column()
                    .alignItems(L.role == LabelRole::Country ? Align::Start
                                                             : Align::Center)
                    .justify(Justify::Center)
                    .gap(labelStyle.lineGap)
                    .key(std::string(keyTag) + std::to_string(keyIndex));
    if (L.pill) {
      node.shape(PillSilhouette{.cutMask = L.cuts});
      node.fill(Fill::color(kCell));
      node.foreground(rimStroke(3.0f, ink));
    }
    for (const char* line : L.lines)
      if (line) node.child(displayText(toU8(line), size, ink, 0.94f));
    return node;
  }

  // --- the layers ------------------------------------------------------------
  /** The field's strip as a material, panned by the front. Nearest sampling
   *  and a whole-pixel pan: a row of the strip IS a row of the plate. */
  mskia::Paint field(const sk_sp<SkImage>& strip) const {
    mskia::Paint m = mskia::Paint::image(
        strip, SkTileMode::kRepeat, SkTileMode::kClamp, SkMatrix::I(),
        SkSamplingOptions(SkFilterMode::kNearest));
    m.offset(std::nullopt, &front);
    return m;
  }

  Element funnelLayer() const {
    using namespace eva;
    // Explicit size, not inset(0): the slot's own node has no dimensions to
    // stretch against, so an absolute child of it lays out 1920x0 — harmless
    // for an outline in absolute coordinates, and a lie in every query.
    // A bake, remade on each step of the pan and blitted between steps: a
    // recording would re-fill the funnel every frame.
    return box()
        .width(kW)
        .height(kH)
        .shape([this](SkSize) { return funnel; })
        .fill(field(fieldStrip))
        .cache(Cache::Texture)
        .key("funnel");
  }

  /** The ribbons' halo: the feathered silhouette, coloured by the field's
   *  tail hue and panned with it. Laid over the ground with the plain
   *  blend — the mask cuts the ribbons out of it and the marks are drawn
   *  above it, so nothing it lands on is brighter than the halo itself —
   *  and baked at half resolution: a feathered field loses nothing to the
   *  upscale, and the bake is remade on every step of the pan. */
  Element ribbonGlow() const {
    using namespace eva;
    return box()
        .width(kW)
        .height(kH)
        .fill(mskia::Paint::blend(
            {{mskia::Paint::image(ribbonHalo), SkBlendMode::kSrc},
             {field(haloStrip), SkBlendMode::kSrcIn}}))
        .cache(Cache::Texture)
        .bakeScale(0.5f)
        .key("ribbonglow");
  }

  /** A MARK ON ITS OWN BAKE, with the bloom baked into it. The bake is the
   *  mark's turned box grown by the halo's reach — a layer effect reaches
   *  no further than the node's own box — so the halo is complete and the
   *  bake is the size of the mark, not of the canvas. The blit is exact at
   *  any angle: a still mark bakes on the device grid, the plate's roll
   *  included, and the recordings above it are pinned to that grid. */
  Element glowing(SkPoint centre, float w, float h, float degrees,
                  const std::string& key,
                  const std::function<Element(SkPoint origin)>& mark) const {
    using namespace eva;
    const SkRect bounds = turnedBounds(centre, w, h, degrees);
    const SkPoint origin{bounds.left() - kHaloReach, bounds.top() - kHaloReach};
    return box()
        .left(origin.fX)
        .top(origin.fY)
        .width(bounds.width() + 2.0f * kHaloReach)
        .height(bounds.height() + 2.0f * kHaloReach)
        .child(mark(origin))
        .effect(tubeBloom())
        .cache(Cache::Texture)
        .key(key);
  }

  Element art() const {
    using namespace eva;
    auto g = box().inset(0);
    // One bake PER MARK, not one over the group: this plate is mostly empty,
    // so bakes the size of the marks cover a fraction of the canvas for the
    // same picture — and a site that falls re-bakes its own halo alone.
    // A fall is a CROSSFADE between two bakes — the friendly state under
    // the hostile, whose opacity the ticker snaps from its rest to one.
    // Opacity rides the blit, so each state's bloom is baked once, at
    // load, and never painted live for a transition.
    const auto& module = tre::kModule;
    for (int i = 0; i < kSiteN; ++i) {
      const Site& s = kSites[i];
      g.child(glowing(unroll(s.centre), module.barWidth, module.totalHeight(),
                      s.rotation, std::string("glow#") + s.name,
                      [&, i](SkPoint origin) {
                        return installation(i, origin, true);
                      }));
      if (s.fallAt >= 0)
        g.child(glowing(unroll(s.centre), module.barWidth,
                        module.totalHeight(), s.rotation,
                        std::string("glow#") + s.name + "#fallen",
                        [&, i](SkPoint origin) {
                          return installation(i, origin, false);
                        })
                    .opacity(&fallAlpha[i]));
    }
    for (int i = 0; i < kLabelN; ++i)
      g.child(glowing(unroll(kLabels[i].centre), kLabels[i].w, kLabels[i].h,
                      kLabels[i].rotate, "glowlab" + std::to_string(i),
                      [&, i](SkPoint origin) {
                        return pillOf(kLabels[i], labelSize[(size_t)i], i,
                                      "lab", origin);
                      }));
    return g;
  }

  /** COLLAPSING blinks, so it is its own (volatile) node — and a TIGHT one:
   *  a full-canvas volatile layer would repaint the whole frame for two
   *  214 px pills. The blink rides the bake's blit; the bloom is inside. */
  Element collapsingLayer(int i) const {
    using namespace eva;
    const Label& L = kCollapsing[i];
    return glowing(unroll(L.centre), L.w, L.h, L.rotate,
                   "glowcol" + std::to_string(i),
                   [&, i](SkPoint origin) {
                     return pillOf(L, siteNameSize[(size_t)i], i, "col",
                                   origin);
                   })
        .opacity(&blink);
  }

  /** THE CAMERA, and its shape is dictated by what a cached blit costs.
   *
   *  The roll goes INSIDE the wrapper (on the child), so a Cache::Texture on
   *  the wrapper bakes the rotation into the pixels and the wrapper blits at
   *  identity. Put the same rotate on the OUTER node and every frame becomes a
   *  rotated resample of a canvas-sized texture.
   *
   *  The pivot is the FRAME CENTRE, not the hub: the coordinates were
   *  measured off an already-rolled frame, and rolling about the centre puts
   *  them back within ~1 px where rolling about the hub leaves the top of
   *  the plate 6 px out. */
  Element describe() {
    using namespace eva;
    auto camera = [](Element e) {
      return box().inset(0).child(
          std::move(e.rotate(kRoll).transformOriginPx({kW * 0.5f, kH * 0.5f})));
    };

    auto root = stack().inset(0);
    auto picture = stack().inset(0);

    // (no ground node: the host clears to ctx.background, and a full-canvas
    //  opaque fill on top of that is pure waste)

    // The ribbons: flat fills of one continuous field, panned by the front.
    // In a SLOT, so a fall's re-describe never reaches the funnel and the
    // funnel's pan never reaches the marks.
    picture.child(camera(slot("funnel")));
    // …their halo, screened over them, and the marks on their own bakes
    // above both — a panel hides the ribbon under it, halo and all.
    picture.child(camera(ribbonGlow()));
    picture.child(camera(art()));
    picture.child(camera(collapsingLayer(0)));
    picture.child(camera(collapsingLayer(1)));
    root.child(std::move(picture).key("phosphor"));

    // the photographed CRT: scanlines + vignette baked once, crept
    mskia::Paint crt =
        mskia::Paint::recipe(sigil::material::field::crtOverlay());
    root.child(box()
                   .left(0)
                   .top(-8)
                   .width(kW)
                   .height(kH + 16)
                   .fill(crt)
                   .translateY(&creep)
                   .cache(Cache::Texture)
                   .key("crt"));
    // phosphor flicker: an alpha-0 plane 99% of the time, so it costs nothing
    root.child(box()
                   .inset(0)
                   .fill(Fill::color({0, 0, 0, 1}))
                   .opacity(&flicker)
                   .key("flicker"));

    if (!failures.empty()) root.child(failureBanner());
    return root;
  }

  /** The rotation check, painted across the plate — and ONLY when it fails.
   *  The reference carries no drafting chrome, so the checks live on stdout in
   *  the normal case; a violated construction rule gets the whole ring dumped
   *  in magenta where nobody can miss it. */
  Element failureBanner() const {
    sigil::compose::feed::TextOptions st;
    st.styles.base(eva::type(23, {0, 0, 0, 1}, 0.95f))
        .set("dim", eva::type(23, {0.25f, 0, 0.25f, 1}, 0.95f))
        .set("heading", eva::type(26, {0, 0, 0, 1}, 0.95f))
        .set("pass", eva::type(23, {0, 0.25f, 0.15f, 1}, 0.95f))
        .set("fail", eva::type(23, {0.6f, 0, 0, 1}, 0.95f));
    st.window.gap = 3.0f;
    st.window.visible = 16;
    return box()
        .left(0)
        .top(300)
        .width(eva::kW)
        .height(150 + 30 * (float)audit.size())
        .fill(Fill::color({1, 0, 1, 0.93f}))
        .column()
        .padding(26)
        .gap(10)
        .child(
            text(u8"ROTATION RULE VIOLATED — this plate is not one component",
                 eva::type(40, {0, 0, 0, 1}, 0.95f)))
        .child(sigil::compose::feed::feed(audit, st));
  }

  // --- host ------------------------------------------------------------------
  void setup(sketch::SketchContext& ctx) override {
    using namespace eva;
    // The plate at exactly 2x. The canvas is the reference frame's own
    // 1920x1080, so halving the capture puts it on the frame directly.
    // The REFERENCE MOMENT this sketch is built to be diffed at: all five
    // outer MAGI fallen (last at 2.28), the hue front not yet moving (3.0).
    // 2.5 s sits inside that hold [2.28, 3.0).
    sketch::kit::stage(ctx, {.size = SkSize::Make(kW, kH),
                             .captureAt = 2.5,
                             .background = kGround,
                             .oversample = 2});

    funnel = funnelPath();
    fieldStrip = eva::fieldStrip(0.0f);
    haloStrip = eva::fieldStrip(eva::kRibbonHueTurn);
    ribbonHalo = eva::ribbonHaloMask(funnel);
    front = 0.0f;
    runAudit();

    // Every semantic role starts on one type register. Measurement only
    // supplies a fit guard for a long name or a multi-line capsule.
    auto solve = [&](const Label& L) {
      const LabelRegister labelStyle = labelRegister(L.role);
      const char* longest = L.lines[0];
      for (const char* l : L.lines)
        if (l && longest && std::strlen(l) > std::strlen(longest)) longest = l;
      float size = labelStyle.size;
      const SkSize run =
          ctx.measure(text(toU8(longest), type(size, kRim, 0.94f)));
      const float availableWidth = L.w - 2.0f * labelStyle.insetX;
      if (run.width() > availableWidth && run.width() > 1.0f)
        size *= availableWidth / run.width();

      const int lines = (L.lines[1] ? (L.lines[2] ? 3 : 2) : 1);
      const SkSize line = ctx.measure(text(u8"Hg", type(size, kRim, 0.94f)));
      const float drawn =
          line.height() * (float)lines + labelStyle.lineGap * (lines - 1);
      const float availableHeight = L.h - 2.0f * labelStyle.insetY;
      if (drawn > availableHeight && drawn > 1.0f)
        size *= availableHeight / drawn;
      return size;
    };
    labelSize.clear();
    for (const auto& label : kLabels) labelSize.push_back(solve(label));
    siteNameSize.clear();
    for (const auto& label : kCollapsing) siteNameSize.push_back(solve(label));
    // The numerals: cap height 61 px, measured off the "3" in MAGI 02's
    // cell 3 (x 1049..1097, y 185..246). measure() returns the LINE box, so
    // solve against a probed cap ratio rather than assuming Helvetica's.
    {
      const SkSize line =
          ctx.measure(text(u8"3", type(100.0f, kNumeral, 0.88f)));
      const float capAt100 =
          line.height() > 1.0f ? line.height() * 0.63f : 71.7f;
      numeralSize = 100.0f * 61.0f / capAt100;
    }

    // --- motion ---
    ctx.ticker.add([this, &ticker = ctx.ticker](double) {
      const double t = ticker.elapsed();
      // scanlines creep one WHOLE PIXEL at a time, 4 px per 8 s: a fractional
      // translate turns the cached CRT texture's blit into a resample.
      creep = (float)(motion::stepIndex(t, 0.5) % 4);
      // phosphor flicker: a 4 s cycle, 1% duty
      const double ph = std::fmod(t, 4.0);
      flicker = ph < 0.04 ? 0.04f : 0.0f;
      // COLLAPSING: hard on/off, 350 on / 250 off (ESTIMATED — a single frame
      // cannot measure a blink, so this rate is not read off the reference)
      blink = std::fmod(t, 0.6) < 0.35 ? 1.0f : 0.0f;
      // The falls: each hostile state snaps in over 180 ms from its second.
      for (int i = 0; i < kSiteN; ++i) {
        const Site& s = kSites[i];
        if (s.fallAt < 0) continue;
        const float u =
            std::clamp((float)((t - s.fallAt) / 0.18), 0.0f, 1.0f);
        fallAlpha[i] = kFallRest + (1.0f - kFallRest) * ch::easeOutQuad(u);
      }
      return true;
    });

    ctx.composer.render(describe());
    ctx.composer.renderSlot("funnel", funnelLayer());
  }

  void update(double elapsed, sketch::SketchContext& ctx) override {
    // The front is a bound pan and never re-describes: derived from
    // `elapsed`, in whole pixels, negative as the field climbs the plate.
    const double sweep = (elapsed - 3.0) / 14.0;
    const double k = sweep <= 0 ? 0.0 : (sweep >= 1 ? 1.0 : sweep);
    const double eased = choreograph::easeInOutQuad((float)k);
    front = -(float)(std::round(eased * eva::kFrontTravel * eva::kH /
                                eva::kFrontStep) *
                     eva::kFrontStep);
    (void)ctx;  // nothing re-describes: the falls are bound alphas too
  }
};

SIGIL_SKETCH(EvaMagiDefense, "Study \xc2\xb7 Film",
             "The End of Evangelion's MAGI plate (1997) \xe2\x80\x94 six "
             "installations are one component, rotated")
