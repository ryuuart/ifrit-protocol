// kumiko_asanoha.cpp — ASANOHA KUMIKO RANMA (麻の葉 組子 欄間)
// =============================================================================
// A generated reconstruction of a Japanese kumiko lattice transom panel: the
// asanoha ("hemp leaf") field, built on the 90° square jigumi, seated in a
// plain masu register and a mitred keyaki frame, hung between the nageshi
// (upper beam) and the kamoi (door-track beam) of a room, lit from the far
// side.
//
// REFERENCES (every dimension and angle below comes from one of these)
//  - japanese-modern.co.jp/en/column/kumiko-zaiku — the two-tier taxonomy:
//    kumiko-zaiku = JIGUMI (base framework) + HA ("leaves", the nested
//    decorative infill). Asuka period origin (~600–700 CE, Buddhist temple
//    joinery); Ōkawa City, Fukuoka as a living center.
//  - e2japan.com/woodwork/joints/asa-no-ha-kumiko — the square-grid variant
//    is "a square base grid with half-lap joints at each intersection",
//    compound-mitred infill, friction fit, no glue.
//  - woodwiki.org/guides/kumiko-panel (transcribing Matt Kenney, *The Art of
//    Kumiko*, Panel 5) — SEVEN pieces per jigumi cell, cut in the order
//    diagonal → 2 fillers → 4 locking pieces; the only three jigs needed are
//    22.5° / 45° / 67.5°; stock 1/2" × 1/8"; graduated notch depths
//    5/16" – 3/16" – 5/16" for a flush three-strip crossing.
//  - rbt.tools kumiko-patterns — square-grid asanoha needs 22.5/45/67.5 jigs
//    (the triangular-grid variant needs only 30/60).
//  - kumikowoodworking.com (Tanihata Co., Toyama) + japanobjects.com/features
//    /kumiko — 0.1 mm assembly tolerance, 12 mm kumiko stock, 12/16/30 mm
//    frames.
//  - Big Sand Woodworking / Kenney — grid pitch 22 mm (contemporary) to
//    25.4 mm (Kenney's beginner panel); stock 12.7 mm deep × 3.2 mm face.
//  - japanese-culture.sakuraweb.com — ranma 600–900 mm wide × 300–450 mm tall,
//    hinoki lattice, keyaki (zelkova) frame.
//
// THE CONSTRUCTION (derived, not traced — change kCell and the lattice
// re-tiles at the new pitch, piece counts and all)
//
//   Per square jigumi cell of side s, one diagonal splits it into two right
//   isoceles triangles. In EACH triangle the three infill pieces run from the
//   triangle's vertices to its INCENTER. That single rule reproduces the
//   source's numbers exactly:
//     * the incircle radius of a right isoceles triangle of legs s is
//       r = s(2−√2)/2 = 0.29289 s, so the incenter of the (A,B,C) half sits
//       at (s−r, r);
//     * the arm from a 45° corner therefore leaves at atan(r/(s−r)) =
//       atan(√2−1) = 22.5° — and its partner in the other half at 67.5°;
//     * the diagonal bisects what is left, so each 90° corner is divided
//       22.5 + 45 + 22.5 = 90 — the three-way bisection, i.e. why only
//       three jigs exist;
//     * 1 diagonal + 2 arms off the right-angle corners + 4 arms off the 45°
//       corners = 7 pieces per cell, the documented count.
//   Cut angles fall out of the same geometry: a 22.5° arm seats against the
//   jigumi face it is shallow to, so its end face is cut 67.5° off its own
//   axis, and the 67.5° arm's is cut 22.5°. The arms off the 90° corners run
//   down the corner bisector, so they are cut 45°/45°.
//   The role names here follow the sources' cutting ORDER — 1 diagonal, then
//   2 "fillers", then 4 "locking pieces" — so kRoleFiller is the pair off the
//   right-angle corners and kRoleLock the four off the 45° corners. Read the
//   cut angles off the geometry above, not off those two names.
//
//   Diagonals alternate direction on a checkerboard of (col+row) parity, which
//   is what puts a 4-fold rotation centre on every jigumi vertex — wallpaper
//   group p4m. The visible consequence: alternating 16-ray and 8-ray stars,
//   every ray a multiple of 22.5°.
//
// EVERY PIECE IS A BOARD, NOT A LINE: each strip is an element with a mitred
// quad outline(), an SkSL timber material (cross-section shading + longitudinal
// grain, seeded per strip), a BevelEmboss arris counter-rotated so raking light
// stays world-fixed, and a hairline seam keyline. The half-lap notch marks and
// the tenon nubs where the jigumi seats into the register are generated from
// the crossing/termination graph and fade in on the seating beat.
//
// MOTION is the real assembly order (frame → register → jigumi verticals
// → jigumi horizontals → per-cell diagonal → fillers → locking pieces →
// joint-seating → the far room's lamp), driven off one clock through per-piece
// delays computed from (role, row, col). The build runs 0 → 3.4 s and holds to
// 6.4 s; --at 1.9 catches the leaf sweep crossing the field, --at 2.74 the
// finished-but-unlit panel, --at 4.2 the hero.
//
// COUNTS at the shipped pitch: 60 cells × 7 ha = 420 leaf pieces, 11 + 7
// jigumi members, 36 register pieces, 4 mitred frame members, 36 tenon heads
// = 514 boards, plus the half-lap seam marks derived from the crossing graph.
// Each board is a real element with its own material, bevel, keyline and pair
// of bound Outputs.
//
//   ./build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
//       src/sketch/sketches/kumiko_asanoha.cpp \
//       --frame /tmp/kumiko_asanoha.png
// =============================================================================

#include <include/core/SkPathBuilder.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilmaterial/core/Bank.h>
#include <sigilmaterial/kit/Grained.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/Animation.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace matkit = sigil::material::kit;
namespace mat = sigil::material;
namespace shapes = sigil::geometry::shapes;
namespace skia = sigil::material::skia;
namespace weave = sigil::weave;

using namespace sigil::compose;
using namespace sigil::motion;
using sigil::material::skia::Paint;
using namespace std::chrono_literals;

