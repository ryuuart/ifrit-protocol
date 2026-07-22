// chladni_tab1.cpp — "Tab. I", the first of eleven Klangfiguren plates in
// Ernst Florens Friedrich Chladni, *Entdeckungen über die Theorie des
// Klanges*, Leipzig: Weidmanns Erben und Reich, 1787. Engraved by Capieux,
// signed "Capieux. sculps. 1786." at the foot of the plate.
//
// REFERENCE
//   Internet Archive item `entdeckungenuber00chla`, IIIF leaf $92, pulled
//   at 1600x2072 (archive.org's copy of the 1787 first edition; ETH e-rara
//   holds the same edition at doi 10.3931/e-rara-4235). Twelve numbered
//   figures, 3 columns x 4 rows: each is a metal disc seen from above,
//   strewn with sand, bowed at the rim and damped with a finger, so the
//   grains bounce off the moving regions and pile along the still ones.
//   Nine figures are symmetric star modes; 7, 9 and 10 are the off-centre
//   variants Chladni describes in his own prose.
//
// EVERY NUMBER BELOW WAS MEASURED OFF THAT SCAN, not eyeballed:
//   * frame rules, by threshold-scanning rows/columns for >60% ink:
//     outer rule at scan (48,153)-(1517,1977), h/w = 1.242; the double
//     rule's second hairline 11-13 px inside it. The left rule sits under
//     the gutter shadow and reads dirty, so it is inferred by symmetry —
//     flagged, not hidden.
//   * twelve circle centres and radii, by brute-force Hough over each grid
//     cell: r = 179 +- 3 scan px everywhere, centres on a 482 x 452 pitch
//     with up to 11 px of hand wander, which is kept below (kFigures) —
//     figure 7 genuinely sits 11 px lower than figure 8.
//   * every star's point count AND inner radius, by walking 720 rays
//     inward from the rim and recording the first ink. THIS IS WHERE THE
//     BRIEF I WAS HANDED WAS WRONG: it guessed innerRatio 0.02-0.08 for
//     all nine stars. Measured, the hub GROWS monotonically with the mode
//     number — 0.05, 0.11, 0.18, 0.24, 0.36, 0.42, 0.46 for n = 1..7 —
//     which is most of what makes figure 12 read as a solid boss with
//     teeth and figure 1 as a bare needle. Spike counts confirmed 2n
//     independently of Chladni's prose (he states 4, 5, 6, 7 lines for
//     figs 6, 8, 11, 12; the rays agree).
//   * figures 3 and 5's blank channel, by walking rays OUTWARD until the
//     hatch starts: a 4-point star at innerRatio 0.35 and a 6-point star
//     at 0.66, both with arms reaching the rim. Same generator as figs 2
//     and 4, worn as a paper-coloured mask over hatched petals.
//
// THE TRACED THREE ARE NOT TRACED — THEY ARE COMPASS WORK.
//   Figures 7, 9 and 10 have no eigenmode formula, so the brief expected
//   hand-placed Beziers. Fitting circles through three measured points on
//   each curve says otherwise: every one is a true circular arc, and the
//   fits close to three decimal places. Figure 7's left hook is centred at
//   (-0.930, -0.447) in unit-disc coordinates with radius 0.540 — i.e. the
//   compass point sits ON THE RIM (|c| = 1.03) and the arc is swung from
//   there. That is how Capieux drew them, and it makes all three figures
//   parametric after all: `Linie{bearing, centreRadius, arcRadius}` below,
//   intersected with the disc. Chladni's own footnote licenses calling
//   these curves "lines": *"ich verstehe darunter jede Linie, die sich vom
//   Rande der Scheibe durch deren innere Theile bis wieder zum Rande
//   erstreckt"* — any path from rim to rim. Counted that way, figure 7 has
//   four lines (a diameter, two hooks, one arch) exactly like figure 6,
//   and figures 9 and 10 have five like figure 8. The variants are the
//   SAME modes, bent.
//   Two further corrections to the brief from the pixels: the hooks do NOT
//   meet the vertical diameter (they stop ~0.35R short of it), and figure
//   10's hooks do not nearly touch (0.5R apart at their closest).
//
// ORDER IS PITCH. Chladni tunes the plate by ear against the previous
// figure and reports intervals: fig 6 two octaves over the fundamental,
// fig 8 "somewhat less than a minor third" above that, fig 11 a false
// fifth above, fig 12 a fourth above — 24.0, 26.7, 32.7, 37.7 semitones.
// Those are monotonic in n, and the plate is already laid out in
// ascending n, so reading order IS frequency order. The bow pass below
// therefore runs 1..12 and the settle speed is keyed to the stated
// semitones where Chladni gives one (higher pitch bounces the sand into
// place faster) and to n where he does not.
//
// NO CURVED LETTERING, DELIBERATELY. Element::onPath() was offered for the
// rim labels; the crops say no. Every one of the 62 reference letters on
// this plate is UPRIGHT, never rotated to the tangent, on both halves of
// every circle. Bending them would be a nicer sketch and a worse study.
//
// BUILT FROM (the library, not by hand):
//   shapes::star()      nine ink figures and two paper-coloured masks
//   shapes::sector()    the ten hatch petals of figures 3 and 5
//   lines::hatch()      their engraved tone, fanned per petal
//   util::disc()        every circle on the plate
//   instancing::Pool    ~5900 sand grains, ONE atlas stamp, Mode::Live
//   bind()              one settle Output per figure, shaped three ways
//   PathFormat::trim*   the bow's travelling contact arc
//   patterns::grain/speckle  plate tone and foxing
//   trim()/withFrom()   the frame, the rims, the reading order
//
// Run:
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/chladni_tab1.cpp \
//       --frame /tmp/chladni_tab1.png --at 9.4
//
// 9.4 s is the settled plate under one bow stroke. 3.2 s is the bow pass
// in flight: the first figures drawn in sand, the last still scattered.

