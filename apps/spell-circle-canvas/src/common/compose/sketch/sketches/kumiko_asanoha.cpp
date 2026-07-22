// kumiko_asanoha.cpp — ASANOHA KUMIKO RANMA (麻の葉 組子 欄間)
// =============================================================================
// A generated reconstruction of a Japanese kumiko lattice transom panel: the
// asanoha ("hemp leaf") field, built on the 90° square jigumi, seated in a
// plain masu register and a mitred keyaki frame, hung between the nageshi
// (upper beam) and the kamoi (door-track beam) of a room, lit from the far
// side.
//
// REFERENCES (from the research brief; every number below is theirs)
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
//       22.5 + 45 + 22.5 = 90 — the brief's "three-way bisection", i.e. why
//       only three jigs exist;
//     * 1 diagonal + 2 arms off the right-angle corners + 4 arms off the 45°
//       corners = 7 pieces per cell, the documented count.
//   Cut angles fall out of the same geometry: a 22.5° arm seats against the
//   jigumi face it is shallow to, so its end face is cut 67.5° off its own
//   axis, and the 67.5° arm's is cut 22.5° — the brief's filler cuts. The
//   arms off the 90° corners run down the corner bisector, so they are cut
//   45°/45° — the brief's locking cuts. (The brief attaches those two labels
//   to the opposite counts; the geometry, angles and piece total are as
//   documented, the naming here follows the cuts.)
//
//   Diagonals alternate direction on a checkerboard of (col+row) parity, which
//   is what puts a 4-fold rotation centre on every jigumi vertex — wallpaper
//   group p4m, as the brief specifies. The visible consequence: alternating
//   16-ray and 8-ray stars, every ray a multiple of 22.5°.
//
// EVERY PIECE IS A BOARD, NOT A LINE: each strip is an element with a mitred
// quad outline(), an SkSL timber material (cross-section shading + longitudinal
// grain, seeded per strip), a BevelEmboss arris counter-rotated so raking light
// stays world-fixed, and a hairline seam keyline. The half-lap notch marks and
// the tenon nubs where the jigumi seats into the register are generated from
// the crossing/termination graph and fade in on the seating beat.
//
// MOTION is the documented assembly order (frame → register → jigumi verticals
// → jigumi horizontals → per-cell diagonal → fillers → locking pieces →
// joint-seating → the far room's lamp), driven off one clock through per-piece
// delays computed from (role, row, col).
//
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/kumiko_asanoha.cpp \
//       --frame /tmp/kumiko_asanoha.png --at 4.2
// =============================================================================

#include <sigilsketch/Sketch.h>

#include <sigilcompose/LayerStyles.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Patterns.h>

#include <include/core/SkPathBuilder.h>
#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

using namespace sigil::compose;
using namespace sigil::compose::util;
using namespace std::chrono_literals;