namespace {

// ---------------------------------------------------------------------------
// Palette — wood-tone matches by eye, not a colorimeter reading

// Hinoki #E9D3A0 is the colour of the stock in daylight. This
// panel is BACKLIT: the wood faces away from the lamp, so the body sits a
// couple of stops under it and only the arris reaches the daylight value —
// otherwise cream wood and cream light have no separation and the fretwork
// stops silhouetting, which is the whole point of a ranma.
const SkColor4f kHinoki = hex(0xD6BC89);      // planed cypress, room-side
const SkColor4f kHinokiLit = hex(0xF5E6C4);   // #E9D3A0's daylight arris
const SkColor4f kHinokiDark = hex(0x8E6C3B);  // notch shadow
const SkColor4f kKeyaki = hex(0x76472A);      // zelkova frame
const SkColor4f kKeyakiLit = hex(0x9C6B3E);
const SkColor4f kKeyakiDark = hex(0x4B2A12);
const SkColor4f kGlow = hex(0xF4E3B8);  // the far room's lamp
const SkColor4f kNight = hex(0x0D0906);
const SkColor4f kSeam = hex(0x4A3620, 0.55f);
const SkColor4f kCaption = hex(0xD8C9A8, 0.60f);

// ---------------------------------------------------------------------------
// Composition. The field is FIXED; the pitch is the free constant —
// change kCell alone and cols/rows re-derive, so the lattice just gets denser.

// The room is 1400 x 1000; under it stands the SHOP DRAWING band, where the
// panel's whole argument — seven pieces per cell, from the incircle, on
// three jigs — is taken apart at cell scale.
constexpr float kW = 1400, kRoom = 1000;
constexpr float kBandH = 210;
constexpr float kH = kRoom + kBandH;

constexpr float kCell =
    90.0f;  // <<< THE PITCH. 60 → a 15×9 field, still legal.
constexpr float kFieldW = 900, kFieldH = 540;
// NOLINTBEGIN(bugprone-throwing-static-initialization): arithmetic on constants
// cannot throw
const int kCols = std::max(2, (int)std::lround(kFieldW / kCell));
const int kRows = std::max(2, (int)std::lround(kFieldH / kCell));
const float kCellW = kFieldW / (float)kCols;
const float kCellH = kFieldH / (float)kRows;

const SkRect kField =
    SkRect::MakeXYWH(700 - kFieldW / 2, 500 - kFieldH / 2, kFieldW, kFieldH);
// The register band is exactly HALF the field pitch, so its plain cells come
// out square (masu = "measuring box") and its members interleave with the
// field's own jigumi at a clean 2:1.
const float kBand = kCell * 0.5f;  // plain masu register band
constexpr float kBorder = 45;      // kumiko-buchi frame
const SkRect kRegOuter = kField.makeOutset(kBand, kBand);
const SkRect kFrameOuter = kRegOuter.makeOutset(kBorder, kBorder);

// Stock face widths, as fractions of the pitch so they re-derive with kCell.
// Real stock is 12.7 mm deep × 3.2 mm face, which on a 22 mm pitch is ≈0.145;
// the jigumi here is drawn a little under that and the ha narrower again,
// because the infill is thinner stock than the framework it seats into.
const float kJigumiW = std::max(6.0f, 0.125f * kCell);
const float kHaW = std::max(5.0f, 0.096f * kCell);
const float kRegW = kJigumiW * 0.80f;
// NOLINTEND(bugprone-throwing-static-initialization)

// ---------------------------------------------------------------------------
// Timeline. One clock, per-piece delays computed from role/row/col.

constexpr double kPeriod = 6.4;
constexpr double kTFrame = 0.00, kDFrame = 0.55;
constexpr double kTReg = 0.45, kDReg = 0.55;
constexpr double kTJigV = 0.95, kTJigH = 1.28, kDJig = 0.42;
constexpr double kTLeaf = 1.62, kLeafSweep = 0.62, kDLeaf = 0.30;
constexpr double kLeafDiag = 0.00, kLeafFill = 0.09, kLeafLock = 0.17;
constexpr double kTSeat = 2.62, kDSeat = 0.24;
constexpr double kTGlow = 2.78, kDGlow = 0.60;

inline float clamp01(double v) { return (float)std::clamp(v, 0.0, 1.0); }
inline float easeOutBack(float p) {
  const float c = 1.70158f, c3 = c + 1;
  const float q = p - 1;
  return 1 + c3 * q * q * q + c * q * q;
}

// ---------------------------------------------------------------------------
// The timber material — ONE SkSL recipe, seeded per strip.
//
// EVERY PIECE IS A BOARD, and `material::kit::timber` is what a board is: a
// flat face between a narrow lit arris and a narrow shadowed one, with grain
// running down the piece and a fine tooth over the whole face — generated
// per pixel from its parameters and a seed, never from an image.
//
//  * `span` is the piece's face width, so the cross-section shading is
//    authored once and lands correctly on a 45 px frame member and a 10 px
//    leaf piece alike.
//  * `flip` picks WHICH long edge is lit, computed per strip from the world
//    light so a rotated lattice still reads under one raking source.
//  * `along` turns the piece to run down local y, so one recipe boards the
//    lattice's rails and its posts.
//
// A `mat::Bank` of 24 buckets holds them: the seed folds to 24 values, so a
// panel of hundreds of boards costs a bounded number of materials rather than
// one per board, and because the instance is held rather than re-minted the
// identity is stable and a re-describe prunes.

struct Timber {
  SkColor4f base, light, dark;
  float grain;
  float figure;
};

const Timber kHinokiTimber{kHinoki, kHinokiLit, kHinokiDark, 0.19f, 0.26f};
const Timber kKeyakiTimber{kKeyaki, kKeyakiLit, kKeyakiDark, 0.055f, 0.38f};
// The room-side members face AWAY from the far room's lamp, so the same
// keyaki reads two stops down on the nageshi/kamoi and the posts.
const Timber kKeyakiShade{hex(0x33200F), hex(0x54341B), hex(0x140C05), 0.045f,
                          0.42f};

class TimberBank {
 public:
  Paint get(const Timber& t, float span, bool flip, uint32_t seed,
            bool along = false) {
    return Paint::recipe(m_bank.get(
        matkit::timberRecipe(),
        matkit::TimberParams{.base = skia::toColor(t.base),
                             .light = skia::toColor(t.light),
                             .dark = skia::toColor(t.dark),
                             .span = span,
                             .flip = flip ? 1.0f : 0.0f,
                             .along = along ? 1.0f : 0.0f,
                             .grain = t.grain,
                             .figure = t.figure,
                             // The tooth is the surface; the figure above is
                             // the wood's story. Keep toothScale x stretch
                             // under about a tenth or the tooth aliases into
                             // hash noise with no diagnostic.
                             .tooth = 0.26f,
                             .toothScale = 0.045f,
                             .stretch = 2.0f},
        seed));
  }