#include <sigilsketch/Sketch.h>

#include <sigilweave/FontContext.h>

#include <sigilcompose/Instances.h>
#include <sigilcompose/Kinetic.h>
#include <sigilcompose/Lines.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Shapes.h>

#include <include/core/SkContourMeasure.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkTypeface.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

using namespace sigil::compose;
using namespace sigil::compose::util;
using namespace std::chrono_literals;
namespace ch = choreograph;

namespace {

constexpr SkColor4f hex(uint32_t v, float a = 1.0f) {
  return {((v >> 16) & 0xffu) / 255.0f, ((v >> 8) & 0xffu) / 255.0f,
          (v & 0xffu) / 255.0f, a};
}

// ---------------------------------------------------------------------------
// palette — the scan is a warm, under-lit archive.org JPEG derivative
// (paper medians #b39b76 / #b49f7e / #a89379 across four clean patches,
// dense star fills #181511). Rag paper this age reads lighter in the hand,
// so the base is lifted and the sampled tone becomes the vignette end.

constexpr SkColor4f kPaper = hex(0xe3d7b6);
constexpr SkColor4f kPaperEdge = hex(0xa08757);
constexpr SkColor4f kInk = hex(0x211c14);      // the dense star fills
constexpr SkColor4f kInkLine = hex(0x362e23);  // hairline rims and rules
constexpr SkColor4f kInkSoft = hex(0x3a3125, 0.72f);
constexpr SkColor4f kFox = hex(0x9c7f57, 0.10f);

// ---------------------------------------------------------------------------
// geometry — canvas = the 1600x2072 scan x 0.975, so every measured pixel
// below converts by one constant.

constexpr float kScale = 0.975f;
constexpr float kW = 1600 * kScale; // 1560
constexpr float kH = 2072 * kScale; // 2020
constexpr float kR = 179.0f * kScale;       // every figure's radius: 174.5
constexpr float kTip = 0.95f;               // star tips stop just inside it
constexpr float kDeg = 3.14159265358979f / 180.0f;

// frame: outer rule (48,153)-(1517,1977) scan; the double rule's partner
// 11 px in. The left rule is inferred from the right (gutter shadow).
constexpr float kFrameL = 48 * kScale, kFrameT = 153 * kScale;
constexpr float kFrameR = 1517 * kScale, kFrameB = 1977 * kScale;
constexpr float kRuleGap = 11 * kScale;

SkPoint polar(SkPoint c, float radius, float bearingDeg) {
  return {c.x() + radius * std::sin(bearingDeg * kDeg),
          c.y() - radius * std::cos(bearingDeg * kDeg)};
}

// ---------------------------------------------------------------------------
// the twelve figures

enum class Kind { Star, Petals, Traced };

struct FigSpec {
  int num;
  float cx, cy;      // Hough-fitted centres, scan px (kept unrounded:
  float points;      // the hand wander is the plate's, not mine)
  float inner;       // measured innerRatio
  Kind kind;
  int n;             // Chladni's nodal-line count
  float semitones;   // above the fundamental; -1 = Chladni states none
  int grains;
};

// points = 2n outer spikes; inner = first-ink radius / rim radius, from
// the 720-ray scan. Figure 12's centre is the grid value (its Hough fit
// collided with the neighbouring caption).
const std::array<FigSpec, 12> kFigures = {{
    {1, 289, 394, 2, 0.05f, Kind::Star, 1, -1, 240},
    {2, 776, 392, 4, 0.11f, Kind::Star, 2, -1, 300},
    {3, 1253, 392, 4, 0.35f, Kind::Petals, 2, -1, 1900},
    {4, 293, 845, 6, 0.18f, Kind::Star, 3, -1, 360},
    {5, 776, 843, 6, 0.66f, Kind::Petals, 3, -1, 1500},
    {6, 1261, 843, 8, 0.24f, Kind::Star, 4, 24.0f, 420},
    {7, 297, 1304, 0, 0, Kind::Traced, 4, -1, 300},
    {8, 780, 1292, 10, 0.36f, Kind::Star, 5, 26.7f, 480},
    {9, 1257, 1296, 0, 0, Kind::Traced, 5, -1, 320},
    {10, 305, 1752, 0, 0, Kind::Traced, 5, -1, 320},
    {11, 780, 1744, 12, 0.42f, Kind::Star, 6, 32.7f, 540},
    {12, 1257, 1748, 14, 0.46f, Kind::Star, 7, 37.7f, 600},
}};

SkPoint centreOf(const FigSpec &f) { return {f.cx * kScale, f.cy * kScale}; }

// ---------------------------------------------------------------------------
// the compass construction for figures 7, 9, 10
//
// A "Linie" is the part of a circle of radius `arcRadius`, struck from a
// compass point at `centreRadius` in direction `bearing`, that lies inside
// the disc. `straight` is the degenerate case Chladni's footnote also
// admits: a plain diameter.

struct Linie {
  float bearing;      // deg clockwise from 12 o'clock
  float centreRadius; // 1 = on the rim
  float arcRadius;
  bool straight = false;
};

// fitted through three measured points per curve; residuals < 0.003 R
const std::vector<Linie> kFig7 = {
    {0, 0, 0, true},        // the vertical diameter
    {297, 1.10f, 0.58f},    // left hook  (measured c=(-0.930,-0.447) r=0.540)
    {63, 1.10f, 0.58f},     // right hook (measured c=( 1.095,-0.521) r=0.627;
                            //   mirrored — the engraver's two hands differ)
    {180, 1.50f, 1.068f},   // the arch below centre, crest at p
};
const std::vector<Linie> kFig9 = {
    {0, 0, 0, true},
    {306.4f, 1.145f, 0.495f},
    {53.6f, 1.145f, 0.495f},
    {180, 2.07f, 2.00f},    // shallow arch crossing the diameter at p
    {180, 1.52f, 1.00f},    // deeper arch crossing at q
};
const std::vector<Linie> kFig10 = {
    {306, 1.058f, 0.606f},  // no diameter at all: it has degenerated away
    {54, 1.058f, 0.606f},
    {0, 1.933f, 1.613f},    // the r-s line: a BOWL, compass point above
    {180, 1.836f, 1.536f},
    {180, 1.540f, 0.850f},
};

const std::vector<Linie> &linienOf(int num) {
  return num == 7 ? kFig7 : (num == 9 ? kFig9 : kFig10);
}

/** The arc of `l` that lies inside the unit disc, sampled into a polyline
 *  in a box of side 2r (centre at (r, r)). Solving |P| = 1 against
 *  |P - O| = rho gives both rim crossings; the arc between them that stays
 *  inside is the one whose midpoint is nearer the origin. */
SkPath liniePath(const Linie &l, float r) {
  SkPathBuilder b;
  const SkPoint mid{r, r};
  if (l.straight) {
    b.moveTo(polar(mid, r, l.bearing));
    b.lineTo(polar(mid, r, l.bearing + 180.0f));
    return b.detach();
  }
  const SkPoint o{std::sin(l.bearing * kDeg) * l.centreRadius,
                  -std::cos(l.bearing * kDeg) * l.centreRadius};
  const float on = std::sqrt(o.fX * o.fX + o.fY * o.fY);
  if (on < 1e-4f)
    return b.detach();
  const float k = (1.0f + on * on - l.arcRadius * l.arcRadius) * 0.5f;
  const float d = k / on;
  const float h2 = 1.0f - d * d;
  if (h2 <= 0.0f)
    return b.detach();
  const float h = std::sqrt(h2);
  const SkPoint u{o.fX / on, o.fY / on};
  const SkPoint nrm{-u.fY, u.fX};
  const SkPoint p0{d * u.fX + h * nrm.fX, d * u.fY + h * nrm.fY};
  const SkPoint p1{d * u.fX - h * nrm.fX, d * u.fY - h * nrm.fY};
  const float a0 = std::atan2(p0.fY - o.fY, p0.fX - o.fX);
  float a1 = std::atan2(p1.fY - o.fY, p1.fX - o.fX);
  // Pick the sweep whose midpoint stays inside the disc.
  auto insideAt = [&](float a) {
    const SkPoint p{o.fX + l.arcRadius * std::cos(a),
                    o.fY + l.arcRadius * std::sin(a)};
    return p.fX * p.fX + p.fY * p.fY;
  };
  float sweep = a1 - a0;
  while (sweep <= -SK_FloatPI)
    sweep += 2 * SK_FloatPI;
  while (sweep > SK_FloatPI)
    sweep -= 2 * SK_FloatPI;
  if (insideAt(a0 + sweep * 0.5f) > 1.0f)
    sweep += sweep > 0 ? -2 * SK_FloatPI : 2 * SK_FloatPI;
  constexpr int kSamples = 72;
  for (int i = 0; i <= kSamples; ++i) {
    const float a = a0 + sweep * (float)i / kSamples;
    const SkPoint p{o.fX + l.arcRadius * std::cos(a),
                    o.fY + l.arcRadius * std::sin(a)};
    const SkPoint q{mid.fX + p.fX * r, mid.fY + p.fY * r};
    if (i == 0)
      b.moveTo(q);
    else
      b.lineTo(q);
  }
  return b.detach();
}

// ---------------------------------------------------------------------------
// reference letters. Convention 2 from the plate: count = 4n, spacing
// 90/n degrees, always UPRIGHT, only on the figures whose production
// Chladni walks through in prose (1-5, 7, 9, 10). Figures 6, 8, 11 and 12
// are bare — verified against the plate, not tidied into symmetry. Figure
// 5 carries six letters where figure 4 carries twelve: also real.

struct Label {
  float bearing;
  float radius; // fraction of the rim
  const char *glyph;
};

const std::vector<Label> kNoLabels;
const std::vector<Label> kL1 = {
    {0, 1.14f, "n"}, {90, 1.16f, "m"}, {180, 1.15f, "p"}, {270, 1.16f, "q"}};
const std::vector<Label> kL2 = {
    {0, 1.14f, "g"},   {45, 1.15f, "p"},  {90, 1.16f, "q"},
    {135, 1.15f, "n"}, {180, 1.15f, "t"}, {225, 1.15f, "m"},
    {270, 1.16f, "r"}, {315, 1.15f, "f"}};
const std::vector<Label> kL4 = {
    {0, 1.14f, "h"},   {30, 1.14f, "q"},  {60, 1.15f, "g"},
    {90, 1.16f, "r"},  {120, 1.15f, "t"}, {150, 1.15f, "p"},
    {180, 1.15f, "f"}, {210, 1.15f, "b"}, {240, 1.15f, "o"},
    {270, 1.16f, "\xcf\x91"}, {300, 1.15f, "n"}, {330, 1.14f, "m"}};
const std::vector<Label> kL5 = {{0, 1.14f, "h"},   {90, 1.16f, "r"},
                                {150, 1.15f, "p"}, {180, 1.15f, "f"},
                                {240, 1.15f, "o"}, {300, 1.15f, "n"}};
const std::vector<Label> kL7 = {{312, 1.15f, "n"},
                                {48, 1.15f, "f"},
                                {163, 0.34f, "p"}, // on the arch's crest
                                {205, 1.12f, "s"},
                                {158, 1.12f, "r"}};
const std::vector<Label> kL9 = {{308, 1.15f, "k"},  {52, 1.15f, "n"},
                                {235, 0.14f, "p"},  {193, 0.40f, "q"},
                                {202, 1.13f, "o"},  {165, 1.11f, "m"}};
const std::vector<Label> kL10 = {{322, 1.16f, "t"}, {37, 1.15f, "n"},
                                 {325, 0.44f, "r"}, {35, 0.44f, "s"},
                                 {285, 1.12f, "k"}, {65, 1.13f, "l"}};

const std::vector<Label> &labelsOf(int num) {
  switch (num) {
  case 1: return kL1;
  case 2: return kL2;
  case 3: return kL2; // figs 2 and 3 share letters AND bearings — the
  case 4: return kL4; //   pairing that proves 3 is 2 drawn as valleys
  case 5: return kL5;
  case 7: return kL7;
  case 9: return kL9;
  case 10: return kL10;
  default: return kNoLabels;
  }
}

// ---------------------------------------------------------------------------

struct Rng {
  uint64_t s;
  explicit Rng(uint64_t seed) : s(seed * 0x9e3779b97f4a7c15ull + 0xda3e39cbu) {}
  float next() {
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    return (float)((s >> 11) & 0xffffffu) / (float)0x1000000u;
  }
  float range(float a, float b) { return a + (b - a) * next(); }
};

sigil::weave::TextStyle type(sk_sp<SkTypeface> face, float size,
                             SkColor4f color, float tracking = 0) {
  sigil::weave::TextStyle s;
  s.shaping.typeface = std::move(face);
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = tracking;
  s.paint.foreground.setColor4f(color, nullptr);
  s.paint.foreground.setAntiAlias(true);
  return s;
}

Transition ramp(float delayMs, float durMs, ch::EaseFn ease = ch::easeOutQuad) {
  Transition t;
  t.duration = std::chrono::milliseconds((int)durMs);
  t.delay = std::chrono::milliseconds((int)delayMs);
  t.ease = std::move(ease);
  return t;
}

// --- the reading order, in seconds -----------------------------------------
constexpr float tFrame = 0.15f;
constexpr float tTitle = 0.70f;
constexpr float tNumeral = 1.00f;
constexpr float tRim = 1.10f;
constexpr float tScatter = 1.35f;
constexpr float tBow0 = 1.75f;   // figure 1's bow stroke
constexpr float tBowStep = 0.30f; // + per figure, in Chladni's pitch order
constexpr float tCredit = 7.60f;
constexpr float tIdle = 7.30f;

} // namespace

