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
// THIS IS A RECONSTRUCTION AND IT SAYS SO. One canvas assembled from four
// documented plates, the way lain_navi was: (1) the Ep 13 deliberation frame,
// THE ANCHOR — but see THE REFERENCE IS IN PERSPECTIVE below, because that
// sentence is the whole methodological point of this study; (2) the Khara
// design art "MAGI SYSTEM V2.3762.123b — Conceptual Diagram", 2000x1397, the
// machine's own portrait, living BEHIND the plate and showing through the hole
// the three panels leave; (3) the Ep 13 cross-section of CASPER's bolted hatch
// — a human brain strapped behind glass — as the study's plate number in the
// corner; (4) the Ep 13 transcript, for every timing. No frame of the episode
// contains all four.
//
// -----------------------------------------------------------------------------
// THE REFERENCE IS IN PERSPECTIVE, AND EVERY NUMBER IN MY FIRST PASS WAS WRONG
//
// `eva13_deliberation.png` is the plate seen at an angle. I built a whole pass
// against it before that was pointed out, and the measurements that looked
// most like findings were the ones that were purely the projection:
//
//   * a "camera roll varying 8.1 -> 16.4 deg with position" — that is what a
//     CONSTANT roll looks like under perspective;
//   * a "0.75x keystone across the plate", quoted as the recession — that is
//     the projection itself, quoted back;
//   * "45 deg cuts, hand-painted, +-8 deg" measured at 47 and -30 in frame;
//   * a "pale halation rim" round each panel, which I attributed to a
//     photographed CRT and which is simply the cel's own edge light.
//
// I could not find a flat original. WHERE I LOOKED: EvaGeeks/EvaWiki (its
// only MAGI-display still is `Eoe_magi_display.jpg`, Ep 25'), the Evangelion
// Fandom wiki's Magi and Episode:13 galleries, nervarchives.com's Ep 13 page,
// the Japanese EVA wikis (wikiwiki.jp/eva-shingeki, evangelion.dancing-doll),
// fontsinuse 28760, and the recreation repos (itorr/magi, 07JP27/MagiSystem,
// hirakujira/MAGI-System, mdrbx + TheGreatGildo/nerv-ui). The recreations are
// flat but they are AUTHORED, not the artefact. So I rectified it explicitly,
// and the work is on disk in `run2/work/flatten.py`, next to its output
// `ref-images/eva13_deliberation_FLAT.png` (+ `_FLAT_GRID.png`).
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
// WHAT SURVIVED, AND IS NOW A MUCH STRONGER CLAIM THAN IT WAS:
//
//   THE GENERATING RULE. "Every panel is a rectangle whose corner FACING THE
//   CENTRE is chamfered off." In the flat plate the panels are literally
//   axis-aligned rectangles — CASPER 390x294 at (158,562), BALTHASAR 330x414
//   at (455,198), MELCHIOR 368x290 at (682,548) — and their cuts measure
//   41.0, 45.9 and 46.2 deg. The brief's "45 deg, hand-painted, +-8" is
//   CONFIRMED, at +-4. And the bearing test tightens from 19.5/19.4/12.2 deg
//   in the frame to 14.6/18.4/2.0 in the flat plate. audit() prints it.
//
// -----------------------------------------------------------------------------
// WHAT ELSE THE MEASUREMENT CORRECTED IN THE BRIEF — FIVE MORE
//
//  1. THE INFECTION IS A SHADER OVER THE FILL, NOT GEOMETRY. Its cell is
//     32 px in the flat plate: red vertical runs p50 32, azure gaps p50 33,
//     red horizontal p50 37 — one grid, three statistics. Cells flip WHOLE
//     (there is no partially-shaded cell anywhere in the frame), which is a
//     step function of position against one advancing value, i.e. exactly a
//     shader and exactly not a growing set of rectangles. Built as one
//     `Material::sksl` overlay per panel with a single bound `uFront`; the
//     panel's own azure is underneath and shows through where the field is
//     off. My first pass built it as ~110 animated rects per panel and it
//     re-described the tree on every step; this is one draw and one uniform.
//  2. THE PANEL RIM IS THE CEL'S EDGE LIGHT, NOT itorr's BLACK KEYLINE. The
//     brief takes `.1em black / .4em currentColor / .5em black` as the panels'
//     frame. Cut across CASPER's left edge: ground (9,0,0) -> (137,94,116) ->
//     (168,140,175) -> azure. A pale lavender rim two px wide, and no dark
//     band anywhere. (itorr's DOUBLE rule on the 審議中 box IS on the plate.)
//  3. THE GREEN BANDS SANDWICH THE KANJI, THEY ARE NOT A DOUBLE RULE. Four
//     independent horizontal rules, ~19 px thick in the flat plate, one over
//     and one under each kanji pair, ~120 px apart. The brief asks for
//     `lines::Rails` at 8 px with small offsets; a Rails value with a 120 px
//     span is a true statement and a useless one. They ARE serrated — a fine
//     checker along their length, built as a crosshatch, not a dash.
//  4. THERE ARE THREE ORANGE RAILS, NOT FOUR. Component analysis of the
//     orange mask finds exactly three, and nothing at all where the brief puts
//     a fourth "centre node" stub. What is there is the MAGI wordmark's own
//     ink. The centre of this diagram is a hole with a word in it.
//  5. THE FILE BLOCK IS SET NEARLY SOLID. The brief reports leading/cap =
//     1.96. The four lines start at y = 415, 439, 463, 487 in the flat plate
//     with cap 18: leading/cap = 1.33, which is what makes it read as a
//     machine dump rather than a caption.
//
// -----------------------------------------------------------------------------
// SOURCES — read and MEASURED
//
//  * The frame itself, rectified as above. `work/flatten.py` is the whole
//    derivation; `work/diff_magi.py` is the per-region diff that closed it.
//  * github.com/itorr/magi (131*) — a recreation of THIS screen. Taken: the
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
//  * github.com/mdrbx/nerv-ui (MIT) — cited as a PALETTE WARNING, the way
//    lain_navi cited the Lain SDDM theme. Its tokens are #FF9900 / #00FF00 /
//    #00FFFF, the fan-canonical "NERV colours", and NONE is on this plate.
//    Its `border-radius: 0` design law is right and is obeyed here.
//  * github.com/TheGreatGildo/nerv-ui — components/crt-effects.css, already
//    cine-01's CRT stack. Reused unchanged so both Eva studies share one tube.
//  * fontsinuse.com/uses/28760 — NERV panels are Helvetica / Helvetica
//    Condensed. Anno picked Matisse EB (Fontworks) for the Japanese; the free
//    stand-in installed here is Songti SC Black, a heavy Ming.
//
// -----------------------------------------------------------------------------
// BUILT FROM (the library, not by hand)
//   Element::outline               every panel: an axis-aligned box with ONE
//                                  corner cut — see the GAP, the cut is not
//                                  square and shapes::chamfered only cuts 45
//   decorations::border            the pale edge-light rim, on those outlines,
//                                  following the cut untold
//   decorations::doubleBorder      the 審議中 box (itorr's triple box-shadow)
//   Material::sksl + one uniform   THE INFECTION. One shader, one bound
//                                  Output, zero nodes per cell
//   rail() + routers::polyline(0)  the three orange stubs, anchored on the
//                                  panels' own keys at normalized points
//   lines::Rails                   BALTHASAR's own doubled circuitry (two
//                                  rails at UNEQUAL offsets) and the
//                                  portrait's bead arc
//   lines::crosshatch              the serration inside the four green bands
//   lines::Line{.midCap = Arrow}   the portrait's chartreuse flow ladder —
//                                  the one place mid-caps are correct
//   brushes::PatternBrush          the portrait's hollow-ring bead runs and
//                                  arrowhead chevron runs; built ONCE as
//                                  members (the bake cache lives in the value)
//   brushes::Ribbon (widthFn-free) the neuron dendrites, tapered
//   shapes::polygon / chamfered    the hex lattice (cornerAngleDeg passed
//                                  EXPLICITLY everywhere); the hatch plate
//   shapes::parametric             the portrait's boundary conics
//   TextPath{orient = Tangent}     `CASPER` stencilled round the hatch arc
//   Material::linearUnit/radialUnit the portrait ONLY — the plate's panels are
//                                  flat (measured p5->p95 spans 10 luma)
//   Cache::Texture                 the portrait, the furniture, the type, CRT
//   hard steps                     every blink; nothing here fades
//
// -----------------------------------------------------------------------------
// LIBRARY GAPS — and ROADMAP §8's second independent citation
//
//  A. `routers::orthogonal()` CANNOT BE HANDED TO `rail()`, AND THIS ARTEFACT
//     IS A PCB. `orthogonal()` is a `Router(SkRect, SkRect)` that always bends
//     at midX; `rail()` needs a `RailRouter(std::span<const SkPoint>)`, and the
//     only ones are polyline / octilinear / orbit. There is no adapter, so the
//     obvious call does not compile — I checked the header rather than assume.
//     ROADMAP §8 logs this, found "building a PCB-style node graph". This is
//     its SECOND independent citation and it comes from the study of an angel
//     that turns a computer into circuitry. Natural API, unchanged from §8:
//     `routers::manhattan(Bend::HFirst|VFirst|MidX)` as a RailRouter.
//  B. NEITHER RAIL ROUTER CAN CUT A CORNER — both only round
//     (SkCornerPathEffect), so `polyline(0)` (no rounding at all) is the only
//     usable setting on a plate whose every elbow is square. §8 already asks
//     for a 45 deg chamfer alternative; seconded.
//  C. NO JUNCTION DOT AND NO HOP-OVER, IN ANY FORM. `PathFormat{.midCap = Dot,
//     .midSpacing = n}` is a tick ladder — a dot every n px of ARC — which is
//     the opposite of a via, whose definition is "where two runs meet". The
//     natural thing is `lines::Junctions{.dotRadius, .minDegree}`, finding
//     them from the path's own self-intersections; it would serve every
//     circuit, metro map and plumbing diagram anyone will draw here.
//  D. **`shapes::chamfered(cut, mask)` TAKES ONE SCALAR AND THEREFORE ONLY
//     CUTS AT 45 DEGREES.** Measured in the flat plate, this artefact's cuts
//     are 155x135, 136x142 and 66x68 px — 41.0, 46.2 and 45.9 deg. Two of the
//     three are 45 to within a degree and CASPER's is not, and there is no
//     spelling for it: `chamfered(155)` and `chamfered(135)` are both wrong.
//     Worked around with a 12-line `cutBox()` below. Natural API:
//     `shapes::chamfered(SkVector cut, Corner mask)` with the scalar overload
//     kept — this is a strictly-wider signature and nothing breaks.
//  E. NO PERSPECTIVE TRANSFORM. `Element` is affine-only, and `outline()` can
//     apply a homography to GEOMETRY but not to text or to a subtree. That is
//     what stopped the FIRST version of this sketch from simply drawing the
//     flat plate and projecting it, and it is why the rectification had to
//     happen in the measurement instead of in the render. One roadmap line —
//     `Element::perspective(SkMatrix)` on a Texture-cached subtree would be
//     the whole feature, since a bake is already a texture and Skia will
//     happily draw one under a 3x3.
//
// What was NOT a gap, checked in the header rather than assumed:
// `PatternBrush::cornerLength`/`cornerAlign` do exactly what the brief says;
// `lines::Rails` really does dash in CENTRELINE arc-space so unequal-offset
// rails stay in register; `decorations::border` really does follow a cut
// outline untold. `Border::cornerAngleDeg` defaults to 30 and finds ZERO
// corners above 12 sides — passed explicitly on every hex here.
//
// -----------------------------------------------------------------------------
// PERF — 147.18 -> 15.42 ms p99, and every step of it was a measurement
//
//  147.2  the honest build of the mechanism: one Material::sksl per panel, its
//         arrival field evaluated FIVE times per pixel (self + four
//         neighbours, for the keyline), each a Manhattan distance over two
//         4-tap smooth value-noise fields = ~65 hash evaluations per pixel.
//         BALTHASAR's 330x414 alone was 72.50 ms and CASPER's 57.22. Also
//         `not baked: promotion opted out`, because I had typed
//         `.cache(Cache::None)` on the infection node out of habit — the
//         library was ASKING to bake it and I had told it not to.
//   20.1  THE LOBES MADE BLOCKY AND THE KEYLINE BRANCHED. The two smooth
//         noises became ONE hash of a coarse cell each (0.28 x 1.0 and
//         1.0 x 0.28 — the anisotropy that makes the front grow in fingers
//         lives in the CELL SHAPE, not in the interpolation), and the
//         neighbour lookups moved behind three gates: only an OFF cell can
//         carry a keyline, only a hash-chosen one does, and only within 0.21
//         of an edge. The average pixel went from 5 arrivals to 1.34 and from
//         ~65 hashes to ~3. 72.50 -> 11.16 ms on the same node — and it looks
//         BETTER, because nothing in this cel is smooth.
//    4.1  THE FRONT QUANTIZED. A continuously bound uniform makes a material
//         live forever, so `not baked: its content changes every frame` and
//         136k px of SkSL ran sixty times a second to show a field that only
//         changes when the front crosses a cell. Held still between steps the
//         automatic promoter bakes it, and the node drops to 0.19 ms reading
//         `[TEXTURE, promoted by the library — not by you]`. The step is
//         invisible because the field is already a per-cell step function.
//         Same lesson as `Material::quantizeTime`, reached from the other
//         end: declared choppiness is what makes a live material cacheable.
//         AND THE ROTOR'S WRAPPER SIZED TO THE ROTOR. Its Texture bake sits
//         under a bound rotation, so it is a LOCAL bake blitted through the
//         transform — an AREA cost. A full-canvas wrapper made that a 1.5 MP
//         resample every frame (6.44 ms); its own 520 px box is 0.27 MP
//         (1.22 ms) for identical pixels. lain_navi's "the binding goes on a
//         wrapper", one level further in: the wrapper must be the right SIZE.
//
//  Then --bench AT FOUR MOMENTS, because one sample lies about a timed reveal
//  exactly the way one capture does. t=3.0 passed at 14.9 and t=1.2 FAILED at
//  24.16, which is the only moment two panels fall at once:
//
//   15.4  (a) A PANEL IREUL HAS NOT REACHED CARRIES NO INFECTION NODE. A
//         shader whose front is below every cell still runs over every pixel
//         to prove it: CASPER's 115k px were 100% waste for twelve seconds of
//         a twenty-six second loop. Gated on `seeded`, which is the one place
//         here where the DATA path beats a uniform.
//         (b) PER-PANEL STEP RATES, STAGGERED. Each step is one re-bake, so
//         the step RATE is the cost. MELCHIOR falls in 2.5 s and 40 steps
//         there is 16 re-bakes a second; 8 is 3/s and indistinguishable. A
//         0.37-of-a-step phase offset keeps two panels off the same frame.
//         (c) A 3-OP R2 (Roberts) HASH FOR THE PER-CELL GRAIN instead of
//         Hoskins' 12-op one — a punch-out mask does not need the quality,
//         and it is a third of every arrival's cost.
//         (d) THE COUNTDOWN AND VERDICT CARD MOVED INTO A `slot()`. t=15
//         failed at 19.07 for a reason that has nothing to do with pixels: a
//         numeral changing once a second called `render(describe())` on a
//         tree carrying three live materials. `renderSlot("hud", ...)` leaves
//         the panels alone; 19.07 -> 12.57.
//
//   MEASURED AT FOUR MOMENTS, 120 frames each, 1440x1052:
//     t = 1.2  (two MAGI falling)  p50 4.09  p99 15.42  mean 5.51
//     t = 2.5  (THE REFERENCE)     p50 3.85  p99 14.67  mean 5.23
//     t = 15   (countdown running) p50 3.70  p99 12.57  mean 4.25
//     t = 20   (settled)           p50 3.70  p99 10.50  mean 3.98
//   PASS at all four. The p99 is a front step re-baking one panel, which
//   happens ~3-6 times a second while a MAGI is falling and never otherwise.
//
//   AND THE ALTERNATIVE, BENCHED AS ASKED: the first version of this study
//   drew the infection as ~110 animated rectangles per panel behind a
//   turn-penalty Dijkstra front, and it was NOT slower — a few hundred
//   axis-aligned drawRects are microseconds on this host. What it was, was
//   WRONG. It re-described the tree on every step; it needed a whole graph
//   search to produce raggedness the shader gets from one anisotropic hash;
//   and it could not be phased onto a coverage number without a bisection,
//   where the shader's field inverts to a quantile in one array index. The
//   shader is the original technique AND the better engineering — but the
//   honest note is that on a CPU raster host the geometry version would have
//   passed the gate too, and on a 2 MP GPU canvas the ranking would reverse.
//
// -----------------------------------------------------------------------------
// Run:
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/eva_magi_interior.cpp \
//       --frame /tmp/eva_magi_interior.png --at 2.5
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