 private:
  mat::Bank m_bank{24};
};

// ---------------------------------------------------------------------------
// A strip: a centreline, a face width, and a CUT-FACE DIRECTION at each end.
//
// The mitre falls straight out of that last field. The two corners of an end
// are the intersections of the cut line (through the centreline endpoint, along
// `cut`) with the two edge lines — in the piece's own frame that is a pure
// shear k = (w/2)·(cut.x / cut.y), which is all outline() needs.

enum Role : uint8_t {
  kRoleFrame,
  kRoleRegister,
  kRoleJigumiV,
  kRoleJigumiH,
  kRoleDiagonal,
  kRoleFiller,
  kRoleLock,
  kRoleBeam
};

struct Strip {
  SkPoint a{}, b{};
  SkVector cutA{}, cutB{};  // cut-face directions (canvas space)
  float w = 10;
  Role role = kRoleJigumiV;
  const Timber* timber = &kHinokiTimber;
  uint32_t seed = 0;
  double delay = 0;
  double dur = 0.3;
  bool litToCenter = false;  // frame members catch light from the opening
};

SkVector perp(SkVector v) { return {-v.y(), v.x()}; }
SkVector norm(SkVector v) {
  const float l = v.length();
  return l > 1e-6f ? SkVector{v.x() / l, v.y() / l} : SkVector{1, 0};
}

// ---------------------------------------------------------------------------
// The panel generator

struct Panel {
  std::vector<Strip> strips;
  // Half-lap seam marks: (point, along, halfSpan, width) generated from the
  // crossing graph, not authored.
  struct Seam {
    SkPoint p;
    SkVector along;
    float halfSpan;
    float w;
  };
  std::vector<Seam> seams;
  std::vector<Strip> nubs;  // terminations seating into the register groove

  uint32_t seedCounter = 1;

  void push(SkPoint a, SkPoint b, float w, Role role, const Timber* t,
            double delay, double dur, SkVector cutA = {0, 0},
            SkVector cutB = {0, 0}, bool litToCenter = false) {
    Strip s;
    s.a = a;
    s.b = b;
    s.w = w;
    s.role = role;
    s.timber = t;
    s.delay = delay;
    s.dur = dur;
    s.seed = seedCounter++ * 2654435761u >> 13u;
    s.litToCenter = litToCenter;
    const SkVector u = norm({b.x() - a.x(), b.y() - a.y()});
    s.cutA = (cutA.length() < 1e-4f) ? perp(u) : norm(cutA);
    s.cutB = (cutB.length() < 1e-4f) ? perp(u) : norm(cutB);
    strips.push_back(s);
  }

  void build() {
    buildFrame();
    buildRegister();
    buildJigumi();
    buildLeaves();
    buildSeams();
  }

  // --- the mitred kumiko-buchi: four members, 45° corner cuts --------------
  void buildFrame() {
    const SkRect& f = kFrameOuter;
    const float h = kBorder * 0.5f;
    const SkVector dTL{0.7071f, 0.7071f}, dTR{-0.7071f, 0.7071f};
    // top, bottom, left, right — each mitred into the corner diagonals.
    push({f.left() + h, f.top() + h}, {f.right() - h, f.top() + h}, kBorder,
         kRoleFrame, &kKeyakiTimber, kTFrame + 0.00, kDFrame, dTL, dTR, true);
    push({f.right() - h, f.top() + h}, {f.right() - h, f.bottom() - h}, kBorder,
         kRoleFrame, &kKeyakiTimber, kTFrame + 0.10, kDFrame, dTR, dTL, true);
    push({f.right() - h, f.bottom() - h}, {f.left() + h, f.bottom() - h},
         kBorder, kRoleFrame, &kKeyakiTimber, kTFrame + 0.20, kDFrame, dTL, dTR,
         true);
    push({f.left() + h, f.bottom() - h}, {f.left() + h, f.top() + h}, kBorder,
         kRoleFrame, &kKeyakiTimber, kTFrame + 0.30, kDFrame, dTR, dTL, true);
  }

  // --- the plain masu register ---------------------------------------------
  // Its inner boundary IS the field's outermost jigumi (in real work one
  // member serves both), so this emits only the outer ring that seats into the
  // frame groove plus the half-pitch ties that square the band's cells up.
  void buildRegister() {
    const SkRect& o = kRegOuter;
    const float h = kRegW * 0.5f;
    int n = 0;
    auto stagger = [&] { return kTReg + 0.008 * (double)(n++); };

    push({o.left(), o.top() + h}, {o.right(), o.top() + h}, kRegW,
         kRoleRegister, &kHinokiTimber, stagger(), kDReg);
    push({o.left(), o.bottom() - h}, {o.right(), o.bottom() - h}, kRegW,
         kRoleRegister, &kHinokiTimber, stagger(), kDReg);
    push({o.left() + h, o.top()}, {o.left() + h, o.bottom()}, kRegW,
         kRoleRegister, &kHinokiTimber, stagger(), kDReg);
    push({o.right() - h, o.top()}, {o.right() - h, o.bottom()}, kRegW,
         kRoleRegister, &kHinokiTimber, stagger(), kDReg);

    // Half-pitch ties: one per field cell, landing exactly between the jigumi
    // members that already run through the band — square masu cells.
    for (int i = 0; i < kCols; ++i) {
      const float x = kField.left() + kCellW * ((float)i + 0.5f);
      push({x, o.top() + h}, {x, kField.top()}, kRegW, kRoleRegister,
           &kHinokiTimber, stagger(), kDReg);
      push({x, kField.bottom()}, {x, o.bottom() - h}, kRegW, kRoleRegister,
           &kHinokiTimber, stagger(), kDReg);
    }
    for (int j = 0; j < kRows; ++j) {
      const float y = kField.top() + kCellH * ((float)j + 0.5f);
      push({o.left() + h, y}, {kField.left(), y}, kRegW, kRoleRegister,
           &kHinokiTimber, stagger(), kDReg);
      push({kField.right(), y}, {o.right() - h, y}, kRegW, kRoleRegister,
           &kHinokiTimber, stagger(), kDReg);
    }
  }

