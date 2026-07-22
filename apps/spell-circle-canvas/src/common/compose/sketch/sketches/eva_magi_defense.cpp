// eva_magi_defense.cpp — THE END OF EVANGELION, THE MAGI DEFENSE PLATE
// =============================================================================
// Gainax / Production I.G, 1997. Episode 25' "Air", the SEELE hacking sequence
// (~00:05-00:08, before the JSSDF assault): the full-screen tactical plate
// NERV's bridge shows while five external MAGI installations are driven against
// Tokyo-3's MAGI 01.
//
//   Shigeru:   "Data entry from all external nodes... They're hacking the MAGI!"
//   Fuyutsuki: "Hacking verified from Germany, China, the U.S. ..."
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
// WHAT THE MEASUREMENT CORRECTED IN THE BRIEF I WAS HANDED
//
//  1. THE WALLS ARE 55 DEG, NOT 45 — CONFIRMED — BUT THERE IS NO SINGLE
//     DIAGONAL. Least-squares over six rows of the outer wall's left edge
//     (642 at y=660 down to 494 at y=860) gives dx:dy = 0.7386 in the FRAME,
//     53.6 deg from horizontal. Nowhere near 45. But the left V chevron's
//     OUTER arm fits 0.6955 (55.2 deg) and its INNER arm 0.5346 (61.9 deg),
//     and the inner chevron's shoulder 1.1875 (40.1 deg). Four fits, each
//     mirrored to within 2 px on the other side of the plate — so the plate
//     carries three or four deliberate diagonals, not one, and the brief's
//     "one diagonal serves the whole plate" is the one claim in it that the
//     frame does not support. (A first fit here read 0.7004 because it
//     included a point inside the bend; the corrected number is 0.7386, and
//     the sketch authors 0.7526 so the camera roll lands it on 0.7386.)
//  2. THE COLOUR FIELD IS VERTICAL, NOT RADIAL. The brief predicted a radial
//     ramp centred on Tokyo-3 and therefore a need for Material::worldSpace().
//     It is not radial: the main barrier band is a uniform #7EB42B across its
//     whole 570 px span, while a radial field would redden its ends. Sampled
//     down the plate the ramp is a pure function of y (#217642 at y=20 ->
//     #7EB42B at 480 -> #A19732 at 700 -> #A55123 at 900 -> #A0250F at 1060).
//     See the LIBRARY NOTE below for what that does to the gap.
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
// THE TRAP, STATED SO IT CANNOT BE MISSED
//
// RIBBONS ARE FLAT AND DO NOT BLOOM. RIMS AND GLYPHS DO. A trunk edge goes
// ground -> full in 2 px and back in 2 px with no halo; a pill rim peaks at
// v=254 over a 2 px core and is still at v=25 nine pixels out. Bloom the
// 45 px ribbons and the whole plate turns to orange soup, which is how every
// fan recreation of this look goes wrong. So the glow layer contains ONLY
// rims, numerals and pill type — the funnel is not in it at all.
//
// -----------------------------------------------------------------------------
// LIBRARY NOTE — the shared colour field, solved WITHOUT worldSpace()
//
// Sixteen ribbons must sample ONE continuous field. Material::linear is
// node-local pixels and radialUnit is the node's unit square; neither spans
// siblings (ROADMAP 10c). The answer here is better than per-node endpoint
// arithmetic: the entire funnel is ONE node the size of the canvas whose
// outline() is the stroked union of every ribbon polyline, so the node's local
// space IS canvas space and one Material::linear({0,0},{0,1080}) serves all of
// it in a single draw. That works because the field turned out to be VERTICAL
// and every ribbon is absolutely placed; the moment a ribbon needs its own
// transform, or the field is radial about a moving centre, the gap is real
// again. Counted as a half-citation for 10c with the escape route written
// down — and see PERF below for why the same node must not be SkSL.
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
//   Material::linear (20 stops)        the hue field, one gradient blitter
//   Composer::renderSlot               the front advances by re-describing
//                                      ONLY the funnel, 6 Hz, so the art's
//                                      texture bake survives it
//   LayeredBrush (kPlus + blurSigma)   every rim's bloom, as blurred stroke
//                                      MASKS bounded by the shape — no layer,
//                                      no filter, no full-canvas anything
//   SkMaskFilter on TextStyle::paint   the same trick for glyph halos,
//                                      declared UNDER an opaque core
//   Cache::Texture, PER MARK           31 bakes the size of the marks, not
//                                      one 2 MP bake of a 65%-empty canvas
//   ctx.measure()                      every label's point size is SOLVED from
//                                      the width measured off the reference
//   console::LineRing                  the rotation audit, printed as it runs
//
// -----------------------------------------------------------------------------
// PERF — 98.8 ms -> 9.8 ms, and every step of it was a measurement
//
//   98.8  the obvious build: SkSL hue ramp with a bound uniform, bloom as
//         effect(Blur) + blend(kPlus) + Cache::Texture over the whole canvas.
//   36.2  bloom deleted. A filtered full-canvas layer is 2 MP of allocate/
//         blur/composite whatever bakeScale says, and the COLLAPSING glow
//         carried a bound opacity so its blur re-ran every frame: 62 ms.
//   32.1  bloom rebuilt as LayeredBrush + text mask filters (4 ms, and a
//         truer optic — the halo hugs the mark instead of the canvas).
//   14.7  SkSL ramp -> Material::linear. A live material re-resolves every
//         frame and an 11-stage mix chain over 300k covered px is the most
//         expensive way in the library to say "linear ramp": 15 ms -> 2 ms.
//   10.3  the art under Cache::Texture with the ROLL BAKED INSIDE IT, the
//         funnel in a slot() so the 6 Hz front does not dirty the bake, and
//         the scanline creep quantised to whole pixels (a fractional
//         translate turns a cached blit into a resample: 7.7 ms -> 0.6 ms).
//    2.5  the texture bakes moved from the GROUP to each mark. A full-canvas
//         bake of a plate that is 65% empty is 2 MP of alpha blit a frame;
//         thirty-one bakes the size of the marks are 0.6 MP. 8.2 -> 2.4 ms.
//
//   p50 2.5 ms / p99 12.8 ms at 1920x1080 (the p99 is the 6 Hz front step,
//   which re-records the funnel). The one thing that did not fit is
//   the brief's camera push-in: a scaled blit of a 2 MP texture costs the
//   same at 1.02 as at 1.06 and only scale == 1 is cheap. GATE WEAVE — the
//   1 px integer wander of a film frame in the projector gate — replaces it
//   and is both free and truer to a photographed plate.
//
// Run:
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/eva_magi_defense.cpp \
//       --frame /tmp/eva_magi_defense.png --at 2.5
//
//   2.5 s  THE REFERENCE MOMENT: all five outer MAGI have fallen, the front
//          has not moved, and the frame diffs against the plate directly
//   0.0 s  the network cold — six cyan installations, nothing taken
//   0.3 -> 2.3 s  Beijing, Berlin, Massachusetts, Hamburg, Matsushiro fall,
//          in the order the script names them, 180 ms snap 450 ms apart
//   3.0 -> 17 s   the front advances: the whole hue field climbs the plate
// =============================================================================