namespace {

// ---------------------------------------------------------------------------
// Palette (brief §4 — wood-tone matches, not a colorimeter reading)

constexpr SkColor4f rgb(uint32_t hex, float a = 1.0f) {
  return {(float)((hex >> 16) & 0xff) / 255.0f,
          (float)((hex >> 8) & 0xff) / 255.0f, (float)(hex & 0xff) / 255.0f, a};
}

// The brief's hinoki #E9D3A0 is the colour of the stock in daylight. This
// panel is BACKLIT: the wood faces away from the lamp, so the body sits a
// couple of stops under it and only the arris reaches the daylight value —
// otherwise cream wood and cream light have no separation and the fretwork
// stops silhouetting, which is the whole point of a ranma.
const SkColor4f kHinoki = rgb(0xD6BC89);     // planed cypress, room-side
const SkColor4f kHinokiLit = rgb(0xF5E6C4);  // #E9D3A0's daylight arris
const SkColor4f kHinokiDark = rgb(0x8E6C3B); // notch shadow
const SkColor4f kKeyaki = rgb(0x76472A);     // zelkova frame
const SkColor4f kKeyakiLit = rgb(0x9C6B3E);
const SkColor4f kKeyakiDark = rgb(0x4B2A12);
const SkColor4f kGlow = rgb(0xF4E3B8);
const SkColor4f kNight = rgb(0x0D0906);
const SkColor4f kSeam = rgb(0x4A3620, 0.55f);
const SkColor4f kCaption = rgb(0xD8C9A8, 0.60f);

// ---------------------------------------------------------------------------
// Composition (brief §7). The field is FIXED; the pitch is the free constant —
// change kCell alone and cols/rows re-derive, so the lattice just gets denser.

constexpr float kW = 1400, kH = 1000;

constexpr float kCell = 90.0f; // <<< THE PITCH. 60 → a 15×9 field, still legal.
constexpr float kFieldW = 900, kFieldH = 540;
const int kCols = std::max(2, (int)std::lround(kFieldW / kCell));
const int kRows = std::max(2, (int)std::lround(kFieldH / kCell));
const float kCellW = kFieldW / (float)kCols;
const float kCellH = kFieldH / (float)kRows;

const SkRect kField = SkRect::MakeXYWH(700 - kFieldW / 2, 500 - kFieldH / 2,
                                       kFieldW, kFieldH);
// The register band is exactly HALF the field pitch, so its plain cells come
// out square (masu = "measuring box") and its members interleave with the
// field's own jigumi at a clean 2:1.
const float kBand = kCell * 0.5f; // plain masu register band
constexpr float kBorder = 45;     // kumiko-buchi frame
const SkRect kRegOuter = kField.makeOutset(kBand, kBand);
const SkRect kFrameOuter = kRegOuter.makeOutset(kBorder, kBorder);

// Stock face widths (12.7 mm deep × 3.2 mm face; 3.2/22 ≈ 0.145 of pitch).
const float kJigumiW = std::max(6.0f, 0.125f * kCell);
const float kHaW = std::max(5.0f, 0.096f * kCell);
const float kRegW = kJigumiW * 0.80f;

// ---------------------------------------------------------------------------
// Timeline (brief §9). One clock, per-piece delays computed from role/row/col.

constexpr double kPeriod = 6.4;
constexpr double kTFrame = 0.00, kDFrame = 0.55;
constexpr double kTReg = 0.45, kDReg = 0.55;
constexpr double kTJigV = 0.95, kTJigH = 1.28, kDJig = 0.42;
constexpr double kTLeaf = 1.62, kLeafSweep = 0.62, kDLeaf = 0.30;
constexpr double kLeafDiag = 0.00, kLeafFill = 0.09, kLeafLock = 0.17;
constexpr double kTSeat = 2.62, kDSeat = 0.24;
constexpr double kTGlow = 2.78, kDGlow = 0.60;

inline float clamp01(double v) { return (float)std::clamp(v, 0.0, 1.0); }
inline float easeOutCubic(float p) { return 1 - (1 - p) * (1 - p) * (1 - p); }
inline float easeOutBack(float p) {
  const float c = 1.70158f, c3 = c + 1;
  const float q = p - 1;
  return 1 + c3 * q * q * q + c * q * q;
}

// ---------------------------------------------------------------------------
// The timber material — ONE SkSL recipe, seeded per strip.
//
//  * uSpan is the piece's face width, so the cross-section shading (lit arris
//    → body → shadowed far edge) is authored once in strip-local space and
//    lands correctly on a 45 px frame member and a 10 px leaf piece alike.
//  * uFlip picks WHICH long edge is lit, computed per strip from the world
//    light so a rotated lattice still reads under one raking source.
//  * grain runs down local +x (the piece's length) at uGrain features/px.

sk_sp<SkRuntimeEffect> timberEffect() {
  static const sk_sp<SkRuntimeEffect> fx = [] {
    auto [effect, err] = SkRuntimeEffect::MakeForShader(SkString(R"(
      uniform float  uSpan;   // face width, px
      uniform float  uFlip;   // 1 → the far edge is the lit one
      uniform float  uSwap;   // 1 → the piece runs down local +y (posts)
      uniform float  uSeed;
      uniform float  uGrain;  // grain features per px along the piece
      uniform float  uFigure; // how hard the grain lines read
      uniform float4 uBase;
      uniform float4 uLight;
      uniform float4 uDark;
      half4 main(float2 p) {
        float2 xy = (uSwap > 0.5) ? float2(p.y, p.x) : p;
        float v = clamp(xy.y / max(uSpan, 1.0), 0.0, 1.0);
        v = (uFlip > 0.5) ? 1.0 - v : v;
        // A PLANED board, not a dowel: a flat face between a narrow lit
        // arris and a narrow shadowed one. Widen these two and the piece
        // immediately reads as rope.
        float lit   = smoothstep(0.17, 0.00, v);
        float shade = smoothstep(0.78, 1.00, v);
        float s = uSeed;
        float x = xy.x * uGrain + s * 17.3;
        float g = sin(x) * 0.5 + sin(x * 2.31 + s * 3.1) * 0.3
                + sin(x * 5.77 + s * 7.7) * 0.2;
        g = g * 0.5 + 0.5;
        float fig = smoothstep(0.48, 0.96, g);
        // A slow lengthwise tone drift so no two pieces read identical.
        float drift = sin(xy.x * 0.011 + s * 5.0) * 0.5 + 0.5;
        float4 c = mix(uBase, uLight, lit * 0.92);
        c = mix(c, uDark, shade * 0.85);
        c = mix(c, uDark, fig * uFigure);
        c = mix(c, uLight, drift * 0.07);
        return half4(half3(c.rgb), 1.0);
      }
    )"));
    if (!effect)
      SkDebugf("kumiko timber shader: %s\n", err.c_str());
    return effect;
  }();
  return fx;
}

struct Timber {
  SkColor4f base, light, dark;
  float grain;
  float figure;
};

const Timber kHinokiTimber{kHinoki, kHinokiLit, kHinokiDark, 0.19f, 0.26f};
const Timber kKeyakiTimber{kKeyaki, kKeyakiLit, kKeyakiDark, 0.055f, 0.38f};
// The room-side members face AWAY from the far room's lamp, so the same
// keyaki reads two stops down on the nageshi/kamoi and the posts.
const Timber kKeyakiShade{rgb(0x33200F), rgb(0x54341B), rgb(0x140C05), 0.045f,
                          0.42f};

// Materials are held (the Pattern/Material identity contract) and shared by
// (timber, span, flip, seed-bucket) so hundreds of pieces cost ~a hundred
// shaders, not one each.
class TimberBank {
public:
  Material get(const Timber &t, float span, bool flip, uint32_t seed,
               bool swap = false) {
    const uint64_t key = (uint64_t)(t.grain * 10000) << 40 |
                         (uint64_t)std::lround(span * 4) << 13 |
                         (uint64_t)(flip ? 1 : 0) << 12 |
                         (uint64_t)(swap ? 1 : 0) << 11 | (seed % 24);
    auto it = m_bank.find(key);
    if (it != m_bank.end())
      return it->second;
    sk_sp<SkRuntimeEffect> fx = timberEffect();
    if (!fx)
      return Material::solid(t.base);
    Material m = Material::sksl(fx, {{"uSpan", span},
                                     {"uFlip", flip ? 1.0f : 0.0f},
                                     {"uSwap", swap ? 1.0f : 0.0f},
                                     {"uSeed", (float)(seed % 24) * 0.41f},
                                     {"uGrain", t.grain},
                                     {"uFigure", t.figure}})
                     .uniform("uBase", t.base)
                     .uniform("uLight", t.light)
                     .uniform("uDark", t.dark);
    // The milled tooth on top of the figure: LUMINANCE noise (equal channels)
    // soft-lit over the timber. patterns::noise() would hue-shift the wood —
    // its three channels are independent fields.
    Material blended =
        Material::blend({{m, SkBlendMode::kSrcOver},
                         {m_tooth, SkBlendMode::kSoftLight}});
    m_bank.emplace(key, blended);
    return blended;
  }

private:
  // Built ONCE (each patterns::grain() call compiles its own effect).
  Material m_tooth = patterns::grain(0.62f, 3, 4.0f);
  std::map<uint64_t, Material> m_bank;
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
  SkVector cutA{}, cutB{}; // cut-face directions (canvas space)
  float w = 10;
  Role role = kRoleJigumiV;
  const Timber *timber = &kHinokiTimber;
  uint32_t seed = 0;
  double delay = 0;
  double dur = 0.3;
  bool litToCenter = false; // frame members catch light from the opening
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
  std::vector<Strip> nubs; // §10 terminations into the register groove

  uint32_t seedCounter = 1;

  void push(SkPoint a, SkPoint b, float w, Role role, const Timber *t,
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
    s.seed = seedCounter++ * 2654435761u >> 13;
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
    const SkRect &f = kFrameOuter;
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
    const SkRect &o = kRegOuter;
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
  // Brief §10: never a strip sliced mid-length. Every jigumi member runs
  // groove to groove and carries a tenon head where it seats.
  void buildJigumi() {
    const SkRect &o = kRegOuter;
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
  // rather than sliced by a rectangle (brief §10) — one per termination.
  void addNub(SkPoint at, SkVector along) {
    const SkVector n = perp(norm(along));
    const float half = kJigumiW * 0.80f;
    Strip s;
    s.a = {at.x() - n.x() * half, at.y() - n.y() * half};
    s.b = {at.x() + n.x() * half, at.y() + n.y() * half};
    s.w = kRegW * 0.5f;
    s.role = kRoleRegister;
    s.timber = &kHinokiTimber;
    s.seed = seedCounter++ * 2654435761u >> 13;
    s.cutA = perp(norm({s.b.x() - s.a.x(), s.b.y() - s.a.y()}));
    s.cutB = s.cutA;
    nubs.push_back(s);
  }

  // --- the ha: seven pieces per cell, incenter construction ---------------
  void buildLeaves() {
    const float rIn = 0.2928932f;  // incircle radius / leg
    const float rOut = 0.7071068f; // 1 − rIn
    const float sin225 = 0.3826834f;
    const float d = kJigumiW * 0.5f + 1.0f; // seat depth against a jigumi face
    const float tShallow = d / sin225;      // 22.5°/67.5° arms
    const float tBisect = d * 1.4142136f;   // 45° arms and the diagonal
    const float over = kHaW * 0.55f;        // overlap at the incenter Y-joint

    const double span =
        (double)std::max(1, kCols + kRows - 2);

    for (int j = 0; j < kRows; ++j) {
      for (int i = 0; i < kCols; ++i) {
        const bool flip = ((i + j) & 1) != 0;
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
          const SkPoint s0{from.x() + u.x() * tStart, from.y() + u.y() * tStart};
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
    case kRoleDiagonal: return 1;
    case kRoleFiller: return 2;
    case kRoleLock: return 3;
    case kRoleJigumiH: return 4;
    case kRoleJigumiV: return 5;
    case kRoleRegister: return 6;
    default: return 0;
    }
  }

  void buildSeams() {
    std::vector<size_t> lat;
    for (size_t i = 0; i < strips.size(); ++i)
      if (rank(strips[i].role) >= 4)
        lat.push_back(i);

    for (size_t a = 0; a < lat.size(); ++a) {
      for (size_t b = a + 1; b < lat.size(); ++b) {
        const Strip &s1 = strips[lat[a]], &s2 = strips[lat[b]];
        if (rank(s1.role) == rank(s2.role))
          continue; // same notch layer: they butt, they don't lap
        const SkVector d1{s1.b.x() - s1.a.x(), s1.b.y() - s1.a.y()};
        const SkVector d2{s2.b.x() - s2.a.x(), s2.b.y() - s2.a.y()};
        const float det = d1.x() * d2.y() - d1.y() * d2.x();
        if (std::abs(det) < 1e-3f)
          continue;
        const float rx = s2.a.x() - s1.a.x(), ry = s2.a.y() - s1.a.y();
        const float t = (rx * d2.y() - ry * d2.x()) / det;
        const float u = (rx * d1.y() - ry * d1.x()) / det;
        const float l1 = d1.length(), l2 = d2.length();
        const float m1 = 2.5f / std::max(l1, 1.0f);
        const float m2 = 2.5f / std::max(l2, 1.0f);
        if (t < m1 || t > 1 - m1 || u < m2 || u > 1 - m2)
          continue; // an endpoint meeting is a butt joint, not a half-lap
        const bool oneOnTop = rank(s1.role) > rank(s2.role);
        const Strip &up = oneOnTop ? s1 : s2;
        const Strip &lo = oneOnTop ? s2 : s1;
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

Element stripElement(const Strip &s, TimberBank &bank,
                     const choreograph::Output<float> *fade,
                     const choreograph::Output<float> *pop) {
  const SkVector d{s.b.x() - s.a.x(), s.b.y() - s.a.y()};
  const float len = d.length();
  const float ang = std::atan2(d.y(), d.x());
  const float cs = std::cos(-ang), sn = std::sin(-ang);
  auto shear = [&](SkVector c) {
    const float cx = c.x() * cs - c.y() * sn;
    const float cy = c.x() * sn + c.y() * cs;
    if (std::abs(cy) < 0.02f)
      return 0.0f;
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
  const SkVector outward{u.y(), -u.x()}; // outward normal of the y=0 edge
  bool lit;
  if (s.litToCenter) {
    const SkVector toCenter =
        norm({700 - (s.a.x() + s.b.x()) * 0.5f, 500 - (s.a.y() + s.b.y()) * 0.5f});
    lit = outward.x() * toCenter.x() + outward.y() * toCenter.y() > 0;
  } else {
    lit = outward.x() * -0.45f + outward.y() * -0.89f > 0;
  }

  const float angDeg = ang * 57.29578f;
  const float bevelDepth = std::clamp(s.w * 0.13f, 0.8f, 3.5f);

  Element e = box()
                  .absolute()
                  .left((s.a.x() + s.b.x()) * 0.5f - boxW * 0.5f)
                  .top((s.a.y() + s.b.y()) * 0.5f - s.w * 0.5f)
                  .width(boxW)
                  .height(s.w)
                  .rotate(angDeg)
                  .outline([shape](SkSize) { return shape; })
                  .fill(bank.get(*s.timber, s.w, !lit, s.seed))
                  // The arris: light angle counter-rotated into the piece's
                  // own frame so one raking source lights every board.
                  .foreground(styles::BevelEmboss{
                      bevelDepth, std::max(1.0f, s.w * 0.16f), 120.0f + angDeg,
                      {1, 0.96f, 0.86f, 0.40f},
                      {0.14f, 0.09f, 0.03f, 0.40f}})
                  // The seam every abutting piece shows against its neighbour.
                  .stroke(util::stroke(0.6f, Fill::color(kSeam),
                                       PathFormat::Align::Inner));
  if (fade)
    e.opacity(fade);
  if (pop)
    e.scale(pop);
  return e;
}

// ---------------------------------------------------------------------------

sigil::weave::TextStyle type(float size, SkColor4f color, float tracking) {
  sigil::weave::TextStyle s;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = tracking;
  s.paint.foreground.setColor(color.toSkColor());
  s.paint.foreground.setAntiAlias(true);
  return s;
}

} // namespace

// ===========================================================================

struct KumikoAsanoha : sigil::compose::sketch::Sketch {
  Panel panel;
  TimberBank bank;
  std::vector<choreograph::Output<float>> fade, pop;
  choreograph::Output<float> glow{0}, seat{0}, frameTrim{0};
  double t = 0;

  // --- the lattice, in paint order: ha under jigumi (a butt joint reads as
  // the ha stopping at the jigumi's face), jigumi under the register.
  Element lattice() {
    auto add = [&](Element &into, Role role) {
      for (size_t i = 0; i < panel.strips.size(); ++i)
        if (panel.strips[i].role == role)
          into.child(stripElement(panel.strips[i], bank, &fade[i], &pop[i]));
    };
    auto group = box().absolute().inset(0, 0, 0, 0);
    add(group, kRoleDiagonal);
    add(group, kRoleFiller);
    add(group, kRoleLock);
    add(group, kRoleJigumiH);
    add(group, kRoleJigumiV);
    add(group, kRoleRegister);
    return group;
  }

  Element frame() {
    auto group = box().absolute().inset(0, 0, 0, 0);
    for (size_t i = 0; i < panel.strips.size(); ++i)
      if (panel.strips[i].role == kRoleFrame)
        group.child(stripElement(panel.strips[i], bank, &fade[i], &pop[i]));
    return group;
  }

  // The joint pass (brief §9 step 5): every half-lap seam mark and every
  // tenon nub arrives on one beat — the craftsman's final seating tap.
  Element joinery() {
    auto seams = panel.seams;
    auto group = stack().absolute().inset(0, 0, 0, 0).opacity(&seat);
    group.child(custom([seams](SkCanvas &c, const PaintContext &) {
                  SkPaint p;
                  p.setAntiAlias(true);
                  p.setStyle(SkPaint::kStroke_Style);
                  for (const Panel::Seam &s : seams) {
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
                })
                    .absolute()
                    .inset(0, 0, 0, 0));
    for (const Strip &n : panel.nubs)
      group.child(stripElement(n, bank, nullptr, nullptr));
    return group;
  }

  Element backlight() {
    const SkRect open = kRegOuter.makeOutset(0, 0);
    return box()
        .absolute()
        .left(open.left())
        .top(open.top())
        .width(open.width())
        .height(open.height())
        .clip(true)
        .opacity(&glow)
        .background(styles::OuterGlow{rgb(0xF4E3B8, 0.34f), 70, 6})
        // Brief §7: core inner radius ~80 px, fading out by ~520 px.
        .child(box().absolute().inset(0, 0, 0, 0).fill(Material::radial(
            {open.width() * 0.5f, open.height() * 0.5f}, 520,
            {{0.00f, rgb(0xFFF8E6, 0.98f)},
             {0.22f, rgb(0xF7E7C0, 0.95f)},
             {0.45f, rgb(0xD9A964, 0.72f)},
             {0.68f, rgb(0x8E5A2C, 0.36f)},
             {0.86f, rgb(0x33200F, 0.12f)},
             {1.00f, rgb(0x0D0906, 0.00f)}})));
  }

  Element beam(float y, float h, bool top) {
    return box()
        .absolute()
        .left(0)
        .top(y)
        .width(kW)
        .height(h)
        .fill(bank.get(kKeyakiShade, h, !top, 7))
        .foreground(styles::InnerShadow{{0, 0, 0, 0.65f}, {0, top ? 8.f : -8.f},
                                        18})
        .foreground(styles::BevelEmboss{2.5f, 4, top ? 300.0f : 120.0f,
                                        {1, 0.88f, 0.68f, 0.16f},
                                        {0, 0, 0, 0.60f}});
  }

  Element post(float x, float w) {
    return box()
        .absolute()
        .left(x)
        .top(118)
        .width(w)
        .height(kH - 236)
        .fill(bank.get(kKeyakiShade, w, x > 700, 3, /*swap=*/true))
        .foreground(
            styles::InnerShadow{{0, 0, 0, 0.60f}, {x > 700 ? -7.f : 7.f, 0},
                                16});
  }

  Element describe(sketch::SketchContext &ctx) {
    (void)ctx;
    const SkRect mid = kRegOuter.makeOutset(1.5f, 1.5f);
    return stack()
        .fill(Fill::color(kNight))
        .child(backlight())
        .child(lattice())
        .child(joinery())
        .child(frame())
        // The mitred frame's keyline draws itself on around the perimeter —
        // one continuous reveal (brief §9 step 1).
        .child(box()
                   .absolute()
                   .left(mid.left())
                   .top(mid.top())
                   .width(mid.width())
                   .height(mid.height())
                   .trim(0.0f, &frameTrim)
                   .stroke(PathFormat{.width = 2.2f,
                                      .strokeFill =
                                          Fill::color(rgb(0xC79A57, 0.60f)),
                                      .align = PathFormat::Align::Center}))
        .child(post(0, 146))
        .child(post(kW - 146, 146))
        .child(beam(0, 122, true))
        .child(beam(kH - 122, 122, false))
        .child(text(toU8("ASANOHA KUMIKO \xc2\xb7 SQUARE JIGUMI \xc2\xb7 "
                         "HINOKI ON KEYAKI \xc2\xb7 900\xc3\x97""400mm TYPE"),
                    type(12, kCaption, 1.1f))
                   .absolute()
                   .left(950)
                   .top(916)
                   .width(300)
                   .textAlign(sigil::weave::TextAlignment::kEnd))
        // A faint vertical vignette — the near-side room, in shadow.
        .child(box().absolute().inset(0, 0, 0, 0).fill(util::radialGradient(
            {700, 500}, 920, {{0, 0, 0, 0}, {0, 0, 0, 0.30f}, {0, 0, 0, 0.62f}},
            {0.30f, 0.72f, 1.0f})));
  }

  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(kW, kH);
    ctx.background(kNight);

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
        const Strip &s = panel.strips[i];
        const float raw = clamp01((now - s.delay) / s.dur);
        fade[i] = std::min(1.0f, easeOutCubic(raw) * 1.35f);
        pop[i] = 0.55f + 0.45f * easeOutBack(raw);
      }
      seat = easeOutCubic(clamp01((now - kTSeat) / kDSeat));
      glow = easeOutCubic(clamp01((now - kTGlow) / kDGlow));
      frameTrim = easeOutCubic(clamp01((now - kTFrame) / (kDFrame + 0.35)));
      return true;
    });

    ctx.composer.render(describe(ctx));
  }

  void update(double, sketch::SketchContext &) override {}
};

SIGIL_SKETCH(KumikoAsanoha)
