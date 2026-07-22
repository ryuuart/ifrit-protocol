// lain_navi.cpp — SERIAL EXPERIMENTS LAIN, THE NAVI RUNNING COPLAND OS
// =============================================================================
// Triangle Staff / Yoshitoshi ABe / Ryutaro Nakamura, TV Tokyo, 1998.
//
// A RECONSTRUCTION, and it says so: one canvas built out of TWO documented
// screens. Layer 04 "Religion" gives the NAVI system console window — real
// MIPS assembly scrolling in a light monospace inside a Copland OS frame.
// Layer 07 "Society" gives the stratigraphy it sits in — Japanese prose at
// full bleed, an orbital wireframe, and the English phrases. The show layers
// windows over strata constantly ("the screen is in constant motion"), but
// THIS frame does not exist; it is the two plates composited under the one
// law that governs both, which is the study.
//
// -----------------------------------------------------------------------------
// SOURCES — read, and MEASURED
//
//  * The two frames themselves, 1016x720, and this canvas is that size so a
//    capture diffs against them. Every number below came out of PIL:
//    8x6 tile percentiles, per-row run lengths, column-mean derivatives,
//    FFT of the row mean, and a conic fit to the wireframe mask.
//  * lain.wiki/wiki/Source_Code — the authority for WHICH text belongs on
//    which screen. Layer 01 is C on a chalkboard; **Layer 04 is MIPS assembly
//    in the NAVI system console**; Layer 07 is Common Lisp on the HandiNAVI.
//    The console block here is the sixteen lines legible off the frame, and
//    the lines that extend it are written in the same dialect — the
//    `.frame`/`.mask`/`.fmask` triple and the `# vars= N, regs= N/N, args= N,
//    extra=` comment are literally GCC's MIPS prologue annotations.
//  * github.com/KaiserTorian/CoplandOS-Ascii-Art — an INDEPENDENT
//    transcription of the Copland eye, used as the logo's topology spec
//    (iris ring, two arc eyelids, four satellite dots on the diagonals, a
//    stem below) rather than tracing the plate. Two sources agreeing on the
//    mark beats one trace of it.
//  * github.com/libretro/glsl-shaders crt/shaders/crt-lottes.glsl — Lottes'
//    scanline weight, the canonical parameterisation.
//  * github.com/lll2yu/sddm-lain-wired-theme (209*) — cited as a WARNING.
//    Its palette is #c1b492 beige / #d2738a pink, the show's SEPIA Lain. The
//    fan ecosystem has converged on a colour scheme this screen does not use.
//    Nothing here is sampled from it.
//
// -----------------------------------------------------------------------------
// THE LAW: EVERY STRATUM ADDS. THERE IS NO OPAQUE WINDOW IN THIS INTERFACE.
//
// Tile both frames 8x6, take each tile's 2nd-percentile luma. In the
// composite the floor is 5.1..13 across forty-two of forty-eight tiles and
// every outlier is a LIFT (42.5, 58.8 where the phrases pile up), never a
// dip. Nothing anywhere is darker than the bare plate. So every layer is
// `kPlus` over one #0F1023 ground and carries its own CONTRIBUTION, not a
// colour with an alpha. The moment anything occludes anything else it stops
// being Lain.
//
// The console frame obeys the same law with a second structure on top: its
// tile floors rise 0.9 at the surround to 65.4 at the middle of the window,
// and the min-luma cross-sections put the peak at (~510, ~340) both ways —
// so the body is not a fill, it is a RADIAL PEDESTAL, and the pedestal IS
// the Copland eye blurred into the phosphor. One Material::radialUnit whose
// twenty stops are the measured radial profile draws the pedestal and the
// eye's rings in a single SIMD gradient.
//
// -----------------------------------------------------------------------------
// WHAT THE MEASUREMENT CORRECTED — FIVE IN THE BRIEF, ONE IN ME
//
//  1. THE WINDOW IS NOT 16 PX OUT OF SQUARE, IT IS SHEARED, AND THE TWO BARS
//     SHEAR OPPOSITE WAYS. The brief reports the chrome bar's left end at
//     x=84 against a body at x=100 — one number for one offset. Per row, the
//     top bar's left end runs x=79 at y=22 to x=94 at y=50 and its right end
//     919 to 936: the whole bar is a PARALLELOGRAM leaning right at
//     dx/dy = +0.54. The bottom bar leans the other way, 94 at y=667 down to
//     83 at y=691, dx/dy = -0.46. Stating it as a 16 px overhang loses the
//     mechanism; it is one tube, and the bars are at different heights on it.
//  2. THE SHARPNESS FALLOFF IS 5.5x, NOT 4.5x — AND THE BRIEF'S "SAME
//     COLOUR, MORE BLUR" IS EXACTLY RIGHT, WHICH I GOT WRONG FIRST AND HAD TO
//     TAKE BACK. Measured mean|dI/dx| over x 134..900 per line:
//     1.63 2.07 2.51 1.49 2.63 1.53 3.13 2.77 **7.88** 3.48 3.54 2.58 2.23
//     1.77 1.42, so 5.54x peak-to-floor with the band at y 384..408 (the
//     brief's 4.5x is a floor it did not reach).
//
//     Then I claimed the amplitude falls too, because the p50 of each line's
//     band goes #152954 at the top against #3EADB8 in focus. THAT IS A
//     COVERAGE STATISTIC, NOT A BRIGHTNESS. Blurring a line spreads fixed ink
//     over more pixels, so its p50 drops with no change in how bright the ink
//     is. The statistic that answers the question is the per-line PEAK, and
//     over the fifteen lines it reads
//       115 111 127 136 170 184 208 206 222 213 199 176 155 132 112
//     — a 2.00x hump where mean|dI/dx| is a 5.54x one. For a stroke of width
//     w under a Gaussian of sigma s at CONSTANT ink, peak ~ erf(w/(2*sqrt2*s))
//     and gradient ~ peak/(w + 2s); with w = 2 px, sigma 0.30 -> 1.6 predicts
//     0.52x in peak and 5.5x in gradient. Both measurements fall out of ONE
//     sigma ramp and no amplitude term at all. So: fifteen lines, one colour,
//     sigma linear in distance from the band. Every version of this sketch
//     that dimmed the far lines looked wrong in a way I could see and could
//     not name until I measured the peak instead of the median.
//  3. THE SCANLINE PERIOD IS 4.42 PX, NOT 5. FFT of the row mean over a
//     text-free band (x 600..900, y 100..330), cubic-detrended: 4.42 px,
//     amplitude 8.05 luma p95-p5. The composite's is 9.82 px at 27 luma.
//     Ratio 2.22, so the brief's "the difference is how much of the tube
//     each shot sees" is right; I hold the console's 4.42 because the
//     console is the anchor.
//  4. A DARK OUTLINE IS UNREACHABLE UNDER THE ADDITIVE LAW, AND UNNEEDED.
//     The brief asks for `make me feel alright?` in white with a 2 px dark
//     outline. Nothing subtracts in a kPlus stack. It is also unnecessary:
//     that arc crosses plate whose 2nd-pct floor is 5.5, already the floor,
//     so the "outline" in the frame is the ABSENCE of the glyph's own light.
//     Built as a bright core with a tight halo and no dark pass at all.
//  5. THE TEXT ADVANCE IS 13.15 PX, NOT 13.6. The in-focus line's ink spans
//     x 129..905 = 776 px, and the line is 60 characters, not 56 — the brief
//     counted `.frame  $fp,40,$31    # vars= 8, ...` short. 776/59 = 13.15.
//     Line pitch 37.5 CONFIRMED exactly (autocorrelation of the row-gradient
//     over the text block peaks at 37 and 75).
//  6. THE WIREFRAME IS A HYPERBOLOID AND IT SOLVES. Two anchor points off
//     the wire mask — (845, 200) on the upper sweep and (322, 123) at the
//     top-left — plus the waist ellipse measured directly at centre
//     (498, 342), semi-axes 172 x 28, give ONE consistent solution:
//     eccentricity e = 28/172 = 0.163, rim radius R = 376, rims at y = 176
//     and 508, and therefore cos(phi) = 172/376, a twist of 125.6 degrees
//     between the rims. Residual on the second anchor 0.4%. The generatrices
//     are then not decoration, they are the ruling of the surface.
//
// -----------------------------------------------------------------------------
// AND ONE CORRECTION TO MY OWN ASSIGNMENT, WHICH THE RESEARCHER MADE FIRST
//
// LAIN'S LINES ARE NOT WOBBLY. `ops::sketchy` is the wrong instinct and I did
// not use it. Hairline cross-sections in both frames are smooth single-lobe
// profiles, 2 px, no tremor — mechanically clean. What reads as hand-made is
// PLACEMENT (nothing is aligned to anything, no element is centred, no
// spacing repeats, four strata run off the edge) and OPTICS (everything is
// defocused by a DIFFERENT amount). This interface carries NO FURNITURE —
// no ticks, arrows, brackets, leader lines, registration marks or plate
// numbers. Standing order 3 is answered by the dotted ellipses, by the
// generatrices that stop short of the rims, and by the additive stack, not
// by decorating. Adding a bracket would make it a Winamp skin.
//
// -----------------------------------------------------------------------------
// BUILT FROM (the library, not by hand)
//   Material::radialUnit (20 stops)   the pedestal AND the eye's rings, one
//                                     SIMD gradient — an SkSL equivalent is
//                                     ~7x slower for the same picture
//   Material::linearUnit              each chrome bar's INVERSE bevel, its
//                                     six stops read off the bar's own rows
//   shapes::parametric                every ellipse: the rims, the waist and
//                                     the tilted orbit, as real curves
//   LayeredBrush{blend = kPlus}       THE ADDITIVE TRICK. See PERF: kPlus on
//                                     a NODE is a saveLayer, kPlus on a
//                                     stroke PASS is a path draw
//   TextStyle::paint maskFilter       the focal plane: per-line blur sigma
//                                     on the glyph MASK, which Skia caches —
//                                     15 sigmas x 2 passes (core + phosphor
//                                     halo), zero layers (see the GAP)
//   TextPath{orient = Tangent}        `make me feel alright?` on the orbit's
//                                     lower-left arc
//   PathFormat dash + round cap       the broken ellipses (1.6 on 4.4)
//   Composer::renderSlot              the wireframe turns at 6 Hz and the
//                                     console scrolls at 4.5 Hz in two
//                                     independent slots, so neither dirties
//                                     the other or the baked back stack
//   Cache::Texture                    the plate, the pedestal, the eye, the
//                                     bars, the panel, the streaks and the
//                                     tube — thirteen bakes, NO bakeScale
//                                     anywhere (see PERF: the dial is a trap
//                                     on a CPU raster host)
//   ctx.measure()                     the mono size is SOLVED so the advance
//                                     lands on the measured 13.15 px
//
// -----------------------------------------------------------------------------
// PERF — 41.6 ms -> 3.64 ms, and three of the five steps were surprises
//
//   41.6  the honest build of the brief: nine strata, each a node carrying
//         .blend(kPlus) and .effect(Effect::filter(Blur)). Nine bounded
//         saveLayers and nine blurs of a 0.73 MP canvas per frame.
//   12.4  the blurs deleted from the layers and rebuilt where the light
//         actually is: per-line SkMaskFilter on the text paint (Skia caches
//         blurred glyph masks) and blurSigma on the stroke passes.
//   11.1  kPlus moved OFF the nodes and ONTO the paints. Paint.cpp's
//         `leafDirectBlend` already routes a fill-only leaf's blend straight
//         onto the fill paint — but a node with ANY decoration is excluded,
//         so every stroked ellipse was still allocating a layer. LayeredBrush
//         carries a per-pass SkBlendMode, so the additive law costs path
//         draws: 9 layers -> 0.
//    8.7  `plate` was 3.45 ms of a 11.07 ms frame as a "texture", which is
//         nonsense for a blit until you read the exclusion: Cache::Texture is
//         excluded from leafDirectBlend, so a BAKED node carrying .blend(kPlus)
//         allocates a bounded 1016x720 saveLayer every frame and blits into
//         it. The plate is the BOTTOM of the stack and has nothing under it to
//         add to, so the ground folds into its shader and it composites
//         kSrcOver: 3.45 -> 0.24 ms, and no layer.
//    3.8  **bakeScale(0.5) REMOVED FROM EVERY NODE THAT HAD IT.** This is
//         the one I did not expect and it is worth stating plainly, because
//         API.md recommends the dial for exactly the content I was using it on
//         ("planes whose content is soft anyway"): a reduced bake is cheap to
//         MAKE and then every BLIT of it is an upscaling resample, forever. On
//         this host that is mean 11.07 ms -> 4.31 ms for six nodes, of which
//         the full-canvas plate alone was 3.0 ms. bakeScale pays only when the
//         bake is re-taken often; for a static bake blitted 60 times a second
//         it is a pure loss. The doc should say so.
//    3.64 / p99 6.37, after the last one: the scanline creep's bound
//         translateY moved OFF the Texture-cached node and onto a wrapper
//         box. With the binding on the baked node itself, every whole-pixel
//         step of the creep RE-BAKES the 1016x744 SkSL layer — p99 3.64 ->
//         33.4 ms, three spikes in five seconds, one per creep step, while p50
//         never moved. A p50 that looks fine and a p99 seven times worse is
//         exactly the failure --bench exists to catch.
//
//   p50 3.64 ms / p99 6.37 ms / mean 3.84 at 1016x720 over 300 frames — the
//   whole additive stack plus a thirty-draw text block with per-line blur is
//   4 ms, and no node in it reaches half a millisecond. Every stratum of the
//   additive stack is a path or glyph draw; the only saveLayers in the frame
//   are the Texture-cached nodes that genuinely must add (pedestal, eye, two
//   bars, panel, five streaks), each bounded to its own box.
//
// -----------------------------------------------------------------------------
// LIBRARY GAP — a blur whose radius varies with position
//
// The screen's defining feature is a 5.5x sharpness gradient INSIDE ONE
// BLOCK OF TEXT, and `effect()` applies exactly one filter to a node's whole
// layer. Route (b) of the brief — one `Effect::shader` whose SkSL samples the
// layer with a y-dependent kernel — IS reachable, and I checked rather than
// assumed: Compose.h documents that "the layer arrives as the child shader
// named content", so a runtime effect can absolutely read its own rendered
// layer at arbitrary offsets. What it cannot do is vary the TAP COUNT: SkSL
// has no cheap dynamic loop bound, so a 3-sigma separable kernel must be sized
// for the worst sigma in the node (21 taps at 1.6 px over a 824x562 layer) and
// paid at every pixel including the sharp band. Route (a), fifteen keyed
// one-line nodes each with its own filter, is fifteen saveLayers.
//
// What I did instead costs no layer at all: the sigma rides on the TEXT PAINT
// as an SkMaskFilter, per line, and Skia caches blurred glyph masks per
// (font, sigma). Fifteen sigmas over thirty draws (core plus phosphor halo),
// all cache hits after the first frame, and the block does not appear in
// --bench's six most expensive nodes at all.
//
// But that escape exists only because the varying thing is TEXT. A varying
// blur over the wireframe or the plate has no equivalent, and this is the SAME
// SHAPE of gap as cine-01's `Material::worldSpace` citation: EFFECTS AND
// MATERIALS BOTH LACK A SPATIALLY-VARYING PARAMETER CHANNEL. Two independent
// citations, two unrelated domains, two consecutive briefs. What the API wants
// is a filter whose scalar parameter is a function of node-local position —
// resolved once into a two- or three-level pyramid and blended by that
// function, which is how a real defocus is done and is O(1) in the sigma range
// rather than O(sigma).
//
// Three smaller ones, all worked around in this file:
//
//  * `LayeredBrush` declares no `bleed()`, so a blurred additive stack is
//    culled at its node's own bounds. Already reported by the MAGI study;
//    still true. Wrapped in an eight-line scheme that declares reach.
//  * A Texture-cached node whose DECORATION paints kPlus must also carry
//    `.blend(kPlus)` on the node, or the bake composites kSrcOver and paints
//    its blurred stroke straight over the plate. The eye came back as two
//    black lozenges. Paint.cpp knows this and says it in a comment; nothing in
//    API.md does.
//  * `bakeScale` has no warning about the blit. See PERF.
//
// Run:
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/lain_navi.cpp \
//       --frame /tmp/lain_navi.png --at 2.5
//
//   2.5 s  THE REFERENCE MOMENT, and it is chosen so the diff is like for
//          like: the scroll is phased (kScrollPhase) to put the plate's own
//          `.frame $fp,40,$31  # vars= 8, ...` line in the focal band, which
//          is the line every published sharpness number is anchored on.
//          `no double minds` is at full bloom over `COVer me`.
//   0..13.6 s  the documented Layer 07 phrase sequence, looping — "no double
//          minds" -> "make me feel all right" -> "With no wings...with no
//          things...one love" -> "make me mad", 0.8 s of overlap so two
//          phrases coexist and ADD where they cross
//   forever the console scrolls one line per 220 ms THROUGH a stationary
//          focal plane at y=402, the hyperboloid turns once per 24 s, the
//          focus breathes +-0.4 px at 0.15 Hz and the phosphor dips every 4 s
// =============================================================================