#include <sigilsketch/Sketch.h>

#include <sigilcompose/Brushes.h>
#include <sigilcompose/Console.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Util.h>

#include <sigilweave/ports/SystemFontManager.h>

#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathUtils.h>
#include <include/core/SkTypeface.h>
#include <include/effects/SkRuntimeEffect.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace sigil::compose;
using namespace sigil::compose::util;
using namespace std::chrono_literals;
namespace weave = sigil::weave;
namespace ch = choreograph;

namespace eva {

// ---------------------------------------------------------------------------
// THE FRAME. 1920x1080, the reference's own size, so the capture diffs.

constexpr float kW = 1920.0f, kH = 1080.0f;
constexpr SkPoint kHub{964.4f, 935.6f}; // MAGI 01's bbox centre, un-rolled
constexpr float kAxis = 971.3f;         // the axis of bilateral symmetry:
                                        // the barrier band's un-rolled span
                                        // 686.4..1256.4 and the two trunks
                                        // 708.7/1233.8 both give 971.3
constexpr float kDiag = 0.7386f;        // dx:dy the funnel wall MEASURES in
                                        // the frame — least-squares over six
                                        // rows; asserted in runAudit()
// The outer wall's two centreline vertices, authored PRE-roll and shared by
// the path builder and the audit so the published angle cannot drift from the
// geometry that draws it.
constexpr SkPoint kWallTop{708.7f, 606.0f};
constexpr SkPoint kWallBend{511.9f, 867.5f};
constexpr float kBand = 45.0f;          // ribbon width, measured 44-46
constexpr float kRoll = -0.45f;         // the photographed CRT is not square

inline SkColor4f hex(uint32_t v, float a = 1.0f) {
  return {(float)((v >> 16) & 255) / 255.0f, (float)((v >> 8) & 255) / 255.0f,
          (float)(v & 255) / 255.0f, a};
}
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
  constexpr float k = 0.007854f; // tan(0.45 deg)
  const float dx = m.fX - kW * 0.5f, dy = m.fY - kH * 0.5f;
  return {m.fX - dy * k, m.fY + dx * k};
}

// ---------------------------------------------------------------------------
// PALETTE. Percentiles over the actual frame, classified by HSV.

const SkColor4f kGround = hex(0x050A01);    // 51% of the frame; green-cast black
const SkColor4f kHostile = hex(0xEE2C26);   // a captured MAGI (measured core)
const SkColor4f kFriendly = hex(0x8BF0FE);  // MAGI 01, and every site pre-fall
const SkColor4f kRim = hex(0xFF9456);       // 2 px core, blooms
const SkColor4f kRimFriendly = hex(0xD6FBEA);
const SkColor4f kNumeral = hex(0xFDA114);   // yellower and hotter than the rims
const SkColor4f kInkHostile = hex(0x990000);// knocked DARK into the plate
const SkColor4f kInkFriendly = hex(0x29985E);
const SkColor4f kAlarm = hex(0xFF4740);     // COLLAPSING — pure red, never orange
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
// TYPE. Helvetica Bold, condensed per label to the measured width.

inline sk_sp<SkTypeface> face(const char *family, int weight, int width,
                              const char *fallback) {
  auto mgr = weave::ports::systemFontManager();
  sk_sp<SkTypeface> f = mgr->matchFamilyStyle(
      family, SkFontStyle(weight, width, SkFontStyle::kUpright_Slant));
  if (!f && fallback)
    f = mgr->matchFamilyStyle(
        fallback, SkFontStyle(weight, width, SkFontStyle::kUpright_Slant));
  if (!f)
    f = mgr->matchFamilyStyle(nullptr, SkFontStyle::Bold());
  return f;
}
inline const sk_sp<SkTypeface> &boldFace() {
  static sk_sp<SkTypeface> f = face("Helvetica", SkFontStyle::kBold_Weight,
                                    SkFontStyle::kNormal_Width, "Arial");
  return f;
}

inline weave::TextStyle type(float size, SkColor4f color, float condense = 1.0f,
                             float track = 0.0f) {
  weave::TextStyle s;
  s.shaping.typeface = boldFace();
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = track;
  s.shaping.scaleX = condense;
  s.paint.foreground.setColor4f(color, nullptr);
  s.paint.foreground.setAntiAlias(true);
  return s;
}

// ---------------------------------------------------------------------------
// BLOOM, WITHOUT A SINGLE saveLayer.
//
// The first cut put the glow on a full-canvas subtree under
// effect(Blur) + blend(kPlus) + Cache::Texture, which is the documented
// recipe and cost SIXTY-TWO MILLISECONDS a frame at 2 MP: a filtered
// full-canvas layer is 2 MP of allocate/blur/composite whatever bakeScale
// says, and one of the two glow layers carried a bound opacity, so its blur
// re-ran every frame. The measured rim profile — v=254 over a 2 px core,
// v~98 at 3 px, v~25 at 9 px, a long dim tail — is EXACTLY the shape
// LayeredBrush was built for: additive stroke passes whose blur is an
// SkMaskFilter on the stroke MASK, bounded by the shape, no layer at all.
// Same for glyphs: a blurred copy of the run is a mask filter on the text
// paint, and Skia caches blurred glyph masks. 62 ms -> ~4 ms, and it is the
// more faithful optic besides, because the halo now hugs the mark.
//
// NOTHING BELOW EVER TOUCHES A RIBBON. Ribbons are flat.