  // --- the structural jigumi, running the whole opening -------------------
  // In real work a jigumi member is never a strip sliced mid-length: every
  // one runs groove to groove and carries a tenon head where it seats.
  void buildJigumi() {
    const SkRect& o = kRegOuter;
    for (int i = 0; i <= kCols; ++i) {
      const float x = kField.left() + kCellW * (float)i;
      push({x, o.top()}, {x, o.bottom()}, kJigumiW, kRoleJigumiV,
           &kHinokiTimber, kTJigV + 0.012 * (double)i, kDJig);
      addNub({x, o.top() + 2.5f}, {0, 1});
      addNub({x, o.bottom() - 2.5f}, {0, 1});
    }
    for (int j = 0; j <= kRows; ++j) {
      const float y = kField.top() + kCellH * (float)j;
      push({o.left(), y}, {o.right(), y}, kJigumiW, kRoleJigumiH,
           &kHinokiTimber, kTJigH + 0.016 * (double)j, kDJig);
      addNub({o.left() + 2.5f, y}, {1, 0});
      addNub({o.right() - 2.5f, y}, {1, 0});
    }
  }

  // A tenon head so a terminated strip reads as SEATED into a milled groove
  // rather than sliced off by a rectangle — one per termination.
  void addNub(SkPoint at, SkVector along) {
    const SkVector n = perp(norm(along));
    const float half = kJigumiW * 0.80f;
    Strip s;
    s.a = {at.x() - n.x() * half, at.y() - n.y() * half};
    s.b = {at.x() + n.x() * half, at.y() + n.y() * half};
    s.w = kRegW * 0.5f;
    s.role = kRoleRegister;
    s.timber = &kHinokiTimber;
    s.seed = seedCounter++ * 2654435761u >> 13u;
    s.cutA = perp(norm({s.b.x() - s.a.x(), s.b.y() - s.a.y()}));
    s.cutB = s.cutA;
    nubs.push_back(s);
  }

  // --- the ha: seven pieces per cell, incenter construction ---------------
  void buildLeaves() {
    const float rIn = 0.2928932f;   // incircle radius / leg
    const float rOut = 0.7071068f;  // 1 − rIn
    const float sin225 = 0.3826834f;
    const float d = kJigumiW * 0.5f + 1.0f;  // seat depth against a jigumi face
    const float tShallow = d / sin225;       // 22.5°/67.5° arms
    const float tBisect = d * 1.4142136f;    // 45° arms and the diagonal
    const float over = kHaW * 0.55f;         // overlap at the incenter Y-joint

    const double span = (double)std::max(1, kCols + kRows - 2);

    for (int j = 0; j < kRows; ++j) {
      for (int i = 0; i < kCols; ++i) {
        const bool flip = ((unsigned)(i + j) & 1u) != 0;
        const float ox = kField.left() + kCellW * (float)i;
        const float oy = kField.top() + kCellH * (float)j;
        auto P = [&](float lx, float ly) {
          return SkPoint{ox + (flip ? kCellW - lx : lx), oy + ly};
        };
        // Canonical: A=TL, B=TR, C=BR, D=BL; the diagonal is A→C.
        const SkPoint A = P(0, 0), B = P(kCellW, 0), C = P(kCellW, kCellH),
                      D = P(0, kCellH);
        const SkPoint I1 = P(kCellW * rOut, kCellH * rIn);  // incenter of ABC
        const SkPoint I2 = P(kCellW * rIn, kCellH * rOut);  // incenter of ACD

        const double cellT = (double)(i + j) / span;
        const double base = kTLeaf + kLeafSweep * cellT;

        auto arm = [&](SkPoint from, SkPoint to, float tStart, SkVector cut,
                       Role role, double off) {
          const SkVector u = norm({to.x() - from.x(), to.y() - from.y()});
          const SkPoint s0{from.x() + u.x() * tStart,
                           from.y() + u.y() * tStart};
          const SkPoint s1{to.x() + u.x() * over, to.y() + u.y() * over};
          push(s0, s1, kHaW, role, &kHinokiTimber, base + off, kDLeaf, cut,
               {0, 0});
        };

        // 1. the long diagonal — 45° into both 90° corners
        {
          const SkVector u = norm({C.x() - A.x(), C.y() - A.y()});
          push({A.x() + u.x() * tBisect, A.y() + u.y() * tBisect},
               {C.x() - u.x() * tBisect, C.y() - u.y() * tBisect}, kHaW,
               kRoleDiagonal, &kHinokiTimber, base + kLeafDiag, kDLeaf);
        }
        // 2. two fillers off the right-angle corners (cut 45°/45°)
        arm(B, I1, tBisect, {0, 0}, kRoleFiller, kLeafFill);
        arm(D, I2, tBisect, {0, 0}, kRoleFiller, kLeafFill + 0.03);
        // 3. four locking pieces off the 45° corners. The shallow one seats
        //    against the jigumi it grazes, so its face is that jigumi's line.
        arm(A, I1, tShallow, {1, 0}, kRoleLock, kLeafLock);
        arm(A, I2, tShallow, {0, 1}, kRoleLock, kLeafLock + 0.02);
        arm(C, I1, tShallow, {0, 1}, kRoleLock, kLeafLock + 0.04);
        arm(C, I2, tShallow, {1, 0}, kRoleLock, kLeafLock + 0.06);
      }
    }
  }

  // --- the half-lap seam marks, from the crossing graph -------------------
  // Every jigumi vertical crosses every jigumi horizontal; a half-lap shows
  // as the pair of hairlines where the upper piece's edges cross the lower.
  static int rank(Role r) {
    switch (r) {
      case kRoleDiagonal:
        return 1;
      case kRoleFiller:
        return 2;
      case kRoleLock:
        return 3;
      case kRoleJigumiH:
        return 4;
      case kRoleJigumiV:
        return 5;
      case kRoleRegister:
        return 6;
      default:
        return 0;
    }
  }