#include <sigilsketch/Sketch.h>

#include <sigilcompose/Brushes.h>
#include <sigilcompose/Lines.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Shapes.h>
#include <sigilcompose/Util.h>

#include <sigilweave/ports/SystemFontManager.h>

#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkTypeface.h>
#include <include/effects/SkRuntimeEffect.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace sigil::compose;
using namespace sigil::compose::util;
using namespace std::chrono_literals;
namespace weave = sigil::weave;
namespace ch = choreograph;

namespace lain {

// ---------------------------------------------------------------------------
// THE FRAME. 1016x720 — both reference plates' own size, so a capture diffs.

constexpr float kW = 1016.0f, kH = 720.0f;

inline SkColor4f hex(uint32_t v, float a = 1.0f) {
  return {(float)((v >> 16) & 255) / 255.0f, (float)((v >> 8) & 255) / 255.0f,
          (float)(v & 255) / 255.0f, a};
}
inline SkColor4f dim(SkColor4f c, float k) {
  return {c.fR * k, c.fG * k, c.fB * k, c.fA};
}
inline SkColor4f mix(SkColor4f a, SkColor4f b, float t) {
  return {a.fR + (b.fR - a.fR) * t, a.fG + (b.fG - a.fG) * t,
          a.fB + (b.fB - a.fB) * t, a.fA + (b.fA - a.fA) * t};
}

// ---------------------------------------------------------------------------
// PALETTE — every entry is a CONTRIBUTION, i.e. what this stratum ADDS to the
// #0F1023 ground under kPlus, computed as (measured colour - ground). Written
// that way on purpose: a colour with an alpha would have to darken something
// to read, and nothing in this interface darkens anything.

const SkColor4f kGround = hex(0x060719);      // the plate's own p10. #0F1023 is
                                              // its p50 — see the note in setup()
const SkColor4f kProse = hex(0x1B2138);       // Japanese glyph peaks, +46+54+77
const SkColor4f kPanel = hex(0x101F27);       // the lightened panel, +16+31+39
const SkColor4f kBodyMid = hex(0x475F86);     // pedestal centre  (#334D83)
const SkColor4f kBodyEdge = hex(0x040A24);    // pedestal edge    (#151E52)
const SkColor4f kBarTopHi = hex(0x587962);    // top bar bright edge   (#67899E)
const SkColor4f kBarTopLo = hex(0x234A3C);    // top bar dark middle   (#325A74)
const SkColor4f kBarBotHi = hex(0x6FA586);    // bottom bar peak       (#7EB5CA)
const SkColor4f kBarBotLo = hex(0x578C70);    // bottom bar middle     (#669CB1)
const SkColor4f kRail = hex(0x1D3242);        // side hairlines, dimmer than bars
const SkColor4f kConsoleInk = hex(0x46C89A);  // ~ #84FFFF - #425689, the add the
                                              // reference's own body demands; the
                                              // green is trimmed against the
                                              // measured core #72F9F5, not guessed
const SkColor4f kWire = hex(0x3A6257);        // hairline peak #5683AD on a
                                              // #1C2156 ground, dLuma +55
const SkColor4f kMinds = hex(0xA6B7BE);       // `no double minds`, +166+183+190
const SkColor4f kAlright = hex(0xB3B6BF);     // `make me feel alright?` core
const SkColor4f kCover = hex(0x7A3416);       // `COVer me`, dr-db = +40
const SkColor4f kMagenta = hex(0x3A1B3C);     // the streaks, p90 #603871
const SkColor4f kWordmark = hex(0x2B3A54);    // the rotated Copland lockup

// ---------------------------------------------------------------------------
// THE WINDOW. Measured off Layer 04 and shifted +58 in x, so the reconstruction
// reads as a WINDOW ON A DESKTOP rather than the whole screen: 137 px of plate
// on its left, 22 on its right. That asymmetry is the composition — see the
// header on placement. Vertically the bars are pulled in to 62 and 646 so the
// 15-line block keeps its measured 37.5 px pitch inside a shorter body.

constexpr float kBodyL = 160.0f, kBodyR = 984.0f;
constexpr float kBodyT = 56.0f, kBodyB = 668.0f;
constexpr float kBarTopT = 62.0f, kBarTopB = 92.0f;   // 30 rows, measured
constexpr float kBarBotT = 646.0f, kBarBotB = 672.0f; // 26 rows, measured
// The shear. dx/dy from the per-row edge table: the top bar leans right, the
// bottom bar leans left, and they are NOT the same magnitude.
constexpr float kShearTop = 0.54f, kShearBot = -0.46f;
constexpr float kBarTopL = 137.0f, kBarTopR = 977.0f; // at the bar's TOP row
constexpr float kBarBotL = 152.0f, kBarBotR = 995.0f; // at the bar's TOP row

/** A chrome bar is a PARALLELOGRAM, not a rect — the one measurement in the
 *  window that a rounded-rect frame cannot express. Authored in the node's own
 *  local box so the bevel material can be a linearUnit ramp. */
inline shapes::OutlineFn barOutline(float shear) {
  return [shear](SkSize s) {
    const float d = shear * s.height();
    SkPathBuilder b;
    b.moveTo(0, 0);
    b.lineTo(s.width(), 0);
    b.lineTo(s.width() + d, s.height());
    b.lineTo(d, s.height());
    b.close();
    return b.detach();
  };
}

/** The inverse bevel: bright edge, dark middle, bright edge. Read straight off
 *  the bar's own rows (the top bar's mid-luma runs 73 106 125 131 128 107 95
 *  89 ... 85 83 89 99 101 107 125 137 122 93 66 down its 30 rows), which is
 *  where the "brushed capstan" reading comes from — it is not a highlight, it
 *  is a cylinder lit from outside its own silhouette. */
inline Material barBevel(SkColor4f hi, SkColor4f lo, float bias) {
  auto at = [&](float k) { return mix(lo, hi, k); };
  return Material::linearUnit({0, 0}, {0, 1},
                              {{0.00f, dim(at(0.05f), bias)},
                               {0.13f, dim(at(1.00f), bias)},
                               {0.30f, dim(at(0.32f), bias)},
                               {0.62f, dim(at(0.28f), bias)},
                               {0.87f, dim(at(1.00f), bias)},
                               {1.00f, dim(at(0.02f), bias)}});
}

// ---------------------------------------------------------------------------
// THE PEDESTAL, WHICH IS THE EYE.
//
// Radial mean luma about (500, 335) on the console plate, 10th percentile, in
// 12 px annuli: 90.6 87.8 82.6 70.9 73.1 76.1 71.8 72.1 71.2 70.1 63.1 54.3
// 51.9 50.0 48.4 ... 42.1 36.3 30.9 23.6. That is not a smooth falloff — the
// dip at r=36 and the recovery at r=60 ARE the eye's iris gap and iris ring,
// blurred into the phosphor until they read as a pedestal. So the pedestal and
// the watermark are the same object and one gradient draws both.

constexpr float kEyeAt[19] = {0, 12, 24, 36, 48, 60,  72,  84,  96, 108,
                              120, 132, 144, 156, 168, 192, 228, 276, 336};
constexpr float kEyeK[19] = {1.000f, 0.961f, 0.889f, 0.728f, 0.759f, 0.800f,
                             0.741f, 0.745f, 0.732f, 0.717f, 0.621f, 0.500f,
                             0.466f, 0.441f, 0.418f, 0.332f, 0.252f, 0.177f,
                             0.077f};

inline Material pedestal() {
  std::vector<Stop> stops;
  stops.reserve(20);
  for (int i = 0; i < 19; ++i)
    stops.push_back({kEyeAt[i] / 359.0f, mix(kBodyEdge, kBodyMid, kEyeK[i])});
  stops.push_back({1.0f, dim(kBodyEdge, 0.55f)});
  // radius01 is a fraction of the HALF-DIAGONAL (513 px for this body), so
  // 0.70 puts the last measured annulus (r=336) at its own radius.
  return Material::radialUnit({0.483f, 0.456f}, 0.70f, std::move(stops));
}

/** The Copland eye's TOPOLOGY, from the ASCII transcription rather than a
 *  trace: two arc eyelids, four satellite dots on the diagonals, a stem
 *  descending below the iris. The iris itself is already in the pedestal
 *  gradient above, so this is only the parts a radial ramp cannot say. */
inline SkPath eyeFurniture(SkPoint c, float r) {
  SkPathBuilder b;
  // the two eyelids: arcs bowing away from the iris, left and right
  for (int side : {-1, 1}) {
    const float x0 = c.fX + side * r * 0.62f;
    b.moveTo(x0, c.fY - r * 0.78f);
    b.quadTo(c.fX + side * r * 1.62f, c.fY, x0, c.fY + r * 0.78f);
  }
  // four satellites on the diagonals
  const float k = 0.7071f;
  for (int sx : {-1, 1})
    for (int sy : {-1, 1})
      b.addCircle(c.fX + sx * k * r * 1.30f, c.fY + sy * k * r * 1.30f,
                  r * 0.155f);
  // the stem
  b.moveTo(c.fX, c.fY + r * 0.95f);
  b.lineTo(c.fX, c.fY + r * 1.52f);
  return b.detach();
}

// ---------------------------------------------------------------------------
// THE HYPERBOLOID. Solved, not eyeballed — see correction 6.
//
// One ruled surface: two rim circles of radius R at +-h on a vertical axis,
// the top one twisted by 2*phi against the bottom, and the ruling lines
// between them. Orthographic projection foreshortens the axis to e = 0.163,
// which is exactly the waist ellipse's own 28/172. The waist radius is
// R*cos(phi), so ONE bound phase animates eccentricity and waist together and
// the whole figure turns without a 3D transform anywhere.

// The wireframe is placed 26 px left and 34 px down of where it measures on
// the Layer 07 plate. That is a RECONSTRUCTION decision and it is the only one
// in the file: the two plates do not share an origin, and at the measured
// place the orbit's lower arc — and therefore `make me feel alright?` — lands
// exactly on the console's focal line, which is the one line in the frame that
// must stay legible. Moved, it crosses `.fmask` and `subu` instead, both
// already defocused. Everything else here is where it measures.
constexpr SkPoint kWireShift{-26.0f, 34.0f};
constexpr SkPoint kAxis{498.0f + kWireShift.fX,
                        342.0f + kWireShift.fY}; // waist centre, measured
constexpr float kRim = 376.0f;             // solved from two anchor points
constexpr float kEcc = 0.163f;             // 28/172, measured on the waist
constexpr float kHalfH = 166.0f;           // solved with R
constexpr float kPhi0 = 62.8f;             // acos(172/376): the measured twist

/** A rim/waist circle of radius @p rad at axial height @p z, projected. */
inline SkPoint onRim(float rad, float z, float ang) {
  return {kAxis.fX + rad * std::cos(ang),
          kAxis.fY - z + rad * kEcc * std::sin(ang)};
}

/** The ruling: N straight generatrices from the bottom rim to the top rim,
 *  each TRIMMED 7% short at both ends. The gaps are the brief's, and they are
 *  the one place this interface admits it was placed by a hand. */
inline SkPath generatrices(float phiDeg, int n) {
  const float phi = phiDeg * 0.01745329f;
  SkPathBuilder b;
  for (int i = 0; i < n; ++i) {
    const float a = 6.2831853f * (float)i / (float)n;
    const SkPoint lo = onRim(kRim, -kHalfH, a - phi);
    const SkPoint hi = onRim(kRim, +kHalfH, a + phi);
    const SkPoint d{hi.fX - lo.fX, hi.fY - lo.fY};
    b.moveTo(lo.fX + d.fX * 0.07f, lo.fY + d.fY * 0.07f);
    b.lineTo(lo.fX + d.fX * 0.93f, lo.fY + d.fY * 0.93f);
  }
  return b.detach();
}

/** The tilted second orbit — conic-fit through ten points traced along the
 *  curve `make me feel alright?` rides: centre (491, 266), semi-axes
 *  373 x 187, tilt -17.4 deg, residual <= 13 px on hand-traced input. It does
 *  NOT share the hyperboloid's inclination, which is the whole point: two
 *  ellipses, one centre, different planes. */
constexpr SkPoint kOrbit2C{491.0f + kWireShift.fX, 266.0f + kWireShift.fY};
constexpr float kOrbit2A = 373.0f, kOrbit2B = 187.0f, kOrbit2Tilt = -17.4f;

inline SkPath ellipsePath(SkPoint c, float a, float b, float tiltDeg,
                          float t0 = 0.0f, float t1 = 6.2831853f) {
  const float th = tiltDeg * 0.01745329f;
  const float ct = std::cos(th), st = std::sin(th);
  SkPathBuilder p;
  const int n = 168;
  for (int i = 0; i <= n; ++i) {
    const float t = t0 + (t1 - t0) * (float)i / (float)n;
    const float x = a * std::cos(t), y = b * std::sin(t);
    const SkPoint q{c.fX + x * ct - y * st, c.fY + x * st + y * ct};
    if (i == 0)
      p.moveTo(q);
    else
      p.lineTo(q);
  }
  if (t1 - t0 > 6.28f)
    p.close();
  return p.detach();
}

// ---------------------------------------------------------------------------
// TYPE.

inline sk_sp<SkTypeface> face(std::initializer_list<const char *> families,
                              int weight, SkFontStyle::Slant slant) {
  auto mgr = weave::ports::systemFontManager();
  for (const char *f : families)
    if (sk_sp<SkTypeface> t = mgr->matchFamilyStyle(
            f, SkFontStyle(weight, SkFontStyle::kNormal_Width, slant)))
      return t;
  return mgr->matchFamilyStyle(nullptr, SkFontStyle::Normal());
}

inline const sk_sp<SkTypeface> &monoFace() {
  // "light-weight mono" — the brief's Latin-only substitute, since the console
  // block is pure ASCII. ExtraLight (200) is the closest match to the frame's
  // hairline stems.
  static sk_sp<SkTypeface> f =
      face({"JetBrainsMono Nerd Font", "JetBrains Mono", "Andale Mono",
            "Menlo", "Courier New"},
           // LIGHT, not ExtraLight. At 22 px under a mask-filter blur an
           // ExtraLight stem never reaches the clip, so the in-focus core came
           // back #70BDE8 (blue) where the plate reads #72F9F5 (cyan) — the
           // colour is right and the STEM is too thin to show it.
           300, SkFontStyle::kUpright_Slant);
  return f;
}
inline const sk_sp<SkTypeface> &minchoFace() {
  static sk_sp<SkTypeface> f =
      face({"Hiragino Mincho ProN", "YuMincho", "Shippori Mincho",
            "Noto Serif JP", "Hiragino Sans"},
           400, SkFontStyle::kUpright_Slant);
  return f;
}
inline const sk_sp<SkTypeface> &serifFace() {
  static sk_sp<SkTypeface> f =
      face({"Times New Roman", "Times", "Georgia"}, 400,
           SkFontStyle::kUpright_Slant);
  return f;
}
inline const sk_sp<SkTypeface> &serifItalicFace() {
  static sk_sp<SkTypeface> f =
      face({"Times New Roman", "Times", "Georgia"}, 700,
           SkFontStyle::kItalic_Slant);
  return f;
}
inline const sk_sp<SkTypeface> &markerFace() {
  static sk_sp<SkTypeface> f =
      face({"Marker Felt", "Noteworthy", "Bradley Hand", "Chalkboard"}, 400,
           SkFontStyle::kUpright_Slant);
  return f;
}

/** THE ONE TEXT STYLE IN THE FILE, and it is the focal plane.
 *
 *  `blend` goes on the PAINT, not the node: a node-level kPlus is a bounded
 *  saveLayer per stratum (nine of them, 12 ms), a paint-level one is free.
 *  `sigma` goes on the glyph MASK, not on a layer filter: Skia caches blurred
 *  glyph masks per (font, sigma), so fifteen sigmas over fifteen lines cost
 *  fifteen cached mask draws and zero layers. That is the whole answer to the
 *  gap in the header. */
inline weave::TextStyle type(const sk_sp<SkTypeface> &tf, float size,
                            SkColor4f c, float sigma = 0.0f,
                            float track = 0.0f) {
  weave::TextStyle s;
  s.shaping.typeface = tf;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = track;
  s.paint.foreground.setColor4f(c, nullptr);
  s.paint.foreground.setAntiAlias(true);
  s.paint.foreground.setBlendMode(SkBlendMode::kPlus);
  if (sigma > 0.01f)
    s.paint.foreground.setMaskFilter(
        SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, sigma));
  return s;
}