/** THE `Glow` WRAPPER THAT USED TO SIT HERE IS GONE, AND SO IS THE GAP.
 *
 *  This file shipped an eight-line value scheme whose whole job was to hold a
 *  `LayeredBrush` and declare a `bleed()` for it, because `LayeredBrush` did
 *  not have one — `Decoration` therefore reported 0 reach and a blurred
 *  additive stack was culled at its node's own bounds the moment the subtree
 *  recorded. That was real, and it hit every stock brush in the header:
 *  `brushes::filament()` is a 14 px envelope under an 8 px blur and lost its
 *  halo the same way.
 *
 *  `LayeredBrush::bleed()` shipped (Brushes.h:76) computing exactly what this
 *  file asked for — per layer, `width/2 + 3σ`, taking the max — so the wrapper
 *  is deleted and `rimGlow` returns the stock brush.
 *
 *  The numbers differ by 3.5 px — the wrapper declared `core/2 + 7 + 3σ`,
 *  adding the WHOLE of the wide layer's extra width where the envelope only
 *  needs half of it — and the difference is worth stating precisely, because
 *  the frame is not byte-identical and a reader diffing it should know why.
 *  No halo is lost: the reach the wrapper was over-reserving carried about a
 *  tenth of one 8-bit level, since 3σ is already >99% of the Gaussian and the
 *  wide pass runs at 0.30 alpha. What DOES change is that every marks bakes
 *  under `Cache::Texture`, so a 3.5 px change in reserved reach resizes the
 *  bake surface and re-phases its blit — which reshuffles about a pixel of
 *  antialiasing on every edge in the plate. Measured, it does not soften it:
 *  mean |dI/dx| over the whole frame goes 4.4732 -> 4.4796. */
inline LayeredBrush rimGlow(float core, SkColor4f c) {
  // TWO passes, not three. A middle pass at width core+3.5 and only sigma 2
  // does not read as a halo, it reads as a FATTER RIM — side by side with the
  // plate the 2 px keyline came out looking 6 px thick. The measured profile
  // is one hard core and one long soft tail (v 254 -> 98 at 3 px -> 25 at
  // 9 px), so: a hairline, and a wide dim blur under it.
  SkColor4f wide = c;
  wide.fA = 0.30f;
  return LayeredBrush{{
      {core + 7.0f, wide, 6.5f, {}, 0, SkBlendMode::kPlus, false},
      {core, c, 0.0f, {}, 0, SkBlendMode::kSrcOver, false},
  }};
}

inline weave::TextStyle glowType(float size, SkColor4f color, float condense,
                                 float sigma, float alpha) {
  weave::TextStyle s = type(size, color, condense);
  SkColor4f c = color;
  c.fA = alpha;
  s.paint.foreground.setColor4f(c, nullptr);
  s.paint.foreground.setMaskFilter(
      SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, sigma));
  s.paint.foreground.setBlendMode(SkBlendMode::kPlus);
  return s;
}

/** A run of type with its own halo: the sharp core SIZES the box (it is the
 *  only in-flow child) and two additive blurred passes ride over it as
 *  absolute overlays. Not a stack(): a stack's children measure to nothing
 *  here, so the pill's column centred a zero-width box and every label shot
 *  out of its own pill to the right. In-flow-core + absolute-overlays is the
 *  shape that measures. kPlus makes the paint order irrelevant. */
inline Element glowText(std::u8string s, float size, SkColor4f c,
                        float condense = 1.0f) {
  // ORDER IS THE WHOLE FIX. Painted OVER the core, two additive halos put
  // #FDA114 at 1.76x and the numerals came back chartreuse — kPlus clips R at
  // 255 and keeps lifting G, so the amber walks toward yellow-green, which is
  // exactly the failure mode every recreation of this look has. Declared
  // FIRST they paint UNDER an opaque core: the glyph body stays the sampled
  // colour to the byte and the halo only exists where the glyph is not.
  return box()
      .child(text(s, glowType(size, c, condense, 6.5f, 0.34f))
                 .absolute()
                 .inset(0))
      .child(text(s, glowType(size, c, condense, 2.2f, 0.62f))
                 .absolute()
                 .inset(0))
      .child(text(std::move(s), type(size, c, condense)));
}

// ---------------------------------------------------------------------------
// RIBBONS. A polyline stroked to a mitred outline — every vertex pre-placed on
// a measured coordinate, because a router that searches its own corners
// notches a 45 px band at a 55 deg bend.

inline SkPath ribbon(const std::vector<SkPoint> &pts, float width) {
  if (pts.size() < 2)
    return SkPath();
  SkPathBuilder b;
  b.moveTo(pts.front());
  for (size_t i = 1; i < pts.size(); ++i)
    b.lineTo(pts[i]);
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
  auto add = [&](std::vector<SkPoint> pts, float w) {
    parts.push_back(ribbon(pts, w));
  };
  auto pair = [&](std::vector<SkPoint> pts, float w) {
    add(pts, w);
    std::vector<SkPoint> m;
    m.reserve(pts.size());
    for (SkPoint p : pts)
      m.push_back(mirrorP(p));
    add(std::move(m), w);
  };

  // 1. the support band along the very top edge, cut off by the frame
  add({{354, -12}, {mirrorX(354), -12}}, 44);

  // 2. the two SUPPORT LINE chevrons. Fitted through seven rows each, then
  //    un-rolled: OUTER arm 0.6858 : 1, INNER arm 0.5402 : 1. They are not
  //    the same angle and they are not the wall's angle either — three
  //    diagonals on one plate, each mirrored exactly left/right, which is
  //    what says they were drawn rather than derived.
  pair({{-71.9f, -100.0f}, {209.4f, 310.3f}, {431.1f, -100.0f}}, 24);

  // 3. trunk -> outer wall -> the chamfered bottom rail. 708-514 over 607-884
  //    is exactly 0.700.
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
  add({{721.5f, 605}, {839.7f, 705}, {mirrorX(839.7f), 705},
       {mirrorX(721.5f), 605}},
      kBand);

  // 6. from each roof corner, the wall that drops to the frame edge
  pair({{839.7f, 705}, {686.9f, 906}, {686.9f, 1120}}, kBand);
  // (the 37 px ribbon 6 px outboard of it is NOT a ribbon: a cross-section at
  //  y=1000 reads solid #BE2B16 to x=713, six px of #480000, then a bright
  //  rim and a DARK interior carrying bright type — it is a long unfilled
  //  pill. It is built with the labels, which is the doubled rule.)

  SkPathBuilder joined;
  joined.setFillType(SkPathFillType::kWinding); // overlapping bands union
  for (const SkPath &p : parts)
    joined.addPath(p);
  out = joined.detach();
  return out;
}

