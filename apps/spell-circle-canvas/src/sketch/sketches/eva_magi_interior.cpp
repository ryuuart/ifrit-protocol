// eva_magi_interior.cpp — NEON GENESIS EVANGELION EP 13, THE MAGI FROM INSIDE
// =============================================================================
// Gainax / Tatsunoko, TV Tokyo, 27 Dec 1995. Episode 13 「使徒、侵入」
// "Lilliputian Hitcher" (dir. Tensai Okamura; Anno / Mitsuo Iso / Satsukawa).
// The eleventh Angel, IREUL, rides in on a corroded protein wall panel,
// evolves into CIRCUITRY, and takes the MAGI from the inside.
//
//   "The growing lines are the electrical circuits. It is a computer itself."
//                — bridge crew, animanga.com/scripts/textesgb/eva13.html
//
//   "Self-destruction suggested by AI Melchior. Rejection, rejection,
//    rejection." … "Melchior is hacking Balthazar." … "Balthazar is now taken
//    over." … "Self-destruction will be executed 02 sec. after all three are
//    agreed about it." — 20 sec … 15 … 10 … 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 —
//   Ritsuko: "We are 1 sec. ahead."  "Self-destruction cancelled by AIs."
//
// THIS IS A RECONSTRUCTION AND IT SAYS SO. The canvas isolates the Ep 13
// deliberation frame: its three panels, infection, labels, rails and verdict.
// The frame is the visual authority; the transcript supplies only the timing.
//
// -----------------------------------------------------------------------------
// THE REFERENCE IS IN PERSPECTIVE, SO NO NUMBER CAN BE READ STRAIGHT OFF IT
//
// `eva13_deliberation.png` is the plate seen at an angle, and the measurements
// that look most like findings are the ones that are purely the projection:
//
//   * a "camera roll varying 8.1 -> 16.4 deg with position" — that is what a
//     CONSTANT roll looks like under perspective;
//   * a "0.75x keystone across the plate", taken for the recession — that is
//     the projection itself, quoted back;
//   * "45 deg cuts, hand-painted, +-8 deg" measure 47 and -30 in frame;
//   * a "pale halation rim" round each panel, which looks like a photographed
//     CRT and is simply the cel's own edge light.
//
// There is no flat original to measure instead. SEARCHED: EvaGeeks/EvaWiki (its
// only MAGI-display still is `Eoe_magi_display.jpg`, Ep 25'), the Evangelion
// Fandom wiki's Magi and Episode:13 galleries, nervarchives.com's Ep 13 page,
// the Japanese EVA wikis (wikiwiki.jp/eva-shingeki, evangelion.dancing-doll),
// fontsinuse 28760, and the recreation repos (itorr/magi, 07JP27/MagiSystem,
// hirakujira/MAGI-System, mdrbx + TheGreatGildo/nerv-ui). The recreations are
// flat but they are AUTHORED, not the artefact. So the frame is rectified
// explicitly, and every number below is measured in the rectified frame.
//
// HOW THE RECTIFICATION WORKS, in four steps that are each checkable:
//   1. Two families of lines that are parallel in the plate but not in the
//      frame: the four green rules, and the panels' vertical edges. Fit each
//      by total least squares; the fits are DEAD STRAIGHT — max residual 1.73,
//      1.40 and 0.71 px on the three cleanly-extractable edges over a 1440 px
//      frame. A photographed CRT bows several px. So the plate is a CEL DRAWN
//      IN PERSPECTIVE, not a photograph of a screen, which also means there is
//      no lens distortion to undo and a homography is exactly right.
//   2. Their vanishing points are (11602, 2946) and (3755, -48316); the line
//      through them is the vanishing line. Sending it to infinity
//      (Hp = [[1,0,0],[0,1,0],l]) makes both families parallel.
//   3. An affine step maps the two directions to the axes. THE FOUR GREEN
//      RULES THEN COME OUT AT -1.27, -0.45, +0.66 and +1.06 DEGREES — they
//      were spread over 13.39..15.82 in the frame. That 2.4 deg spread was the
//      "varying roll". It is projection, and there is no roll: the plate is
//      level and every run of type on it is horizontal.
//   4. One unknown is left — the ratio of the x and y scales — and it is the
//      honest weakness of this study. Three independent calibrators:
//        (a) the chamfer rule, if the cuts are 45 deg      -> K ~ 1.25
//        (b) the kanji, if a left/right Han compound's ink
//            is ~0.87 as tall as it is wide                -> K ~ 1.41
//        (c) Helvetica Bold CONDENSED (fontsinuse 28760 names Condensed for
//            NERV panels), advance/cap ~ 0.82              -> K ~ 1.28
//      They agree to +-11%, which is a triangulation rather than a fit, and
//      K = 1.30 (the median) is adopted. Everything below is measured in that
//      flat frame, and the capture diffs against the FLAT plate.
//
// WHAT THE RECTIFICATION MAKES CHECKABLE:
//
//   THE GENERATING RULE. "Every panel is a rectangle whose corner FACING THE
//   CENTRE is chamfered off." In the flat plate the panels are literally
//   axis-aligned rectangles — CASPER 390x294 at (158,562), BALTHASAR 330x414
//   at (455,198), MELCHIOR 368x290 at (682,548) — and their cuts measure
//   41.0, 45.9 and 46.2 deg: 45 by hand, to within 4. And the bearing test
//   tightens from 19.5/19.4/12.2 deg in the frame to 14.6/18.4/2.0 in the
//   flat plate. audit() prints it.
//
// -----------------------------------------------------------------------------
// FIVE MORE THINGS THE MEASUREMENT SETTLED
//
//  1. THE INFECTION IS A SHADER OVER THE FILL, NOT GEOMETRY. Its cell is
//     32 px in the flat plate: red vertical runs p50 32, azure gaps p50 33,
//     red horizontal p50 37 — one grid, three statistics. Cells flip WHOLE
//     (there is no partially-shaded cell anywhere in the frame), which is a
//     step function of position against one advancing value, i.e. exactly a
//     shader and exactly not a growing set of rectangles. Built as one
//     `Paint::sksl` overlay per panel with a single bound `uFront`; the
//     panel's own azure is underneath and shows through where the field is
//     off. Built instead as ~110 animated rects per panel it would re-describe
//     the tree on every step; this is one draw and one uniform.
//  2. THE PANEL RIM IS THE CEL'S EDGE LIGHT, NOT itorr's BLACK KEYLINE. That
//     recreation frames its panels `.1em black / .4em currentColor / .5em
//     black`. Cut across CASPER's left edge: ground (9,0,0) -> (137,94,116) ->
//     (168,140,175) -> azure. A pale lavender rim two px wide, and no dark
//     band anywhere. (itorr's DOUBLE rule on the 審議中 box IS on the plate.)
//  3. THE GREEN BANDS SANDWICH THE KANJI, THEY ARE NOT A DOUBLE RULE. Four
//     independent horizontal rules, ~19 px thick in the flat plate, one over
//     and one under each kanji pair, ~120 px apart. A `lines::Rails` pair
//     could describe them, but a Rails value with a 120 px span is a true
//     statement and a useless one. They ARE serrated — a fine checker along
//     their length, built as a crosshatch, not a dash.
//  4. THERE ARE THREE ORANGE RAILS. Component analysis of the orange mask
//     finds exactly three, and nothing at all at the centre where a fourth
//     "centre node" stub would go. What is there is the MAGI wordmark's own
//     ink. The centre of this diagram is a hole with a word in it.
//  5. THE FILE BLOCK IS SET NEARLY SOLID. The four lines start at y = 415,
//     439, 463, 487 in the flat plate with cap 18: leading/cap = 1.33, which
//     is what makes it read as a machine dump rather than a caption.
//
// -----------------------------------------------------------------------------
// SOURCES — read and MEASURED
//
//  * The frame itself, rectified as above — the four steps are the whole
//    derivation and each of them can be redone from the frame.
//  * github.com/itorr/magi — a recreation of THIS screen. Taken: the
//    verdict box as a triple box-shadow, `--flash-time: .4s` stepped hard
//    (`step-end`) with a `.1s` EX mode, the voting->voted state machine, and
//    `transform: scale(1.2, 1)` — a HORIZONTAL STRETCH on type, which is the
//    same idiom that makes this plate's kanji wider than they are tall.
//    NOT taken: its palette (aquamarine/chartreuse/#C00/orange) or its verdict
//    words — it says 承認/否定 where Ep 13's plate says 可決/否決/審議中.
//  * github.com/jackestar/PCB-trace-animation (MIT) — the growth model, with
//    its real constants (lineWidth 3, lineSpacing 10, lineAngleVariation
//    0.008, lineEndCoefficient 0.005, speed 4, an occupancy grid so traces
//    never overlap). The ALGORITHM is taken — anisotropic, occluded, blocky —
//    and re-expressed per-pixel; the PROPORTIONS are corrected to the measured
//    32 px cell.
//  * github.com/mdrbx/nerv-ui (MIT) — cited as a PALETTE WARNING. Its tokens
//    are #FF9900 / #00FF00 / #00FFFF, the fan-canonical "NERV colours", and
//    NONE is on this plate. Its `border-radius: 0` design law is right and is
//    obeyed here.
//  * github.com/TheGreatGildo/nerv-ui — components/crt-effects.css. Transcribed
//    unchanged, so this study and eva_magi_defense.cpp share one tube.
//  * fontsinuse.com/uses/28760 — NERV panels are Helvetica / Helvetica
//    Condensed. Anno picked Matisse EB (Fontworks) for the Japanese; the
//    system stand-in is Hiragino Mincho W6 with a restrained emboldening
//    stroke so the glyph forms stay Japanese and approach the extra-black cut.
//
// -----------------------------------------------------------------------------
// BUILT FROM (the library, not by hand)
//   Element::outline               every panel: an axis-aligned box with an
//                                  independently sized cut on selected corners
//   decorations::border            the pale edge-light rim, on those outlines,
//                                  following the cut untold
//   decorations::doubleBorder      the 審議中 box (itorr's triple box-shadow)
//   Paint::sksl + one uniform   THE INFECTION. One shader, one bound
//                                  Output, zero nodes per cell
//   rail() + routers::polyline(0)  the three orange stubs, anchored on the
//                                  panels' own keys at normalized points
//   lines::Rails                   BALTHASAR's own doubled circuitry, with two
//                                  rails at UNEQUAL offsets
//   lines::crosshatch              the serration inside the four green bands
//   Cache::Texture                 the furniture, the type and the CRT overlay
//   hard steps                     every blink; nothing here fades
//
// -----------------------------------------------------------------------------
// WHAT THE LIBRARY CANNOT SAY HERE, AND WHAT THIS FILE DOES INSTEAD
//
//  A. `routers::orthogonal()` BENDS AT midX, AND THIS ARTEFACT IS A PCB.
//     `routers::fromPairwise()` adapts it to the `RailRouter` `rail()`
//     wants, so the call compiles — but every leg would break at its own
//     midpoint, which is a Z where the plate has an L. The three orange
//     stubs go through `polyline(0)` on anchor points chosen by hand.
//  B. NEITHER RAIL ROUTER CAN CUT A CORNER — both only round
//     (SkCornerPathEffect), so `polyline(0)` (no rounding at all) is the only
//     usable setting on a plate whose every elbow is square.
//  C. THERE IS NO JUNCTION DOT AND NO HOP-OVER. `PathFormat{.midCap = Dot,
//     .midSpacing = n}` is a tick ladder — a dot every n px of ARC — which is
//     the opposite of a via, whose definition is "where two runs meet", so
//     nothing on this plate wears one.
//  D. `shapes::chamfered(cut, mask)` takes one scalar. The panel grammar needs
//     independent horizontal and vertical cuts, plus a mask that can select
//     more than one corner, so the shared panel generator carries both axes.
//
// Checked against the headers:
// `lines::Rails` really does dash in CENTRELINE arc-space so unequal-offset
// rails stay in register; `decorations::border` really does follow a cut
// outline untold.
//
// -----------------------------------------------------------------------------
// Run:
//   ./build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
//       src/sketch/sketches/eva_magi_interior.cpp \
//       --frame /tmp/eva_magi_interior.png
//
//   2.5 s  THE REFERENCE MOMENT — MELCHIOR taken (measured 95.2% red),
//          BALTHASAR at the measured 30.2% with a ragged front, CASPER clean
//          (93.3% azure), 提訴 mid-pulse, the gold box mid-blink. The front is
//          SOLVED to land there: setup() inverts the shader's own arrival
//          field to a coverage quantile, so 2.5 s is not a tuned number.
//   0.0-1.6  MELCHIOR falls.   0.5-6.0  BALTHASAR falls.
//   6.5-8.5  否決 x4 (500 ms each) -> 可決, held: under NERV special order
//          582 two AIs cannot cancel what three have carried.
//   9.0-25   the countdown: 20, 15, 10, then one a second from 9.
//   24.0   CASPER snaps back — "We are 1 sec. ahead."     26 s  loop.
// =============================================================================

