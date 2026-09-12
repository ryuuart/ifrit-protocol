#pragma once

#include <include/core/SkPathBuilder.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Operations.h>
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
namespace operations = sigil::geometry::path::operations;
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
const SkColor4f kHinoki = hexColor(0xD6BC89);      // planed cypress, room-side
const SkColor4f kHinokiLit = hexColor(0xF5E6C4);   // #E9D3A0's daylight arris
const SkColor4f kHinokiDark = hexColor(0x8E6C3B);  // notch shadow
const SkColor4f kKeyaki = hexColor(0x76472A);      // zelkova frame
const SkColor4f kKeyakiLit = hexColor(0x9C6B3E);
const SkColor4f kKeyakiDark = hexColor(0x4B2A12);
const SkColor4f kGlow = hexColor(0xF4E3B8);  // the far room's lamp
const SkColor4f kNight = hexColor(0x0D0906);
const SkColor4f kSeam = hexColor(0x4A3620, 0.55f);
const SkColor4f kCaption = hexColor(0xD8C9A8, 0.60f);

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
// Timeline. One clock, per-piece delays computed from role/row/col. Six
// laws, not one ladder: the frame's four boards, the register's grid, the
// two jig passes and the leaves each open on their own base at their own
// step, and a leaf's base is a function of which CELL it belongs to. A
// cascade numbers units 0..N-1 and spaces them evenly, which is one of
// the six.

constexpr double kPeriod = 6.4;
constexpr double kTFrame = 0.00, kDFrame = 0.55;
constexpr double kTReg = 0.45, kDReg = 0.55;
constexpr double kTJigV = 0.95, kTJigH = 1.28, kDJig = 0.42;
constexpr double kTLeaf = 1.62, kLeafSweep = 0.62, kDLeaf = 0.30;
constexpr double kLeafDiag = 0.00, kLeafFill = 0.09, kLeafLock = 0.17;
constexpr double kTSeat = 2.62, kDSeat = 0.24;
constexpr double kTGlow = 2.78, kDGlow = 0.60;

inline float clamp01(double v) { return (float)std::clamp(v, 0.0, 1.0); }

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
const Timber kKeyakiShade{hexColor(0x33200F), hexColor(0x54341B),
                          hexColor(0x140C05), 0.045f, 0.42f};

class TimberBank {
 public:
  Paint get(const Timber& t, float span, bool flip, uint32_t seed,
            bool along = false) {
    return Paint::recipe(m_bank.get(
        matkit::timberRecipe(),
        matkit::TimberParameters{.base = skia::toColor(t.base),
                                 .light = skia::toColor(t.light),
                                 .dark = skia::toColor(t.dark),
                                 .span = span,
                                 .flip = flip ? 1.0f : 0.0f,
                                 .along = along ? 1.0f : 0.0f,
                                 .grain = t.grain,
                                 .figure = t.figure,
                                 // The tooth is the surface; the figure above
                                 // is the wood's story. Keep toothScale x
                                 // stretch under about a tenth or the tooth
                                 // aliases into hash noise with no diagnostic.
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
    // Every piece as stock, indexed as the panel holds it, so a lap the
    // joinery finds names the boards it is between. The tolerance is the
    // distance at which a crossing is a piece landing on another's face
    // — a butt joint, which shows no lap.
    std::vector<operations::Strip> stock;
    stock.reserve(strips.size());
    for (const Strip& s : strips)
      stock.push_back({{s.a.x(), s.a.y()}, {s.b.x(), s.b.y()}, s.w});

    for (const operations::StripLap& lap :
         operations::stripLaps(stock, {.tolerance = 2.5f, .lapLimit = 3.0f})) {
      const Strip& s1 = strips[(size_t)lap.pieces[0]];
      const Strip& s2 = strips[(size_t)lap.pieces[1]];
      // Only the lattice laps: a leaf piece sits on the face of what it
      // crosses rather than through it, and two members of one notch
      // layer butt instead of lapping.
      if (rank(s1.role) < 4 || rank(s2.role) < 4) continue;
      if (rank(s1.role) == rank(s2.role)) continue;
      const int up = rank(s1.role) > rank(s2.role) ? 0 : 1;
      seams.push_back({{lap.at.x, lap.at.y},
                       {lap.along[up].x, lap.along[up].y},
                       lap.halfSpan[up],
                       strips[(size_t)lap.pieces[up]].w});
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
          .shape(heldPath(shape))
          .fill(bank.get(*s.timber, s.w, !lit, s.seed))
          // The arris: light angle counter-rotated into the piece's
          // own frame so one raking source lights every board.
          .foreground(styles::BevelEmboss{bevelDepth,
                                          bevelSize,
                                          120.0f + angDeg,
                                          {1, 0.96f, 0.86f, bevelAlpha},
                                          {0.14f, 0.09f, 0.03f, bevelAlpha}})
          // The seam every abutting piece shows against its neighbour.
          .stroke(stroke(0.6f, Fill::color(kSeam), PathFormat::Align::Inner))
          // A PIECE IS A PICTURE OF A PIECE. The timber is a shader and
          // the arris a decoration over it, and neither changes once the
          // board is cut: what the entrance moves is where the board is
          // and how present it is, never what is on its face. Baked, the
          // face is resolved once at the sheet's own density and the
          // entrance is a blit that fades and swells; recorded, every
          // strip re-runs its grain over every one of its pixels on every
          // frame of the entrance, and there are hundreds of them.
          .cache(Cache::Texture);
  if (fade) e.opacity(fade);
  if (pop) e.scale(pop);
  return e;
}

// ---------------------------------------------------------------------------

}  // namespace

// ===========================================================================