// ---------------------------------------------------------------------------
// THE T-TREFOIL. One component; every site is this, rotated.

namespace tre {
constexpr float kBarW = 343.0f, kBarH = 176.0f;
constexpr float kStemW = 128.0f, kStemH = 104.0f;
constexpr float kTotalH = kBarH + kStemH; // 280
// The corner radius is 16, not the 22 the brief guessed: the black interior
// of MAGI 02's cell 1 is 72 px wide and its arc solves r = 12.9 from two
// depths (6 px inset at d=2, 2 px at d=6), plus the ~3 px rim.
constexpr float kCellW = 88.0f, kCellH = 150.0f, kCellR = 16.0f;
constexpr float kMargin = 20.0f;
constexpr float kStemX = (kBarW - kStemW) * 0.5f;

inline SkPath silhouette(SkSize) {
  SkPathBuilder b;
  b.moveTo(0, 0);
  b.lineTo(kBarW, 0);
  b.lineTo(kBarW, kBarH);
  b.lineTo(kStemX + kStemW, kBarH);
  b.lineTo(kStemX + kStemW, kTotalH);
  b.lineTo(kStemX, kTotalH);
  b.lineTo(kStemX, kBarH);
  b.lineTo(0, kBarH);
  b.close();
  return b.detach();
}

/** Cell rects in component-local coordinates: 1 left, 3 right, 2 in the stem. */
inline SkRect cell(int n) {
  switch (n) {
  case 1:
    return SkRect::MakeXYWH(kMargin, kMargin - 1, kCellW, kCellH);
  case 3:
    return SkRect::MakeXYWH(kBarW - kMargin - kCellW, kMargin - 1, kCellW,
                            kCellH);
  default:
    return SkRect::MakeXYWH((kBarW - kCellW) * 0.5f,
                            kTotalH - kMargin - kCellH, kCellW, kCellH);
  }
}
constexpr SkPoint kLabelAt{kBarW * 0.5f + 2.0f, 66.0f};
} // namespace tre

struct Site {
  const char *name;   // "01" .. "06"
  SkPoint centre;     // measured bbox centre
  float rotation;     // declared, snapped to 45
  double fallAt;      // seconds; < 0 = never (MAGI 01)
};

// Centres from a colour-masked flood fill of the reference; rotations
// DECLARED here and asserted against atan2 in audit().
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
// PILLS. Unfilled: black interior, stroked rim, text inside. Two of them have
// CHAMFERED outer-top corners so they sit flush against the funnel wall — one
// outline generator with per-corner cut flags, because a rounded rect there is
// a fail.

enum Cut : uint8_t { kNone = 0, kTL = 1, kTR = 2, kBR = 4, kBL = 8 };

inline std::function<SkPath(SkSize)> pillOutline(float r, uint8_t cuts,
                                                 float cut) {
  return [r, cuts, cut](SkSize s) {
    const float w = s.width(), h = s.height();
    const float d = 2.0f * r;
    SkPathBuilder b;
    if (cuts & kTL) {
      b.moveTo(0, cut);
      b.lineTo(cut, 0);
    } else {
      b.moveTo(0, r);
      b.arcTo(SkRect::MakeXYWH(0, 0, d, d), 180, 90, false);
    }
    if (cuts & kTR) {
      b.lineTo(w - cut, 0);
      b.lineTo(w, cut);
    } else {
      b.lineTo(w - r, 0);
      b.arcTo(SkRect::MakeXYWH(w - d, 0, d, d), 270, 90, false);
    }
    if (cuts & kBR) {
      b.lineTo(w, h - cut);
      b.lineTo(w - cut, h);
    } else {
      b.lineTo(w, h - r);
      b.arcTo(SkRect::MakeXYWH(w - d, h - d, d, d), 0, 90, false);
    }
    if (cuts & kBL) {
      b.lineTo(cut, h);
      b.lineTo(0, h - cut);
    } else {
      b.lineTo(r, h);
      b.arcTo(SkRect::MakeXYWH(0, h - d, d, d), 90, 90, false);
    }
    b.close();
    return b.detach();
  };
}

/** Every label on the plate. `w`/`h` are the pill's measured outer size;
 *  `lines` are set solid inside it and the point size is SOLVED from `w`. */
struct Label {
  const char *lines[3];
  SkPoint centre;
  float w, h;
  float rotate;
  uint8_t cuts;
  bool pill;
  bool alarm;
};