#include <include/core/SkCanvas.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkTypeface.h>
#include <include/effects/SkRuntimeEffect.h>
#include <shared/EvangelionUi.h>
#include <sigilcompose/brush/Adaptors.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/Hatches.h>
#include <sigilcompose/brush/Lines.h>
#include <sigilcompose/brush/Rails.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Routers.h>
#include <sigilcompose/kit/Strokes.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Corners.h>
#include <sigilgeometry/kit/Curves.h>
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
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace mskia = sigil::material::skia;
namespace motion = sigil::motion;
namespace path = sigil::geometry::path;
namespace shapes = sigil::geometry::shapes;

using namespace sigil::compose;
using namespace std::chrono_literals;
namespace weave = sigil::weave;
namespace ch = choreograph;

namespace magi {

constexpr float kW = 1440.0f, kH = 1052.0f;

inline SkColor4f hex(uint32_t v, float a = 1.0f) noexcept {
  return {(float)((v >> 16u) & 255u) / 255.0f,
          (float)((v >> 8u) & 255u) / 255.0f, (float)(v & 255u) / 255.0f, a};
}

// ---------------------------------------------------------------------------
// PALETTE — HSV-class percentiles over the anchor. There is NO WHITE.

const SkColor4f kGround = hex(0x040001);  // dark p50 — a RED-cast black
// THE PANEL BLUE IS A CEL BLUE. Sampled off the frame it is a deep,
// low-chroma blue that sits UNDER the red rather than beside it; the
// brighter reading is a flat-UI sky blue, which on a black ground makes
// the two blue panels the loudest thing in the picture and MELCHIOR's red
// a third colour rather than the subject.
const SkColor4f kAzure = hex(0x60E0B9);
const SkColor4f kRed = hex(0xAA0506);     // MELCHIOR p50 — and the traces, to
const SkColor4f kRedHot = hex(0xB0090A);  // within 1 LSB: ONE colour, TWO
                                          // coverages, which is the whole idea
const SkColor4f kTraceDark = hex(0x080102);  // the pour's own keyline
const SkColor4f kInk = hex(0x00093C);        // label on a panel: dark NAVY
const SkColor4f kInkRed = hex(0x3A0A18);     // the same ink over MELCHIOR's red
const SkColor4f kOrange = hex(0xE56C1A);     // rails, rims, the MAGI mark
const SkColor4f kOrangeDim = hex(0xB4571A);
const SkColor4f kKanji = hex(0xE16417);  // very slightly redder than the rails
const SkColor4f kKanjiHot = hex(0xFFA050);  // 提訴 mid-pulse
const SkColor4f kGold = hex(0xC4AC50);
const SkColor4f kGoldHot = hex(0xE8D371);
const SkColor4f kGoldPeak = hex(0xF4DD8C);
const SkColor4f kGreen = hex(0x175746);    // 0.16% of the frame and it carries
const SkColor4f kGreenHi = hex(0x2A6B58);  // the whole top band
const SkColor4f kEdgeLight = hex(0xA88CAF);  // the cel's own edge light
const SkColor4f kBgRule = hex(0x6E3228);  // the plate's dim diagonal furniture

// The portrait's warm palette — a DIFFERENT plate, kept different on purpose.
// Two palettes, one canvas; they share the orange, so the orange is the seam.
constexpr float kBack = 0.42f;
const SkColor4f kPBody = scaleRgb(hex(0x711B0F), kBack);
const SkColor4f kPBodyHi = scaleRgb(hex(0xB63014), kBack);
const SkColor4f kPRail = scaleRgb(hex(0x703912), kBack);
const SkColor4f kPRailHi = scaleRgb(hex(0xE17B33), kBack);
const SkColor4f kPRailDim = scaleRgb(hex(0xB05A20), kBack);
const SkColor4f kPChart = scaleRgb(hex(0xB1CE3C), kBack);
const SkColor4f kPPin = scaleRgb(hex(0xEFE033), kBack);
const SkColor4f kPMagenta = scaleRgb(hex(0xAD5196), kBack);
const SkColor4f kPViolet = scaleRgb(hex(0x4A3283), kBack);

// ---------------------------------------------------------------------------
// TYPE

inline const sk_sp<SkTypeface>& latin() { return evangelion::condensedBold(); }
inline const sk_sp<SkTypeface>& latinPlain() {
  return evangelion::condensedRegular();
}
inline const sk_sp<SkTypeface>& han() { return evangelion::minchoHeavy(); }

inline weave::TextStyle type(const sk_sp<SkTypeface>& tf, float size,
                             SkColor4f color, float condense = 1.0f,
                             float track = 0.0f) {
  return evangelion::type(tf, size, color, condense, track);
}

// ---------------------------------------------------------------------------
// THE PLATE, in a rectified coordinate system. The side panels are one module
// mirrored around the central axis; the upper panel presents a matching flat.

struct Panel {
  const char* key;
  const char* label;
  const char* number;
  SkRect box;
  float rotation;
  int circuitRuns;
  SkPoint seed;
};

struct InfectionTiming {
  double seedAt;
  double fullAt;
  double levels;
  double phase;
};

// Whole cells still switch as steps, but each panel advances through enough
// levels that adjacent traces turn over instead of large regions jumping.
inline constexpr std::array<InfectionTiming, 3> kInfection = {{
    {12.0, 22.0, 64.0, 0.0},
    {0.48, 6.0, 96.0, 0.0},
    {-0.5, 2.0, 48.0, 0.37},
}};

inline std::vector<Panel> panels() {
  const evangelion::MagiVoteLayout layout;

  return {
      {"casper",
       "CASPER",
       "3",
       layout.moduleRect(3),
       layout.rotationFor(3),
       0,
       {11.5f, 1.5f}},
      {"balthasar",
       "BALTHASAR",
       "2",
       layout.moduleRect(2),
       layout.rotationFor(2),
       13,
       {6.5f, 5.0f}},
      {"melchior",
       "MELCHIOR",
       "1",
       layout.moduleRect(1),
       layout.rotationFor(1),
       6,
       {10.5f, 0.5f}},
  };
}

/** THE INFECTION, as one shader over the fill.
 *
 *  The cell is 32 px, measured: red vertical runs p50 32, azure gaps p50 33,
 *  red horizontal p50 37 — one grid, three statistics. Cells flip WHOLE (there
 *  is no partially-shaded cell in the frame), so every cell is a step function
 *  of its own arrival value against one advancing uniform.
 *
 *  ARRIVAL is jackestar's growth model re-expressed as a metric: a Manhattan
 *  distance from the seed, divided by a speed drawn from TWO ANISOTROPIC value
 *  noises — one stretched 6:1 along x, one along y — so the front advances in
 *  long orthogonal FINGERS that fill in behind themselves, rather than as a
 *  disc. That is the difference between "the growing lines are the electrical
 *  circuits" and a stain. A third, isotropic hash punches out ~14% of cells
 *  permanently: jackestar's occupancy grid, and the azure islands the frame
 *  shows inside the pour.
 *
 *  main() is MONOLITHIC and no loop is bounded by a uniform — the sketch host
 *  links Skia twice and a stock SkSL material that breaks either rule faults
 *  on PAC (stock_materials.cpp is the guard test).
 */
inline const char* kInfectionSrc = R"SKSL(
uniform float2 uResolution;
uniform float  uFront;      // the advancing front, in arrival units
uniform float2 uSeed;       // seed cell
uniform float2 uCells;      // cells across / down
uniform float4 uPour;       // the pour
uniform float4 uKey;        // its keyline

// Dave Hoskins' fract/dot hash, NOT sin(). The CPU mirror below has to agree
// with this BIT FOR BIT — the front is solved by inverting the arrival field
// on the CPU, so a hash whose two implementations disagree turns "30.1% by
// construction" into a number that renders as 20.6%. sin() of a ~5000 rad
// argument does exactly that; this uses only fract/dot/mul/add.
float h21(float2 p) {
  float3 q = fract(float3(p.x, p.y, p.x) * 0.1031);
  q += dot(q, float3(q.y, q.z, q.x) + 33.33);
  return fract((q.x + q.y) * q.z);
}

// ARRIVAL. Manhattan distance from the seed over an ANISOTROPIC speed field:
// two COARSE lobes, one 3.5 cells long in x and one in y, so the front
// advances in long orthogonal FINGERS that fill in behind themselves rather
// than as a disc. That is the difference between "the growing lines are the
// electrical circuits" and a stain.
//
// The lobes are BLOCKY — one hash of a coarse cell, not a smooth 4-tap value
// noise. Smoothing would pay four hashes plus their blends on every arrival
// call, for a softness nothing in this cel has — the blocky read is both the
// cheaper and the truer choice. A third hash punches out ~11% of cells
// permanently — jackestar's occupancy grid, and the azure islands the frame
// shows inside the pour.
float arrival(float2 c, float2 seed) {
  float2 d = c - seed;
  float manh = abs(d.x) + abs(d.y);
  // LONGER LOBES. A lobe 3.5 cells long at a thirty-two pixel cell was a
  // hundred pixels of finger; at a ten pixel cell the same 0.28 is thirty,
  // which is a dash. Stretching to 0.085 keeps the finger the length the
  // cel's traces run and makes it a twelfth of the width.
  float nh = h21(floor(c * float2(0.085, 1.0)));
  float nv = h21(floor(c * float2(1.0, 0.085)) + 41.0);
  // R2 (Roberts' low-discrepancy) for the per-cell grain: three ops against
  // h21's twelve, and a punch-out mask does not need Hoskins' quality.
  float grain = fract(c.x * 0.7548777 + c.y * 0.5698403 + 0.137);
  float m = max(nh, nv);
  return manh / (0.38 + 1.70 * m * m) + 1.7 * grain + step(grain, 0.11) * 1e4;
}

half4 main(float2 xy) {
  float2 uv   = xy / max(uResolution, float2(1.0));
  float2 cell = floor(uv * uCells);
  float2 sub  = fract(uv * uCells);

  float on = step(arrival(cell, uSeed), uFront);

  // The keyline lives on the AZURE side of the boundary — measured 23% below /
  // 24% above / 22% right / 13% left of a red edge, i.e. on part of the
  // perimeter and never inside. Gated three ways so the common pixel costs ONE
  // arrival: only an OFF cell can wear it, only a hash-chosen cell does (the
  // cel is hand-painted, not outlined), and only within 0.21 of an edge.
  float k = 0.0;
  if (on < 0.5 && h21(cell + 5.5) > 0.42) {
    float dx = min(sub.x, 1.0 - sub.x);
    float dy = min(sub.y, 1.0 - sub.y);
    if (dx < 0.21) {
      float nx = sub.x < 0.5 ? -1.0 : 1.0;
      k = max(k, step(arrival(cell + float2(nx, 0.0), uSeed), uFront));
    }
    if (dy < 0.21) {
      float ny = sub.y < 0.5 ? -1.0 : 1.0;
      k = max(k, step(arrival(cell + float2(0.0, ny), uSeed), uFront));
    }
  }

  float3 rgb = mix(uKey.rgb, uPour.rgb, on);
  float alpha = max(on, k);
  return half4(half3(rgb * alpha), half(alpha));
}
)SKSL";