// ===========================================================================

struct ChladniTab1 : sigil::compose::sketch::Sketch {
  // One settle phase per figure, shaped three different ways at three
  // different call sites by bind() — ink opacity, label opacity, label
  // rise — instead of three Outputs kept in sync in the tick loop.
  std::array<ch::Output<float>, 12> settle;
  std::array<ch::Output<float>, 12> bowPhase, bowAlpha;

  double now = 0;
  std::array<float, 12> bowShake{};

  struct Grain {
    SkPoint from, to;
    float rotFrom, rotTo;
    float t0, dur, scale, alpha, phase;
    int fig, frame;
  };
  std::vector<Grain> grains;
  std::shared_ptr<instancing::Atlas> atlas;
  std::shared_ptr<instancing::Pool> pool;

  Pattern foxing, foxingLL;
  Material inkMat, paperMat;
  sk_sp<SkTypeface> faceNumeral, faceLabel, faceSwash;
  ch::EaseFn settleEase;

  // ------------------------------------------------------------------
  // shapes

  static std::function<SkPath(SkSize)> starOutline(float points, float inner) {
    return shapes::star((int)points, inner);
  }

  /** The path of one Linie in the node's own box. */
  static std::function<SkPath(SkSize)> linieOutline(Linie l) {
    return [l](SkSize s) { return liniePath(l, s.width() * 0.5f); };
  }