  void buildSeams() {
    std::vector<size_t> lat;
    for (size_t i = 0; i < strips.size(); ++i)
      if (rank(strips[i].role) >= 4) lat.push_back(i);

    for (size_t a = 0; a < lat.size(); ++a) {
      for (size_t b = a + 1; b < lat.size(); ++b) {
        const Strip &s1 = strips[lat[a]], &s2 = strips[lat[b]];
        if (rank(s1.role) == rank(s2.role))
          continue;  // same notch layer: they butt, they don't lap
        const SkVector d1{s1.b.x() - s1.a.x(), s1.b.y() - s1.a.y()};
        const SkVector d2{s2.b.x() - s2.a.x(), s2.b.y() - s2.a.y()};
        const float det = d1.x() * d2.y() - d1.y() * d2.x();
        if (std::abs(det) < 1e-3f) continue;
        const float rx = s2.a.x() - s1.a.x(), ry = s2.a.y() - s1.a.y();
        const float t = (rx * d2.y() - ry * d2.x()) / det;
        const float u = (rx * d1.y() - ry * d1.x()) / det;
        const float l1 = d1.length(), l2 = d2.length();
        const float m1 = 2.5f / std::max(l1, 1.0f);
        const float m2 = 2.5f / std::max(l2, 1.0f);
        if (t < m1 || t > 1 - m1 || u < m2 || u > 1 - m2)
          continue;  // an endpoint meeting is a butt joint, not a half-lap
        const bool oneOnTop = rank(s1.role) > rank(s2.role);
        const Strip& up = oneOnTop ? s1 : s2;
        const Strip& lo = oneOnTop ? s2 : s1;
        const SkVector uu = norm(oneOnTop ? d1 : d2);
        const SkVector ul = norm(oneOnTop ? d2 : d1);
        const float sinT = std::abs(uu.x() * ul.y() - uu.y() * ul.x());
        const float halfSpan =
            std::min(lo.w * 3.0f, (lo.w * 0.5f) / std::max(sinT, 0.15f));
        seams.push_back({{s1.a.x() + d1.x() * t, s1.a.y() + d1.y() * t},
                         uu,
                         halfSpan,
                         up.w});
      }
    }
  }
};

// ---------------------------------------------------------------------------
// Element for one strip. The mitre becomes an outline(); the timber becomes a
// fill; the arris becomes a counter-rotated BevelEmboss so the light stays
// world-fixed across ~700 differently-angled boards.

Element stripElement(const Strip& s, TimberBank& bank,
                     const choreograph::Output<float>* fade,
                     const choreograph::Output<float>* pop) {
  const SkVector d{s.b.x() - s.a.x(), s.b.y() - s.a.y()};
  const float len = d.length();
  const float ang = std::atan2(d.y(), d.x());
  const float cs = std::cos(-ang), sn = std::sin(-ang);
  auto shear = [&](SkVector c) {
    const float cx = c.x() * cs - c.y() * sn;
    const float cy = c.x() * sn + c.y() * cs;
    if (std::abs(cy) < 0.02f) return 0.0f;
    return (s.w * 0.5f) * (cx / cy);
  };
  const float kA = shear(s.cutA);
  const float kB = shear(s.cutB);
  const float pad = std::max(std::abs(kA), std::abs(kB)) + 0.5f;
  const float boxW = len + 2 * pad;
  const float xa = pad, xb = pad + len;

  SkPathBuilder quad;
  quad.moveTo(xa - kA, 0);
  quad.lineTo(xb - kB, 0);
  quad.lineTo(xb + kB, s.w);
  quad.lineTo(xa + kA, s.w);
  quad.close();
  SkPath shape = quad.detach();

  // Which long edge catches the light? Lattice pieces take one raking source
  // from the upper left; frame members take the light of the opening.
  const SkVector u = norm(d);
  const SkVector outward{u.y(), -u.x()};  // outward normal of the y=0 edge
  bool lit;
  if (s.litToCenter) {
    const SkVector toCenter = norm(
        {700 - (s.a.x() + s.b.x()) * 0.5f, 500 - (s.a.y() + s.b.y()) * 0.5f});
    lit = outward.x() * toCenter.x() + outward.y() * toCenter.y() > 0;
  } else {
    lit = outward.x() * -0.45f + outward.y() * -0.89f > 0;
  }

  const float angDeg = ang * 57.29578f;
  // The timber material already paints the arris. A bevel sized for a 45 px
  // frame member, applied to an 8 px leaf piece, double-counts it and the
  // piece stops being a board and becomes a length of rope — so the bevel
  // scales with the stock and stays a hint on the thin stuff.
  const bool heavy = s.w > 20.0f;
  const float bevelDepth = heavy ? s.w * 0.09f : 0.7f;
  const float bevelSize = heavy ? s.w * 0.14f : 1.0f;
  const float bevelAlpha = heavy ? 0.42f : 0.26f;

  Element e =
      box()
          .left((s.a.x() + s.b.x()) * 0.5f - boxW * 0.5f)
          .top((s.a.y() + s.b.y()) * 0.5f - s.w * 0.5f)
          .width(boxW)
          .height(s.w)
          .rotate(angDeg)
          .shape([shape](SkSize) { return shape; })
          .fill(bank.get(*s.timber, s.w, !lit, s.seed))
          // The arris: light angle counter-rotated into the piece's
          // own frame so one raking source lights every board.
          .foreground(styles::BevelEmboss{bevelDepth,
                                          bevelSize,
                                          120.0f + angDeg,
                                          {1, 0.96f, 0.86f, bevelAlpha},
                                          {0.14f, 0.09f, 0.03f, bevelAlpha}})
          // The seam every abutting piece shows against its neighbour.
          .stroke(stroke(0.6f, Fill::color(kSeam), PathFormat::Align::Inner));
  if (fade) e.opacity(fade);
  if (pop) e.scale(pop);
  return e;
}

// ---------------------------------------------------------------------------

}  // namespace

// ===========================================================================

struct KumikoAsanoha : sketch::Sketch {
  Panel panel;
  TimberBank bank;
  std::vector<choreograph::Output<float>> fade, pop;
  choreograph::Output<float> glow{0}, seat{0}, frameTrim{0};
  double t = 0;