inline sk_sp<SkRuntimeEffect> infectionEffect() {
  static sk_sp<SkRuntimeEffect> fx = [] {
    auto [effect, err] =
        SkRuntimeEffect::MakeForShader(SkString(kInfectionSrc));
    if (!effect) SkDebugf("magi infection shader: %s\n", err.c_str());
    return effect;
  }();
  return fx;
}

/** THE CPU MIRROR of the shader's arrival(), float for float, so the front
 *  can be SOLVED rather than tuned: sample every cell, sort, and the
 *  coverage->front map is the quantile function. Kept next to the SkSL on
 *  purpose. It is also the reason the hash is Hoskins' and not sin(): with
 *  sin() the two sides disagreed enough that a model coverage of 30.1%
 *  rendered as 20.6%, and the printed number would have been a lie. */
inline float fract1(float v) { return v - std::floor(v); }
inline float h21(float px, float py) {
  float qx = fract1(px * 0.1031f);
  float qy = fract1(py * 0.1031f);
  float qz = fract1(px * 0.1031f);
  const float d = qx * (qy + 33.33f) + qy * (qz + 33.33f) + qz * (qx + 33.33f);
  qx += d;
  qy += d;
  qz += d;
  return fract1((qx + qy) * qz);
}
inline float arrival(float cx, float cy, SkPoint seed) {
  const float manh = std::fabs(cx - seed.fX) + std::fabs(cy - seed.fY);
  const float nh = h21(std::floor(cx * 0.28f), std::floor(cy * 1.00f));
  const float nv =
      h21(std::floor(cx * 1.00f) + 41.0f, std::floor(cy * 0.28f) + 41.0f);
  const float grain = fract1(cx * 0.7548777f + cy * 0.5698403f + 0.137f);
  const float m = std::max(nh, nv);
  return manh / (0.38f + 1.70f * m * m) + 1.7f * grain +
         (grain < 0.11f ? 1e4f : 0.0f);
}

/** One panel's cells, sorted by arrival and carrying the AREA each actually
 *  contributes — a cell the panel's cut clips in half is half a cell of pour,
 *  and the anchor's 30.2% is an AREA fraction. Counting clipped cells whole
 *  makes the model claim 30.1% of a panel that renders at 24.5%. */
struct Arrivals {
  std::vector<std::pair<float, float>> cells;  // (arrival, area weight)
  float total = 0;
};
inline Arrivals arrivalTable(SkRect box, float cell, SkPoint seed,
                             const SkPath& outline) {
  Arrivals A;
  const int cx = std::max(1, (int)std::ceil(box.width() / cell));
  const int cy = std::max(1, (int)std::ceil(box.height() / cell));
  // the SHADER's cell size: uCells tiles the node's box exactly
  const float cw = box.width() / (float)cx, chh = box.height() / (float)cy;
  A.cells.reserve((size_t)cx * cy);
  for (int y = 0; y < cy; ++y)
    for (int x = 0; x < cx; ++x) {
      // 4x4 stratified sample of the cell against the panel's own outline
      int in = 0;
      for (int sy = 0; sy < 4; ++sy)
        for (int sx = 0; sx < 4; ++sx)
          if (outline.contains(((float)x + ((float)sx + 0.5f) * 0.25f) * cw,
                               ((float)y + ((float)sy + 0.5f) * 0.25f) * chh))
            ++in;
      if (in == 0) continue;
      const float w = (float)in / 16.0f;
      A.cells.emplace_back(arrival((float)x, (float)y, seed), w);
      A.total += w;
    }
  std::sort(A.cells.begin(), A.cells.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
  return A;
}
/** The front value that lands the AREA coverage on `frac`. */
inline float frontFor(const Arrivals& A, float frac) {
  if (A.cells.empty() || A.total <= 0) return -1.0f;
  if (frac <= 0.0f)
    return -1.0f;  // nothing on: CASPER is CLEAN and must render clean
  const float want = frac * A.total;
  float acc = 0;
  for (const auto& c : A.cells) {
    acc += c.second;
    if (acc >= want) return c.first + 0.001f;
  }
  return A.cells.back().first + 0.001f;
}

// ---------------------------------------------------------------------------
// The panel's own orthogonal circuitry — near-navy hairlines on the azure,
// generated as point runs and fed to lines::Rails, because the frame shows
// DOUBLED runs and a single hairline is not the honest spelling.

inline uint32_t hash32(uint32_t x) {
  x ^= x >> 16u;
  x *= 0x7feb352du;
  x ^= x >> 15u;
  x *= 0x846ca68bu;
  x ^= x >> 16u;
  return x;
}
inline float hashF(int a, int b, int salt) {
  return (float)(hash32((uint32_t)(a * 73856093) ^ (uint32_t)(b * 19349663) ^
                        (uint32_t)(salt * 83492791)) &
                 0xffffffu) /
         16777215.0f;
}
// THE CELL IS THE TRACE'S WIDTH. At thirty-two pixels a finger is a slab
// and the pour reads as a mask over the panel; the epigraph the whole
// artefact hangs on is "the growing lines are the electrical circuits",
// and a circuit's line is thin. Ten pixels puts nine times as many cells
// under the same front, so the anisotropic lobes read as the long
// orthogonal runs they compute — a trace network, not a stain.
constexpr float kCell = 10.0f;
constexpr int kDX[4] = {1, 0, -1, 0};
constexpr int kDY[4] = {0, 1, 0, -1};

inline SkPath ownCircuitry(SkSize s, int salt, int runs) {
  SkPathBuilder b;
  for (int r = 0; r < runs; ++r) {
    float x = std::round(hashF(r, 3, salt) * s.width() / kCell) * kCell;
    float y = std::round(hashF(r, 7, salt) * s.height() / kCell) * kCell;
    int dir = (int)((unsigned)(int)(hashF(r, 11, salt) * 4.0f) & 3u);
    const int steps = 3 + (int)(hashF(r, 13, salt) * 7.0f);
    bool started = false;
    for (int i = 0; i < steps; ++i) {
      // jackestar's lineAngleVariation, as a per-step turn probability
      if (hashF(r, i * 31 + 5, salt) < 0.26f)
        dir = (int)((unsigned)(dir + (hashF(r, i * 37, salt) < 0.5f ? 1 : 3)) &
                    3u);
      const float nx = x + (float)kDX[dir] * kCell;
      const float ny = y + (float)kDY[dir] * kCell;
      if (nx < 4 || ny < 4 || nx > s.width() - 4 || ny > s.height() - 4) break;
      if (!started) {
        b.moveTo(x, y);
        started = true;
      }
      b.lineTo(nx, ny);
      x = nx;
      y = ny;
    }
  }
  return b.detach();
}

/** The hollow pads the frame shows on the azure. */
inline SkPath ownPads(SkSize s, int salt, int count) {
  SkPathBuilder b;
  for (int i = 0; i < count; ++i) {
    const float x = std::round(hashF(i, 21, salt) * s.width() / kCell) * kCell;
    const float y = std::round(hashF(i, 23, salt) * s.height() / kCell) * kCell;
    const float w = kCell * (1.0f + std::floor(hashF(i, 27, salt) * 3.0f));
    const float h = kCell * (hashF(i, 29, salt) < 0.6f ? 0.5f : 1.0f);
    if (x + w > s.width() - 6 || y + h > s.height() - 6 || x < 6 || y < 6)
      continue;
    b.addRect(SkRect::MakeXYWH(x, y, w, h));
  }
  return b.detach();
}

// ---------------------------------------------------------------------------
// THE CRT. Transcribed from TheGreatGildo/nerv-ui components/crt-effects.css
// and reused UNCHANGED so this study and eva_magi_defense.cpp share one tube:
// 2 px scanlines at ~4% black, a 70%/70% vignette ellipse to 40%.

}  // namespace magi