  // ------------------------------------------------------------------
  // sand: ~5900 grains, one pool, one atlas stamp

  void seedGrains() {
    grains.clear();
    pool = std::make_shared<instancing::Pool>();

    for (size_t fi = 0; fi < kFigures.size(); ++fi) {
      const FigSpec &f = kFigures[fi];
      const SkPoint c = centreOf(f);
      Rng rng(1787u + f.num * 61u);

      // The bow strikes in plate order, which IS ascending pitch (see the
      // header). Where Chladni states an interval, it also sets how fast
      // the sand finds the line: 24 semitones = 1.00x, 37.7 = 0.72x.
      const float pitch = f.semitones > 0 ? f.semitones : 20.0f + 2.4f * f.n;
      const float speed = std::clamp(24.0f / pitch, 0.68f, 1.25f);
      const float bowAt = tBow0 + tBowStep * (float)fi;

      // target geometry, per kind
      SkPath starPath;
      std::vector<sk_sp<SkContourMeasure>> curves;
      std::vector<float> curveLen;
      float totalLen = 0;
      if (f.kind == Kind::Star) {
        starPath = starOutline(f.points, f.inner)(
            SkSize{2 * kR * kTip, 2 * kR * kTip});
      } else if (f.kind == Kind::Petals) {
        starPath = starOutline(f.points, f.inner)(SkSize{2 * kR, 2 * kR});
      } else {
        for (const Linie &l : linienOf(f.num)) {
          SkContourMeasureIter it(liniePath(l, kR), false);
          if (sk_sp<SkContourMeasure> m = it.next()) {
            curveLen.push_back(m->length());
            totalLen += m->length();
            curves.push_back(std::move(m));
          }
        }
      }

      for (int g = 0; g < f.grains; ++g) {
        Grain grain;
        grain.fig = (int)fi;
        // scattered start: uniform over the disc
        const float sa = rng.range(0, 360.0f);
        const float sr = kR * 0.965f * std::sqrt(rng.next());
        grain.from = polar(c, sr, sa);
        grain.rotFrom = rng.range(0, 2 * SK_FloatPI);

        if (f.kind == Kind::Star) {
          const float half = kR * kTip;
          SkPoint p{0, 0};
          for (int tries = 0; tries < 400; ++tries) {
            p = {rng.range(0, 2 * half), rng.range(0, 2 * half)};
            if (starPath.contains(p.fX, p.fY))
              break;
          }
          grain.to = {c.fX - half + p.fX, c.fY - half + p.fY};
          grain.rotTo = rng.range(0, 2 * SK_FloatPI);
        } else if (f.kind == Kind::Petals) {
          // the valleys BETWEEN the star's arms, density biased outward
          // the way the engraved fan is
          SkPoint p{0, 0};
          for (int tries = 0; tries < 400; ++tries) {
            const float a = rng.range(0, 360.0f);
            const float rr = kR * (0.30f + 0.68f * std::sqrt(rng.next()));
            p = polar({kR, kR}, rr, a);
            if (!starPath.contains(p.fX, p.fY))
              break;
          }
          grain.to = {c.fX - kR + p.fX, c.fY - kR + p.fY};
          // the fan: every stroke points radially out of the centre
          grain.rotTo = std::atan2(grain.to.fY - c.fY, grain.to.fX - c.fX) +
                        rng.range(-0.16f, 0.16f);
        } else {
          float pick = rng.next() * totalLen;
          size_t ci = 0;
          while (ci + 1 < curves.size() && pick > curveLen[ci]) {
            pick -= curveLen[ci];
            ++ci;
          }
          SkPoint pos{0, 0};
          SkVector tan{1, 0};
          curves[ci]->getPosTan(std::min(pick, curveLen[ci]), &pos, &tan);
          const float off = (rng.next() + rng.next() - 1.0f) * 3.1f;
          grain.to = {c.fX - kR + pos.fX - tan.fY * off,
                      c.fY - kR + pos.fY + tan.fX * off};
          grain.rotTo = std::atan2(tan.fY, tan.fX) + rng.range(-0.12f, 0.12f);
        }

        grain.t0 = bowAt + rng.range(0.0f, 0.24f);
        grain.dur = (0.80f + rng.range(0.0f, 0.42f)) * speed;
        grain.scale = rng.range(0.62f, 1.06f);
        grain.alpha = rng.range(0.62f, 1.0f);
        grain.phase = rng.range(0, 6.28f);
        grains.push_back(grain);
      }
    }

    pool->resize(grains.size());
    auto frames = pool->frames();
    Rng fr(4242);
    for (size_t i = 0; i < grains.size(); ++i)
      frames[i] = fr.next() < 0.42f ? 1 : (fr.next() < 0.5f ? 2 : 0);
  }