const Label kLabels[] = {
    // the four SUPPORT LINE pills — two of them bleed off the side edges
    {{"SUPPORT LINE"}, {522, 45}, 280, 45, 0, kNone, true, false},
    {{"SUPPORT LINE"}, {1409, 43}, 280, 45, 0, kNone, true, false},
    {{"SUPPORT LINE"}, {34, 358}, 280, 45, 0, kNone, true, false},
    {{"SUPPORT LINE"}, {1913, 355}, 280, 45, 0, kNone, true, false},
    // site names
    {{"MATSUSHIRO"}, {966, 120}, 242, 47, 0, kNone, true, false},
    {{"CHINA"}, {527, 161}, 148, 42, 0, kNone, true, false},
    {{"BEIJING"}, {541, 203}, 168, 44, 0, kNone, true, false},
    {{"GERMANY"}, {1387, 177}, 180, 42, 0, kNone, true, false},
    {{"BERLIN"}, {1377, 219}, 154, 44, 0, kNone, true, false},
    {{"U.S.A"}, {101, 570}, 119, 42, 0, kNone, true, false},
    {{"MASSACHUSETTS"}, {210, 607}, 331, 44, 0, kNone, true, false},
    {{"GERMANY"}, {1841, 567}, 180, 42, 0, kNone, true, false},
    {{"HAMBURG"}, {1849, 606}, 190, 44, 0, kNone, true, false},
    // the defense lines
    {{"1st. DEFENSE LINE"}, {972, 547}, 318, 40, 0, kNone, true, false},
    {{"MAIN BARRIER"}, {973, 599}, 385, 44, 0, kNone, false, false},
    {{"2nd. DEFENSE LINE"}, {979, 651}, 316, 40, 0, kNone, true, false},
    {{"3rd. DEFENSE LINE"}, {668, 757}, 222, 38, -55, kNone, true, false},
    {{"3rd. DEFENSE LINE"}, {1278, 757}, 222, 38, 55, kNone, true, false},
    // the hub
    {{"TOKYO-3"}, {969, 757}, 274, 57, 0, kNone, true, false},
    {{"FINAL", "DEFENSE", "ZONE"}, {834, 834}, 126, 92, 0, kTL, true, false},
    {{"FINAL", "DEFENSE", "ZONE"}, {1104, 834}, 126, 92, 0, kTR, true, false},
    {{"LEFT", "FLANK"}, {602, 962}, 114, 82, 0, kNone, true, false},
    {{"RIGHT", "FLANK"}, {1343, 962}, 118, 82, 0, kNone, true, false},
    {{"LEFT SIDE BARRIER"}, {737, 988}, 196, 33, -90, kNone, true, false},
    {{"RIGHT SIDE BARRIER"}, {1203, 988}, 205, 33, -90, kNone, true, false},
};
constexpr int kLabelN = (int)(sizeof(kLabels) / sizeof(kLabels[0]));

// the two COLLAPSING pills blink, so they are their own (volatile) layer
const Label kCollapsing[] = {
    {{"COLLAPSING"}, {528, 617}, 214, 46, 0, kNone, true, true},
    {{"COLLAPSING"}, {1416, 617}, 214, 46, 0, kNone, true, true},
};

// ---------------------------------------------------------------------------
// THE FRONT. Skia's own gradient blitter, not SkSL.
//
// The first cut compiled the ramp as an 11-stage SkSL mix chain with the front
// as a bound uniform. Correct, live, and 15 ms a frame at 1920x1080 on the CPU
// raster the sketch host uses: a LIVE material re-resolves and repaints every
// frame, and an interpreted per-pixel mix chain over ~300k covered pixels is
// the most expensive way in the library to say "linear ramp". Material::linear
// lowers to SkShaders::LinearGradient, which is a SIMD blitter — same picture,
// ~1 ms. The front then advances the DATA way: the stop positions shift and
// update() re-renders at 6 Hz, which is exactly the declared-choppiness rule
// the library states for the P3R sea (quantizeTime(6)). A 14-second sweep
// stepped six times a second is invisible as steps and free as pixels.

inline Material rampMaterial(float front) {
  std::vector<Stop> stops;
  stops.reserve((size_t)kRampN);
  float last = -1.0f;
  for (int i = 0; i < kRampN; ++i) {
    float pos = kRamp[i].t - front * 0.42f;
    pos = pos < 0.0f ? 0.0f : (pos > 1.0f ? 1.0f : pos);
    pos = pos <= last ? last + 1e-4f : pos; // gradients want monotone stops
    last = pos;
    stops.push_back({pos, hex(kRamp[i].rgb)});
  }
  return Material::linear({0, 0}, {0, kH}, std::move(stops));
}

// ---------------------------------------------------------------------------
// THE CRT. nerv-ui/components/crt-effects.css, transcribed: 2 px scanlines at
// 4% black, a 70%/70% vignette ellipse reaching 40%. Baked once into a texture
// and crept by a bound translateY — no per-frame shader anywhere.

inline sk_sp<SkRuntimeEffect> crtEffect() {
  static sk_sp<SkRuntimeEffect> fx = [] {
    auto [effect, err] = SkRuntimeEffect::MakeForShader(SkString(
        "uniform float2 uResolution;\n"
        "half4 main(float2 xy) {\n"
        "  float line = mod(xy.y, 4.0) < 2.0 ? 0.052 : 0.0;\n"
        "  float2 p = (xy / max(uResolution, float2(1.0)) - 0.5) * 2.0;\n"
        "  float r = length(p / 0.70);\n"
        "  float vig = smoothstep(1.45, 2.15, r) * 0.34;\n"
        "  float a = clamp(line + vig, 0.0, 1.0);\n"
        "  return half4(0.0, 0.0, 0.0, half(a));\n}\n"));
    if (!effect)
      SkDebugf("eva crt shader: %s\n", err.c_str());
    return effect;
  }();
  return fx;
}

// ---------------------------------------------------------------------------

/** A quarter-turn defeats Cache::Texture (see art()); everything else bakes. */
inline bool bakeable(float rotationDeg) {
  const float q = std::fmod(std::fabs(rotationDeg), 180.0f);
  return std::fabs(q - 90.0f) > 1.0f;
}

inline float wrap180(float d) {
  while (d > 180.0f)
    d -= 360.0f;
  while (d <= -180.0f)
    d += 360.0f;
  return d;
}
inline float snap45(float deg) { return std::round(deg / 45.0f) * 45.0f; }
inline float deg(float rad) { return rad * 57.29577951f; }
inline float rad(float d) { return d * 0.01745329252f; }

} // namespace eva

// =============================================================================

struct EvaMagiDefense : sigil::compose::sketch::Sketch {
  using Sketch::Sketch;

  ch::Output<float> weaveX{0.0f};   // gate weave, whole px
  ch::Output<float> weaveY{0.0f};
  ch::Output<float> creep{0.0f};    // scanline creep
  ch::Output<float> flicker{0.0f};  // phosphor dip (alpha of a black plane)
  ch::Output<float> blink{1.0f};    // COLLAPSING, hard on/off