// =============================================================================

struct EvaMagiInterior : sketch::Sketch {
  std::vector<magi::Panel> panels = magi::panels();
  std::vector<magi::Arrivals> arrivals;  // per panel, sorted by arrival
  SkPoint centre{613, 602};
  weave::FontContext* fonts = nullptr;

  // measured off the FLAT plate, inside the measured polygons
  static constexpr double kRefT = 2.5;
  static constexpr float kWant[3] = {0.000f, 0.302f, 1.000f};

  // --- animation: everything is a bound Output; nothing re-describes ------
  ch::Output<float> front0{0}, front1{0}, front2{0};
  ch::Output<float> kanjiHot{0};
  ch::Output<float> goldOn{1};
  ch::Output<float> creep{0};
  ch::Output<float> flicker{0};
  double clock = 0;

  std::array<bool, 3> taken{false, false, false};
  // A panel Ireul has not reached yet carries NO infection node at all:
  // a shader whose front is below every cell still runs over every pixel of
  // the panel to prove it, and CASPER stays clean for the first twelve
  // seconds of a twenty-six second loop. This is the one place the DATA path
  // beats a uniform.
  std::array<bool, 3> seeded{false, false, false};
  int verdictStep = -1;
  int countdown = -1;

  brush::Pattern beadBrush;
  brush::Pattern chevronBrush;
  bool auditOk = true;

  // ==========================================================================
  // THE AUDIT — three copies of one square, equally spaced on a radial
  // register.
  void audit() {
    SkPoint c{0, 0};
    for (const auto& p : panels) {
      c.fX += p.box.centerX();
      c.fY += p.box.centerY();
    }
    centre = {c.fX / (float)panels.size(), c.fY / (float)panels.size()};
    auditOk = true;
    std::printf("MAGI INTERIOR — modular voting plate\n");
    std::printf("  module centroid = (%.0f, %.0f)\n", centre.fX, centre.fY);
    const auto distance = [](SkPoint a, SkPoint b) {
      return std::hypot(a.fX - b.fX, a.fY - b.fY);
    };
    const SkPoint p0{panels[0].box.centerX(), panels[0].box.centerY()};
    const SkPoint p1{panels[1].box.centerX(), panels[1].box.centerY()};
    const SkPoint p2{panels[2].box.centerX(), panels[2].box.centerY()};
    const float sides[3] = {distance(p0, p1), distance(p1, p2),
                            distance(p2, p0)};
    const float expectedRotation[3] = {60.0f, 0.0f, -60.0f};
    for (size_t i = 0; i < panels.size(); ++i) {
      const auto& p = panels[i];
      const bool square = std::fabs(p.box.width() - p.box.height()) < 0.1f;
      const bool equilateral = std::fabs(sides[i] - sides[(i + 1) % 3]) < 0.5f;
      const bool oriented = std::fabs(p.rotation - expectedRotation[i]) < 0.1f;
      const bool ok = square && equilateral && oriented;
      auditOk = auditOk && ok;
      std::printf("  %-10s square %.0f  edge %.1f  rotate %+4.0f  %s\n", p.key,
                  p.box.width(), sides[i], p.rotation, ok ? "OK" : "FAIL");
    }
    if (!auditOk) std::printf("  *** MODULE RULE VIOLATED\n");
  }

  // ==========================================================================
  // TYPE, SOLVED. metrics() gives capHeight without laying anything out and
  // capHeight is linear in size; capSlack() turns "the reference's ink starts
  // at (222, 692)" into a coordinate. The WIDTH is solved too, because Matisse
  // EB and Songti SC Black do not share a set width and NERV panels are set in
  // Helvetica CONDENSED — the frame is the authority on how much.

  weave::TextStyle fitCap(const sk_sp<SkTypeface>& tf, float cap, SkColor4f c,
                          float* slack = nullptr) const {
    float size = cap * 1.4f;
    if (fonts) {
      const TextMetrics m = metrics(magi::type(tf, 100.0f, c), *fonts);
      if (m.capHeight > 1.0f) size = 100.0f * cap / m.capHeight;
    }
    weave::TextStyle st = magi::type(tf, size, c);
    if (slack) *slack = fonts ? metrics(st, *fonts).capSlack() : size * 0.22f;
    return st;
  }

  weave::TextStyle fitRun(const sk_sp<SkTypeface>& tf, const std::u8string& s,
                          float cap, float inkW, SkColor4f c,
                          float* slack = nullptr) const {
    weave::TextStyle st = fitCap(tf, cap, c, slack);
    if (fonts && inkW > 1.0f) {
      const SkSize m = sigil::compose::intrinsicSize(text(s, st), *fonts);
      if (m.width() > 1.0f)
        st.shaping.scaleX = std::clamp(inkW / m.width(), 0.40f, 1.8f);
    }
    return st;
  }

  weave::TextStyle fitWithin(const sk_sp<SkTypeface>& tf,
                             const std::u8string& s, float cap, float maxWidth,
                             SkColor4f c) const {
    weave::TextStyle st = fitCap(tf, cap, c);
    if (!fonts) return st;
    const SkSize measured = sigil::compose::intrinsicSize(text(s, st), *fonts);
    if (measured.width() > maxWidth && measured.width() > 1.0f)
      st.shaping.scaleX = std::min(maxWidth / measured.width(), 1.0f);
    return st;
  }

  /** A CJK run is sized by its ADVANCE — a Ming's em IS its advance, and the
   *  flat plate gives it directly: 提/訴 step 148 px, 決/議 130. */
  weave::TextStyle fitEmSpan(const std::u8string& s, float advanceSpan,
                             SkColor4f c, float* slack = nullptr) const {
    // The plate's Han is drawn WIDER than it is tall — the same horizontal
    // stretch itorr's recreation encodes as `transform: scale(1.2, 1)` — so
    // the stretch goes on BEFORE the measurement, or the solve fights it.
    constexpr float kStretch = 1.12f;
    float em = advanceSpan * 0.5f;
    if (fonts) {
      weave::TextStyle probe = evangelion::minchoDisplay(100.0f, c, kStretch);
      const SkSize m = sigil::compose::intrinsicSize(text(s, probe), *fonts);
      if (m.width() > 1.0f) em = 100.0f * advanceSpan / m.width();
    }
    weave::TextStyle st = evangelion::minchoDisplay(em, c, kStretch);
    if (slack) *slack = fonts ? metrics(st, *fonts).capSlack() : em * 0.10f;
    return st;
  }

  /** A run of type placed by its measured INK top-left. Nothing on this plate
   *  is rotated: the rectification says every baseline is horizontal to within
   *  1.3 deg, and the "varying roll" was the projection. */
  Element inked(std::u8string s, const weave::TextStyle& st, SkPoint ink,
                float slack) {
    return box().left(ink.fX).top(ink.fY - slack).child(text(std::move(s), st));
  }

  // ==========================================================================
  // THE PLATE

  Element panelNode(int i) {
    const magi::Panel& p = panels[(size_t)i];
    const evangelion::MagiVoteLayout layout;
    const int number = 3 - i;
    const SkSize sz{p.box.width(), p.box.height()};
    const bool red = taken[(size_t)i];
    const SkPath circuit = magi::ownCircuitry(sz, 17 + i * 13, p.circuitRuns);
    const SkPath pads = magi::ownPads(sz, 41 + i * 7, p.circuitRuns ? 6 : 0);
    const ch::Output<float>* fr =
        i == 0 ? &front0 : (i == 1 ? &front1 : &front2);

    mskia::Paint infection =
        mskia::Paint::sksl(magi::infectionEffect())
            .uniform("uSeed", std::array<float, 2>{p.seed.fX, p.seed.fY})
            .uniform("uCells",
                     std::array<float, 2>{std::ceil(sz.width() / magi::kCell),
                                          std::ceil(sz.height() / magi::kCell)})
            .uniform("uPour", magi::kRed)
            .uniform("uKey", magi::kTraceDark)
            .uniform("uFront", fr);

    Element node =
        box()
            .left(p.box.left())
            .top(p.box.top())
            .width(sz.width())
            .height(sz.height())
            .rotate(p.rotation)
            .transformOrigin(0.5f, 0.5f)
            .key(p.key)
            .shape(evangelion::panel({}))
            .fill(mskia::Paint::solid(red ? magi::kRed : magi::kAzure))
            .clip(true)
            .style(decorations::doubleBorder(
                decorations::border(5.0f, Fill::color(magi::kOrange), 0.0f),
                decorations::border(3.0f, Fill::color(magi::kTraceDark),
                                    8.0f)));

    if (!red && p.circuitRuns > 0) {
      // UNEQUAL offsets, unequal widths: a heavy run with a hairline beside it
      // at a different clearance, which is the thing lines::Line's symmetric
      // `parallels` cannot say at all.
      node.child(
          box()
              .inset(0)
              .fill(Fill::none())
              // the callable is invoked on every layout, so its capture must
              // survive each return
              // NOLINTNEXTLINE(performance-no-automatic-move)
              .shape([circuit](SkSize) { return circuit; })
              .stroke(lines::Rails{.rails = {{.across = 6.0f,
                                              .width = 3.0f,
                                              .fill = Fill::color(magi::kInk),
                                              .cap = SkPaint::kSquare_Cap,
                                              .join = SkPaint::kMiter_Join},
                                             {.across = -3.0f,
                                              .width = 1.8f,
                                              .fill = Fill::color(magi::kInk),
                                              .cap = SkPaint::kSquare_Cap,
                                              .join = SkPaint::kMiter_Join}},
                                   .offsetStep = 6.0f}));
      node.child(box()
                     .inset(0)
                     .fill(Fill::none())
                     // the callable is invoked on every layout, so its capture
                     // must survive each return
                     // NOLINTNEXTLINE(performance-no-automatic-move)
                     .shape([pads](SkSize) { return pads; })
                     .stroke(PathFormat{.width = 2.0f,
                                        .strokeFill = Fill::color(magi::kInk),
                                        .join = SkPaint::kMiter_Join}));
    }
    if (!red && seeded[(size_t)i]) node.child(box().inset(0).fill(infection));
    const SkColor4f labelInk = red ? magi::kInkRed : magi::kInk;
    node.child(text(toU8(p.number), fitCap(magi::latin(), 86.0f, labelInk))
                   .centerAt({sz.width() * 0.5f,
                              sz.height() * layout.numberSlotY(number)}));
    node.child(
        text(toU8(p.label), fitWithin(magi::latin(), toU8(p.label), 31.0f,
                                      sz.width() - 44.0f, labelInk))
            .centerAt(
                {sz.width() * 0.5f, sz.height() * layout.nameSlotY(number)}));
    return node;
  }