  // ------------------------------------------------------------------
  // one cell of the plate

  void figure(Element &root, size_t fi, sketch::SketchContext &) {
    const FigSpec &f = kFigures[fi];
    const SkPoint c = centreOf(f);
    const std::string tag = "f" + std::to_string(f.num);
    const float rimDelay = tRim * 1000.0f + (float)fi * 26.0f;

    // ink opacity: the drawing surfaces once the sand has found the line
    auto inkIn = [&] {
      return bind(&settle[fi]).from(0.58f, 0.94f).clamp(0.0f, 1.0f);
    };

    // ---- the rim hairline ----
    root.child(disc(c, kR)
                   .key(tag + "rim")
                   .corners({kR})
                   .fill(Fill::none())
                   .stroke(stroke(1.5f, Fill::color(kInkLine)))
                   .trim(0.0f, withFrom(0.0f, 1.0f,
                                        ramp(rimDelay, 620, ch::easeOutQuad))));

    // ---- the figure itself ----
    if (f.kind == Kind::Star) {
      root.child(disc(c, kR * kTip)
                     .key(tag + "star")
                     .outline(starOutline(f.points, f.inner))
                     .fill(inkMat)
                     .opacity(inkIn()));
    } else if (f.kind == Kind::Petals) {
      // Figures 3 and 5 are figures 2 and 4's star worn as a mask: the
      // hatched petals are the disc MINUS the star, so each valley gets
      // its own sector with the engraved rules fanned to its bearing,
      // and the star is then painted back in paper colour on top. No
      // boolean path ops needed — z-order is the cutout.
      const int petals = (int)f.points;
      const float sweep = 360.0f / (float)petals;
      for (int p = 0; p < petals; ++p) {
        const float b0 = (float)p * sweep;
        const float bc = b0 + sweep * 0.5f;
        root.child(
            disc(c, kR)
                .key(tag + "petal" + std::to_string(p))
                .outline(shapes::sector(b0 - 90.0f, sweep))
                .fill(Fill::none())
                .background(lines::hatch(Fill::color(hex(0x211c14, 0.60f)),
                                         6.4f, 1.15f, bc - 90.0f))
                .opacity(bind(&settle[fi]).from(0.52f, 0.98f).clamp(0, 1)));
      }
      root.child(disc(c, kR * 1.002f)
                     .key(tag + "mask")
                     .outline(starOutline(f.points, f.inner))
                     .fill(Fill::color(kPaper)));
    } else {
      const std::vector<Linie> &lines = linienOf(f.num);
      for (size_t li = 0; li < lines.size(); ++li)
        root.child(
            disc(c, kR)
                .key(tag + "l" + std::to_string(li))
                .outline(linieOutline(lines[li]))
                .fill(Fill::none())
                .stroke(stroke(2.6f, Fill::color(kInk)))
                .opacity(inkIn())
                .trim(0.0f,
                      bind(&settle[fi]).from(0.55f, 0.98f).clamp(0.0f, 1.0f)));
    }

    // ---- the bow's contact arc: a travelling window on the rim, as a
    // per-decoration trim (one node, its own window) ----
    PathFormat bow = stroke(4.4f, Fill::color(hex(0x211c14, 0.9f)));
    bow.trimStart = 0.0f;
    bow.trimEnd = 0.10f;
    bow.trimPhase = &bowPhase[fi];
    root.child(disc(c, kR)
                   .key(tag + "bow")
                   .corners({kR})
                   .fill(Fill::none())
                   .stroke(bow)
                   .opacity(&bowAlpha[fi])
                   .cache(Cache::None));

    // ---- the numeral, upper left of its circle (measured at
    // -0.80R, -1.04R from the centre, baseline-left) ----
    root.child(text(toU8(std::to_string(f.num) + "."),
                    type(faceNumeral, 37, kInk, 0.5f))
                   .key(tag + "num")
                   .absolute()
                   .centerAt({c.fX - 0.71f * kR, c.fY - 1.11f * kR})
                   .opacity(withFrom(0.0f, 1.0f,
                                     ramp(tNumeral * 1000 + (float)fi * 22.0f,
                                          360))));

    // ---- reference letters: upright, never rotated ----
    const std::vector<Label> &labels = labelsOf(f.num);
    for (size_t li = 0; li < labels.size(); ++li) {
      const Label &l = labels[li];
      root.child(
          text(toU8(l.glyph), type(faceLabel, 30, kInk))
              .key(tag + "lab" + std::to_string(li))
              .absolute()
              .centerAt(polar(c, kR * l.radius, l.bearing))
              .opacity(bind(&settle[fi]).from(0.86f, 1.06f).clamp(0.0f, 1.0f))
              .translateY(bind(&settle[fi])
                              .from(0.86f, 1.06f)
                              .map(ch::easeOutQuad)
                              .invert()
                              .to(0.0f, 7.0f)
                              .clamp(0.0f, 7.0f)));
    }
  }