  // --- the lattice, in paint order: ha under jigumi (a butt joint reads as
  // the ha stopping at the jigumi's face), jigumi under the register.
  Element lattice() {
    auto add = [&](Element& into, Role role) {
      for (size_t i = 0; i < panel.strips.size(); ++i)
        if (panel.strips[i].role == role)
          into.child(stripElement(panel.strips[i], bank, &fade[i], &pop[i]));
    };
    auto group = box().inset(0, 0, 0, 0).cache(Cache::Group);
    add(group, kRoleDiagonal);
    add(group, kRoleFiller);
    add(group, kRoleLock);
    add(group, kRoleJigumiH);
    add(group, kRoleJigumiV);
    add(group, kRoleRegister);
    // ONE LINE, and it is what makes a lattice of this size affordable, so
    // the reasoning behind it stays.
    //
    // Every strip is a wood-grain SkSL fill plus a BevelEmboss, rotated to
    // its own jig angle, carrying a bound opacity and a bound scale for the
    // entrance. Those bindings keep each strip volatile forever — the Output
    // never disconnects — so no per-node cache will ever hold pixels for it,
    // and replaying the picture re-runs every shader over every pixel on
    // every frame.
    //
    // Two narrower bakes do NOT work here, and both are worth knowing about
    // before trying them again:
    //   * Per-strip .cache(Cache::Texture) does bake, but a bake ISOLATES.
    //     Each bevel arris, and the compositing where two strips abut,
    //     resolves differently baked than live, so the panel visibly changes.
    //   * A container-level .cache(Cache::Texture) on this box is a no-op:
    //     Texture bakes a node's OWN paint, and a fill-less container has
    //     none. Giving it a transparent fill or forcing a stacking context
    //     does not change that.
    //
    // Cache::Group is the same idea at the right granularity. The whole
    // lattice composites ONCE into a single unrotated device-space layer, so
    // the rotations, arrises and abutments all resolve inside that bake at
    // full precision, and the layer is held only while every bound opacity
    // and scale beneath it still reads what it read last frame. The staggered
    // entrance therefore plays live and the settled panel costs one blit.
    return group;
  }

  Element frame() {
    // The same argument as lattice(), on four members instead of hundreds.
    // The mitred keyaki boards carry the same bound entrance, so they are
    // volatile forever too — and they are the largest single boards on the
    // canvas, so replaying their timber shader per frame costs more than
    // their count suggests.
    auto group = box().inset(0, 0, 0, 0).cache(Cache::Group);
    for (size_t i = 0; i < panel.strips.size(); ++i)
      if (panel.strips[i].role == kRoleFrame)
        group.child(stripElement(panel.strips[i], bank, &fade[i], &pop[i]));
    return group;
  }

  // The joint pass: every half-lap seam mark and every tenon nub arrives on
  // one beat — the craftsman's final seating tap.
  Element joinery() {
    auto seams = panel.seams;
    auto group = stack().inset(0, 0, 0, 0).opacity(&seat);
    group.child(custom([seams](SkCanvas& c, const PaintContext&) {
                  SkPaint p;
                  p.setAntiAlias(true);
                  p.setStyle(SkPaint::kStroke_Style);
                  for (const Panel::Seam& s : seams) {
                    const SkVector n = perp(s.along);
                    for (int side = -1; side <= 1; side += 2) {
                      const float o = (float)side * s.w * 0.5f;
                      const SkPoint m{s.p.x() + n.x() * o, s.p.y() + n.y() * o};
                      p.setStrokeWidth(1.5f);
                      p.setColor4f({0.28f, 0.18f, 0.07f, 0.55f}, nullptr);
                      c.drawLine(m.x() - s.along.x() * s.halfSpan,
                                 m.y() - s.along.y() * s.halfSpan,
                                 m.x() + s.along.x() * s.halfSpan,
                                 m.y() + s.along.y() * s.halfSpan, p);
                      p.setStrokeWidth(0.7f);
                      p.setColor4f({1, 0.95f, 0.82f, 0.30f}, nullptr);
                      const float k = (float)side * 0.9f;
                      c.drawLine(m.x() - s.along.x() * s.halfSpan + n.x() * k,
                                 m.y() - s.along.y() * s.halfSpan + n.y() * k,
                                 m.x() + s.along.x() * s.halfSpan + n.x() * k,
                                 m.y() + s.along.y() * s.halfSpan + n.y() * k,
                                 p);
                    }
                  }
                }).inset(0, 0, 0, 0));
    for (const Strip& n : panel.nubs)
      group.child(stripElement(n, bank, nullptr, nullptr));
    return group;
  }

  Element backlight() {
    const SkRect& open = kRegOuter;  // the frame's opening
    return box()
        .left(open.left())
        .top(open.top())
        .width(open.width())
        .height(open.height())
        .clip(true)
        .opacity(&glow)
        .background(styles::OuterGlow{hex(0xF4E3B8, 0.34f), 70, 6})
        // A SHOJI DIFFUSES. Paper over a lamp is a lit field, not a point
        // source seen through a hole: the ramp falls a third of a stop
        // from the middle of the opening to its corners, and the pattern —
        // which is the whole subject — is readable everywhere across it.
        // A hot core with a dark rim blows out the middle third and puts
        // the corner cells in the dark, which loses the field at both ends
        // at once. The outer radius is the opening's diagonal.
        .child(box()
                   .inset(0, 0, 0, 0)
                   .fill(Paint::radial(
                       {open.width() * 0.5f, open.height() * 0.5f}, 585,
                       {{0.00f, hex(0xF7E8C6, 0.88f)},
                        {0.30f, hex(0xF2E0B4, 0.85f)},
                        {0.58f, hex(0xE6CE9A, 0.79f)},
                        {0.80f, hex(0xD3B37C, 0.71f)},
                        {1.00f, hex(0xBE9862, 0.62f)}})));
  }

  Element beam(float y, float h, bool top) {
    return box()
        .left(0)
        .top(y)
        .width(kW)
        .height(h)
        .fill(bank.get(kKeyakiShade, h, !top, 7))
        .foreground(
            styles::InnerShadow{{0, 0, 0, 0.65f}, {0, top ? 8.f : -8.f}, 18})
        .foreground(styles::BevelEmboss{2.5f,
                                        4,
                                        top ? 300.0f : 120.0f,
                                        {1, 0.88f, 0.68f, 0.16f},
                                        {0, 0, 0, 0.60f}});
  }

  Element post(float x, float w) {
    return box()
        .left(x)
        .top(118)
        .width(w)
        .height(kRoom - 236)
        .fill(bank.get(kKeyakiShade, w, x > 700, 3, /*swap=*/true))
        .foreground(styles::InnerShadow{
            {0, 0, 0, 0.60f}, {x > 700 ? -7.f : 7.f, 0}, 16});
  }