#include <sigilsketch/Sketch.h>

#include <sigilcompose/Brushes.h>
#include <sigilcompose/Decorations.h>
#include <sigilcompose/Lines.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Routers.h>
#include <sigilcompose/Shapes.h>
#include <sigilcompose/Util.h>

#include <sigilweave/ports/SystemFontManager.h>

#include <include/core/SkCanvas.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkTypeface.h>
#include <include/effects/SkRuntimeEffect.h>

#include <algorithm>
#include <array>
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

namespace magi {

constexpr float kW = 1440.0f, kH = 1052.0f;

inline SkColor4f hex(uint32_t v, float a = 1.0f) {
  return {(float)((v >> 16) & 255) / 255.0f, (float)((v >> 8) & 255) / 255.0f,
          (float)(v & 255) / 255.0f, a};
}
/** Scale a colour's light. The portrait is a different plate at a different
 *  exposure and sits BEHIND this one; the anchor's own floor (37% of the frame
 *  below luma 12, p99 153, max 241) is what it has to stay under. */
inline SkColor4f dim(SkColor4f c, float k) {
  return {c.fR * k, c.fG * k, c.fB * k, c.fA};
}

// ---------------------------------------------------------------------------
// PALETTE — HSV-class percentiles over the anchor. There is NO WHITE.

const SkColor4f kGround = hex(0x040001);  // dark p50 — a RED-cast black
const SkColor4f kAzure = hex(0x579ED5);   // measured p50 inside CASPER; FLAT
const SkColor4f kRed = hex(0xAA0506);     // MELCHIOR p50 — and the traces, to
const SkColor4f kRedHot = hex(0xB0090A);  // within 1 LSB: ONE colour, TWO
                                          // coverages, which is the whole idea
const SkColor4f kTraceDark = hex(0x080102); // the pour's own keyline
const SkColor4f kInk = hex(0x00093C);     // label on a panel: dark NAVY
const SkColor4f kInkRed = hex(0x3A0A18);  // the same ink over MELCHIOR's red
const SkColor4f kOrange = hex(0xE56C1A);  // rails, rims, the MAGI mark
const SkColor4f kOrangeDim = hex(0xB4571A);
const SkColor4f kKanji = hex(0xE16417);   // very slightly redder than the rails
const SkColor4f kKanjiHot = hex(0xFFA050); // 提訴 mid-pulse
const SkColor4f kGold = hex(0xC4AC50);
const SkColor4f kGoldHot = hex(0xE8D371);
const SkColor4f kGoldPeak = hex(0xF4DD8C);
const SkColor4f kGreen = hex(0x175746);   // 0.16% of the frame and it carries
const SkColor4f kGreenHi = hex(0x2A6B58); // the whole top band
const SkColor4f kEdgeLight = hex(0xA88CAF); // the cel's own edge light
const SkColor4f kBgRule = hex(0x6E3228);  // the plate's dim diagonal furniture

// The portrait's warm palette — a DIFFERENT plate, kept different on purpose.
// Two palettes, one canvas; they share the orange, so the orange is the seam.
constexpr float kBack = 0.42f;
const SkColor4f kPBody = dim(hex(0x711B0F), kBack);
const SkColor4f kPBodyHi = dim(hex(0xB63014), kBack);
const SkColor4f kPRail = dim(hex(0x703912), kBack);
const SkColor4f kPRailHi = dim(hex(0xE17B33), kBack);
const SkColor4f kPRailDim = dim(hex(0xB05A20), kBack);
const SkColor4f kPChart = dim(hex(0xB1CE3C), kBack);
const SkColor4f kPPin = dim(hex(0xEFE033), kBack);
const SkColor4f kPMagenta = dim(hex(0xAD5196), kBack);
const SkColor4f kPViolet = dim(hex(0x4A3283), kBack);

// ---------------------------------------------------------------------------
// TYPE

inline sk_sp<SkTypeface> face(const char *family, int weight,
                              const char *fallback) {
  auto mgr = weave::ports::systemFontManager();
  sk_sp<SkTypeface> f = mgr->matchFamilyStyle(
      family, SkFontStyle(weight, SkFontStyle::kNormal_Width,
                          SkFontStyle::kUpright_Slant));
  if (!f && fallback)
    f = mgr->matchFamilyStyle(
        fallback, SkFontStyle(weight, SkFontStyle::kNormal_Width,
                              SkFontStyle::kUpright_Slant));
  if (!f)
    f = mgr->matchFamilyStyle(nullptr, SkFontStyle::Bold());
  return f;
}
inline const sk_sp<SkTypeface> &latin() {
  static sk_sp<SkTypeface> f =
      face("Helvetica", SkFontStyle::kBold_Weight, "Arial");
  return f;
}
inline const sk_sp<SkTypeface> &latinPlain() {
  static sk_sp<SkTypeface> f =
      face("Helvetica", SkFontStyle::kNormal_Weight, "Arial");
  return f;
}
inline const sk_sp<SkTypeface> &han() {
  static sk_sp<SkTypeface> f =
      face("Songti SC", SkFontStyle::kBlack_Weight, "Hiragino Mincho ProN");
  return f;
}

inline weave::TextStyle type(const sk_sp<SkTypeface> &tf, float size,
                             SkColor4f color, float condense = 1.0f,
                             float track = 0.0f) {
  weave::TextStyle s;
  s.shaping.typeface = tf;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = track;
  s.shaping.scaleX = condense;
  s.paint.foreground.setColor4f(color, nullptr);
  s.paint.foreground.setAntiAlias(true);
  return s;
}

// ---------------------------------------------------------------------------
// THE PLATE, measured in the RECTIFIED frame. Axis-aligned boxes with one
// corner cut — which is the generating rule, stated as a construction now
// that the projection is out of the way.

/** GAP D's workaround: a box with an INDEPENDENT x and y cut per corner.
 *  `shapes::chamfered` takes one scalar and therefore only cuts at 45 deg;
 *  this plate's three cuts measure 41.0, 45.9 and 46.2. Twelve lines, and the
 *  library should just widen the signature. */
enum CutCorner { CutTL = 1, CutTR = 2, CutBR = 4, CutBL = 8 };
inline shapes::OutlineFn cutBox(float cx, float cy, int mask) {
  return [cx, cy, mask](SkSize s) {
    const float w = s.width(), h = s.height();
    const float x = std::min(cx, w * 0.5f), y = std::min(cy, h * 0.5f);
    SkPathBuilder b;
    b.moveTo((mask & CutTL) ? x : 0.0f, 0);
    if (mask & CutTR) {
      b.lineTo(w - x, 0);
      b.lineTo(w, y);
    } else {
      b.lineTo(w, 0);
    }
    if (mask & CutBR) {
      b.lineTo(w, h - y);
      b.lineTo(w - x, h);
    } else {
      b.lineTo(w, h);
    }
    if (mask & CutBL) {
      b.lineTo(x, h);
      b.lineTo(0, h - y);
    } else {
      b.lineTo(0, h);
    }
    if (mask & CutTL)
      b.lineTo(0, y);
    b.close();
    return b.detach();
  };
}

struct Panel {
  const char *key;
  const char *label;
  SkRect box;         // the axis-aligned rectangle, measured in the flat plate
  SkVector cut;       // the corner cut, x and y independently
  int cutMask;
  SkPoint labelInk;   // measured ink top-left of the label run
  float labelCap;     // measured cap height
  float labelInkW;    // measured ink span — the condensation solves off it
  int circuitRuns;    // how much of its own circuitry the frame shows
  SkPoint seed;       // where Ireul enters, in cell units
};

inline std::vector<Panel> panels() {
  return {
      // CASPER·3 — the biggest, and the one that survives. Cuts its TOP-RIGHT.
      // ink x 222..508 -> span 286, cap 38.
      {"casper", "CASPER·3",
       SkRect::MakeLTRB(158, 562, 548, 856), {155, 135}, CutTR,
       {221, 698}, 44.0f, 278.0f, 0, {11.5f, 1.5f}},
      // BALTHASAR·2 — above the centre, so it cuts BOTH bottom corners and
      // presents a flat.
      {"balthasar", "BALTHASAR·2",
       SkRect::MakeLTRB(455, 198, 785, 612), {66, 68}, CutBL | CutBR,
       {452, 449}, 44.0f, 330.0f, 13, {6.5f, 5.0f}},
      // MELCHIOR·1 — taken. Cuts its TOP-LEFT.
      {"melchior", "MELCHIOR·1",
       SkRect::MakeLTRB(682, 548, 1050, 846), {136, 142}, CutTL,
       {700, 691}, 43.0f, 328.0f, 6, {10.5f, 0.5f}},
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
inline const char *kInfectionSrc = R"SKSL(
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
// noise. That was a 24x cost cut (see PERF) and it is also the truer choice:
// nothing in this cel is smooth. A third hash punches out ~11% of cells
// permanently — jackestar's occupancy grid, and the azure islands the frame
// shows inside the pour.
float arrival(float2 c, float2 seed) {
  float2 d = c - seed;
  float manh = abs(d.x) + abs(d.y);
  float nh = h21(floor(c * float2(0.28, 1.0)));
  float nv = h21(floor(c * float2(1.0, 0.28)) + 41.0);
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
    if (!effect)
      SkDebugf("magi infection shader: %s\n", err.c_str());
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
  const float d =
      qx * (qy + 33.33f) + qy * (qz + 33.33f) + qz * (qx + 33.33f);
  qx += d;
  qy += d;
  qz += d;
  return fract1((qx + qy) * qz);
}
inline float arrival(float cx, float cy, SkPoint seed) {
  const float manh = std::fabs(cx - seed.fX) + std::fabs(cy - seed.fY);
  const float nh = h21(std::floor(cx * 0.28f), std::floor(cy * 1.00f));
  const float nv = h21(std::floor(cx * 1.00f) + 41.0f,
                       std::floor(cy * 0.28f) + 41.0f);
  const float grain = fract1(cx * 0.7548777f + cy * 0.5698403f + 0.137f);
  const float m = std::max(nh, nv);
  return manh / (0.38f + 1.70f * m * m) + 1.7f * grain +
         (grain < 0.11f ? 1e4f : 0.0f);
}

/** One panel's cells, sorted by arrival and carrying the AREA each actually
 *  contributes — a cell the panel's cut clips in half is half a cell of pour,
 *  and the anchor's 30.2% is an AREA fraction. Without this the model reports
 *  30.1% while the render measures 24.5%, which is exactly the kind of quiet
 *  6-point lie this program exists to catch. */
struct Arrivals {
  std::vector<std::pair<float, float>> cells; // (arrival, area weight)
  float total = 0;
};
inline Arrivals arrivalTable(SkRect box, float cell, SkPoint seed,
                             const SkPath &outline) {
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
      if (in == 0)
        continue;
      const float w = (float)in / 16.0f;
      A.cells.push_back({arrival((float)x, (float)y, seed), w});
      A.total += w;
    }
  std::sort(A.cells.begin(), A.cells.end(),
            [](const auto &a, const auto &b) { return a.first < b.first; });
  return A;
}
/** The front value that lands the AREA coverage on `frac`. */
inline float frontFor(const Arrivals &A, float frac) {
  if (A.cells.empty() || A.total <= 0)
    return -1.0f;
  if (frac <= 0.0f)
    return -1.0f; // nothing on: CASPER is CLEAN and must render clean
  const float want = frac * A.total;
  float acc = 0;
  for (const auto &c : A.cells) {
    acc += c.second;
    if (acc >= want)
      return c.first + 0.001f;
  }
  return A.cells.back().first + 0.001f;
}

// ---------------------------------------------------------------------------
// The panel's own orthogonal circuitry — near-navy hairlines on the azure,
// generated as point runs and fed to lines::Rails, because the frame shows
// DOUBLED runs and a single hairline is not the honest spelling.

inline uint32_t hash32(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  x ^= x >> 16;
  return x;
}
inline float hashF(int a, int b, int salt) {
  return (float)(hash32((uint32_t)(a * 73856093) ^ (uint32_t)(b * 19349663) ^
                        (uint32_t)(salt * 83492791)) &
                 0xffffff) /
         16777215.0f;
}
constexpr float kCell = 32.0f;
constexpr int kDX[4] = {1, 0, -1, 0};
constexpr int kDY[4] = {0, 1, 0, -1};

inline SkPath ownCircuitry(SkSize s, int salt, int runs) {
  SkPathBuilder b;
  for (int r = 0; r < runs; ++r) {
    float x = std::round(hashF(r, 3, salt) * s.width() / kCell) * kCell;
    float y = std::round(hashF(r, 7, salt) * s.height() / kCell) * kCell;
    int dir = (int)(hashF(r, 11, salt) * 4.0f) & 3;
    const int steps = 3 + (int)(hashF(r, 13, salt) * 7.0f);
    bool started = false;
    for (int i = 0; i < steps; ++i) {
      // jackestar's lineAngleVariation, as a per-step turn probability
      if (hashF(r, i * 31 + 5, salt) < 0.26f)
        dir = (dir + (hashF(r, i * 37, salt) < 0.5f ? 1 : 3)) & 3;
      const float nx = x + (float)kDX[dir] * kCell;
      const float ny = y + (float)kDY[dir] * kCell;
      if (nx < 4 || ny < 4 || nx > s.width() - 4 || ny > s.height() - 4)
        break;
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
// THE CRT. cine-01's stack, transcribed from TheGreatGildo/nerv-ui
// components/crt-effects.css and reused UNCHANGED so both Eva studies share
// one tube: 2 px scanlines at ~4% black, a 70%/70% vignette ellipse to 40%.

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
      SkDebugf("magi crt shader: %s\n", err.c_str());
    return effect;
  }();
  return fx;
}

} // namespace magi

// =============================================================================

struct EvaMagiInterior : sigil::compose::sketch::Sketch {
  std::vector<magi::Panel> panels = magi::panels();
  std::vector<magi::Arrivals> arrivals; // per panel, sorted by arrival
  SkPoint centre{613, 602};
  weave::FontContext *fonts = nullptr;

  // measured off the FLAT plate, inside the measured polygons
  static constexpr double kRefT = 2.5;
  static constexpr float kWant[3] = {0.000f, 0.302f, 0.952f};

  // --- animation: everything is a bound Output; nothing re-describes ------
  ch::Output<float> front0{0}, front1{0}, front2{0};
  ch::Output<float> kanjiHot{0};
  ch::Output<float> goldOn{1};
  ch::Output<float> creep{0};
  ch::Output<float> flicker{0};
  ch::Output<float> spinRotor{0};
  ch::Output<float> rotorGlow{1};
  double clock = 0;

  std::array<bool, 3> taken{false, false, false};
  // A panel Ireul has not reached yet carries NO infection node at all:
  // a shader whose front is below every cell still runs over every pixel
  // to prove it, and CASPER's 115k px were 100% waste for 12 s of a 26 s
  // loop. This is the one place the DATA path beats a uniform.
  std::array<bool, 3> seeded{false, false, false};
  int verdictStep = -1;
  int countdown = -1;

  brushes::PatternBrush beadBrush;
  brushes::PatternBrush chevronBrush;
  bool auditOk = true;

  // ==========================================================================
  // THE AUDIT — the generating rule, asserted and printed on every load.
  // In the flat plate the panels ARE axis-aligned rectangles, so the rule can
  // be tested where it lives: intersect the two edges each cut interrupts and
  // compare that virtual corner's bearing from the panel's centroid with the
  // bearing to the plate's centre.
  void audit() {
    SkPoint c{0, 0};
    for (const auto &p : panels) {
      c.fX += p.box.centerX();
      c.fY += p.box.centerY();
    }
    centre = {c.fX / (float)panels.size(), c.fY / (float)panels.size()};
    auditOk = true;
    std::printf("MAGI INTERIOR — the chamfer rule, in the RECTIFIED plate\n");
    std::printf("  plate centre (centroid of the three) = (%.0f, %.0f)\n",
                centre.fX, centre.fY);
    for (const auto &p : panels) {
      const SkPoint g{p.box.centerX(), p.box.centerY()};
      SkPoint corner;
      const char *what = "cut";
      if (p.cutMask == (magi::CutBL | magi::CutBR)) {
        corner = {p.box.centerX(), p.box.bottom()}; // the presented flat
        what = "flat";
      } else if (p.cutMask == magi::CutTR) {
        corner = {p.box.right(), p.box.top()};
      } else if (p.cutMask == magi::CutTL) {
        corner = {p.box.left(), p.box.top()};
      } else if (p.cutMask == magi::CutBR) {
        corner = {p.box.right(), p.box.bottom()};
      } else {
        corner = {p.box.left(), p.box.bottom()};
      }
      const float bearing =
          std::atan2(corner.fY - g.fY, corner.fX - g.fX) * 57.29578f;
      const float toC =
          std::atan2(centre.fY - g.fY, centre.fX - g.fX) * 57.29578f;
      float diff = std::fmod(std::fabs(bearing - toC), 360.0f);
      if (diff > 180.0f)
        diff = 360.0f - diff;
      const bool ok = diff <= 25.0f;
      auditOk = auditOk && ok;
      const float cutDeg =
          std::atan2(p.cut.fY, p.cut.fX) * 57.29578f;
      std::printf("  %-10s %s bearing %+7.1f  centre %+7.1f  |d| %5.1f   "
                  "cut %3.0fx%3.0f = %4.1f deg   %s\n",
                  p.key, what, bearing, toC, diff, p.cut.fX, p.cut.fY, cutDeg,
                  ok ? "OK" : "FAIL");
    }
    if (!auditOk)
      std::printf("  *** CHAMFER RULE VIOLATED — this plate is not one rule\n");
  }

  // ==========================================================================
  // TYPE, SOLVED. metrics() gives capHeight without laying anything out and
  // capHeight is linear in size; capSlack() turns "the reference's ink starts
  // at (222, 692)" into a coordinate. The WIDTH is solved too, because Matisse
  // EB and Songti SC Black do not share a set width and NERV panels are set in
  // Helvetica CONDENSED — the frame is the authority on how much.

  weave::TextStyle fitCap(const sk_sp<SkTypeface> &tf, float cap, SkColor4f c,
                          float *slack = nullptr) const {
    float size = cap * 1.4f;
    if (fonts) {
      const TextMetrics m = metrics(magi::type(tf, 100.0f, c), *fonts);
      if (m.capHeight > 1.0f)
        size = 100.0f * cap / m.capHeight;
    }
    weave::TextStyle st = magi::type(tf, size, c);
    if (slack)
      *slack = fonts ? metrics(st, *fonts).capSlack() : size * 0.22f;
    return st;
  }

  weave::TextStyle fitRun(const sk_sp<SkTypeface> &tf, const std::u8string &s,
                          float cap, float inkW, SkColor4f c,
                          float *slack = nullptr) const {
    weave::TextStyle st = fitCap(tf, cap, c, slack);
    if (fonts && inkW > 1.0f) {
      const SkSize m = sigil::compose::measure(text(s, st), *fonts);
      if (m.width() > 1.0f)
        st.shaping.scaleX = std::clamp(inkW / m.width(), 0.40f, 1.8f);
    }
    return st;
  }

  /** A CJK run is sized by its ADVANCE — a Ming's em IS its advance, and the
   *  flat plate gives it directly: 提/訴 step 148 px, 決/議 130. */
  weave::TextStyle fitEmSpan(const std::u8string &s, float advanceSpan,
                             SkColor4f c, float *slack = nullptr) const {
    // The plate's Han is drawn WIDER than it is tall — the same horizontal
    // stretch itorr's recreation encodes as `transform: scale(1.2, 1)` — so
    // the stretch goes on BEFORE the measurement, or the solve fights it.
    constexpr float kStretch = 1.12f;
    float em = advanceSpan * 0.5f;
    if (fonts) {
      weave::TextStyle probe = magi::type(magi::han(), 100.0f, c);
      probe.shaping.scaleX = kStretch;
      const SkSize m = sigil::compose::measure(text(s, probe), *fonts);
      if (m.width() > 1.0f)
        em = 100.0f * advanceSpan / m.width();
    }
    weave::TextStyle st = magi::type(magi::han(), em, c);
    st.shaping.scaleX = kStretch;
    if (slack)
      *slack = fonts ? metrics(st, *fonts).capSlack() : em * 0.10f;
    return st;
  }

  /** A run of type placed by its measured INK top-left. Nothing on this plate
   *  is rotated: the rectification says every baseline is horizontal to within
   *  1.3 deg, and the "varying roll" was the projection. */
  Element inked(std::u8string s, const weave::TextStyle &st, SkPoint ink,
                float slack) {
    return box()
        .absolute()
        .left(ink.fX)
        .top(ink.fY - slack)
        .child(text(std::move(s), st));
  }

  // ==========================================================================
  // THE PLATE

  Element panelNode(int i) {
    const magi::Panel &p = panels[(size_t)i];
    const SkSize sz{p.box.width(), p.box.height()};
    const bool red = taken[(size_t)i];
    const SkPath circuit = magi::ownCircuitry(sz, 17 + i * 13, p.circuitRuns);
    const SkPath pads = magi::ownPads(sz, 41 + i * 7, p.circuitRuns ? 6 : 0);
    const ch::Output<float> *fr =
        i == 0 ? &front0 : (i == 1 ? &front1 : &front2);

    Material infection = Material::sksl(magi::infectionEffect())
                             .uniform("uSeed", std::array<float, 2>{p.seed.fX, p.seed.fY})
                             .uniform("uCells",
                                      std::array<float, 2>{
                                          std::ceil(sz.width() / magi::kCell),
                                          std::ceil(sz.height() / magi::kCell)})
                             .uniform("uPour", magi::kRed)
                             .uniform("uKey", magi::kTraceDark)
                             .uniform("uFront", fr);

    Element node = box()
                       .absolute()
                       .left(p.box.left())
                       .top(p.box.top())
                       .width(sz.width())
                       .height(sz.height())
                       .key(p.key)
                       .outline(magi::cutBox(p.cut.fX, p.cut.fY, p.cutMask))
                       .fill(Material::solid(red ? magi::kRed : magi::kAzure))
                       .clip(true)
                       // the cel's own edge light — measured, not itorr's rule
                       .foreground(decorations::border(
                           2.0f, Fill::color(magi::kEdgeLight), 1.0f));

    if (!red && p.circuitRuns > 0) {
      // UNEQUAL offsets, unequal widths: a heavy run with a hairline beside it
      // at a different clearance, which is the thing lines::Line's symmetric
      // `parallels` cannot say at all.
      node.child(box()
                     .absolute()
                     .inset(0)
                     .fill(Fill::none())
                     .outline([circuit](SkSize) { return circuit; })
                     .stroke(lines::Rails{
                         .rails = {{.offset = -6.0f,
                                    .width = 3.0f,
                                    .fill = Fill::color(magi::kInk),
                                    .cap = SkPaint::kSquare_Cap,
                                    .join = SkPaint::kMiter_Join},
                                   {.offset = 3.0f,
                                    .width = 1.8f,
                                    .fill = Fill::color(magi::kInk),
                                    .cap = SkPaint::kSquare_Cap,
                                    .join = SkPaint::kMiter_Join}},
                         .offsetStep = 6.0f}));
      node.child(box()
                     .absolute()
                     .inset(0)
                     .fill(Fill::none())
                     .outline([pads](SkSize) { return pads; })
                     .stroke(PathFormat{.width = 2.0f,
                                        .strokeFill = Fill::color(magi::kInk),
                                        .join = SkPaint::kMiter_Join}));
    }
    if (!red && seeded[(size_t)i])
      node.child(box().absolute().inset(0).fill(infection));
    return node;
  }

  /** A green band: a horizontal bar, dark sea-green, SERRATED by a crosshatch
   *  (correction 3 — ~19 px thick, four independent rules, not a Rails pair,
   *  and there is no lean because the plate is level). */
  Element greenBand(float x0, float x1, float y) {
    return box()
        .absolute()
        .left(x0)
        .top(y - 9.0f)
        .width(x1 - x0)
        .height(19.0f)
        .fill(Material::solid(magi::kGreen))
        .overlay(lines::crosshatch(Fill::color(magi::kGreenHi), 8.0f, 1.6f,
                                   45.0f));
  }

  Element plateFurniture() {
    Element g = box().absolute().inset(0);
    // The plate's own dim diagonal furniture, still diagonal AFTER
    // rectification — so it is drawn that way and not a residue of the camera.
    for (int k = 0; k < 3; ++k)
      g.child(box()
                  .absolute()
                  .left(140.0f)
                  .top(898.0f + (float)k * 26.0f)
                  .width(600.0f)
                  .height(4.0f)
                  .rotate(-2.6f)
                  .transformOrigin(0.0f, 0.5f)
                  .fill(Material::solid(magi::kBgRule)));
    for (int k = 0; k < 2; ++k)
      g.child(box()
                  .absolute()
                  .left(520.0f)
                  .top(126.0f + (float)k * 34.0f)
                  .width(580.0f)
                  .height(4.0f)
                  .rotate(-3.2f)
                  .transformOrigin(0.0f, 0.5f)
                  .fill(Material::solid(magi::kBgRule)));
    for (int k = 0; k < 2; ++k)
      g.child(box()
                  .absolute()
                  .left(100.0f + (float)k * 22.0f)
                  .top(250.0f)
                  .width(4.0f)
                  .height(400.0f)
                  .fill(Material::solid(magi::kBgRule)));
    // the four green rules — measured horizontal to within 1.3 deg
    g.child(greenBand(128, 455, 209));   // over 提訴
    g.child(greenBand(128, 455, 332));   // under it
    g.child(greenBand(785, 1080, 209));  // over 決議
    g.child(greenBand(785, 1080, 339));  // under it
    return g;
  }

  Element plateType() {
    Element g = box().absolute().inset(0);
    float sCode = 0, sFile = 0, sMagi = 0, sK1 = 0, sK2 = 0;
    // CODE : 263 — ink x 148..400, cap 40.
    g.child(inked(u8"CODE : 263",
                  fitRun(magi::latin(), u8"CODE : 263", 47.0f, 249.0f,
                         magi::kOrange, &sCode),
                  {151, 343}, sCode));
    // The FILE block — cap 18, LEADING 24 (correction 5).
    const auto file = fitRun(magi::latin(), u8"EXTENTION:2004", 18.0f, 257.0f,
                             magi::kOrange, &sFile);
    static const char *kBlock[5] = {"FILE:MAGI_SYS", "EXTENTION:2004",
                                    "EX_MODE:OFF", "PRIORITY:AAA", nullptr};
    // Verbatim, EXTENTION and all — the plate's own error, reproduced.
    for (int i = 0; kBlock[i]; ++i)
      g.child(inked(toU8(kBlock[i]), file,
                    {196.0f, 406.0f + (float)i * 30.5f}, sFile));

    // 提訴 — a proposal is FILED. Two runs stacked: an opaque core and a hot
    // copy whose bound opacity steps 0/1, so the pulse costs one saveLayer the
    // size of the KANJI, not of the canvas (API.md: 18.38 -> 7.94 ms).
    const auto k1 = fitEmSpan(u8"提訴", 266.0f, magi::kKanji, &sK1);
    const auto k1h = fitEmSpan(u8"提訴", 266.0f, magi::kKanjiHot);
    g.child(inked(u8"提訴", k1, {152, 234}, sK1));
    g.child(box()
                .absolute()
                .left(152)
                .top(234.0f - sK1)
                .opacity(&kanjiHot)
                .child(text(u8"提訴", k1h)));
    // 決議 — RESOLUTION.
    const auto k2 = fitEmSpan(u8"決議", 244.0f, magi::kKanji, &sK2);
    g.child(inked(u8"決議", k2, {810, 246}, sK2));

    // MAGI, in the hole the three panels leave — cap 37, ink span 118. The
    // flat plate's own runs are M 559-592, A 595-622, G 627-655, I 664-673:
    // ink 559..673 over rows 648..683. Set 40/138 it ran to 696 and the I was
    // cut by MELCHIOR's chamfer, which reaches x 684 at that baseline.
    g.child(inked(u8"MAGI",
                  fitRun(magi::latin(), u8"MAGI", 37.0f, 118.0f, magi::kOrange,
                         &sMagi),
                  {558, 646}, sMagi));
    return g;
  }

  /** 審議中 in its gold box: a REAL double border — gold outer rule, dark gap,
   *  thin inner rule. Measured 200 x 90; itorr builds the same object as a
   *  triple box-shadow at .03em / .07em / .1em. */
  Element verdictBox() {
    float sl = 0;
    const auto st = fitEmSpan(u8"審議中", 156.0f, magi::kGoldPeak, &sl);
    return box()
        .absolute()
        .left(868)
        .top(424)
        .width(174)
        .height(94)
        .opacity(&goldOn)
        .fill(Material::solid(magi::hex(0x140A02)))
        .style(decorations::doubleBorder(
            decorations::border(5.0f, Fill::color(magi::kGold), 0.0f),
            decorations::border(2.0f, Fill::color(magi::kGoldHot), 10.0f)))
        .child(text(u8"審議中", st).absolute().left(22).top(20.0f - sl + 14.0f));
  }

  /** The verdict card — 否決 x4, then 可決, then the struck-through 否決 of
   *  "we are 1 sec. ahead". It appears only AFTER the reference moment, so the
   *  capture at 2.5 s still diffs against the anchor. */
  Element verdictCard() {
    if (verdictStep < 0)
      return box().absolute().width(0).height(0);
    const bool carried = verdictStep == 4;
    const SkColor4f ink = carried ? magi::kGoldPeak : magi::kRedHot;
    float sl = 0;
    const auto st = fitEmSpan(carried ? u8"可決" : u8"否決", 150.0f, ink, &sl);
    // 620 put the card straight through portrait labels 1-3 — it is only
    // ever up after 6.5 s, and the anchor capture is at 2.5. 430 is the gap
    // the margin actually leaves, between the countdown and the first rail.
    Element card =
        box()
            .absolute()
            .left(1130)
            .top(430)
            .width(220)
            .height(120)
            .outline(shapes::chamfered(22.0f, shapes::Corner::Diagonal))
            .fill(Material::solid(magi::hex(0x0A0102)))
            .foreground(decorations::border(4.0f, Fill::color(ink), 3.0f))
            .child(text(carried ? u8"可決" : u8"否決", st)
                       .absolute()
                       .left(28)
                       .top(24.0f - sl + 16.0f));
    if (verdictStep == 5)
      card.child(box()
                     .absolute()
                     .left(18)
                     .top(58)
                     .width(184)
                     .height(7)
                     .fill(Material::solid(magi::kOrange)));
    return card;
  }

  /** The countdown. Helvetica Bold, bleeding off the frame — 20, 15, 10, then
   *  one a second from 9, and a 2 s stay at zero because "self-destruction
   *  will be executed 02 sec. after all three agree". */
  Element countdownNumeral() {
    if (countdown < 0)
      return box().absolute().width(0).height(0);
    char buf[8];
    std::snprintf(buf, sizeof buf, "%d", countdown);
    return box()
        .absolute()
        .left(1096)
        .top(96)
        .child(text(toU8(buf),
                    magi::type(magi::latin(), 260.0f, magi::kRedHot, 1.2f)));
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
                SkColor4f fill, const char *l1, const char *l2,
                float textSize) {
    Element h = box()
                    .absolute()
                    .left(p.fX - across * 0.5f)
                    .top(p.fY - across * 0.5f)
                    .width(across)
                    .height(across)
                    .outline(shapes::polygon(6, 0.0f))
                    .fill(Material::solid(fill))
                    // Border::cornerAngleDeg defaults to 30 and finds ZERO
                    // corners above 12 sides. Passed explicitly everywhere.
                    .foreground(Border{.width = rimW,
                                       .fill = Fill::color(rim),
                                       .cornerAngleDeg = 20.0f});
    if (l1)
      h.child(text(toU8(l1),
                   magi::type(magi::latinPlain(), textSize, magi::kPRailHi))
                  .absolute()
                  .left(across * 0.30f)
                  .top(across * 0.34f));
    if (l2)
      h.child(text(toU8(l2),
                   magi::type(magi::latinPlain(), textSize, magi::kPRailDim))
                  .absolute()
                  .left(across * 0.30f)
                  .top(across * 0.34f + textSize * 1.15f));
    return h;
  }

  void buildBrushes() {
    // The bake cache lives IN THE BRUSH VALUE, so these are members built
    // once: a PatternBrush constructed inside describe() re-bakes every tile
    // through snapshot() every frame.
    beadBrush = brushes::PatternBrush{
        .side = box()
                    .width(10.0f)
                    .height(10.0f)
                    .outline(shapes::circle())
                    .fill(Fill::none())
                    .foreground(decorations::border(
                        1.8f, Fill::color(magi::kPRailHi))),
        .advance = 14.0f,
        .cornerAngleDeg = 34.0f,
        .cornerLength = 0.0f,
        .cornerAlign = brushes::PatternBrush::CornerAlign::Bisector,
        .stretchToFit = true,
        .reach = 12.0f};
    chevronBrush = brushes::PatternBrush{
        .side = box()
                    .width(9.0f)
                    .height(8.0f)
                    .outline(shapes::arrow(0.10f, 0.90f))
                    .fill(Material::solid(magi::kPBodyHi)),
        .advance = 10.0f,
        .cornerAngleDeg = 34.0f,
        .cornerLength = 0.0f,
        .cornerAlign = brushes::PatternBrush::CornerAlign::Outgoing,
        .stretchToFit = true,
        .reach = 11.0f};
  }

  Element portraitStatic() {
    Element g = box().absolute().inset(0);
    const float S = kPScale;

    // two faint boundary conics — shapes::parametric, real curves
    for (float r : {780.0f * S, 860.0f * S})
      g.child(box()
                  .absolute()
                  .left(kPCX - r)
                  .top(kPCY - r * 0.985f)
                  .width(r * 2)
                  .height(r * 1.97f)
                  .outline(shapes::parametric(
                      [](float t) {
                        return SkPoint{0.5f + 0.5f * std::cos(t),
                                       0.5f + 0.5f * std::sin(t)};
                      },
                      0.0f, 6.2831853f, 240, true))
                  .fill(Fill::none())
                  .foreground(decorations::border(
                      2.0f, Fill::color(magi::dim(magi::hex(0x5A1A0C),
                                                  magi::kBack)))));

    // 12 neuron somas at r 690..790, each trailing dendrites BACK toward the
    // centre — brushes::Ribbon, tapered.
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
                  .absolute()
                  .inset(0)
                  .fill(Fill::none())
                  .outline([dend](SkSize) { return dend; })
                  .stroke(brushes::Ribbon{
                      .fill = Fill::color(
                          magi::dim(magi::hex(0x8A2412), magi::kBack)),
                      .widthStart = 8.0f,
                      .widthEnd = 1.0f,
                      .step = 6.0f}));
      g.child(box()
                  .absolute()
                  .left(kPCX + p.fX - d * 0.5f)
                  .top(kPCY + p.fY - d * 0.5f)
                  .width(d)
                  .height(d)
                  .outline(shapes::circle())
                  .fill(Material::radialUnit(
                      {0.5f, 0.5f}, 1.0f,
                      {{0.0f, magi::kPBodyHi},
                       {0.55f, magi::kPBody},
                       {1.0f, magi::dim(magi::hex(0x3A0E06), magi::kBack)}}))
                  .foreground(lines::concentric(
                      Fill::color(magi::dim(magi::hex(0xC03C18), magi::kBack)),
                      4, 1.2f)));
    }

    // 24 small hexagons at r 555..665, two lines of tiny text each
    for (int k = 0; k < 24; ++k) {
      const float a = (float)k * 15.0f - 90.0f;
      const float r = ((k % 2) ? 610.0f : 560.0f) * S;
      const SkPoint p = polar(r, a);
      g.child(hexAt({kPCX + p.fX, kPCY + p.fY}, 48.0f * S, magi::kPRailHi, 1.4f,
                    magi::dim(magi::hex(0x2A0C05), magi::kBack), "TYPE", "M-04",
                    4.6f));
    }

    // the heavy arc the 12 big hexagons sit on, dressed with the bead and
    // chevron runs — PatternBrush on an arc, alternating tiles.
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
      Element run = box()
                        .absolute()
                        .inset(0)
                        .fill(Fill::none())
                        .outline([arcp](SkSize) { return arcp; });
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
                  .absolute()
                  .inset(0)
                  .fill(Fill::none())
                  .outline([arcp](SkSize) { return arcp; })
                  .stroke(lines::Rails{
                      .rails = {{.offset = -6.0f,
                                 .width = 4.0f,
                                 .fill = Fill::color(magi::kPRailHi)},
                                {.offset = 0.0f,
                                 .width = 1.0f,
                                 .fill = Fill::color(magi::kPPin),
                                 .dash = {3.0f, 11.0f}},
                                {.offset = 6.0f,
                                 .width = 1.8f,
                                 .fill = Fill::color(magi::kPRail)}},
                      .offsetStep = 3.0f}));
    }

    for (int k = 0; k < 12; ++k) {
      const float a = (float)k * 30.0f - 90.0f;
      const SkPoint p = polar(490.0f * S, a);
      g.child(hexAt({kPCX + p.fX, kPCY + p.fY}, 74.0f * S, magi::kPRailHi, 2.2f,
                    magi::dim(magi::hex(0x351107), magi::kBack), "APS", "17",
                    5.4f));
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
                  .absolute()
                  .inset(0)
                  .fill(Fill::none())
                  .outline([fan](SkSize) { return fan; })
                  .stroke(PathFormat{
                      .width = 0.8f,
                      .strokeFill = Fill::color(
                          magi::dim(magi::hex(0xD08A9A), magi::kBack))}));
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
                  .absolute()
                  .inset(0)
                  .fill(Fill::none())
                  .outline([comb](SkSize) { return comb; })
                  .stroke(lines::Line{.width = 1.8f,
                                      .fill = Fill::color(magi::kPPin)}));
      SkPathBuilder rb;
      const SkPoint r0 = polar(320.0f * S, a + 12.0f);
      const SkPoint r1 = polar(470.0f * S, a + 12.0f);
      rb.moveTo(kPCX + r0.fX, kPCY + r0.fY);
      rb.lineTo(kPCX + r1.fX, kPCY + r1.fY);
      const SkPath ladder = rb.detach();
      g.child(box()
                  .absolute()
                  .inset(0)
                  .fill(Fill::none())
                  .outline([ladder](SkSize) { return ladder; })
                  .stroke(lines::Line{.width = 0.8f,
                                      .fill = Fill::color(magi::kPChart),
                                      .midCap = lines::Cap::Arrow,
                                      .midSpacing = 24.0f}));
    }

    // the 6-fold core: seven flat-top hexagons in a honeycomb
    const float hexA = 88.0f * S;
    g.child(hexAt({kPCX, kPCY}, hexA, magi::kPRailHi, 2.0f,
                  magi::dim(magi::hex(0x4A140A), magi::kBack), "MAGI", "SYS",
                  5.6f));
    for (int k = 0; k < 6; ++k) {
      const float a = (float)k * 60.0f - 90.0f;
      const SkPoint p = polar(hexA * 0.90f, a);
      g.child(hexAt({kPCX + p.fX, kPCY + p.fY}, hexA, magi::kPRailHi, 1.8f,
                    magi::dim(magi::hex(0x3E1108), magi::kBack), "TYPE", "0417",
                    5.2f));
    }
    return g;
  }

  /** The 16-fold rotor: radial capsules, long axis pointing at the centre,
   *  alternating magenta 37x21 and violet 29x16, glowing. Its own node so it
   *  can turn against the rest. */
  /** Sized to the ROTOR, not to the canvas: this node lives under a bound
   *  rotation, so its Texture bake is a LOCAL bake blitted through the
   *  transform — and that blit is an area cost. A full-canvas wrapper made it
   *  a 1.5 MP resample every frame (6.44 ms); its own 520 px box is 0.27 MP
   *  (1.1 ms) for identical pixels. */
  static constexpr float kRotorR = 260.0f;
  Element portraitRotor() {
    Element g = box()
                    .absolute()
                    .left(kPCX - kRotorR)
                    .top(kPCY - kRotorR)
                    .width(kRotorR * 2)
                    .height(kRotorR * 2);
    const float S = kPScale;
    for (int spoke = 0; spoke < 16; ++spoke) {
      const float a = (float)spoke * 22.5f - 90.0f;
      for (int j = 0; j < 5; ++j) {
        const float r = (215.0f + (float)j * 42.0f) * S;
        const bool mag = ((spoke + j) & 1) != 0;
        const float lw = (mag ? 37.0f : 29.0f) * S;
        const float lh = (mag ? 21.0f : 16.0f) * S;
        const SkPoint p = polar(r, a);
        g.child(box()
                    .absolute()
                    .left(kRotorR + p.fX - lw * 0.5f)
                    .top(kRotorR + p.fY - lh * 0.5f)
                    .width(lw)
                    .height(lh)
                    .rotate(a)
                    .corners(Corners{lh * 0.5f})
                    .fill(Material::linearUnit(
                        {0, 0}, {1, 0},
                        {{0.0f, mag ? magi::kPMagenta : magi::kPViolet},
                         {0.5f, magi::dim(mag ? magi::hex(0xC464A5)
                                              : magi::hex(0x643D93),
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
    static const char *kRuns[8][2] = {
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
    // here; its tail ("at the time of partition un-developing. )") was tried
    // and taken back out — a third line closes to 15 px of run 3, the same
    // leading the label uses internally, and the two captions read as one.
    static const float kDy[8] = {-33, 11, 58, 122, 166, 224, 301, 349};
    Element g = box().absolute().inset(0);
    const auto hi = magi::type(magi::latinPlain(), 11.5f, magi::kPRailHi);
    const auto dim = magi::type(magi::latinPlain(), 11.5f, magi::kPRailDim);
    for (int i = 0; i < 8; ++i) {
      const float y = kPCY + kDy[i];
      g.child(box()
                  .absolute()
                  .left(1078.0f)
                  .top(y)
                  .width(74.0f)
                  .height(1.1f)
                  .fill(Material::solid(magi::kPRail)));
      g.child(box()
                  .absolute()
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
                .absolute()
                .left(20.0f)
                .top(18.0f)
                .column()
                .gap(2.0f)
                .child(text(u8"MAGI SYSTEM V2.3762.123b",
                            magi::type(magi::latin(), 12.5f, magi::kPRailHi)))
                .child(text(u8"Conceptual Diagram",
                            magi::type(magi::latinPlain(), 11.5f,
                                       magi::kPRailDim))));
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
                    .absolute()
                    .left(148)
                    .top(872)
                    .width(Wd)
                    .height(Ht)
                    .outline(shapes::chamfered(22.0f, shapes::Corner::All))
                    .fill(Material::solid(magi::hex(0x322A36)))
                    .clip(true)
                    .foreground(Border{.width = 13.0f,
                                       .fill = Fill::color(magi::hex(0x090509)),
                                       .inset = 6.5f,
                                       .cornerAngleDeg = 20.0f});
    for (int k = 0; k < 4; ++k) {
      const float bx = (k & 1) ? Wd - 25.0f : 25.0f;
      const float by = (k & 2) ? Ht - 23.0f : 23.0f;
      g.child(disc({bx, by}, 8.5f).fill(Material::solid(magi::hex(0x120A12))));
      g.child(disc({bx - 2.0f, by - 2.0f}, 5.0f)
                  .fill(Fill::none())
                  .foreground(decorations::border(
                      1.8f, Fill::color(magi::hex(0x6E5E70)))));
    }
    Element tissue =
        box()
            .absolute()
            .left(Wd * 0.30f)
            .top(Ht * 0.26f)
            .width(Wd * 0.44f)
            .height(Ht * 0.52f)
            .rotate(-9.0f)
            .fill(Material::radialUnit({0.44f, 0.40f}, 1.05f,
                                       {{0.0f, magi::hex(0xDBC49A)},
                                        {0.62f, magi::hex(0xC0A277)},
                                        {1.0f, magi::hex(0x97785D)}}))
            .overlay(lines::Line{.width = 2.0f,
                                 .fill = Fill::color(magi::hex(0x4A2E1E)),
                                 .waveAmplitude = 3.2f,
                                 .waveLength = 16.0f})
            .foreground(
                decorations::border(1.8f, Fill::color(magi::hex(0x1A0F14))));
    // THE SULCI. `overlay()` dresses a node's OUTLINE, and this node's
    // outline is its rectangle — so the wavy Line above deckles the tissue's
    // EDGE and lays nothing across it, which left the one thing the hatch
    // exists to show (a human brain behind glass) reading as a blank card.
    // The folds need geometry of their own; the ink and the wave are the
    // ones already chosen above.
    tissue.child(
        box()
            .absolute()
            .inset(0)
            .outline([](SkSize s) {
              const float w = s.width(), h = s.height();
              SkPathBuilder b;
              // the longitudinal fissure
              b.moveTo(w * 0.54f, h * 0.03f);
              b.quadTo(w * 0.39f, h * 0.30f, w * 0.55f, h * 0.53f);
              b.quadTo(w * 0.71f, h * 0.77f, w * 0.49f, h * 0.97f);
              // gyri, each stopping short of the fissure and of the rim
              const float ys[3] = {0.24f, 0.52f, 0.79f};
              for (float y : ys) {
                b.moveTo(w * 0.06f, h * y);
                b.quadTo(w * 0.24f, h * (y - 0.10f), w * 0.42f, h * y);
                b.moveTo(w * 0.62f, h * (y + 0.05f));
                b.quadTo(w * 0.80f, h * (y - 0.04f), w * 0.94f, h * (y + 0.07f));
              }
              return b.detach();
            })
            .stroke(lines::Line{.width = 1.6f,
                                .fill = Fill::color(magi::hex(0x4A2E1E)),
                                .waveAmplitude = 1.5f,
                                .waveLength = 10.0f}));
    g.child(std::move(tissue));
    g.child(box()
                .absolute()
                .left(Wd * 0.31f)
                .top(Ht * 0.63f)
                .child(text(u8"MAGI", magi::type(magi::latin(), 28.0f,
                                                 magi::hex(0x8C2A1E), 0.86f))));
    for (int k = 0; k < 2; ++k)
      g.child(box()
                  .absolute()
                  .left(Wd * 0.24f)
                  .top(Ht * (k ? 0.72f : 0.24f))
                  .width(Wd * 0.58f)
                  .height(5.0f)
                  .rotate(k ? -34.0f : 34.0f)
                  .transformOrigin(0.0f, 0.5f)
                  .fill(Material::solid(magi::hex(0x090509))));
    g.child(box()
                .absolute()
                .inset(0)
                .fill(Fill::none())
                .child(text(u8"CASPER",
                            magi::type(magi::latin(), 32.0f,
                                       magi::hex(0x0B060B), 0.88f, 3.0f))
                           .absolute()
                           .inset(0)
                           .onPath(TextPath{
                               .path = [stencilArc](SkSize) { return stencilArc; },
                               .at = 0.5f,
                               .align = TextPath::Align::Center,
                               .orient = TextPath::Orient::Tangent})));
    return g;
  }

  // ==========================================================================

  /** The HUD slot: everything that changes on a CLOCK rather than on the
   *  front. Re-describing the whole tree once a second for a countdown
   *  numeral is how a 4 ms frame becomes a 19 ms one — the panels carry live
   *  materials, and a re-describe puts their bakes at risk. */
  Element hud() {
    return box().absolute().inset(0).child(verdictCard()).child(
        countdownNumeral());
  }

  Element describe() {
    Element root = box().absolute().inset(0);

    // --- the portrait, behind everything, bleeding off all four edges ------
    root.child(portraitStatic().cache(Cache::Texture).key("portrait"));
    root.child(box()
                   .absolute()
                   .left(kPCX - kRotorR)
                   .top(kPCY - kRotorR)
                   .width(kRotorR * 2)
                   .height(kRotorR * 2)
                   .rotate(&spinRotor)
                   .child(portraitRotor()
                              .absolute()
                              .left(0)
                              .top(0)
                              .cache(Cache::Texture)
                              .key("rotor")));
    root.child(portraitLabels().cache(Cache::Texture).key("plabels"));

    // --- the plate ----------------------------------------------------------
    root.child(plateFurniture().cache(Cache::Texture).key("furniture"));
    for (int i = 0; i < 3; ++i)
      root.child(panelNode(i));
    // The labels ride OVER the infection: the frame shows BALTHASAR·2 knocked
    // straight through the pour, still in its own navy.
    for (int i = 0; i < 3; ++i) {
      const magi::Panel &p = panels[(size_t)i];
      float slack = 0;
      const auto st =
          fitRun(magi::latin(), toU8(p.label), p.labelCap, p.labelInkW,
                 taken[(size_t)i] ? magi::kInkRed : magi::kInk, &slack);
      root.child(inked(toU8(p.label), st, p.labelInk, slack));
    }

    // --- the three orange rails, as rail() on the panels' own keys ---------
    // Anchors are NORMALIZED points on the panels' resolved bounds, so the
    // stubs are a relationship and not three hand-placed rectangles.
    struct RailSpec {
      const char *a;
      SkPoint na;
      const char *b;
      SkPoint nb;
    };
    static const RailSpec kRails[3] = {
        {"casper", {0.826f, 0.129f}, "balthasar", {0.197f, 1.048f}},
        {"balthasar", {0.791f, 0.971f}, "melchior", {0.190f, 0.275f}},
        {"casper", {1.000f, 0.599f}, "melchior", {0.011f, 0.652f}},
    };
    for (int i = 0; i < 3; ++i)
      root.child(rail({{kRails[i].a, kRails[i].na}, {kRails[i].b, kRails[i].nb}},
                      routers::polyline(0.0f))
                     .absolute()
                     .inset(0)
                     .stroke(lines::Rails{
                         .rails = {{.offset = 0.0f,
                                    .width = 13.0f,
                                    .fill = Fill::color(magi::kOrange),
                                    .cap = SkPaint::kButt_Cap},
                                   {.offset = -7.5f,
                                    .width = 2.0f,
                                    .fill = Fill::color(magi::kOrangeDim),
                                    .cap = SkPaint::kButt_Cap}}}));

    // --- the plate's type, gold box, verdict card, countdown ---------------
    root.child(plateType().cache(Cache::Texture).key("ptype"));
    root.child(verdictBox());
    root.child(slot("hud"));
    root.child(hatchPlate().cache(Cache::Texture).key("hatch"));

    // --- the tube ----------------------------------------------------------
    root.child(box()
                   .absolute()
                   .left(0)
                   .top(-8)
                   .width(magi::kW)
                   .height(magi::kH + 16)
                   .fill(Material::sksl(magi::crtEffect()))
                   .translateY(&creep)
                   .cache(Cache::Texture)
                   .key("crt"));
    root.child(box()
                   .absolute()
                   .inset(0)
                   .fill(Fill::color({0, 0, 0, 1}))
                   .opacity(&flicker)
                   .key("flicker"));

    if (!auditOk)
      root.child(box()
                     .absolute()
                     .left(0)
                     .top(420)
                     .width(magi::kW)
                     .height(96)
                     .fill(Fill::color({1, 0, 1, 0.94f}))
                     .child(text(u8"CHAMFER RULE VIOLATED",
                                 magi::type(magi::latin(), 56.0f, {0, 0, 0, 1}))
                                .absolute()
                                .left(30)
                                .top(20)));
    return root;
  }

  // ==========================================================================
  // Coverage -> front, and the schedule. NOTHING here is tuned: the shader's
  // arrival field is mirrored on the CPU, sorted, and the quantile IS the
  // front, so asking for "30.2% of BALTHASAR at t = 2.5" is an array index.

  float frontAt(int i, double t) const {
    // per-panel schedule, in seconds: (seed, full)
    static const double kSeed[3] = {12.0, 0.34, -0.5};
    static const double kFull[3] = {22.0, 6.0, 2.0};
    const double s = kSeed[i], f = kFull[i];
    double k = (t - s) / (f - s);
    k = std::clamp(k, 0.0, 1.0);
    // ease so the front decelerates as the panel fills, which is what the
    // episode's dialogue describes ("Balthazar is now taken over" lands late)
    double frac = k * k * (3.0 - 2.0 * k);
    // QUANTIZED to kSteps levels — and this is a perf decision as much as an
    // aesthetic one. A continuously bound uniform makes the material live
    // FOREVER, so its 136k px re-run every frame (11.16 ms measured). Held
    // still between steps, the automatic promoter bakes it and the frame is a
    // blit; the shader is paid ~5 times a second instead of 60. The step is
    // also invisible, because the field is already a per-cell step function.
    // PER PANEL, and staggered. Each step costs one re-bake of that panel, so
    // the step RATE is the cost: MELCHIOR falls in 2.1 s and 40 steps there is
    // 19 re-bakes a second (p99 24.16 ms, measured, at t = 1.2 when it and
    // BALTHASAR are both filling). Sixteen steps over its fall is 7/s and
    // indistinguishable, and the 0.37-of-a-step offset keeps two panels from
    // stepping on the same frame.
    static const double kSteps[3] = {20.0, 32.0, 8.0};
    static const double kPhase[3] = {0.0, 0.0, 0.37};
    const double n = kSteps[i];
    frac = std::floor(frac * n + kPhase[i] + 1e-6) / n;
    frac = std::clamp(frac, 0.0, 1.0);
    return magi::frontFor(arrivals[(size_t)i], (float)frac);
  }

  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(magi::kW, magi::kH);
    ctx.background(magi::kGround);
    fonts = ctx.fonts;
    audit();
    buildBrushes();

    arrivals.clear();
    std::printf("MAGI INTERIOR — the front, SOLVED (cell %.0f px)\n",
                magi::kCell);
    for (int i = 0; i < 3; ++i) {
      seeded[(size_t)i] = false;
      const magi::Panel &pp = panels[(size_t)i];
      arrivals.push_back(magi::arrivalTable(
          pp.box, magi::kCell, pp.seed,
          magi::cutBox(pp.cut.fX, pp.cut.fY, pp.cutMask)(
              {pp.box.width(), pp.box.height()})));
      // report what the schedule actually lands on at the reference moment
      double k = 0;
      {
        static const double kSeed[3] = {12.0, 0.34, -0.5};
        static const double kFull[3] = {22.0, 6.0, 2.0};
        k = std::clamp((kRefT - kSeed[i]) / (kFull[i] - kSeed[i]), 0.0, 1.0);
        k = std::floor(k * k * (3.0 - 2.0 * k) * 32.0 + 1e-6) / 32.0;
      }
      const auto &tab = arrivals.back();
      float reach = 0;
      for (const auto &c : tab.cells)
        if (c.first < 1e3f)
          reach += c.second;
      std::printf("  %-10s cells %3d  area %6.1f  reachable %.0f%%  "
                  "coverage(2.5) = %5.1f%%  (measured %4.1f%%)\n",
                  panels[(size_t)i].key, (int)tab.cells.size(),
                  (double)tab.total, 100.0 * reach / (double)tab.total,
                  100.0 * k, 100.0 * kWant[i]);
    }

    ctx.ticker.add([this](double dt) {
      clock += dt;
      const double t = std::fmod(clock, 26.0);
      front0 = frontAt(0, t >= 24.0 ? -1.0 : t); // "we are 1 sec. ahead"
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
      spinRotor = (float)(3.0 * clock);
      rotorGlow = 0.72f + 0.28f * (float)std::sin(clock * 1.047);
      return true;
    });

    ctx.composer.render(describe());
    ctx.composer.renderSlot("hud", hud());
  }

  void update(double elapsed, sketch::SketchContext &ctx) override {
    // The DATA path, and the ONLY re-describe: a panel is taken (3x a loop),
    // the verdict card steps (6x), the countdown ticks (13x). The infection
    // itself never re-describes — it is one uniform.
    const double t = std::fmod(elapsed, 26.0);
    static const double kFull[3] = {22.0, 6.0, 2.0};
    static const double kSeedT[3] = {12.0, 0.34, -0.5};
    std::array<bool, 3> now{false, false, false};
    std::array<bool, 3> sow{false, false, false};
    for (int i = 0; i < 3; ++i) {
      now[(size_t)i] = t >= kFull[i] && t < 24.0;
      sow[(size_t)i] = t >= kSeedT[i] && t < 24.0;
    }
    if (t < 12.0)
      now[0] = false;

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
    if (!structural && !hudChanged)
      return;
    taken = now;
    seeded = sow;
    verdictStep = vs;
    countdown = cd;
    if (structural)
      ctx.composer.render(describe());
    ctx.composer.renderSlot("hud", hud());
  }
};

SIGIL_SKETCH(EvaMagiInterior)