  // ------------------------------------------------------------------
  Element describe(sketch::SketchContext &ctx) {
    auto root = stack().fill(Fill::color(kPaper));

    // ---- paper: fractal tone, foxing (biased lower-left, as the scan
    // is), a vignette that lands on the sampled scan colour ----
    root.child(box().absolute().inset(0).fill(paperMat).opacity(0.20f).blend(
        SkBlendMode::kSoftLight));
    root.child(box().absolute().inset(0).fill(foxing.material()));
    root.child(box()
                   .absolute()
                   .left(0)
                   .top(kH * 0.52f)
                   .width(kW * 0.55f)
                   .height(kH * 0.48f)
                   .fill(foxingLL.material()));
    root.child(box().absolute().inset(0).fill(radialGradient(
        {kW * 0.48f, kH * 0.44f}, kW * 0.86f,
        {hex(0x000000, 0.0f), hex(0x000000, 0.0f),
         SkColor4f{kPaperEdge.fR, kPaperEdge.fG, kPaperEdge.fB, 0.38f}},
        {0.0f, 0.55f, 1.0f})));

    // ---- the frame's double hairline ----
    for (int i = 0; i < 2; ++i) {
      const float g = (float)i * kRuleGap;
      root.child(box()
                     .absolute()
                     .left(kFrameL + g)
                     .top(kFrameT + g)
                     .width(kFrameR - kFrameL - 2 * g)
                     .height(kFrameB - kFrameT - 2 * g)
                     .key("frame" + std::to_string(i))
                     .fill(Fill::none())
                     .stroke(stroke(i == 0 ? 2.0f : 1.3f, Fill::color(kInkLine)))
                     .trim(0.0f, withFrom(0.0f, 1.0f,
                                          ramp(tFrame * 1000 + (float)i * 90,
                                               880, ch::easeOutQuint))));
    }

    // ---- "Tab. I.", swash italic, above the frame at the right ----
    GlyphFx pen;
    pen.effect = glyphfx::typeOn();
    pen.stagger = {.eachMs = 0, .amountMs = 520, .durationMs = 60};
    pen.progress =
        withFrom(0.0f, 1.0f, ramp(tTitle * 1000, 620, ch::easeNone));
    root.child(text(toU8("Tab. I."), type(faceSwash, 62, kInk, 1.0f))
                   .key("title")
                   .glyphFx(std::move(pen))
                   .absolute()
                   .centerAt({1372 * kScale, 108 * kScale}));

    // ---- the twelve figures ----
    for (size_t i = 0; i < kFigures.size(); ++i)
      figure(root, i, ctx);

    // ---- the sand: every grain in one pool, one atlas stamp ----
    root.child(box().absolute().inset(0).child(
        instancing::instances(atlas, pool, instancing::Mode::Live)));

    // ---- the engraver's signature, inside the frame at the foot ----
    root.child(text(toU8("Capieux. sculps. 1786."),
                    type(faceSwash, 27, kInkSoft, 0.3f))
                   .key("credit")
                   .absolute()
                   .centerAt({1352 * kScale, 1826 * kScale})
                   .opacity(withFrom(0.0f, 1.0f, ramp(tCredit * 1000, 700))));

    return root;
  }