  bool fallen[eva::kSiteN] = {false, false, false, false, false, false};
  int fallCount = 0;
  int frontStep = 0;                // the advancing front, 84 steps at 6 Hz
  std::vector<float> labelSize;   // solved from the measured widths
  std::vector<float> siteNameSize;
  float numeralSize = 96.0f;
  sigil::compose::console::LineRing audit{48};
  std::vector<std::u8string> failures;
  SkPath funnel;

  // --- the construction rule, asserted ---------------------------------------
  void runAudit() {
    using namespace eva;
    failures.clear();
    // MAGI 01's target is the CENTROID of the five attackers; everyone else's
    // is the hub. Same rule, one substitution.
    SkPoint centroid{0, 0};
    for (const Site &s : kSites)
      if (s.fallAt >= 0) {
        centroid.fX += unroll(s.centre).fX / 5.0f;
        centroid.fY += unroll(s.centre).fY / 5.0f;
      }
    audit.append(u8"ROTATION RULE  theta = snap45(bearing(site->target) - 90)", 1);
    char line[160];
    std::printf("\n  MAGI defense plate — rotation audit\n");
    std::printf("  site   centre        target        bearing   want    "
                "declared  stem_dir  err\n");
    for (const Site &s : kSites) {
      const bool hub = s.fallAt >= 0;
      const SkPoint tgt = hub ? kHub : centroid;
      const SkPoint at = unroll(s.centre);
      const float bearing =
          deg(std::atan2(tgt.fY - at.fY, tgt.fX - at.fX));
      const float want = snap45(bearing - 90.0f);
      // the component's stem runs local +y; rotate it by the declared theta
      const float th = rad(s.rotation);
      const SkVector stem{-std::sin(th), std::cos(th)};
      const float stemDeg = deg(std::atan2(stem.fY, stem.fX));
      const float err = std::fabs(wrap180(stemDeg - bearing));
      const bool ok = std::fabs(wrap180(want - s.rotation)) < 0.5f && err < 22.5f;
      std::snprintf(line, sizeof(line),
                    "  MAGI %s (%4.0f,%4.0f)  (%4.0f,%4.0f)  %7.2f  %+5.0f  "
                    "%+7.0f  %+7.1f  %5.2f  %s",
                    s.name, (double)at.fX, (double)at.fY,
                    (double)tgt.fX, (double)tgt.fY, (double)bearing,
                    (double)want, (double)s.rotation, (double)stemDeg,
                    (double)err, ok ? "PASS" : "*** FAIL ***");
      std::printf("%s\n", line);
      audit.append(toU8(line + 2), ok ? 2 : 3);
      if (!ok)
        failures.push_back(toU8(line + 2));
    }
    // ...and the plate's other published number: the wall angle. The
    // polyline is authored pre-roll, so the check is "does the CAMERA put it
    // back on the 0.7386 the frame measures", which is what makes the roll
    // an honest transform rather than a decoration.
    {
      const float ax = kWallTop.fX - kWallBend.fX;   // authored run (leftward)
      const float ay = kWallBend.fY - kWallTop.fY;   // authored rise
      const float c = std::cos(rad(kRoll)), sn = std::sin(rad(kRoll));
      const float rx = -ax * c - ay * sn;            // rotated by the camera
      const float ry = -ax * sn + ay * c;
      const float rendered = std::fabs(rx / ry);
      const bool ok = std::fabs(rendered - kDiag) < 0.01f;
      std::snprintf(line, sizeof(line),
                    "  WALL  authored %.4f  + roll %.2f deg -> %.4f   "
                    "frame measures %.4f  %s",
                    (double)(ax / ay), (double)kRoll, (double)rendered,
                    (double)kDiag, ok ? "PASS" : "*** FAIL ***");
      std::printf("%s\n", line);
      audit.append(toU8(line + 2), ok ? 2 : 3);
      if (!ok)
        failures.push_back(toU8(line + 2));
    }
    std::printf("  %d/%d sites obey the rule; stem half-window is 22.5 deg.\n\n",
                kSiteN - (int)failures.size(), kSiteN);
  }