  /** The headings are bracketed by three parallel green rules. */
  Element greenBand(float x0, float x1, float y) {
    Element band = box().inset(0);
    for (int i = -1; i <= 1; ++i)
      band.child(box()
                     .left(x0)
                     .top(y + (float)i * 7.0f)
                     .width(x1 - x0)
                     .height(2.5f)
                     .fill(mskia::Paint::solid(i == 0 ? magi::kGreenHi
                                                      : magi::kGreen)));
    return band;
  }

  Element plateFurniture() {
    Element g = box().inset(0);
    const evangelion::MagiVoteLayout layout;
    const SkRect frame = layout.frame();
    g.child(
        box()
            .left(frame.left())
            .top(frame.top())
            .width(frame.width())
            .height(frame.height())
            .fill(Fill::none())
            .foreground(decorations::border(7.0f, Fill::color(magi::kOrange))));
    g.child(kit::disc(layout.busCentre, layout.busRadius)
                .shape(shapes::circle())
                .fill(Fill::none())
                .foreground(decorations::border(
                    5.0f, Fill::color(magi::kOrangeDim), 0.0f)));
    g.child(greenBand(145.0f, 520.0f, 116.0f));
    g.child(greenBand(145.0f, 520.0f, 251.0f));
    g.child(greenBand(920.0f, 1295.0f, 116.0f));
    g.child(greenBand(920.0f, 1295.0f, 251.0f));
    return g;
  }

  Element plateType() {
    Element g = box().inset(0);
    float sCode = 0, sFile = 0;
    g.child(inked(u8"CODE : 132",
                  fitRun(magi::latin(), u8"CODE : 132", 45.0f, 270.0f,
                         magi::kOrange, &sCode),
                  {151.0f, 294.0f}, sCode));
    const auto file = fitRun(magi::latin(), u8"EXTENTION:2048", 22.0f, 286.0f,
                             magi::kOrange, &sFile);
    static const char* kBlock[5] = {"FILE:MAGI_SYS", "EXTENTION:2048",
                                    "EX_MODE:ON", "PRIORITY:A__", nullptr};
    for (int i = 0; kBlock[i]; ++i)
      g.child(inked(toU8(kBlock[i]), file, {151.0f, 350.0f + (float)i * 32.0f},
                    sFile));

    const auto k1 = fitEmSpan(u8"提訴", 300.0f, magi::kKanji);
    const auto k1h = fitEmSpan(u8"提訴", 300.0f, magi::kKanjiHot);
    g.child(text(u8"提訴", k1).centerAt({332.5f, 184.0f}));
    g.child(box().inset(0).opacity(&kanjiHot).child(
        text(u8"提訴", k1h).centerAt({332.5f, 184.0f})));
    const auto k2 = fitEmSpan(u8"決議", 300.0f, magi::kKanji);
    g.child(text(u8"決議", k2).centerAt({1107.5f, 184.0f}));

    g.child(text(u8"MAGI", fitCap(magi::latin(), 54.0f, magi::kOrange))
                .centerAt({720.0f, 535.0f}));
    return g;
  }

  /** 審議中 in its gold box: a REAL double border — gold outer rule, dark gap,
   *  thin inner rule. Measured 200 x 90; itorr builds the same object as a
   *  triple box-shadow at .03em / .07em / .1em. */
  Element verdictBox() {
    const auto st = fitEmSpan(u8"審議中", 188.0f, magi::kGoldPeak);
    return box()
        .left(995)
        .top(295)
        .width(275)
        .height(130)
        .opacity(&goldOn)
        .fill(mskia::Paint::solid(hex(0x140A02)))
        .style(decorations::doubleBorder(
            decorations::border(5.0f, Fill::color(magi::kGold), 0.0f),
            decorations::border(2.0f, Fill::color(magi::kGoldHot), 10.0f)))
        .child(text(u8"審議中", st).centerAt({137.5f, 65.0f}));
  }

  /** The verdict card — 否決 x4, then 可決, then the struck-through 否決 of
   *  "we are 1 sec. ahead". It appears only AFTER the reference moment, so the
   *  capture at 2.5 s still diffs against the anchor. */
  Element verdictCard() {
    if (verdictStep < 0) return box().absolute().width(0).height(0);
    const bool carried = verdictStep == 4;
    const SkColor4f ink = carried ? magi::kGoldPeak : magi::kRedHot;
    const auto st = fitEmSpan(carried ? u8"可決" : u8"否決", 150.0f, ink);
    // 430 is the gap the right margin leaves, between the countdown numeral
    // and the first portrait leader line. Below it the card crosses
    // portrait labels 1-3.
    Element card =
        box()
            .left(995)
            .top(295)
            .width(275)
            .height(130)
            .shape(shapes::chamfered(22.0f, shapes::Corner::Diagonal))
            .fill(mskia::Paint::solid(hex(0x0A0102)))
            .foreground(decorations::border(4.0f, Fill::color(ink), 3.0f))
            .child(text(carried ? u8"可決" : u8"否決", st)
                       .centerAt({137.5f, 65.0f}));
    if (verdictStep == 5)
      card.child(box().left(24).top(62).width(227).height(7).fill(
          mskia::Paint::solid(magi::kOrange)));
    return card;
  }

  /** The countdown. Helvetica Bold, bleeding off the frame — 20, 15, 10, then
   *  one a second from 9, and a 2 s stay at zero because "self-destruction
   *  will be executed 02 sec. after all three agree". */
  Element countdownNumeral() {
    if (countdown < 0) return box().absolute().width(0).height(0);
    char buf[8];
    std::snprintf(buf, sizeof buf, "%d", countdown);
    return box().left(1096).top(96).child(text(
        toU8(buf), magi::type(magi::latin(), 260.0f, magi::kRedHot, 1.2f)));
  }

  // ==========================================================================
  // THE PORTRAIT — Khara's conceptual diagram, behind the plate, showing
  // through the hole the three panels leave. Measured about its centre
  // (992,717) in its own 2000 px space; every radius below is that x 0.62 for
  // this canvas. THREE INCOMMENSURATE SYMMETRIES: a 6-fold core, a 16-fold
  // rotor, a 12-fold frame. Round the rotor to 12 and the plate stops being
  // engineering and becomes a mandala.

  static constexpr float kPScale = 0.62f;
  static constexpr float kPCX = 616.0f, kPCY = 640.0f;

  static SkPoint polar(float r, float deg) {
    const float a = deg * 0.017453293f;
    return {std::cos(a) * r, std::sin(a) * r};
  }

  Element hexAt(SkPoint p, float across, SkColor4f rim, float rimW,
                SkColor4f fill, const char* l1, const char* l2,
                float textSize) {
    Element h = box()
                    .left(p.fX - across * 0.5f)
                    .top(p.fY - across * 0.5f)
                    .width(across)
                    .height(across)
                    .shape(shapes::polygon(6, 0.0f))
                    .fill(mskia::Paint::solid(fill))
                    // Border::cornerAngleDeg defaults to 30 and finds ZERO
                    // corners above 12 sides. Passed explicitly everywhere.
                    .foreground(Border{.width = rimW,
                                       .fill = Fill::color(rim),
                                       .cornerAngleDeg = 20.0f});
    if (l1)
      h.child(text(toU8(l1),
                   magi::type(magi::latinPlain(), textSize, magi::kPRailHi))
                  .left(across * 0.30f)
                  .top(across * 0.34f));
    if (l2)
      h.child(text(toU8(l2),
                   magi::type(magi::latinPlain(), textSize, magi::kPRailDim))
                  .left(across * 0.30f)
                  .top(across * 0.34f + textSize * 1.15f));
    return h;
  }

  void buildBrushes() {
    // The bake cache lives IN THE BRUSH VALUE, so these are members built
    // once: a brush::Pattern constructed inside describe() re-bakes every tile
    // through snapshot() every frame.
    beadBrush =
        brush::Pattern{.side = box()
                                   .width(10.0f)
                                   .height(10.0f)
                                   .shape(shapes::circle())
                                   .fill(Fill::none())
                                   .foreground(decorations::border(
                                       1.8f, Fill::color(magi::kPRailHi))),
                       .advance = 14.0f,
                       .cornerAngleDeg = 34.0f,
                       .cornerLength = 0.0f,
                       // No corner art on this brush, so there is nothing for a
                       // corner alignment to align.
                       .stretchToFit = true,
                       .reach = 12.0f};
    chevronBrush =
        brush::Pattern{.side = box()
                                   .width(9.0f)
                                   .height(8.0f)
                                   .shape(shapes::arrow(0.10f, 0.90f))
                                   .fill(mskia::Paint::solid(magi::kPBodyHi)),
                       .advance = 10.0f,
                       .cornerAngleDeg = 34.0f,
                       .cornerLength = 0.0f,
                       // Likewise: no corner art on this brush either.
                       .stretchToFit = true,
                       .reach = 11.0f};
  }