  // ------------------------------------------------------------------
  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(kW, kH);
    ctx.background(kPaper);

    auto family = [&](const char *name, SkFontStyle st) -> sk_sp<SkTypeface> {
      if (!ctx.fonts || !ctx.fonts->fontManager())
        return nullptr;
      return ctx.fonts->fontManager()->matchFamilyStyle(name, st);
    };
    // The plate's numerals are a modern face with hairline serifs; the
    // reference letters are its italic; "Tab. I." and the credit are a
    // flourished engraver's chancery, a genuinely different hand.
    faceNumeral = family("Didot", SkFontStyle::Normal());
    faceLabel = family("Didot", SkFontStyle::Italic());
    faceSwash = family("Apple Chancery", SkFontStyle::Normal());
    if (!faceNumeral)
      faceNumeral = family("Bodoni 72", SkFontStyle::Normal());
    if (!faceLabel)
      faceLabel = family("Baskerville", SkFontStyle::Italic());
    if (!faceSwash)
      faceSwash = family("Snell Roundhand", SkFontStyle::Normal());
    if (!faceSwash)
      faceSwash = faceLabel;

    paperMat = patterns::grain(0.013f, 4, 9.0f);
    foxing = patterns::speckle(210, 5, 2.0f, 8.0f, {kFox});
    foxing.seed(17);
    foxingLL = patterns::speckle(170, 6, 2.5f, 11.0f, {kFox});
    foxingLL.seed(53);
    // Ink on rag paper is never flat: luminance noise, so it shades the
    // fill rather than hue-shifting it.
    inkMat = Material::blend({{Material::solid(kInk), SkBlendMode::kSrc},
                              {patterns::grain(0.09f, 3, 4.0f, 0.35f),
                               SkBlendMode::kSoftLight}});