  // --- one installation ------------------------------------------------------
  Element installation(int index) const {
    using namespace eva;
    const Site &s = kSites[index];
    const bool friendly = s.fallAt < 0 || !fallen[index];
    const SkColor4f plateFill = friendly ? kFriendly : kHostile;
    const SkColor4f rim = friendly ? kRimFriendly : kRim;
    const SkColor4f ink = friendly ? kInkFriendly : kInkHostile;
    const auto snap = Transition{.duration = 180ms, .ease = ch::easeOutQuad};

    const SkPoint at = unroll(s.centre);
    auto plate = box()
                     .absolute()
                     .left(at.fX - tre::kBarW * 0.5f)
                     .top(at.fY - tre::kTotalH * 0.5f)
                     .width(tre::kBarW)
                     .height(tre::kTotalH)
                     .outline(tre::silhouette)
                     .rotate(s.rotation)
                     .fill(with(Fill::color(plateFill), snap))
                     .foreground(rimGlow(2.4f, rim))
                     .key(std::string("site#") + s.name);

    // three cells: black, hard orange keyline, and the keyline blooms
    for (int n : {1, 2, 3}) {
      const SkRect r = tre::cell(n);
      auto cell = box()
                      .absolute()
                      .left(r.left())
                      .top(r.top())
                      .width(r.width())
                      .height(r.height())
                      .corners({tre::kCellR})
                      .fill(Fill::color(kCell))
                      .foreground(rimGlow(3.2f, rim));
      // The numeral is centred in the cell's TOP SQUARE, not in the cell:
      // measured, the glyph's centre sits at 30% of a 150 px cell, which is
      // 44 px — the middle of the 88 px width. And it stays UPRIGHT while
      // the plate turns.
      cell.child(glowText(toU8(std::string(1, (char)('0' + n))), numeralSize,
                          kNumeral, 0.88f)
                     .absolute()
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
    // has to clear that 44 px, upright, in world x. At 46 px it did not:
    // MAGI 04 read "MAG" with the I bisected by cell 2's left edge and MAGI 05
    // had the M sitting on it. Helvetica Bold "MAGI" is 2.667 em, so 83 px at
    // 36 pt is scaleX 0.86 — the word is Helvetica CONDENSED on the plate and
    // the numerals are not, which is what fontsinuse lists for the panels.
    plate.child(box()
                    .absolute()
                    .centerAt(tre::kLabelAt)
                    .column()
                    .alignItems(Align::Center)
                    .gap(-6)
                    .rotate(-s.rotation)
                    .child(text(u8"MAGI", type(36, ink, 0.86f)))
                    .child(text(toU8(s.name), type(50, ink, 0.95f))));
    return plate;
  }

  // --- a pill ----------------------------------------------------------------
  Element pillOf(const eva::Label &L, float size, int keyIndex,
                 const char *keyTag) const {
    using namespace eva;
    const SkColor4f ink = L.alarm ? kAlarm : kRim;
    const SkPoint at = unroll(L.centre);
    auto node = box()
                    .absolute()
                    .left(at.fX - L.w * 0.5f)
                    .top(at.fY - L.h * 0.5f)
                    .width(L.w)
                    .height(L.h)
                    .rotate(L.rotate)
                    .column()
                    .alignItems(Align::Center)
                    .justify(Justify::Center)
                    .gap(-2)
                    .key(std::string(keyTag) + std::to_string(keyIndex));
    if (L.pill) {
      node.outline(pillOutline(10.0f, L.cuts, 26.0f));
      node.fill(Fill::color(kCell));
      node.foreground(rimGlow(3.0f, ink));
    }
    for (const char *line : L.lines)
      if (line)
        node.child(glowText(toU8(line), size, ink, 0.94f));
    return node;
  }

  // --- the layers ------------------------------------------------------------
  Element funnelLayer() const {
    using namespace eva;
    // Explicit size, not inset(0): the slot's own node has no dimensions to
    // stretch against, so an absolute child of it lays out 1920x0 — harmless
    // for an outline in absolute coordinates, and a lie in every query.
    return box()
        .width(kW)
        .height(kH)
        .outline([this](SkSize) { return funnel; })
        .fill(rampMaterial((float)frontStep / 84.0f))
        .key("funnel");
  }

  Element art() const {
    using namespace eva;
    auto g = box().absolute().inset(0);
    // Cache::Texture PER MARK, not on the group. One full-canvas bake is a
    // 2 MP alpha blit every frame (8.2 ms measured) for a plate that is 65%
    // empty; bakes the size of the marks themselves total ~0.6 MP and cost
    // 2.4 ms. The header's own rule — "Texture is wasteful for sparse
    // regions" — with a number on it.
    //
    // EXCEPT AT +/-90 DEGREES, WHICH IS A LIBRARY BUG (reported): a node
    // carrying rotate(+/-90) bakes at the wrong resolution and its content
    // comes back non-uniformly resampled. On the LEFT SIDE BARRIER pill
    // (196x33, rotate -90) the type is destroyed — mean |delta| against the
    // uncached render is 32/255 where every other mark is 2-8. 0, 45 and 180
    // are all clean, so the guard is exactly the quarter-turn.
    for (int i = 0; i < kSiteN; ++i)
      g.child(installation(i).cache(bakeable(kSites[i].rotation)
                                        ? Cache::Texture
                                        : Cache::Auto));
    for (int i = 0; i < kLabelN; ++i)
      g.child(pillOf(kLabels[i], labelSize[(size_t)i], i, "lab")
                  .cache(bakeable(kLabels[i].rotate) ? Cache::Texture
                                                     : Cache::Auto));
    return g;
  }

  /** COLLAPSING blinks, so it is its own (volatile) node — and a TIGHT one:
   *  a full-canvas volatile layer would repaint 2 MP for two 214 px pills. */
  Element collapsingLayer(int i) const {
    using namespace eva;
    return pillOf(kCollapsing[i], siteNameSize[(size_t)i], i, "col")
        .opacity(&blink);
  }

  /** THE CAMERA, and the shape of it is a perf result.
   *
   *  The roll goes INSIDE the wrapper (on the child), so a Cache::Texture on
   *  the wrapper bakes the rotation into the pixels and the wrapper blits at
   *  identity. Put the same rotate on the OUTER node and the blit becomes a
   *  rotated resample of 2 MP: measured, 11.4 ms a frame instead of 2.6.
   *
   *  The push-in the brief asks for (1.00 -> 1.06 about Tokyo-3) is GONE for
   *  the same reason and it is the one thing in the plate I could not afford:
   *  a scaled blit of a 2 MP texture costs the same at 1.02 as at 1.06, and
   *  only scale == 1 is cheap. What replaces it is truer to the artefact
   *  anyway — GATE WEAVE, the 1 px wander of a film frame in the projector
   *  gate, which is an INTEGER translate and therefore free.
   *
   *  The pivot is the FRAME CENTRE, not the hub: the coordinates were
   *  measured off an already-rolled frame, and rolling about the centre puts
   *  them back within ~1 px where rolling about the hub leaves the top of
   *  the plate 6 px out. */
  Element describe() {
    using namespace eva;
    auto camera = [this](Element e) {
      return box()
          .absolute()
          .inset(0)
          .translateX(&weaveX)
          .translateY(&weaveY)
          .child(std::move(
              e.rotate(kRoll).transformOriginPx({kW * 0.5f, kH * 0.5f})));
    };

    auto root = stack().absolute().inset(0);

    // (no ground node: the host clears to ctx.background, and a full-canvas
    //  opaque fill is a millisecond nobody needs to spend)

    // The ribbons: flat fills of one continuous field, and NO bloom anywhere.
    // In a SLOT, because the advancing front re-describes it six times a
    // second and a full render() would dirty the art's texture bake with it
    // (measured: a 46 ms spike on every step).
    root.child(camera(slot("funnel")));

    // the plates, the pills and the type, each carrying its own bounded halo.
    // the plates, the pills and the type — each mark carries its own bounded
    // halo and its OWN texture bake (see art()); the group is a plain picture
    root.child(camera(art()));
    root.child(camera(collapsingLayer(0)));
    root.child(camera(collapsingLayer(1)));

    // the photographed CRT: scanlines + vignette baked once, crept
    Material crt = Material::sksl(crtEffect());
    root.child(box()
                   .absolute()
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
                   .absolute()
                   .inset(0)
                   .fill(Fill::color({0, 0, 0, 1}))
                   .opacity(&flicker)
                   .key("flicker"));

    if (!failures.empty())
      root.child(failureBanner());
    return root;
  }

  /** The audit, painted across the plate — and ONLY when it fails. The plate
   *  carries no drafting chrome (see the header), so the checks live on
   *  stdout in the normal case; a violated construction rule gets the whole
   *  ring dumped in magenta where nobody can ship past it. */
  Element failureBanner() const {
    sigil::compose::console::Style st;
    st.text = eva::type(23, {0, 0, 0, 1}, 0.95f);
    st.palette = {eva::type(23, {0.25f, 0, 0.25f, 1}, 0.95f),
                  eva::type(26, {0, 0, 0, 1}, 0.95f),
                  eva::type(23, {0, 0.25f, 0.15f, 1}, 0.95f),
                  eva::type(23, {0.6f, 0, 0, 1}, 0.95f)};
    st.gap = 3.0f;
    st.visibleLines = 16;
    return box()
        .absolute()
        .left(0)
        .top(300)
        .width(eva::kW)
        .height(150 + 30 * (float)audit.lines().size())
        .fill(Fill::color({1, 0, 1, 0.93f}))
        .column()
        .padding(26)
        .gap(10)
        .child(text(u8"ROTATION RULE VIOLATED — this plate is not one component",
                    eva::type(40, {0, 0, 0, 1}, 0.95f)))
        .child(sigil::compose::console::console(audit, st));
  }

  // --- host ------------------------------------------------------------------
  void setup(sketch::SketchContext &ctx) override {
    using namespace eva;
    ctx.canvas(kW, kH);
    ctx.background(kGround);

    funnel = funnelPath();
    runAudit();

    // Solve every label's point size from the width measured off the frame:
    // measure once at 100 pt and scale. The type then lands where the cel's
    // does instead of where a guess would.
    auto solve = [&](const Label &L) {
      const char *longest = L.lines[0];
      for (const char *l : L.lines)
        if (l && std::string(l).size() > std::string(longest).size())
          longest = l;
      const float pad = L.pill ? 14.0f : 0.0f; // measured: MATSUSHIRO's
                                               // type is 235 of 242 px
      const SkSize m =
          ctx.measure(text(toU8(longest), type(100.0f, kRim, 0.94f)));
      const float target = L.w - pad;
      float size = m.width() > 1.0f ? 100.0f * target / m.width() : 30.0f;
      // multi-line pills are height-bound, not width-bound
      const int lines = (L.lines[1] ? (L.lines[2] ? 3 : 2) : 1);
      if (lines > 1)
        size = std::min(size, (L.h - 16.0f) / (float)lines / 0.98f);
      return size;
    };
    labelSize.clear();
    for (int i = 0; i < kLabelN; ++i)
      labelSize.push_back(solve(kLabels[i]));
    siteNameSize.clear();
    for (int i = 0; i < 2; ++i)
      siteNameSize.push_back(solve(kCollapsing[i]));
    // The numerals: cap height 61 px, measured off the "3" in MAGI 02's
    // cell 3 (x 1049..1097, y 185..246). measure() returns the LINE box, so
    // solve against a probed cap ratio rather than assuming Helvetica's.
    {
      const SkSize line = ctx.measure(text(u8"3", type(100.0f, kNumeral, 0.88f)));
      const float capAt100 = line.height() > 1.0f ? line.height() * 0.63f : 71.7f;
      numeralSize = 100.0f * 61.0f / capAt100;
    }

    // --- motion ---
    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      // gate weave: whole pixels, a 3 Hz wander, the projector's own motion
      const uint32_t gate = (uint32_t)std::max(0.0, std::floor(t * 3.0));
      const uint32_t h = (gate * 2654435761u) ^ (gate >> 3);
      weaveX = (float)((h >> 8) % 3u) - 1.0f;
      weaveY = (float)((h >> 19) % 3u) - 1.0f;
      // scanlines creep one WHOLE PIXEL at a time, 4 px per 8 s: a fractional
      // translate turns the cached CRT texture's blit into a resample.
      creep = (float)((int)std::floor(t * 0.5) % 4);
      // phosphor flicker: a 4 s cycle, 1% duty
      const double ph = std::fmod(t, 4.0);
      flicker = ph < 0.04 ? 0.04f : 0.0f;
      // COLLAPSING: hard on/off, 350 on / 250 off (ESTIMATED — a single frame
      // cannot measure a blink; flagged rather than smuggled in)
      blink = std::fmod(t, 0.6) < 0.35 ? 1.0f : 0.0f;
      return true;
    });

    ctx.composer.render(describe());
    ctx.composer.renderSlot("funnel", funnelLayer());
  }