  Element portraitStatic() {
    Element g = box().inset(0);
    const float S = kPScale;

    // two faint boundary conics — shapes::parametric, real curves
    for (float r : {780.0f * S, 860.0f * S})
      g.child(
          box()
              .left(kPCX - r)
              .top(kPCY - r * 0.985f)
              .width(r * 2)
              .height(r * 1.97f)
              .shape(shapes::parametric(
                  [](float t) {
                    return SkPoint{0.5f + 0.5f * std::cos(t),
                                   0.5f + 0.5f * std::sin(t)};
                  },
                  0.0f, 6.2831853f, 240, true))
              .fill(Fill::none())
              .foreground(decorations::border(
                  2.0f, Fill::color(scaleRgb(hex(0x5A1A0C), magi::kBack)))));

    // 12 neuron somas at r 690..790, each trailing dendrites BACK toward the
    // centre — brush::Ribbon, tapered.
    for (int k = 0; k < 12; ++k) {
      const float a = (float)k * 30.0f - 90.0f;
      const float r = (740.0f + (k % 2 ? 46.0f : -46.0f)) * S;
      const SkPoint p = polar(r, a);
      const float d = 92.0f * S;
      SkPathBuilder db;
      for (int j = 0; j < 9; ++j) {
        const float spread = ((float)j - 4.0f) * 3.4f;
        const SkPoint tip = polar(r - 210.0f * S, a + spread);
        const SkPoint mid = polar(r - 100.0f * S, a + spread * 0.45f);
        db.moveTo(kPCX + p.fX, kPCY + p.fY);
        db.quadTo(kPCX + mid.fX, kPCY + mid.fY, kPCX + tip.fX, kPCY + tip.fY);
      }
      const SkPath dend = db.detach();
      g.child(box()
                  .inset(0)
                  .fill(Fill::none())
                  // the callable is invoked on every layout, so its capture
                  // must survive each return
                  // NOLINTNEXTLINE(performance-no-automatic-move)
                  .shape([dend](SkSize) { return dend; })
                  .stroke(brush::Ribbon{
                      .fill = Fill::color(scaleRgb(hex(0x8A2412), magi::kBack)),
                      .widthStart = 8.0f,
                      .widthEnd = 1.0f,
                      .step = 6.0f}));
      g.child(
          box()
              .left(kPCX + p.fX - d * 0.5f)
              .top(kPCY + p.fY - d * 0.5f)
              .width(d)
              .height(d)
              .shape(shapes::circle())
              .fill(mskia::Paint::radialUnit(
                  {0.5f, 0.5f}, 1.0f,
                  {{0.0f, magi::kPBodyHi},
                   {0.55f, magi::kPBody},
                   {1.0f, scaleRgb(hex(0x3A0E06), magi::kBack)}}))
              .foreground(lines::presets::concentric(
                  Fill::color(scaleRgb(hex(0xC03C18), magi::kBack)), 4, 1.2f)));
    }

    // 24 small hexagons at r 555..665, two lines of tiny text each
    for (int k = 0; k < 24; ++k) {
      const float a = (float)k * 15.0f - 90.0f;
      const float r = ((k % 2) ? 610.0f : 560.0f) * S;
      const SkPoint p = polar(r, a);
      g.child(hexAt({kPCX + p.fX, kPCY + p.fY}, 48.0f * S, magi::kPRailHi, 1.4f,
                    scaleRgb(hex(0x2A0C05), magi::kBack), "TYPE", "M-04",
                    4.6f));
    }

    // the heavy arc the 12 big hexagons sit on, dressed with the bead and
    // chevron runs — brush::Pattern on an arc, alternating tiles.
    for (int seg = 0; seg < 12; ++seg) {
      const float a0 = (float)seg * 30.0f - 88.0f;
      const float r = 490.0f * S;
      SkPathBuilder ab;
      for (int s = 0; s <= 12; ++s) {
        const SkPoint p = polar(r, a0 + (float)s * 26.0f / 12.0f);
        if (s == 0)
          ab.moveTo(kPCX + p.fX, kPCY + p.fY);
        else
          ab.lineTo(kPCX + p.fX, kPCY + p.fY);
      }
      const SkPath arcp = ab.detach();
      Element run = box().inset(0).fill(Fill::none()).shape([arcp](SkSize) {
        // the callable is invoked on every layout, so its capture must survive
        // each return
        // NOLINTNEXTLINE(performance-no-automatic-move)
        return arcp;
      });
      if (seg % 2)
        run.stroke(beadBrush);
      else
        run.stroke(chevronBrush);
      g.child(std::move(run));
    }
    {
      const float r = 470.0f * S;
      SkPathBuilder ab;
      for (int s = 0; s <= 240; ++s) {
        const SkPoint p = polar(r, (float)s * 1.5f);
        if (s == 0)
          ab.moveTo(kPCX + p.fX, kPCY + p.fY);
        else
          ab.lineTo(kPCX + p.fX, kPCY + p.fY);
      }
      const SkPath arcp = ab.detach();
      g.child(box()
                  .inset(0)
                  .fill(Fill::none())
                  // the callable is invoked on every layout, so its capture
                  // must survive each return
                  // NOLINTNEXTLINE(performance-no-automatic-move)
                  .shape([arcp](SkSize) { return arcp; })
                  .stroke(lines::Rails{
                      .rails = {{.across = 6.0f,
                                 .width = 4.0f,
                                 .fill = Fill::color(magi::kPRailHi)},
                                {.across = 0.0f,
                                 .width = 1.0f,
                                 .fill = Fill::color(magi::kPPin),
                                 .dash = {3.0f, 11.0f}},
                                {.across = -6.0f,
                                 .width = 1.8f,
                                 .fill = Fill::color(magi::kPRail)}},
                      .offsetStep = 3.0f}));
    }

    for (int k = 0; k < 12; ++k) {
      const float a = (float)k * 30.0f - 90.0f;
      const SkPoint p = polar(490.0f * S, a);
      g.child(hexAt({kPCX + p.fX, kPCY + p.fY}, 74.0f * S, magi::kPRailHi, 2.2f,
                    scaleRgb(hex(0x351107), magi::kBack), "APS", "17", 5.4f));
    }

    // 12 pin combs at r ~300, plus the fan of fine hairlines out to the
    // hexagons; the chartreuse FLOW LADDER is the one honest use of mid-caps.
    for (int k = 0; k < 12; ++k) {
      const float a = (float)k * 30.0f - 90.0f;
      SkPathBuilder fb;
      for (int j = -3; j <= 3; ++j) {
        const SkPoint i0 = polar(300.0f * S, a + (float)j * 1.6f);
        const SkPoint i1 = polar(450.0f * S, a + (float)j * 4.2f);
        fb.moveTo(kPCX + i0.fX, kPCY + i0.fY);
        fb.lineTo(kPCX + i1.fX, kPCY + i1.fY);
      }
      const SkPath fan = fb.detach();
      g.child(box()
                  .inset(0)
                  .fill(Fill::none())
                  // the callable is invoked on every layout, so its capture
                  // must survive each return
                  // NOLINTNEXTLINE(performance-no-automatic-move)
                  .shape([fan](SkSize) { return fan; })
                  .stroke(PathFormat{.width = 0.8f,
                                     .strokeFill = Fill::color(scaleRgb(
                                         hex(0xD08A9A), magi::kBack))}));
      SkPathBuilder cb;
      const SkPoint base = polar(296.0f * S, a);
      const float m = std::hypot(base.fX, base.fY);
      const SkVector u{base.fX / m, base.fY / m};
      const SkVector n{-u.fY, u.fX};
      for (int j = -3; j <= 3; ++j) {
        const float off = (float)j * 5.0f;
        cb.moveTo(kPCX + base.fX + n.fX * off, kPCY + base.fY + n.fY * off);
        cb.lineTo(kPCX + base.fX + n.fX * off + u.fX * 11.0f,
                  kPCY + base.fY + n.fY * off + u.fY * 11.0f);
      }
      const SkPath comb = cb.detach();
      g.child(box()
                  .inset(0)
                  .fill(Fill::none())
                  // the callable is invoked on every layout, so its capture
                  // must survive each return
                  // NOLINTNEXTLINE(performance-no-automatic-move)
                  .shape([comb](SkSize) { return comb; })
                  .stroke(lines::Line{.width = 1.8f,
                                      .fill = Fill::color(magi::kPPin)}));
      SkPathBuilder rb;
      const SkPoint r0 = polar(320.0f * S, a + 12.0f);
      const SkPoint r1 = polar(470.0f * S, a + 12.0f);
      rb.moveTo(kPCX + r0.fX, kPCY + r0.fY);
      rb.lineTo(kPCX + r1.fX, kPCY + r1.fY);
      const SkPath ladder = rb.detach();
      g.child(box()
                  .inset(0)
                  .fill(Fill::none())
                  // the callable is invoked on every layout, so its capture
                  // must survive each return
                  // NOLINTNEXTLINE(performance-no-automatic-move)
                  .shape([ladder](SkSize) { return ladder; })
                  .stroke(lines::Line{.width = 0.8f,
                                      .fill = Fill::color(magi::kPChart),
                                      .midCap = lines::Cap::Arrow,
                                      .midSpacing = 24.0f}));
    }

    // the 6-fold core: seven flat-top hexagons in a honeycomb
    const float hexA = 88.0f * S;
    g.child(hexAt({kPCX, kPCY}, hexA, magi::kPRailHi, 2.0f,
                  scaleRgb(hex(0x4A140A), magi::kBack), "MAGI", "SYS", 5.6f));
    for (int k = 0; k < 6; ++k) {
      const float a = (float)k * 60.0f - 90.0f;
      const SkPoint p = polar(hexA * 0.90f, a);
      g.child(hexAt({kPCX + p.fX, kPCY + p.fY}, hexA, magi::kPRailHi, 1.8f,
                    scaleRgb(hex(0x3E1108), magi::kBack), "TYPE", "0417",
                    5.2f));
    }
    return g;
  }

