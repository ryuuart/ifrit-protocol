#pragma once

#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkTypeface.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/brush/Adaptors.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/Lines.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Curves.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilgeometry/path/Edges.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/bind/Bind.h>
#include <sigilmotion/values/Keyframes.h>
#include <sigilmotion/values/Time.h>
#include <sigilmotion/values/Transition.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace arrange = sigil::geometry::arrange;
namespace sketch = sigil::sketch;
namespace mskia = sigil::material::skia;
namespace motion = sigil::motion;
namespace path = sigil::geometry::path;
namespace field = sigil::material::field;
namespace mat = sigil::material;
namespace patterns = sigil::material::pattern;
namespace shapes = sigil::geometry::shapes;

using namespace sigil::compose;
using namespace std::chrono_literals;
namespace weave = sigil::weave;
namespace ch = choreograph;

namespace lain {

// ---------------------------------------------------------------------------
// THE FRAME. 1016x720 — both reference plates' own size, so a capture diffs.

constexpr float kW = 1016.0f, kH = 720.0f;

// ---------------------------------------------------------------------------
// PALETTE — every entry is a CONTRIBUTION, i.e. what this stratum ADDS to the
// #0F1023 ground under kPlus, computed as (measured colour - ground). Written
// that way on purpose: a colour with an alpha would have to darken something
// to read, and nothing in this interface darkens anything.

const SkColor4f kGround = hexColor(0x060719);  // the plate's own p10; #0F1023,
                                               // its p50, is what the CONTRIB-
                                               // UTIONS above are taken against
const SkColor4f kProse = hexColor(0x1B2138);  // Japanese glyph peaks, +46+54+77
const SkColor4f kPanel = hexColor(0x101F27);  // the lightened panel, +16+31+39
const SkColor4f kBodyMid = hexColor(0x475F86);   // pedestal centre  (#334D83)
const SkColor4f kBodyEdge = hexColor(0x040A24);  // pedestal edge    (#151E52)
const SkColor4f kBarTopHi =
    hexColor(0x587962);  // top bar bright edge   (#67899E)
const SkColor4f kBarTopLo =
    hexColor(0x234A3C);  // top bar dark middle   (#325A74)
const SkColor4f kBarBotHi = hexColor(0x6FA586);  // bottom bar peak (#7EB5CA)
const SkColor4f kBarBotLo = hexColor(0x578C70);  // bottom bar middle (#669CB1)
const SkColor4f kRail = hexColor(0x1D3242);  // side hairlines, dimmer than bars
const SkColor4f kConsoleInk =
    hexColor(0x46C89A);  // ~ #84FFFF - #425689, the add the
                         // reference's own body demands; the
                         // green is trimmed against the
                         // measured core #72F9F5, not guessed
const SkColor4f kWire = hexColor(0x3A6257);   // hairline peak #5683AD on a
                                              // #1C2156 ground, dLuma +55
const SkColor4f kMinds = hexColor(0xA6B7BE);  // `no double minds`, +166+183+190
const SkColor4f kAlright = hexColor(0xB3B6BF);   // `make me feel alright?` core
const SkColor4f kCover = hexColor(0x7A3416);     // `COVer me`, dr-db = +40
const SkColor4f kMagenta = hexColor(0x3A1B3C);   // the streaks, p90 #603871
const SkColor4f kWordmark = hexColor(0x2B3A54);  // the rotated Copland lockup

// ---------------------------------------------------------------------------
// THE WINDOW. Measured off Layer 04 and shifted +58 in x, so the reconstruction
// reads as a WINDOW ON A DESKTOP rather than the whole screen: 137 px of plate
// on its left, 22 on its right. That asymmetry is the composition — see the
// header on placement. Vertically the bars are pulled in to 62 and 646 so the
// 15-line block keeps its measured 37.5 px pitch inside a shorter body.

constexpr float kBodyL = 160.0f, kBodyR = 984.0f;
constexpr float kBodyT = 56.0f, kBodyB = 668.0f;
constexpr float kBarTopT = 62.0f, kBarTopB = 92.0f;    // 30 rows, measured
constexpr float kBarBotT = 646.0f, kBarBotB = 672.0f;  // 26 rows, measured
// The shear. dx/dy from the per-row edge table: the top bar leans right, the
// bottom bar leans left, and they are NOT the same magnitude.
constexpr float kShearTop = 0.54f, kShearBot = -0.46f;
constexpr float kBarTopL = 137.0f, kBarTopR = 977.0f;  // at the bar's TOP row
constexpr float kBarBotL = 152.0f, kBarBotR = 995.0f;  // at the bar's TOP row

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
inline mskia::Paint barBevel(SkColor4f hi, SkColor4f lo, float bias) {
  auto at = [&](float k) { return mskia::mixLinear(lo, hi, k); };
  return mskia::Paint::linearUnit({0, 0}, {0, 1},
                                  {{0.00f, mskia::scale(at(0.05f), bias)},
                                   {0.13f, mskia::scale(at(1.00f), bias)},
                                   {0.30f, mskia::scale(at(0.32f), bias)},
                                   {0.62f, mskia::scale(at(0.28f), bias)},
                                   {0.87f, mskia::scale(at(1.00f), bias)},
                                   {1.00f, mskia::scale(at(0.02f), bias)}});
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

constexpr float kEyeAt[19] = {0,   12,  24,  36,  48,  60,  72,  84,  96, 108,
                              120, 132, 144, 156, 168, 192, 228, 276, 336};
constexpr float kEyeK[19] = {1.000f, 0.961f, 0.889f, 0.728f, 0.759f,
                             0.800f, 0.741f, 0.745f, 0.732f, 0.717f,
                             0.621f, 0.500f, 0.466f, 0.441f, 0.418f,
                             0.332f, 0.252f, 0.177f, 0.077f};

inline mskia::Paint pedestal() {
  std::vector<mskia::Stop> stops;
  stops.reserve(20);
  for (int i = 0; i < 19; ++i)
    stops.push_back(
        {kEyeAt[i] / 359.0f, mskia::mixLinear(kBodyEdge, kBodyMid, kEyeK[i])});
  stops.push_back({1.0f, mskia::scale(kBodyEdge, 0.55f)});
  // radius01 is a fraction of the HALF-DIAGONAL (513 px for this body), so
  // 0.70 puts the last measured annulus (r=336) at its own radius.
  return mskia::Paint::radialUnit({0.483f, 0.456f}, 0.70f, std::move(stops));
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
                        342.0f + kWireShift.fY};  // waist centre, measured
constexpr float kRim = 376.0f;    // solved from two anchor points
constexpr float kEcc = 0.163f;    // 28/172, measured on the waist
constexpr float kHalfH = 166.0f;  // solved with R
constexpr float kPhi0 = 62.8f;    // acos(172/376): the measured twist

/** A rim/waist circle of radius @p rad at axial height @p z, projected. */
inline SkPoint onRim(float rad, float z, float ang) {
  return {kAxis.fX + rad * std::cos(ang),
          kAxis.fY - z + rad * kEcc * std::sin(ang)};
}

/** The ruling: N straight generatrices from the bottom rim to the top rim,
 *  each TRIMMED 7% short at both ends. Those gaps are in the frame, and they
 *  are the one place this interface admits it was placed by a hand. */
inline SkPath generatrices(float phiDeg, int n) {
  const float phi = phiDeg * 0.01745329f;
  SkPathBuilder b;
  for (int i = 0; i < n; ++i) {
    const float a = arrange::along(0.0f, 6.2831853f, (size_t)i, (size_t)n,
                                   arrange::Turn::Closed);
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

/** IT IS NOT `shapes::arc` ON A ROTATED BOX, which draws the same curve —
 *  Skia's oval angles are parametric, so the cut this needs is expressible
 *  there. What differs is the PATH: 168 chords against Skia's conics. Every
 *  rim on this frame is a DOTTED hairline and a dash walks the flattened
 *  path, so the dots land where the chords put them and land elsewhere on a
 *  conic. Converting moves the plate; it is not a cleanup. */
inline SkPath ellipsePath(SkPoint c, float a, float b, float tiltDeg,
                          float t0 = 0.0f, float t1 = 6.2831853f) {
  const float th = tiltDeg * 0.01745329f;
  const float ct = std::cos(th), st = std::sin(th);
  SkPathBuilder p;
  const int n = 168;
  for (int i = 0; i <= n; ++i) {
    const float t = arrange::along(t0, t1 - t0, (size_t)i, (size_t)n + 1,
                                   arrange::Turn::Open);
    const float x = a * std::cos(t), y = b * std::sin(t);
    const SkPoint q{c.fX + x * ct - y * st, c.fY + x * st + y * ct};
    if (i == 0)
      p.moveTo(q);
    else
      p.lineTo(q);
  }
  if (t1 - t0 > 6.28f) p.close();
  return p.detach();
}

// ---------------------------------------------------------------------------
// TYPE.

inline sk_sp<SkTypeface> monoFace() {
  // A light-weight Latin mono stands in for the frame's face; the console
  // block is pure ASCII, so nothing wider is needed. ExtraLight (200) is the
  // closest match to the frame's hairline stems.
  return weave::ports::face(
      {"JetBrainsMono Nerd Font", "JetBrains Mono", "Andale Mono", "Menlo",
       "Courier New"},
      // LIGHT, not ExtraLight. At 22 px under a mask-filter blur an
      // ExtraLight stem never reaches full coverage, which reads as
      // #70BDE8 (blue) where the plate is #72F9F5 (cyan) — the
      // colour is right and the STEM is too thin to show it.
      300);
}
inline sk_sp<SkTypeface> minchoFace() {
  return weave::ports::face(
      {"Hiragino Mincho ProN", "YuMincho", "Shippori Mincho", "Noto Serif JP",
       "Hiragino Sans"},
      400);
}
inline sk_sp<SkTypeface> serifFace() {
  return weave::ports::face({"Times New Roman", "Times", "Georgia"}, 400);
}
inline sk_sp<SkTypeface> serifItalicFace() {
  return weave::ports::face({"Times New Roman", "Times", "Georgia"}, 700,
                            SkFontStyle::kItalic_Slant);
}
/** THE ENGLISH PHRASES ARE TITLES. Layer 07 sets them upright, at large
 *  size, in a plain grotesque — they read as cards over the picture, not
 *  as handwriting on it. A casual script leaned nine degrees is a
 *  different register and it made the one warm thing in the frame read as
 *  a scribble. */
inline sk_sp<SkTypeface> phraseFace() {
  return weave::ports::face({"Helvetica Neue", "Helvetica", "Arial"}, 500);
}

/** THE ONE TEXT STYLE IN THE FILE, and it is the focal plane.
 *
 *  `blend` goes on the PAINT, not the node: a node-level kPlus opens a bounded
 *  saveLayer for every stratum, a paint-level one opens none.
 *  `sigma` goes on the glyph MASK, not on a layer filter: Skia caches blurred
 *  glyph masks per (font, sigma), so fifteen sigmas over fifteen lines cost
 *  fifteen cached mask draws and zero layers. That is the whole answer to the
 *  varying-blur problem described in the header. */
inline weave::TextStyle type(const sk_sp<SkTypeface>& tf, float size,
                             SkColor4f c, float sigma = 0.0f,
                             float track = 0.0f) {
  weave::TextStyle s =
      weave::textStyle({.face = tf, .size = size, .color = c, .track = track});
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

const char* const kListing[] = {
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
constexpr float kPitch = 37.5f;     // measured: autocorrelation peaks 37, 75
constexpr float kAdvance = 13.15f;  // measured: 776 px / 59 gaps
constexpr float kTextX = 188.0f;    // 130 measured + the window's 58 px shift
constexpr float kFirstBase = 112.0f;
constexpr float kFocus = 402.0f;  // the stationary focal plane
constexpr int kScrollPhase = 27;  // see the note above kListing

/** CONSTANT INK, LINEAR SIGMA. The amplitude does not fall with distance
 *  from the focal band — a coverage statistic falls, and coverage is not
 *  brightness. 0.30 px at the band to 1.43 at the block's ends is a 5.4x
 *  sigma swing, which is what the reference's own
 *  5.54x in mean|dI/dx| and 1.98x in per-line PEAK jointly solve to. */
inline float focusSigma(float baselineY) {
  const float d = std::fabs(baselineY - kFocus);
  return 0.30f + d * 0.00390f;  // 0.30 at the band, 1.43 at the block's ends
}

// ---------------------------------------------------------------------------
// THE PROSE. Transcribed off the Layer 07 plate where it is legible — it is a
// passage arguing that what looks like psychic force is the body's own latent
// capacity ("the fabled strength of a man at a fire"). The runs the tube ate
// are FILLED IN THE SAME REGISTER and are not a transcription; the plate is a
// blurred second-generation capture and I am not going to pretend otherwise.

const char8_t* const kProseLines[] = {
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
  const char8_t* text;
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
// shots, both beats present. Baked once, crept by whole pixels. The library's
// overlay carries the line, the beam, the beat and the grain; this states
// which of them this plate was shot with.

inline mat::Material crtTube() {
  return field::crtOverlay({// No hard line: this tube is shot close enough
                            // that the beam's own profile is what shows.
                            .uScanStrength = 0.0f,
                            // The corner falloff of a curved tube, gentle:
                            // the vignette barely enters frame.
                            .uVigInner = 1.05f,
                            .uVigOuter = 1.90f,
                            .uVigStrength = 0.30f,
                            .uSqueeze = 0.86f,
                            .uBeamPitch = 4.42f,
                            .uBeamFalloff = 2.6f,
                            .uBeamStrength = 0.032f,
                            .uBeatPitch = 9.82f,
                            .uBeatFalloff = 1.4f,
                            .uBeatStrength = 0.014f,
                            .uGrain = 0.055f});
}

/** The base plate: a photographed city at night, defocused past recognition.
 *  Not an image — a low-frequency field, because the one thing the frame says
 *  about it is that it has NO edges anywhere: its 8x6 tile floor is flat to
 *  sd 10.8 and every lift is a stratum above it, not a feature in it. */
inline sk_sp<SkRuntimeEffect> plateEffect() {
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
  if (!effect) SkDebugf("lain plate shader: %s\n", err.c_str());
  return effect;
}

/** An additive stroke pass. THE load-bearing call of the whole sketch: kPlus
 *  lives on the pass, so a stratum of strokes is path draws and not a layer.
 *  `blurSigma` is an SkMaskFilter on the stroke mask, bounded by the shape,
 *  so the blur costs a cached mask rather than a full-layer filter. */
inline LayeredBrush add(float width, SkColor4f c, float sigma = 0.0f,
                        std::vector<SkScalar> dash = {}) {
  return LayeredBrush{
      {{width, c, sigma, std::move(dash), 0, SkBlendMode::kPlus, true}}};
}

/** A NOTE ON REACH, because the obvious hand-rolled version is a trap.
 *
 *  A blurred additive stroke paints outside its node's own bounds, so the
 *  node must declare how far. `LayeredBrush::bleed()` computes that per layer
 *  as `width / 2 + 3σ` and takes the max, which is what the strokes here
 *  need, so every call site hands the brush straight to `foreground()` and
 *  nothing declares a reach by hand.
 *
 *  Hand-set reach numbers are the trap: a σ = 0 additive stroke needs
 *  `width / 2` and nothing more, while a stroke with two blurred layers needs
 *  far more than the width suggests. Guessed values here came out both too
 *  large and too small, and neither error is visible until a blurred stroke
 *  reaches past its box and is clipped. */

}  // namespace lain

// =============================================================================