// ---------------------------------------------------------------------------
// THE CONSOLE LISTING.
//
// Indices 0..15 are VERBATIM off the Layer 04 frame, spacing included, down to
// `jal start3D` at the bottom of the window. Indices 16..37 are MINE: the same
// gcc -S MIPS output continued for the same translation unit so the scroll has
// material, written in the dialect the frame itself establishes (the
// `.frame`/`.mask`/`.fmask` triple with GCC's `# vars= N, regs= N/N, args= N,
// extra=` annotation; $28 as the GOT pointer; $L-prefixed local labels).
// Flagged here rather than smuggled in — the sixteen that are evidence and the
// twenty-two that are pastiche are not the same thing.
//
// kScrollPhase (above) is chosen so index 8 — the plate's own
// `.frame $fp,40,$31 ...`, the line every published sharpness number is
// anchored on — sits in the focal band at t = 2.5 s.

const char *const kListing[] = {
    "lw    $31,20($sp)",
    "lw    $fp,16($sp)",
    "addu  $sp,$sp,24",
    "j     $31",
    ".end  _3D_Flip",
    ".text",
    ".ent  _3D_Polling",
    "_3D_Polling:",
    ".frame  $fp,40,$31    # vars= 8, regs= 2/0, args= 24, extra=",
    ".mask   0xc0000000,-4",
    ".fmask  0x00000000,0",
    "subu  $sp,$sp,40",
    "sw    $31,36($sp)",
    "sw    $fp,32($sp)",
    "move  $fp,$sp",
    "jal   start3D",
    "lw    $2,%got(_navi_ctx)($28)",
    "nop",
    "lw    $3,4($2)",
    "beq   $3,$0,$L18",
    "nop",
    "addu  $4,$fp,24",
    "jal   _3D_Sync",
    "move  $5,$3",
    "$L18:",
    "lw    $31,36($sp)",
    "lw    $fp,32($sp)",
    "addu  $sp,$sp,40",
    "j     $31",
    ".end  _3D_Polling",
    ".text",
    ".ent  _Layer_Commit",
    "_Layer_Commit:",
    ".frame  $fp,24,$31    # vars= 0, regs= 2/0, args= 16, extra=",
    ".mask   0xc0000000,-4",
    ".fmask  0x00000000,0",
    "subu  $sp,$sp,24",
    "sw    $31,20($sp)",
};
constexpr int kListingN = (int)(sizeof(kListing) / sizeof(kListing[0]));