  /** The 16-fold rotor: radial capsules, long axis pointing at the centre,
   *  alternating magenta 37x21 and violet 29x16, glowing. Its own node so it
   *  can turn against the rest. */
  /** Sized to the ROTOR, not to the canvas: this node lives under a bound
   *  rotation, so its Texture bake is a LOCAL bake blitted through the
   *  transform, and that blit is an area cost paid every frame. A full-canvas
   *  wrapper resamples the whole canvas to draw a 520 px disc; sizing the
   *  wrapper to the disc gives identical pixels. */
  static constexpr float kRotorR = 260.0f;
  Element portraitRotor() {
    Element g = box()
                    .left(kPCX - kRotorR)
                    .top(kPCY - kRotorR)
                    .width(kRotorR * 2)
                    .height(kRotorR * 2);
    const float S = kPScale;
    for (int spoke = 0; spoke < 16; ++spoke) {
      const float a = (float)spoke * 22.5f - 90.0f;
      for (int j = 0; j < 5; ++j) {
        const float r = (215.0f + (float)j * 42.0f) * S;
        const bool mag = ((unsigned)(spoke + j) & 1u) != 0;
        const float lw = (mag ? 37.0f : 29.0f) * S;
        const float lh = (mag ? 21.0f : 16.0f) * S;
        const SkPoint p = polar(r, a);
        g.child(box()
                    .left(kRotorR + p.fX - lw * 0.5f)
                    .top(kRotorR + p.fY - lh * 0.5f)
                    .width(lw)
                    .height(lh)
                    .rotate(a)
                    .corners(Corners{lh * 0.5f})
                    .fill(mskia::Paint::linearUnit(
                        {0, 0}, {1, 0},
                        {{0.0f, mag ? magi::kPMagenta : magi::kPViolet},
                         {0.5f, scaleRgb(mag ? hex(0xC464A5) : hex(0x643D93),
                                         magi::kBack)},
                         {1.0f, mag ? magi::kPMagenta : magi::kPViolet}})));
      }
    }
    return g;
  }

  /** Eight leader lines, DEAD HORIZONTAL, no arrowhead, no dot, no bend, at
   *  IRREGULAR spacings because they track their features. The portrait's own
   *  drafting chrome — kept off the plate, which carries none. Labels are
   *  two-tone: alternate words step #E17B33 / #B05A20. */
  Element portraitLabels() {
    static const char* kRuns[8][2] = {
        {"MAGI SYSTEM", "V2.3762.123b (Main nerve processing frame )"},
        {"MAGI SYSTEM", "Mirroring dummy (Extraordinary employment"},
        {"Nerve connection", "controller (Receptor)"},
        {"Nerve connection", "controller (Synapse)"},
        {"Thinking inflow", "partition (A priori SYSTEM)"},
        {"Thinking processing", "receptor (NERV MAGISYSTEM v1.187.12)"},
        {"MAGI thinking", "element unit (Main thinking neural network)"},
        {"All the virtual", "data areas (Main Data buffer)"},
    };
    // 2000-space y 664/734/811/914/984/1079/1202/1279 about centre 717,
    // x 0.62 — spacings 70,77,103,70,95,123,77: irregular, NOT a grid. The
    // space before the closing paren is Khara's own typo; kept.
    // Run 2's caption is three lines on the design art and is trimmed to two
    // here. Its tail ("at the time of partition un-developing. )") does not
    // fit: a third line closes to 15 px of run 3, the same leading the label
    // uses internally, and the two captions then read as one.
    static const float kDy[8] = {-33, 11, 58, 122, 166, 224, 301, 349};
    Element g = box().inset(0);
    const auto hi = magi::type(magi::latinPlain(), 11.5f, magi::kPRailHi);
    const auto dim = magi::type(magi::latinPlain(), 11.5f, magi::kPRailDim);
    for (int i = 0; i < 8; ++i) {
      const float y = kPCY + kDy[i];
      g.child(box().left(1078.0f).top(y).width(74.0f).height(1.1f).fill(
          mskia::Paint::solid(magi::kPRail)));
      g.child(box()
                  .left(1160.0f)
                  .top(y - 7.0f)
                  .maxWidth(272.0f)
                  .wrapLines()
                  .row()
                  .gap(4.0f)
                  .child(text(toU8(kRuns[i][0]), hi))
                  .child(text(toU8(kRuns[i][1]), dim)));
    }
    g.child(box()
                .left(20.0f)
                .top(18.0f)
                .column()
                .gap(2.0f)
                .child(text(u8"MAGI SYSTEM V2.3762.123b",
                            magi::type(magi::latin(), 12.5f, magi::kPRailHi)))
                .child(text(
                    u8"Conceptual Diagram",
                    magi::type(magi::latinPlain(), 11.5f, magi::kPRailDim))));
    return g;
  }

  // ==========================================================================
  // THE PLATE NUMBER — Ep 13's cross-section of CASPER's hatch, small in the
  // corner: the armoured plate, bolt bosses, `CASPER` stencilled ON A CURVE
  // (TextPath earning its keep), a window strapped shut with two crossed bars,
  // and a human brain behind the glass. The answer to "what is in there".

  Element hatchPlate() {
    constexpr float Wd = 208.0f, Ht = 164.0f;
    SkPathBuilder arcb;
    for (int s = 0; s <= 40; ++s) {
      const float t = (float)s / 40.0f;
      const float ang = (-168.0f + t * 104.0f) * 0.017453293f;
      const SkPoint p{Wd * 0.52f + std::cos(ang) * 82.0f,
                      Ht * 0.60f + std::sin(ang) * 70.0f};
      if (s == 0)
        arcb.moveTo(p);
      else
        arcb.lineTo(p);
    }
    const SkPath stencilArc = arcb.detach();

    Element g = box()
                    .left(148)
                    .top(872)
                    .width(Wd)
                    .height(Ht)
                    .shape(shapes::chamfered(22.0f, shapes::Corner::All))
                    .fill(mskia::Paint::solid(hex(0x322A36)))
                    .clip(true)
                    .foreground(Border{.width = 13.0f,
                                       .fill = Fill::color(hex(0x090509)),
                                       .inset = 6.5f,
                                       .cornerAngleDeg = 20.0f});
    for (int k = 0; k < 4; ++k) {
      const float bx = ((unsigned)k & 1u) ? Wd - 25.0f : 25.0f;
      const float by = ((unsigned)k & 2u) ? Ht - 23.0f : 23.0f;
      g.child(kit::disc(SkPoint{bx, by}, 8.5f)
                  .fill(mskia::Paint::solid(hex(0x120A12))));
      g.child(kit::disc(SkPoint{bx - 2.0f, by - 2.0f}, 5.0f)
                  .fill(Fill::none())
                  .foreground(
                      decorations::border(1.8f, Fill::color(hex(0x6E5E70)))));
    }
    Element tissue =
        box()
            // THE BRAIN IS THE SUBJECT of this plate, so it fills the
            // window rather than sitting in the middle of it as a card.
            .left(Wd * 0.20f)
            .top(Ht * 0.17f)
            .width(Wd * 0.60f)
            .height(Ht * 0.66f)
            // A ROUNDED OUTLINE, because a rectangle with folds drawn on
            // it is a card with folds drawn on it. The corners take almost
            // half the short side, which is as close to an organ as a
            // shape this small needs to be.
            .corners({Ht * 0.30f})
            .rotate(-9.0f)
            .fill(mskia::Paint::radialUnit({0.44f, 0.40f}, 1.05f,
                                           {{0.0f, hex(0xDBC49A)},
                                            {0.62f, hex(0xC0A277)},
                                            {1.0f, hex(0x97785D)}}))
            .overlay(lines::Line{.width = 2.0f,
                                 .fill = Fill::color(hex(0x4A2E1E)),
                                 .waveAmplitude = 3.2f,
                                 .waveLength = 16.0f})
            .foreground(decorations::border(1.8f, Fill::color(hex(0x1A0F14))));
    // THE SULCI. `overlay()` dresses a node's OUTLINE, and this node's
    // outline is its rectangle — so the wavy Line above deckles the tissue's
    // EDGE and lays nothing across it. The folds need geometry of their own
    // or the hatch shows a blank card instead of a brain; the ink and the
    // wave are the ones already chosen above.
    tissue.child(box()
                     .inset(0)
                     .shape([](SkSize s) {
                       const float w = s.width(), h = s.height();
                       SkPathBuilder b;
                       // the longitudinal fissure
                       b.moveTo(w * 0.54f, h * 0.03f);
                       b.quadTo(w * 0.39f, h * 0.30f, w * 0.55f, h * 0.53f);
                       b.quadTo(w * 0.71f, h * 0.77f, w * 0.49f, h * 0.97f);
                       // gyri, each stopping short of the fissure and of the
                       // rim
                       const float ys[3] = {0.24f, 0.52f, 0.79f};
                       for (float y : ys) {
                         b.moveTo(w * 0.06f, h * y);
                         b.quadTo(w * 0.24f, h * (y - 0.10f), w * 0.42f, h * y);
                         b.moveTo(w * 0.62f, h * (y + 0.05f));
                         b.quadTo(w * 0.80f, h * (y - 0.04f), w * 0.94f,
                                  h * (y + 0.07f));
                       }
                       return b.detach();
                     })
                     .stroke(lines::Line{.width = 1.6f,
                                         .fill = Fill::color(hex(0x4A2E1E)),
                                         .waveAmplitude = 1.5f,
                                         .waveLength = 10.0f}));
    g.child(std::move(tissue));
    g.child(box()
                .left(Wd * 0.31f)
                .top(Ht * 0.63f)
                .child(text(u8"MAGI", magi::type(magi::latin(), 28.0f,
                                                 hex(0x8C2A1E), 0.86f))));
    // THE STRAPS. "A human brain strapped behind glass" — two steel bands
    // bolted corner to corner, and they have to READ as steel: two flat
    // black bars of five pixels across a small card are a cancellation
    // cross, which is what an error placeholder looks like. So they run
    // rivet to rivet, they are wide enough to carry a bevel, and the bevel
    // is what says band rather than stroke.
    for (int k = 0; k < 2; ++k) {
      const float x0 = 25.0f, y0 = k ? Ht - 23.0f : 23.0f;
      const float x1 = Wd - 25.0f, y1 = k ? 23.0f : Ht - 23.0f;
      const float len = std::hypot(x1 - x0, y1 - y0);
      const float ang = std::atan2(y1 - y0, x1 - x0) * 57.29578f;
      g.child(box()
                  .left(x0)
                  .top(y0 - 7.0f)
                  .width(len)
                  .height(14.0f)
                  .rotate(ang)
                  .transformOrigin(0.0f, 0.5f)
                  .fill(mskia::Paint::linear({0, 0}, {0, 14},
                                             {{0.00f, hex(0x6A6470)},
                                              {0.22f, hex(0x8E8896)},
                                              {0.55f, hex(0x413B48)},
                                              {1.00f, hex(0x14101A)}}))
                  .foreground(
                      decorations::border(1.2f, Fill::color(hex(0x08050A)))));
    }
    // …and the glass they are behind: one diagonal sheen over the whole
    // plate, which is the difference between a card and a window.
    g.child(
        box()
            .inset(0)
            .fill(mskia::Paint::linear({0, Ht}, {Wd, 0},
                                       {{0.00f, {1, 1, 1, 0.00f}},
                                        {0.44f, {1, 1, 1, 0.00f}},
                                        {0.52f, {0.82f, 0.90f, 1.0f, 0.16f}},
                                        {0.60f, {1, 1, 1, 0.00f}},
                                        {1.00f, {1, 1, 1, 0.00f}}}))
            .blend(SkBlendMode::kPlus));
    g.child(
        box()
            .inset(0)
            .fill(Fill::none())
            .child(text(u8"CASPER", magi::type(magi::latin(), 32.0f,
                                               hex(0x0B060B), 0.88f, 3.0f))
                       .inset(0)
                       .onPath(TextPath{
                           // the callable is invoked on every layout, so its
                           // capture must survive each return
                           // NOLINTNEXTLINE(performance-no-automatic-move)
                           .path = [stencilArc](SkSize) { return stencilArc; },
                           .at = 0.5f,
                           .align = TextPath::Align::Center,
                           .orient = TextPath::Orient::Tangent})));
    return g;
  }