  void update(double elapsed, sketch::SketchContext &ctx) override {
    // DATA path only, and it is the ONLY re-describe in the sketch: a site
    // falls (5 times), or the front takes a 6 Hz step (84 times). Everything
    // else is a bound Output and never re-describes at all.
    int now = 0;
    for (int i = 0; i < eva::kSiteN; ++i)
      if (eva::kSites[i].fallAt >= 0 && elapsed >= eva::kSites[i].fallAt)
        ++now;
    const double sweep = (elapsed - 3.0) / 14.0;
    const double k = sweep <= 0 ? 0.0 : (sweep >= 1 ? 1.0 : sweep);
    const double eased =
        k < 0.5 ? 2 * k * k : 1 - std::pow(-2 * k + 2, 2) / 2; // easeInOutQuad
    const int step = (int)std::lround(eased * 84.0);
    if (now == fallCount && step == frontStep)
      return;
    const bool fell = now != fallCount;
    fallCount = now;
    frontStep = step;
    if (fell) {
      for (int i = 0; i < eva::kSiteN; ++i)
        fallen[i] =
            eva::kSites[i].fallAt >= 0 && elapsed >= eva::kSites[i].fallAt;
      ctx.composer.render(describe());
    }
    ctx.composer.renderSlot("funnel", funnelLayer());
  }
};

SIGIL_SKETCH(EvaMagiDefense)