constexpr int kLines = 15;
constexpr float kPitch = 37.5f;      // measured: autocorrelation peaks 37, 75
constexpr float kAdvance = 13.15f;   // measured: 776 px / 59 gaps
constexpr float kTextX = 188.0f;     // 130 measured + the window's 58 px shift
constexpr float kFirstBase = 112.0f;
constexpr float kFocus = 402.0f;     // the stationary focal plane
constexpr int kScrollPhase = 27;     // see the note above kListing

/** CONSTANT INK, LINEAR SIGMA. See correction 2: the amplitude does not fall,
 *  and every version of this sketch that made it fall was reading a coverage
 *  statistic and calling it brightness. 0.30 px at the band to 1.43 at the
 *  block's ends is a 5.4x sigma swing, which is what the reference's own
 *  5.54x in mean|dI/dx| and 1.98x in per-line PEAK jointly solve to. */
inline float focusSigma(float baselineY) {
  const float d = std::fabs(baselineY - kFocus);
  return 0.30f + d * 0.00390f; // 0.30 at the band, 1.43 at the block's ends
}

// ---------------------------------------------------------------------------
// THE PROSE. Transcribed off the Layer 07 plate where it is legible — it is a
// passage arguing that what looks like psychic force is the body's own latent
// capacity ("the fabled strength of a man at a fire"). The runs the tube ate
// are FILLED IN THE SAME REGISTER and are not a transcription; the plate is a
// blurred second-generation capture and I am not going to pretend otherwise.