    settleEase = ease::outBounce();

    // The grain atlas: three engraved marks, baked once, stamped ~5900x.
    atlas = std::make_shared<instancing::Atlas>(3.0f);
    atlas->cell(box().width(8.6f).height(2.3f).corners({1.15f}).fill(
                    Fill::color(hex(0x211c14, 0.94f))),
                {10, 4});
    atlas->cell(box().width(6.0f).height(1.7f).corners({0.85f}).fill(
                    Fill::color(hex(0x2c2519, 0.88f))),
                {8, 3});
    atlas->cell(box().width(3.1f).height(3.1f).corners({1.55f}).fill(
                    Fill::color(hex(0x211c14, 0.9f))),
                {5, 5});
    seedGrains();

    // ---- the simulation ------------------------------------------------
    // instancing::Pool has no per-instance tween, so the ~5900 little
    // timelines (start, target, delay, duration, ease) are bookkept here
    // and stepped by hand. See the report: this is the study's main gap.
    ctx.ticker.add([this](double dt) {
      now += dt;
      const float t = (float)now;

      // per-figure settle phase, and the idle bow pass afterwards
      for (size_t i = 0; i < kFigures.size(); ++i) {
        const float bowAt = tBow0 + tBowStep * (float)i;
        settle[i] = std::clamp((t - bowAt) / 1.42f, 0.0f, 1.0f);
        bowShake[i] = 0.0f;
        bowAlpha[i] = 0.0f;
      }
      if (t > tIdle) {
        constexpr float kStroke = 0.44f;
        const float u = std::fmod(t - tIdle, kStroke * 12.0f) / kStroke;
        const size_t active = std::min<size_t>((size_t)u, 11);
        const float local = u - (float)active;
        bowPhase[active] = local;
        bowAlpha[active] = std::sin(local * SK_FloatPI) * 0.8f;
        bowShake[active] = std::sin(local * SK_FloatPI) * 1.5f;
      } else {
        // the bow arrives with the grain migration itself
        for (size_t i = 0; i < kFigures.size(); ++i) {
          const float bowAt = tBow0 + tBowStep * (float)i;
          const float u = (t - bowAt + 0.22f) / 0.62f;
          if (u > 0 && u < 1) {
            bowPhase[i] = u;
            bowAlpha[i] = std::sin(u * SK_FloatPI) * 0.85f;
          }
        }
      }

      auto pos = pool->positions();
      auto rot = pool->rotations();
      auto scl = pool->scales();
      auto tint = pool->tints();
      for (size_t i = 0; i < grains.size(); ++i) {
        const Grain &g = grains[i];
        float u = (t - g.t0) / g.dur;
        float appear = 0.0f;
        if (t >= tScatter)
          appear = std::min(1.0f, (t - tScatter) / 0.40f);
        if (u < 0.0f)
          u = 0.0f;
        const bool moving = u > 0.0f && u < 1.0f;
        u = std::min(u, 1.0f);
        const float e = settleEase ? settleEase(u) : u;
        // sand does not slide, it hops: a decaying shiver across the walk,
        // plus a nudge whenever the bow is on this figure's rim
        const float amp =
            (1.0f - u) * (moving ? 5.6f : 1.5f) + bowShake[g.fig];
        const float sx = std::sin(t * 21.0f + g.phase) * amp;
        const float sy = std::cos(t * 17.0f + g.phase * 1.7f) * amp * 0.8f;
        pos[i] = {g.from.fX + (g.to.fX - g.from.fX) * e + sx,
                  g.from.fY + (g.to.fY - g.from.fY) * e + sy};
        rot[i] = g.rotFrom + (g.rotTo - g.rotFrom) * e;
        scl[i] = g.scale;
        tint[i] = {1, 1, 1, appear * g.alpha};
      }
      return true;
    });

    ctx.composer.render(describe(ctx));
  }

  void update(double, sketch::SketchContext &) override {}
};

SIGIL_SKETCH(ChladniTab1)