  // ==========================================================================

  /** The HUD slot: everything that changes on a CLOCK rather than on the
   *  front. The panels carry live materials, and a re-describe puts their
   *  bakes at risk, so a countdown numeral changing once a second must not
   *  drag the whole tree through render(describe()) with it. */
  Element hud() {
    return box().inset(0).child(verdictCard()).child(countdownNumeral());
  }

  Element describe() {
    Element root = box().inset(0);
    Element picture = box().inset(0);

    // The bus is drawn first. The square modules are masks over it, and their
    // labels live inside their rotated local coordinate systems.
    picture.child(plateFurniture().cache(Cache::Texture).key("furniture"));
    for (int i = 0; i < 3; ++i) picture.child(panelNode(i));
    // Headings and state cards occupy the frontmost UI layer.
    picture.child(plateType().cache(Cache::Texture).key("ptype"));
    picture.child(verdictBox());
    picture.child(slot("hud"));
    root.child(
        std::move(picture)
            .effect(mskia::Effect::phosphorBloom(10.0f, 0.46f, 0.44f, 0.84f))
            .key("phosphor"));

    // --- the tube ----------------------------------------------------------
    root.child(
        box()
            .left(0)
            .top(-8)
            .width(magi::kW)
            .height(magi::kH + 16)
            .fill(mskia::Paint::recipe(sigil::material::field::crtOverlay()))
            .translateY(&creep)
            .cache(Cache::Texture)
            .key("crt"));
    root.child(box()
                   .inset(0)
                   .fill(Fill::color({0, 0, 0, 1}))
                   .opacity(&flicker)
                   .key("flicker"));

    if (!auditOk)
      root.child(box()
                     .left(0)
                     .top(420)
                     .width(magi::kW)
                     .height(96)
                     .fill(Fill::color({1, 0, 1, 0.94f}))
                     .child(text(u8"MODULE RULE VIOLATED",
                                 magi::type(magi::latin(), 56.0f, {0, 0, 0, 1}))
                                .left(30)
                                .top(20)));
    return root;
  }

  // ==========================================================================
  // Coverage -> front, and the schedule. NOTHING here is tuned: the shader's
  // arrival field is mirrored on the CPU, sorted, and the quantile IS the
  // front, so asking for "30.2% of BALTHASAR at t = 2.5" is an array index.

  float frontAt(int i, double t) const {
    const magi::InfectionTiming timing = magi::kInfection[(size_t)i];
    double k = (t - timing.seedAt) / (timing.fullAt - timing.seedAt);
    k = std::clamp(k, 0.0, 1.0);
    // ease so the front decelerates as the panel fills, which is what the
    // episode's dialogue describes ("Balthazar is now taken over" lands late)
    double frac = k * k * (3.0 - 2.0 * k);
    // The material only changes when the front reaches another trace cell.
    // A small phase separates the panels' transition boundaries.
    frac =
        std::floor(frac * timing.levels + timing.phase + 1e-6) / timing.levels;
    frac = std::clamp(frac, 0.0, 1.0);
    return magi::frontFor(arrivals[(size_t)i], (float)frac);
  }

  void setup(sketch::SketchContext& ctx) override {
    // The reference moment the arrival field is SOLVED to land on — MELCHIOR
    // taken, BALTHASAR at the measured 30.2% with a ragged front; exact by
    // construction. By 6.0 s both MAGI are flat red and the verdict card is
    // still unfiled — nothing of the arrival field is left to see.
    sketch::kit::stage(ctx, {.size = SkSize::Make(magi::kW, magi::kH),
                             .captureAt = 2.5,
                             .background = magi::kGround});
    fonts = ctx.fonts;
    audit();

    arrivals.clear();
    std::printf("MAGI INTERIOR — the front, SOLVED (cell %.0f px)\n",
                magi::kCell);
    for (int i = 0; i < 3; ++i) {
      seeded[(size_t)i] = false;
      const magi::Panel& pp = panels[(size_t)i];
      arrivals.push_back(magi::arrivalTable(
          pp.box, magi::kCell, pp.seed,
          evangelion::panel({})({pp.box.width(), pp.box.height()})));
      // report what the schedule actually lands on at the reference moment
      double k = 0;
      {
        const magi::InfectionTiming timing = magi::kInfection[(size_t)i];
        k = std::clamp(
            (kRefT - timing.seedAt) / (timing.fullAt - timing.seedAt), 0.0,
            1.0);
        k = k * k * (3.0 - 2.0 * k);
        k = std::floor(k * timing.levels + timing.phase + 1e-6) / timing.levels;
      }
      const auto& tab = arrivals.back();
      float reach = 0;
      for (const auto& c : tab.cells)
        if (c.first < 1e3f) reach += c.second;
      std::printf(
          "  %-10s cells %3d  area %6.1f  reachable %.0f%%  "
          "coverage(2.5) = %5.1f%%  (measured %4.1f%%)\n",
          panels[(size_t)i].key, (int)tab.cells.size(), (double)tab.total,
          100.0 * reach / (double)tab.total, 100.0 * k, 100.0 * kWant[i]);
    }

    ctx.ticker.add([this](double dt) {
      clock += dt;
      const double t = std::fmod(clock, 26.0);
      front0 = frontAt(0, t >= 24.0 ? -1.0 : t);  // "we are 1 sec. ahead"
      front1 = frontAt(1, t);
      front2 = frontAt(2, t);
      // 提訴 — a proposal is FILED: 3 hard blinks, 220 on / 180 off.
      // ESTIMATED; a single frame cannot measure a blink, so it is flagged.
      kanjiHot = (t < 1.5 && std::fmod(t, 0.40) < 0.22) ? 1.0f : 0.0f;
      // 審議中 — itorr's --flash-time: .4s, step-end, dropping to .1s in EX
      // mode. EX mode here is "two MAGI taken".
      const double flash = (taken[2] && taken[1]) ? 0.10 : 0.40;
      goldOn = std::fmod(t, flash * 2.0) < flash ? 1.0f : 0.34f;
      creep = (float)((int)std::floor(clock * 0.5) % 4);
      flicker = std::fmod(clock, 4.0) < 0.04 ? 0.045f : 0.0f;
      return true;
    });

    ctx.composer.render(describe());
    ctx.composer.renderSlot("hud", hud());
  }

  void update(double elapsed, sketch::SketchContext& ctx) override {
    // The DATA path, and the ONLY re-describe: a panel is taken (3x a loop),
    // the verdict card steps (6x), the countdown ticks (13x). The infection
    // itself never re-describes — it is one uniform.
    const double t = std::fmod(elapsed, 26.0);
    std::array<bool, 3> now{false, false, false};
    std::array<bool, 3> sow{false, false, false};
    for (int i = 0; i < 3; ++i) {
      const magi::InfectionTiming timing = magi::kInfection[(size_t)i];
      now[(size_t)i] = t >= timing.fullAt && t < 24.0;
      sow[(size_t)i] = t >= timing.seedAt && t < 24.0;
    }
    if (t < 12.0) now[0] = false;

    int vs = -1;
    if (t >= 6.5 && t < 8.5)
      vs = std::min(3, (int)((t - 6.5) / 0.5));
    else if (t >= 8.5 && t < 24.0)
      vs = 4;
    else if (t >= 24.0)
      vs = 5;

    int cd = -1;
    if (t >= 9.0 && t < 11.0)
      cd = 20;
    else if (t >= 11.0 && t < 13.0)
      cd = 15;
    else if (t >= 13.0 && t < 14.0)
      cd = 10;
    else if (t >= 14.0 && t < 23.0)
      cd = 9 - (int)(t - 14.0);
    else if (t >= 23.0 && t < 25.0)
      cd = 0;

    const bool structural = now != taken || sow != seeded;
    const bool hudChanged = vs != verdictStep || cd != countdown;
    if (!structural && !hudChanged) return;
    taken = now;
    seeded = sow;
    verdictStep = vs;
    countdown = cd;
    if (structural) ctx.composer.render(describe());
    ctx.composer.renderSlot("hud", hud());
  }
};

SIGIL_SKETCH(
    EvaMagiInterior, "Study \xc2\xb7 Film",
    "Evangelion Ep 13 under Ireul \xe2\x80\x94 the camera roll was the "
    "projection; the infection is a shader")