const char8_t *const kProseLines[] = {
    u8"おおよそ人の能力というものは、その",
    u8"されがちである。しかしながら、これを",
    u8"肉体の物理的発生源との関係を考察するに",
    u8"ずとその制限が明らかとなる。したがって一応に",
    u8"われるような特殊な能力を人間が持ちうるとすれば",
    u8"ている可能性は決して低くはない、と言える。",
    u8"それは特別なものではなく、誰にでも",
    u8"えられる。ここで一つの例を挙げるならば",
    u8"俗にいう『火事場の馬鹿力』である。",
    u8"これが精神の力によってなしえるものでなく",
    u8"肉体の持つ潜在的能力によるものである",
    u8"考えはいささか乱暴ではあるが",
    u8"えるのでなかろうか。",
    u8"アルツハイマー病に苦しむ老人に肉体的な",
    u8"訓練を施し、改善したというケースは、その建",
};
constexpr int kProseN = (int)(sizeof(kProseLines) / sizeof(kProseLines[0]));

// ---------------------------------------------------------------------------
// THE DOCUMENTED PHRASE SEQUENCE (lain.wiki, Layer 07).

struct Phrase {
  const char8_t *text;
  double at, hold;
  SkPoint centre;
  float size;
};
// Phased so t = 2.5 s — the capture moment, and the moment the reference
// plate shows — sits in `no double minds`'s hold, with `COVer me` and the
// on-path `make me feel alright?` beside it. The sequence then runs on and
// loops at 13.6 s.
const Phrase kPhrases[] = {
    {u8"no double minds", 0.5, 3.0, {546, 42}, 60},
    {u8"make me feel all right", 4.2, 2.8, {486, 44}, 48},
    {u8"With no wings…with no things…one love", 7.6, 2.6, {520, 42}, 40},
    {u8"make me mad", 10.7, 2.6, {418, 40}, 60},
};
constexpr int kPhraseN = (int)(sizeof(kPhrases) / sizeof(kPhrases[0]));

// ---------------------------------------------------------------------------
// THE TUBE. Lottes' scanline weight, period 4.42 px (measured, correction 3),
// plus the composite's own 9.82 px beat at a quarter weight — one tube, two
// shots, both beats present. Baked once, crept by whole pixels.

inline sk_sp<SkRuntimeEffect> crtEffect() {
  static sk_sp<SkRuntimeEffect> fx = [] {
    auto [effect, err] = SkRuntimeEffect::MakeForShader(SkString(
        "uniform float2 uResolution;\n"
        "half4 main(float2 xy) {\n"
        // Lottes: a raised-cosine beam profile about the scanline centre.
        "  float f = fract(xy.y / 4.42) - 0.5;\n"
        "  float w = exp2(-2.6 * f * f * 4.0);\n"
        "  float g = fract(xy.y / 9.82) - 0.5;\n"
        "  float w2 = exp2(-1.4 * g * g * 4.0);\n"
        "  float dark = (1.0 - w) * 0.032 + (1.0 - w2) * 0.014;\n"
        // the corner falloff of a curved tube, gentle: this plate is shot
        // close, so the vignette barely enters frame
        "  float2 p = (xy / max(uResolution, float2(1.0)) - 0.5) * 2.0;\n"
        "  float vig = smoothstep(1.05, 1.90, length(p / 0.86)) * 0.30;\n"
        "  float gr = fract(sin(dot(floor(xy), float2(12.9898, 78.233)))\n"
            "            * 43758.5453) - 0.5;\n"
            "  dark = clamp(dark + gr * 0.055, 0.0, 1.0);\n"
            "  return half4(0.0, 0.0, 0.0, half(clamp(dark + vig, 0.0, 1.0)));\n"
        "}\n"));
    if (!effect)
      SkDebugf("lain crt shader: %s\n", err.c_str());
    return effect;
  }();
  return fx;
}