  // =========================================================================
  // THE SHOP DRAWING — one cell, taken apart
  //
  // The field's whole argument is a rule about ONE cell, and on the panel it
  // is invisible: seven pieces per jigumi cell, each running from a vertex of
  // one of the two right isoceles triangles the diagonal makes to that
  // triangle's INCENTER. Everything else — the 22.5°/45°/67.5° jig angles,
  // the three-way bisection of every right angle, the piece count — follows
  // from the incircle. So the cell is drawn again here at four times the
  // panel's pitch, with the two incircles struck, the incenters marked, and
  // the seven pieces pushed a little off their seats.
  //
  // The pieces are generated by the SAME rule the field uses and drawn by the
  // same `stripElement`, so this cannot drift out of agreement with the panel
  // beside it: change the construction and both change.

  /** The seven ha of one cell of side @p side at @p origin, each pushed
   *  @p burst px along its own axis so the joints open. */
  static std::vector<Strip> cellPieces(SkPoint origin, float side,
                                       float stock, float burst) {
    const float rIn = 0.2928932f;   // incircle radius / leg
    const float rOut = 0.7071068f;  // 1 - rIn
    std::vector<Strip> out;
    auto P = [&](float lx, float ly) {
      return SkPoint{origin.x() + lx, origin.y() + ly};
    };
    const SkPoint A = P(0, 0), B = P(side, 0), C = P(side, side),
                  D = P(0, side);
    const SkPoint I1 = P(side * rOut, side * rIn);  // incenter of ABC
    const SkPoint I2 = P(side * rIn, side * rOut);  // incenter of ACD

    uint32_t seed = 101;
    auto piece = [&](SkPoint from, SkPoint to, Role role, SkVector cut) {
      const SkVector u = norm({to.x() - from.x(), to.y() - from.y()});
      Strip st;
      st.a = {from.x() + u.x() * burst, from.y() + u.y() * burst};
      st.b = {to.x() + u.x() * burst, to.y() + u.y() * burst};
      st.w = stock;
      st.role = role;
      st.timber = &kHinokiTimber;
      st.seed = seed++ * 2654435761u >> 13u;
      st.cutA = (cut.length() < 1e-4f) ? perp(u) : norm(cut);
      st.cutB = perp(u);
      out.push_back(st);
    };
    // 1 diagonal, 2 fillers off the right angles, 4 locking pieces off the
    // 45° corners — the cutting order the sources give.
    piece(A, C, kRoleDiagonal, {0, 0});
    piece(B, I1, kRoleFiller, {0, 0});
    piece(D, I2, kRoleFiller, {0, 0});
    piece(A, I1, kRoleLock, {1, 0});
    piece(A, I2, kRoleLock, {0, 1});
    piece(C, I1, kRoleLock, {0, 1});
    piece(C, I2, kRoleLock, {1, 0});
    return out;
  }

  Element shopDrawing() {
    const float y0 = kRoom;
    const float side = 138.0f;
    const SkPoint org{206, y0 + 38};
    const float rIn = 0.2928932f, rOut = 0.7071068f;
    const SkPoint I1{org.x() + side * rOut, org.y() + side * rIn};
    const SkPoint I2{org.x() + side * rIn, org.y() + side * rOut};
    const float r = side * rIn;  // the incircle radius of each half

    const auto rule = weave::textStyle(
        {.size = 10.5f, .color = hex(0xC9B78F, 0.75f), .track = 0.9f});
    const auto ink = weave::textStyle(
        {.size = 12, .color = hex(0xE4D5B2, 0.86f), .track = 1.3f});
    const auto dim = weave::textStyle(
        {.size = 10.5f, .color = hex(0xB7A281, 0.55f), .track = 0.5f});

    Element g = box().left(0).top(y0).width(kW).height(kBandH).fill(
        Fill::color(hex(0x120C07)));
    // the drawing's own ground: a hairline ruled off the room above it
    g.child(box().left(0).top(0).width(kW).height(1).fill(
        Fill::color(hex(0x4A3620, 0.9f))));

    Element art = box().left(0).top(0).width(kW).height(kBandH);

    // the jigumi cell it all sits in
    art.child(box()
                  .left(org.x())
                  .top(org.y() - y0)
                  .width(side)
                  .height(side)
                  .stroke(stroke(1.0f, Fill::color(hex(0x8E6C3B, 0.85f)),
                                 PathFormat::Align::Center)));
    // the two incircles, struck: the construction the whole pattern is
    // derived from, and the only circles anywhere in a kumiko panel
    for (const SkPoint& c : {I1, I2})
      art.child(kit::disc(SkPoint{c.x(), c.y() - y0}, r)
                    .shape(shapes::circle())
                    .fill(Fill::none())
                    .stroke(PathFormat{
                        .width = 0.9f,
                        .strokeFill = Fill::color(hex(0xC79A57, 0.45f)),
                        .dashIntervals = {4.0f, 4.0f}}));
    for (const SkPoint& c : {I1, I2})
      art.child(kit::disc(SkPoint{c.x(), c.y() - y0}, 2.4f)
                    .shape(shapes::circle())
                    .fill(Fill::color(hex(0xF4E3B8, 0.9f))));

    // the seven pieces, exploded off their seats
    for (const Strip& st : cellPieces({org.x(), org.y() - y0}, side,
                                      std::max(6.0f, kHaW * 1.6f), 5.0f))
      art.child(stripElement(st, bank, nullptr, nullptr));

    g.child(std::move(art));

    // the three jigs, as the three angles one right angle is cut into
    const char* jig[3] = {"22.5\xc2\xb0", "45\xc2\xb0", "67.5\xc2\xb0"};
    for (int i = 0; i < 3; ++i) {
      const float x = 430.0f + (float)i * 118.0f;
      const float a0 = 22.5f * (float)i;
      g.child(kit::disc(SkPoint{x, 96.0f}, 44.0f)
                  .shape(shapes::sector(-90.0f + a0, 22.5f, 0.0f))
                  .fill(Fill::color(hex(0xC79A57, 0.16f)))
                  .stroke(stroke(0.9f, Fill::color(hex(0xC79A57, 0.55f)),
                                 PathFormat::Align::Inner)));
      g.child(text(toU8(jig[i]), rule)
                  .left(x - 30)
                  .top(150)
                  .width(60)
                  .textAlign(weave::TextAlignment::kCenter));
    }
    g.child(text(toU8("THREE JIGS \xe2\x80\x94 AND A RIGHT ANGLE IS "
                      "22.5 + 45 + 22.5"),
                 dim)
                .left(392)
                .top(24)
                .width(300));

    // the reading
    g.child(text(toU8("ONE CELL, TAKEN APART"), ink).left(760).top(30));
    g.child(
        text(toU8("The diagonal cuts the cell into two right isoceles "
                  "triangles. In each, the three infill pieces run from the "
                  "triangle's vertices to its INCENTER \xe2\x80\x94 and every "
                  "number the panel is built on falls out of that one rule."),
             rule)
            .left(760)
            .top(56)
            .width(520));
    g.child(text(toU8("incircle r = s(2\xe2\x88\x92\xe2\x88\x9a" "2)/2 = "
                      "0.29289 s  \xc2\xb7  arm off a 45\xc2\xb0 corner = "
                      "atan(\xe2\x88\x9a" "2\xe2\x88\x92" "1) = "
                      "22.5\xc2\xb0"),
                 rule)
                .left(760)
                .top(126)
                .width(520));
    g.child(text(toU8("1 diagonal + 2 fillers + 4 locking pieces = 7 per "
                      "cell  \xc2\xb7  60 cells = 420 ha"),
                 rule)
                .left(760)
                .top(148)
                .width(520));
    return g;
  }

