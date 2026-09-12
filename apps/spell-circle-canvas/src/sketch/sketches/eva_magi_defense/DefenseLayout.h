#pragma once

// Site topology, label registers and the defense display's colour field.

#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathUtils.h>
#include <include/core/SkSurface.h>
#include <include/core/SkTypeface.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/kit/Strokes.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/path/Edges.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmeasure/check/Check.h>
#include <sigilmotion/bind/Bind.h>
#include <sigilmotion/schedule/Cascade.h>
#include <sigilmotion/values/Keyframes.h>
#include <sigilmotion/values/Time.h>
#include <sigilmotion/values/Transition.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/kit/Rows.h>
#include <sigilsketch/kit/Theme.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "../eva_magi_interior/EvangelionUi.h"

namespace sketch = sigil::sketch;
namespace mskia = sigil::material::skia;
namespace motion = sigil::motion;
namespace path = sigil::geometry::path;
namespace shapes = sigil::geometry::shapes;
namespace weave = sigil::weave;
namespace measure = sigil::measure;

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

const SkColor4f kGround =
    hexColor(0x050A01);  // 51% of the frame; green-cast black
const SkColor4f kHostile =
    hexColor(0xEE2C26);  // a captured MAGI (measured core)
const SkColor4f kFriendly =
    hexColor(0x8BF0FE);                     // MAGI 01, and every site pre-fall
const SkColor4f kRim = hexColor(0xFF9418);  // 2 px core, blooms
const SkColor4f kRimFriendly = hexColor(0xD6FBEA);
const SkColor4f kNumeral =
    hexColor(0xFDA114);  // yellower and hotter than the rims
const SkColor4f kInkHostile =
    hexColor(0x990000);  // knocked DARK into the plate
const SkColor4f kInkFriendly = hexColor(0x29985E);
const SkColor4f kAlarm =
    hexColor(0xFF4740);  // COLLAPSING — pure red, never orange
const SkColor4f kCell = hexColor(0x060200);  // the cells are not quite black

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
// TYPE. A bold grotesque, with cap height selected by semantic role. A run is
// a PARTIAL — face, size, condensation — set in the ink in force where it
// lands; a probe measured outside the tree is the partial made whole.

inline sk_sp<SkTypeface> boldFace() { return evangelion::groteskBold(); }

// Horizontal condensation is independent of the selected cap height.
inline weave::Type type(float size, float condense = 1.0f) {
  return {.face = boldFace(), .size = size, .condense = condense};
}

// ---------------------------------------------------------------------------
// EMISSIVE MARKS. The strokes and glyphs remain crisp here. The completed
// marks receive a continuous halo after their geometry and type are drawn.
inline LayeredBrush rimStroke(float core, SkColor4f c) {
  return LayeredBrush{{
      {core, c, 0.0f, {}, 0, SkBlendMode::kSrcOver, false},
  }};
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
  pair({{-80.0f, 310.3f}, {209.4f, 310.3f}}, 24);

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
  bool falls;        // an outer installation; MAGI 01 is never taken
};

// Centres from a colour-masked flood fill of the reference; rotations
// DECLARED here and asserted against atan2 in runAudit(). THE FALLS ARE IN
// THIS ORDER, which is the order the script names them.
constexpr Site kSites[] = {
    {"06", {357.5f, 378.0f}, -45.0f, true},   // CHINA / BEIJING
    {"03", {1582.5f, 374.0f}, 45.0f, true},   // GERMANY / BERLIN
    {"04", {193.0f, 816.5f}, -90.0f, true},   // U.S.A / MASSACHUSETTS
    {"05", {1748.5f, 813.5f}, 90.0f, true},   // GERMANY / HAMBURG
    {"02", {966.5f, 297.0f}, 0.0f, true},     // MATSUSHIRO
    {"01", {967.5f, 935.5f}, 180.0f, false},  // TOKYO-3
};
constexpr int kSiteN = (int)(sizeof(kSites) / sizeof(kSites[0]));
constexpr int kFallN = kSiteN - 1;

/** THE FALLS, AS THE ONE LAW THEY ARE: five snaps of 180 ms, 450 ms
 *  apart, the first at 0.30 s. Stated once as a cascade rather than as a
 *  column of five start times in the table above, which is the same
 *  ladder written out — and a ladder written out is a ladder that can
 *  disagree with itself. */
constexpr double kFirstFall = 0.30;
inline const motion::Spread kFalls{.eachMs = 450.0f, .durationMs = 180.0f};

// ---------------------------------------------------------------------------
// PILLS. Unfilled: black interior, stroked rim, text inside. The label role
// chooses a stable cap height; horizontal condensation fits the capsule.

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
      return {31.0f, 10.0f, 2.0f, 1.0f};
    case LabelRole::Place:
      return {34.0f, 8.0f, 2.0f, 1.0f};
    case LabelRole::Country:
      return {33.0f, 0.0f, 0.0f, 1.0f};
    case LabelRole::Defense:
      return {29.0f, 8.0f, 1.0f, 1.0f};
    case LabelRole::Barrier:
      return {46.0f, 0.0f, 0.0f, 1.0f};
    case LabelRole::Hub:
      return {45.0f, 10.0f, 2.0f, 1.0f};
    case LabelRole::Zone:
      return {25.0f, 10.0f, 5.0f, 1.0f};
    case LabelRole::Flank:
      return {31.0f, 8.0f, 5.0f, 2.0f};
    case LabelRole::Side:
      return {24.0f, 8.0f, 1.0f, 1.0f};
    case LabelRole::Alarm:
      return {45.0f, 10.0f, 8.0f, 1.0f};
  }
  return {34.0f, 8.0f, 2.0f, 1.0f};
}

/** Each capsule supplies the width its label fills and the height its cap
 *  register fits. Country headings keep their natural width within the slot. */
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
     66,
     0,
     evangelion::CutNone,
     true,
     true,
     LabelRole::Alarm},
    {{"COLLAPSING"},
     {1416, 617},
     214,
     66,
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
    stops.push_back({stop.t, hexColor(turnHue(stop.rgb, hueTurn))});
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

constexpr float kBloomRadius = 6.0f;
constexpr float kHaloReach = kBloomRadius * 2.0f + 8.0f;

inline mskia::Effect tubeBloom() { return evangelion::phosphor(2.2f, 0.80f); }

/** The ribbons' halo mask: the funnel's silhouette feathered twice — a
 *  tight pass and a wide one, the tail — with the silhouette itself cut
 *  back out, so the glow stands beside the ribbons and never over them. */
constexpr float kRibbonHaloNear = 2.0f;
constexpr float kRibbonHaloFar = 5.0f;
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
  feather(kRibbonHaloNear, 0.28f);
  feather(kRibbonHaloFar, 0.16f);
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
 *  and its bake — here a bloom — is remade with it. `keyedShape` is the
 *  library's answer: the numbers the generator is a function of ARE its
 *  identity, and equal keys mean equal drawings. */
inline Shape siteSilhouette() {
  return keyedShape(0, [](SkSize s) { return tre::kModule.outline()(s); });
}
inline Shape pillSilhouette(uint8_t cutMask, float radius = 10.0f,
                            SkVector cut = {26.0f, 26.0f}) {
  return keyedShape(std::tuple(radius, cut.fX, cut.fY, cutMask), [=](SkSize s) {
    return evangelion::panel(
        {.radius = radius, .cut = cut, .cutMask = cutMask})(s);
  });
}

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