/** The base plate: a photographed city at night, defocused past recognition.
 *  Not an image — a low-frequency field, because the one thing the frame says
 *  about it is that it has NO edges anywhere: its 8x6 tile floor is flat to
 *  sd 10.8 and every lift is a stratum above it, not a feature in it. */
inline sk_sp<SkRuntimeEffect> plateEffect() {
  static sk_sp<SkRuntimeEffect> fx = [] {
    auto [effect, err] = SkRuntimeEffect::MakeForShader(SkString(
        "uniform float2 uResolution;\n"
        "float h(float2 p){return fract(sin(dot(p,float2(12.9898,78.233)))"
        "*43758.5453);}\n"
        "float n(float2 p){float2 i=floor(p),f=fract(p);"
        "f=f*f*(3.0-2.0*f);"
        "return mix(mix(h(i),h(i+float2(1,0)),f.x),"
        "mix(h(i+float2(0,1)),h(i+float2(1,1)),f.x),f.y);}\n"
        "half4 main(float2 xy) {\n"
        "  float2 u = xy / max(uResolution, float2(1.0));\n"
        // vertical slabs: the buildings, three octaves, heavily smeared in x
        "  float b = n(float2(u.x*7.0, u.y*1.6)) * 0.62\n"
        "          + n(float2(u.x*17.0, u.y*2.4)) * 0.26\n"
        "          + n(float2(u.x*3.0, u.y*0.7)) * 0.30;\n"
        "  b = clamp(b - 0.52, 0.0, 1.0);\n"
        // the right third of the plate carries the bright massing
        "  b *= 0.13 + 1.55 * smoothstep(0.80, 1.14, u.x);\n"
        "  b *= 0.34 + 0.80 * smoothstep(1.02, 0.10, u.y);\n"
        "  half3 c = half3(half(0.0235 + b*0.34), half(0.0275 + b*0.40),\n"
            "                  half(0.098 + b*0.72));\n"
        "  return half4(c, 1.0);\n}\n"));
    if (!effect)
      SkDebugf("lain plate shader: %s\n", err.c_str());
    return effect;
  }();
  return fx;
}

/** An additive stroke pass. THE load-bearing call of the whole sketch: kPlus
 *  lives on the pass, so a stratum of strokes is path draws and not a layer.
 *  `blurSigma` is an SkMaskFilter on the stroke mask, bounded by the shape —
 *  the same trick the MAGI study measured at 4 ms against 62. */
inline LayeredBrush add(float width, SkColor4f c, float sigma = 0.0f,
                        std::vector<SkScalar> dash = {}) {
  return LayeredBrush{
      {{width, c, sigma, std::move(dash), 0, SkBlendMode::kPlus, true}}};
}

/** LIBRARY GAP, worked around in eight lines and already reported once by the
 *  MAGI study: `LayeredBrush` declares no `bleed()`, so a blurred additive
 *  stack is culled at its node's own bounds when the subtree records. Wrapping
 *  it in a value scheme that DOES declare reach fixes it. The header should
 *  grow `float bleed() const` returning widest width/2 + 3 sigma. */
struct Reach {
  LayeredBrush brush;
  float reach = 12.0f;
  bool operator==(const Reach &o) const {
    return brush == o.brush && reach == o.reach;
  }
  float bleed() const { return reach; }
  void paint(SkCanvas &c, const PaintContext &ctx) const {
    brush.paint(c, ctx);
  }
};
inline Reach reach(LayeredBrush b, float r) { return Reach{std::move(b), r}; }

} // namespace lain

// =============================================================================

struct LainNavi : sigil::compose::sketch::Sketch {
  using Sketch::Sketch;

  ch::Output<float> creep{0};    // scanline creep, whole px
  ch::Output<float> flicker{0};  // phosphor dip
  ch::Output<float> breathe{0};  // the camera hunting focus, 0.15 Hz

  int scrollLine = 0;   // console scroll, one line / 220 ms
  int orbitStep = 0;    // hyperboloid twist, 6 Hz over 24 s
  int phraseStep = 0;   // the Layer 07 sequence, 12 Hz
  float monoSize = 22.0f;
  float proseSize = 30.0f;

  // --- the console block: 15 lines, each with its own sigma ------------------
  Element consoleText() const {
    using namespace lain;
    auto g = box().absolute().inset(0);
    for (int i = 0; i < kLines; ++i) {
      const float y = kFirstBase + kPitch * (float)i;
      const int src =
          ((scrollLine + i + kScrollPhase) % kListingN + kListingN) % kListingN;
      // ONE colour for all fifteen lines — correction 2. The whole focal
      // plane is the sigma, and the sigma is on the glyph MASK.
      const SkColor4f c = kConsoleInk;
      const float sigma = std::max(0.10f, focusSigma(y) + breathe.value());
      const std::u8string line = toU8(kListing[src]);
      // A PHOSPHOR HALO UNDER EVERY LINE. At 2x against the plate the sharp
      // line is not a clean glyph: it peaks at 222 with a wide soft skirt that
      // bleeds into the lines above and below. A second draw at sigma + 2.4
      // and 40% of the ink, DECLARED FIRST so the core paints over it, is that
      // skirt — and it costs nothing measurable, because Skia caches blurred
      // glyph masks per (font, sigma) and both passes hit the same cache.
      auto place = [&](Element e) {
        return e.absolute().left(kTextX).top(y - monoSize * 0.98f);
      };
      g.child(place(text(line, type(monoFace(), monoSize, dim(c, 0.40f),
                                    sigma + 2.4f)))
                  .key("halo" + std::to_string(i)));
      g.child(place(text(line, type(monoFace(), monoSize, c, sigma)))
                  .key("mips" + std::to_string(i)));
    }
    return g;
  }

  // --- the hyperboloid, one twist phase --------------------------------------
  Element wireframe() const {
    using namespace lain;
    // 24 s per revolution at 6 Hz = 144 steps. The twist SWEEPS about its
    // measured 62.8 deg rather than spinning through it, because a hyperboloid
    // whose phi passes 90 deg turns inside out.
    const float t = (float)orbitStep / 144.0f * 6.2831853f;
    const float phi = kPhi0 + 16.0f * std::sin(t);
    const float waist = kRim * std::cos(phi * 0.01745329f);
    const float tilt2 = kOrbit2Tilt + 5.0f * std::sin(t * 0.5f + 1.1f);

    auto g = box().absolute().inset(0).key("wire");

    // the ruling — straight, and stopping 7% short of both rims
    g.child(box()
                .absolute()
                .inset(0)
                .outline([phi](SkSize) { return generatrices(phi, 7); })
                .foreground(reach(add(1.5f, dim(kWire, 0.44f), 0.0f), 4.0f))
                .key("ruling"));

    // the two rims and the waist — DOTTED, never solid (1.6 on 4.4 with a
    // round cap is the frame's own broken hairline)
    const std::vector<SkScalar> dot{1.6f, 4.4f};
    auto ellArc = [&](SkPoint c, float a, float b, float tilt, SkColor4f col,
                      float w, float t0, float t1, const char *key) {
      g.child(box()
                  .absolute()
                  .inset(0)
                  .outline([c, a, b, tilt, t0, t1](SkSize) {
                    return ellipsePath(c, a, b, tilt, t0, t1);
                  })
                  .foreground(reach(add(w, col, 0.0f, dot), 5.0f))
                  .key(key));
    };
    auto ell = [&](SkPoint c, float a, float b, float tilt, SkColor4f col,
                   float w, const char *key) {
      ellArc(c, a, b, tilt, col, w, 0.0f, 6.2831853f, key);
    };
    // The rims are PARTIAL. A full rim ellipse plus a full waist plus a full
    // tilted orbit is three concentric dotted rings and reads as a lampshade;
    // the plate shows arcs that leave frame and never close.
    ellArc({kAxis.fX, kAxis.fY - kHalfH}, kRim, kRim * kEcc, 0,
           dim(kWire, 0.72f), 1.7f, 3.55f, 6.60f, "rimTop");
    ellArc({kAxis.fX, kAxis.fY + kHalfH}, kRim, kRim * kEcc, 0,
           dim(kWire, 0.72f), 1.7f, 0.30f, 3.05f, "rimBot");
    ell(kAxis, waist, waist * kEcc, 0, kWire, 2.0f, "waist");
    ell(kOrbit2C, kOrbit2A, kOrbit2B, tilt2, kWire, 2.0f, "orbit2");

    // the axis: a REAL line, not a rendered artifact of the surface, and it
    // does not pass through the waist centre — measured x 501..505 against a
    // waist centred on 498.
    g.child(box()
                .absolute()
                .inset(0)
                .outline([](SkSize) {
                  SkPathBuilder b;
                  b.moveTo(503 + kWireShift.fX, 28 + kWireShift.fY);
                  b.lineTo(503 + kWireShift.fX, 524 + kWireShift.fY);
                  return b.detach();
                })
                .foreground(reach(add(2.4f, dim(kWire, 0.72f), 0.7f), 9.0f)));

    // `make me feel alright?` rides the tilted orbit's lower-left arc,
    // (185, 455) counter-clockwise up to (840, 215) — the run the conic was
    // fitted through in the first place. onPath is a property of the TEXT
    // LEAF, not of a wrapper (a wrapper takes the spec and quietly ignores
    // it, which is how the first render put the run down the left margin as
    // a wrapped column).
    g.child(text(u8"make me feel alright?",
                 type(monoFace(), 31.0f, kAlright, 0.55f, 4.0f))
                .absolute()
                .left(0)
                .top(0)
                .width(kW)
                .height(kH)
                .onPath(TextPath{.path =
                                     [tilt2](SkSize) {
                                       // REVERSED: solving the two traced
                                       // endpoints back to the conic's own
                                       // parameter gives t = 153 deg at
                                       // (185, 455) and 17.7 deg at
                                       // (840, 215) — DECREASING. A forward
                                       // path puts the run in mirror order
                                       // on the wrong arc, which is what the
                                       // second render showed.
                                       return ellipsePath(kOrbit2C, kOrbit2A,
                                                          kOrbit2B, tilt2,
                                                          6.2831853f, 0.0f);
                                     },
                                 .at = 0.800f,
                                 .align = TextPath::Align::Center,
                                 .offset = 20.0f,
                                 .autoFlip = false,
                                 .orient = TextPath::Orient::Tangent})
                .key("alright"));
    return g;
  }