  Element describe(sketch::SketchContext& ctx) {
    (void)ctx;
    const SkRect mid = kRegOuter.makeOutset(1.5f, 1.5f);
    return stack()
        .fill(Fill::color(kNight))
        .child(backlight())
        .child(lattice())
        .child(joinery())
        // Backlight bleed: the lamp's halo added OVER the
        // fretwork, so the light visibly wraps the pieces it is behind
        // instead of stopping dead at their silhouettes.
        .child(
            box()
                .left(kRegOuter.left())
                .top(kRegOuter.top())
                .width(kRegOuter.width())
                .height(kRegOuter.height())
                .clip(true)
                .opacity(&glow)
                .blend(SkBlendMode::kPlus)
                .fill(Paint::radial(
                    {kRegOuter.width() * 0.5f, kRegOuter.height() * 0.5f}, 360,
                    {{0.00f, hex(0xFFF2D2, 0.13f)},
                     {0.45f, hex(0xE6BC7C, 0.07f)},
                     {1.00f, hex(0x000000, 0.00f)}})))
        .child(frame())
        // The mitred frame's keyline draws itself on around the perimeter —
        // one continuous reveal, the first beat of the assembly.
        .child(box()
                   .left(mid.left())
                   .top(mid.top())
                   .width(mid.width())
                   .height(mid.height())
                   .stroke(spans::upTo(&frameTrim),
                           PathFormat{
                               .width = 2.2f,
                               .strokeFill = Fill::color(hex(0xC79A57, 0.60f)),
                               .align = PathFormat::Align::Center}))
        .child(post(0, 146))
        .child(post(kW - 146, 146))
        .child(beam(0, 122, true))
        .child(beam(kRoom - 122, 122, false))
        .child(text(toU8("ASANOHA KUMIKO \xc2\xb7 SQUARE JIGUMI \xc2\xb7 "
                         "HINOKI ON KEYAKI \xc2\xb7 900\xc3\x97"
                         "400mm TYPE"),
                    weave::textStyle({.size = 12, .color = kCaption, .track = 1.1f}))
                   .left(950)
                   .top(916)
                   .width(300)
                   .textAlign(weave::TextAlignment::kEnd))
        // A faint vertical vignette — the near-side room, in shadow. It
        // stops at the room's floor: the shop drawing under it is a
        // drawing, not part of the room.
        .child(box()
                   .left(0)
                   .top(0)
                   .width(kW)
                   .height(kRoom)
                   .fill(radialGradient(
                       {700, 500}, 920,
                       {{0, 0, 0, 0}, {0, 0, 0, 0.30f}, {0, 0, 0, 0.62f}},
                       {0.30f, 0.72f, 1.0f})))
        .child(shopDrawing());
  }

  void setup(sketch::SketchContext& ctx) override {
    // This sketch brings its own canvas size and unlit background rather
    // than inheriting a default, and photographs itself mid-hold: the panel
    // is complete and lit from kTGlow + kDGlow onwards, and the loop tears
    // down and reassembles at kPeriod. 4.2 s sits well clear of both edges,
    // so a small timing change on either side cannot catch the plate
    // half-built.
    sketch::kit::stage(
        ctx,
        {.size = SkSize::Make(kW, kH), .captureAt = 4.2, .background = kNight});

    panel = Panel{};
    panel.build();
    fade = std::vector<choreograph::Output<float>>(panel.strips.size());
    pop = std::vector<choreograph::Output<float>>(panel.strips.size());
    for (size_t i = 0; i < fade.size(); ++i) {
      fade[i] = 0.0f;
      pop[i] = 1.0f;
    }
    t = 0;

    ctx.ticker.add([this](double dt) {
      t += dt;
      const double now = std::fmod(t, kPeriod);
      for (size_t i = 0; i < panel.strips.size(); ++i) {
        const Strip& s = panel.strips[i];
        const float raw = clamp01((now - s.delay) / s.dur);
        fade[i] = std::min(1.0f, choreograph::easeOutCubic(raw) * 1.35f);
        pop[i] = 0.55f + 0.45f * easeOutBack(raw);
      }
      seat = choreograph::easeOutCubic(clamp01((now - kTSeat) / kDSeat));
      glow = choreograph::easeOutCubic(clamp01((now - kTGlow) / kDGlow));
      frameTrim = choreograph::easeOutCubic(
          clamp01((now - kTFrame) / (kDFrame + 0.35)));
      return true;
    });

    ctx.composer.render(describe(ctx));
  }

  void update(double, sketch::SketchContext&) override {}
};

SIGIL_SKETCH(KumikoAsanoha, "Study \xc2\xb7 Pattern",
             "A hinoki asanoha ranma \xe2\x80\x94 514 mitred boards, per-piece "
             "assembly staggering")