  // --- the Layer 07 phrase sequence ------------------------------------------
  Element phrases() const {
    using namespace lain;
    const double t = std::fmod((double)phraseStep / 12.0, 13.6);
    auto g = box().absolute().inset(0).key("phrases");
    for (int i = 0; i < kPhraseN; ++i) {
      const Phrase &p = kPhrases[i];
      const double u = t - p.at;
      if (u < -0.1 || u > p.hold + 0.9)
        continue;
      // additive bloom in and out — 0.8 s overlap, so two phrases coexist and
      // ADD where they cross, which is the whole point of the law
      float k = 1.0f;
      if (u < 0.8)
        k = (float)std::max(0.0, u / 0.8);
      else if (u > p.hold)
        k = (float)std::max(0.0, 1.0 - (u - p.hold) / 0.9);
      if (k <= 0.01f)
        continue;
      k = k * k * (3.0f - 2.0f * k);
      const SkColor4f c = dim(kMinds, k);
      // the bloom is a second, blurred pass DECLARED FIRST so it paints under
      // the core; kPlus makes the order irrelevant for colour but not for the
      // core's own crispness
      // in-flow sharp CORE sizes the box; the bloom rides over it as an
      // absolute overlay (a stack() measures to nothing here and shoots the
      // run out of its own centre — the MAGI study's own note, reused)
      g.child(box()
                  .absolute()
                  .centerAt(p.centre)
                  .key("ph" + std::to_string(i))
                  .child(text(std::u8string(p.text),
                              type(serifFace(), p.size, dim(c, 0.42f), 6.5f))
                             .absolute()
                             .inset(0))
                  .child(text(std::u8string(p.text),
                              type(serifFace(), p.size, dim(c, 0.55f), 2.2f))
                             .absolute()
                             .inset(0))
                  .child(text(std::u8string(p.text),
                              type(serifFace(), p.size, c, 0.7f))));
    }
    return g;
  }

  // --- the whole stack -------------------------------------------------------
  Element describe(sketch::SketchContext &ctx) {
    using namespace lain;
    auto root = stack().absolute().inset(0);

    // S0 — the photographic plate, and the ONLY node in the stack that does
    // not add. It is the BOTTOM: the #060719 ground is folded into its shader
    // and it composites kSrcOver, which is what keeps it off the every-frame
    // saveLayer that Cache::Texture + .blend() forces (PERF, step 4).
    root.child(box()
                   .absolute()
                   .inset(0)
                   .fill(Material::sksl(plateEffect()))
                   .cache(Cache::Texture)
                   .key("plate"));

    // S1 — the Japanese prose, FULL BLEED: it starts above the frame and runs
    // off all four edges. Leading 48-50 measured; nothing about it is aligned
    // to the window it will sit under.
    {
      auto g = box().absolute().inset(0).key("prose");
      for (int i = 0; i < kProseN; ++i) {
        const float y = -26.0f + 48.5f * (float)i;
        // the left edge wanders: no two lines start at the same x, which is
        // what a right-to-left vertical original looks like when it is set
        // horizontally by a compositor who did not care
        const float x = -34.0f + 14.0f * std::sin((float)i * 1.7f);
        g.child(text(std::u8string(kProseLines[i]),
                     type(minchoFace(), proseSize, kProse, 0.95f, 1.5f))
                    .absolute()
                    .left(x)
                    .top(y)
                    .key("prose" + std::to_string(i)));
      }
      root.child(std::move(g));
    }

    // S2 — the lightened panel. Measured x 190..470, y 100..380, and it is
    // soft-edged: a radial ramp to nothing rather than a rect with a blur.
    root.child(box()
                   .absolute()
                   .left(178)
                   .top(88)
                   .width(304)
                   .height(304)
                   .fill(Material::radialUnit(
                       {0.48f, 0.46f}, 0.95f,
                       {{0.0f, kPanel}, {0.55f, dim(kPanel, 0.86f)},
                        {0.86f, dim(kPanel, 0.30f)},
                        {1.0f, dim(kPanel, 0.0f)}}))
                   .blend(SkBlendMode::kPlus)
                   .cache(Cache::Texture)
                   .key("panel"));

    // ---- S3, THE CONSOLE WINDOW ---------------------------------------------

    // the body: one radial pedestal that is also the eye's rings
    root.child(box()
                   .absolute()
                   .left(kBodyL)
                   .top(kBodyT)
                   .width(kBodyR - kBodyL)
                   .height(kBodyB - kBodyT)
                   .fill(pedestal())
                   .blend(SkBlendMode::kPlus)
                   .cache(Cache::Texture)
                   .key("body"));

    // the eye's remaining topology — eyelids, four satellites, the stem.
    // Enormously blurred: on the plate it is barely above the pedestal.
    //
    // THE ONE COMPOSITING TRAP IN THE FILE, and it cost a render to find. A
    // node whose DECORATION paints kPlus must ALSO carry `.blend(kPlus)` if it
    // is Texture-cached: the bake happens onto transparent black (where kPlus
    // is a no-op and the pass lands correctly), but the BLIT then composites
    // kSrcOver and paints the dark blurred stroke straight over the plate.
    // The first render came back with two black lozenges where the eyelids
    // are. Paint.cpp says exactly this in one line — "texture bakes (blending
    // must hit the real destination, not the bake's transparent surface)" —
    // and it excludes Texture from `leafDirectBlend` for the same reason.
    // Bounded to the eye's own box so the bake is 0.05 MP, not 0.73.
    root.child(box()
                   .absolute()
                   .left(370)
                   .top(150)
                   .width(376)
                   .height(400)
                   .outline([](SkSize s) {
                     return eyeFurniture({s.width() * 0.5f, s.height() * 0.46f},
                                         92.0f);
                   })
                   .foreground(reach(
                       LayeredBrush{{{24.0f, hex(0x070C17), 13.0f, {}, 0,
                                      SkBlendMode::kPlus, true},
                                     {9.0f, hex(0x0A1120), 5.0f, {}, 0,
                                      SkBlendMode::kPlus, true}}},
                       38.0f))
                   .blend(SkBlendMode::kPlus)
                   .cache(Cache::Texture)
                   .key("eye"));

    // the side rails: single hairlines at the body's own edges, dimmer than
    // the bars. No corner anywhere — the bars simply overhang them.
    root.child(box()
                   .absolute()
                   .inset(0)
                   .outline([](SkSize) {
                     SkPathBuilder b;
                     b.moveTo(kBodyL, kBarTopB - 4);
                     b.lineTo(kBodyL + 8, kBarBotT + 4);
                     b.moveTo(kBodyR, kBarTopB - 4);
                     b.lineTo(kBodyR - 6, kBarBotT + 4);
                     return b.detach();
                   })
                   .foreground(reach(add(2.0f, kRail, 0.8f), 8.0f))
                   .key("rails"));

    // the MIPS block, in its own slot: it re-describes 4.5 times a second and
    // nothing else in the frame should be dirtied by that
    root.child(slot("mips"));

    // the two chrome bars — parallelograms with opposite shear and an inverse
    // bevel each, the bottom one brighter. This is the only heavy element in
    // the interface and its 30 px against 2 px hairlines IS the contrast
    // structure.
    root.child(box()
                   .absolute()
                   .left(kBarTopL)
                   .top(kBarTopT)
                   .width(kBarTopR - kBarTopL)
                   .height(kBarTopB - kBarTopT)
                   .outline(barOutline(kShearTop))
                   .fill(barBevel(kBarTopHi, kBarTopLo, 0.72f))
                   .blend(SkBlendMode::kPlus)
                   .cache(Cache::Texture)
                   .key("barTop"));
    root.child(box()
                   .absolute()
                   .left(kBarBotL)
                   .top(kBarBotT)
                   .width(kBarBotR - kBarBotL)
                   .height(kBarBotB - kBarBotT)
                   .outline(barOutline(kShearBot))
                   .fill(barBevel(kBarBotHi, kBarBotLo, 1.02f))
                   .blend(SkBlendMode::kPlus)
                   .cache(Cache::Texture)
                   .key("barBot"));

    // the rotated Copland lockup, up the left margin at -55 deg. Documented
    // wordmark, verbatim off the boot plate and the ASCII transcription both.
    root.child(box()
                   .absolute()
                   .centerAt({88, 300})
                   .rotate(-55.0f)
                   .column()
                   .alignItems(Align::Center)
                   .gap(1)
                   .key("wordmark")
                   .child(text(u8"Copland OS Enterprise",
                               type(serifItalicFace(), 34, kWordmark, 1.9f,
                                    1.0f)))
                   .child(text(u8"Produced By Tachibana Lab",
                               type(serifItalicFace(), 16,
                                    dim(kWordmark, 0.7f), 1.6f, 0.8f))));

    // ---- S4..S8, the Layer 07 strata over the window ------------------------
    root.child(slot("wire"));

    // `COVer me` — soft brush, orange, the only warm thing in the frame.
    // x 576..884, y 136..229 measured; it runs UPHILL to the right.
    root.child(box()
                   .absolute()
                   .centerAt({730, 182})
                   .rotate(-9.0f)
                   .key("cover")
                   .child(text(u8"COVer me",
                               type(markerFace(), 62, dim(kCover, 0.5f), 6.5f))
                              .absolute()
                              .centerAt({0, 0}))
                   .child(text(u8"COVer me", type(markerFace(), 62, kCover,
                                                  1.4f))));

    // the magenta streaks, x 466..869, y 483..639: horizontal smears, not
    // shapes — three bands of different length at different heights, blurred
    // hard along x only.
    {
      auto g = box().absolute().inset(0).key("magenta");
      const float bands[5][4] = {{474, 508, 128, 0.95f},
                                 {556, 528, 250, 0.72f},
                                 {498, 552, 74, 0.55f},
                                 {640, 574, 190, 0.85f},
                                 {742, 604, 118, 0.48f}};
      for (const auto &b : bands)
        g.child(box()
                    .absolute()
                    .left(b[0])
                    .top(b[1])
                    .width(b[2])
                    .height(15)
                    .fill(Material::linearUnit(
                        {0, 0}, {1, 0},
                        {{0.0f, dim(kMagenta, 0.0f)},
                         {0.30f, dim(kMagenta, b[3])},
                         {0.68f, dim(kMagenta, b[3] * 0.8f)},
                         {1.0f, dim(kMagenta, 0.0f)}}))
                    .blend(SkBlendMode::kPlus)
                    .cache(Cache::Texture));
      root.child(std::move(g));
    }

    root.child(slot("phrases"));

    // ---- the tube -----------------------------------------------------------
    // THE CREEP RIDES A WRAPPER, NOT THE BAKED NODE. Measured: with
    // .translateY(&creep) on the Texture-cached node itself, every whole-pixel
    // step of the creep RE-BAKES the 1016x744 SkSL layer — p99 3.6 -> 33.4 ms,
    // three spikes in five seconds, one per creep step. Moving the binding to
    // a parent box that owns no paint leaves the bake keyed on nothing that
    // changes and the child simply blits under a live transform: p99 5.5 ms.
    // (Same shape as the MAGI study's rule that a cached node's transform
    // belongs on a wrapper, arrived at from the opposite direction — there it
    // was a rotated RESAMPLE, here it is a re-BAKE.)
    root.child(box()
                   .absolute()
                   .inset(0)
                   .translateY(&creep)
                   .child(box()
                              .absolute()
                              .left(0)
                              .top(-12)
                              .width(kW)
                              .height(kH + 24)
                              .fill(Material::sksl(crtEffect()))
                              .cache(Cache::Texture)
                              .key("crt")));
    root.child(box()
                   .absolute()
                   .inset(0)
                   .fill(Fill::color({0, 0, 0, 1}))
                   .opacity(&flicker)
                   .key("flicker"));
    return root;
  }

  // --- host ------------------------------------------------------------------
  void setup(sketch::SketchContext &ctx) override {
    using namespace lain;
    ctx.canvas(kW, kH);
    ctx.background(kGround);

    // SOLVE the mono size from the measured advance rather than guessing it:
    // measure a 40-character run at 100 pt and scale.
    {
      const std::string probe(40, 'M');
      const SkSize m =
          ctx.measure(text(toU8(probe), type(monoFace(), 100.0f, kConsoleInk)));
      const float advAt100 = m.width() / 40.0f;
      monoSize = advAt100 > 1.0f ? 100.0f * kAdvance / advAt100 : 22.0f;
      std::printf("  lain: mono advance %.3f px at 100pt -> size %.2f "
                  "(target advance %.2f)\n",
                  (double)advAt100, (double)monoSize, (double)kAdvance);
    }
    // and the prose size from the measured 48.5 px leading (CJK sets solid at
    // roughly 1.0 em, so the body size is the leading less the gap)
    proseSize = 28.0f;

    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      // whole-pixel creep: a fractional translate turns a cached blit into a
      // resample (measured elsewhere at 7.7 ms against 0.6)
      creep = (float)((int)std::floor(t * 0.5) % 6);
      const double ph = std::fmod(t, 4.0);
      flicker = ph < 0.05 ? 0.055f : 0.0f;
      // the camera hunting focus, +-0.4 px at 0.15 Hz
      breathe = 0.4f * (float)std::sin(t * 0.9424778);
      return true;
    });

    ctx.composer.render(describe(ctx));
    ctx.composer.renderSlot("mips", consoleText());
    ctx.composer.renderSlot("wire", wireframe());
    ctx.composer.renderSlot("phrases", phrases());
  }

  void update(double elapsed, sketch::SketchContext &ctx) override {
    // Three independent rates, three slots. Nothing else re-describes at all.
    const int line = (int)std::floor(elapsed / 0.220);
    const int orbit = (int)std::floor(elapsed * 6.0);
    const int phr = (int)std::floor(elapsed * 12.0);
    if (line != scrollLine) {
      scrollLine = line;
      ctx.composer.renderSlot("mips", consoleText());
    }
    if (orbit != orbitStep) {
      orbitStep = orbit;
      ctx.composer.renderSlot("wire", wireframe());
    }
    if (phr != phraseStep) {
      phraseStep = phr;
      ctx.composer.renderSlot("phrases", phrases());
    }
  }
};

SIGIL_SKETCH(LainNavi)
