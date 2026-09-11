#pragma once

// Construction data and drawing primitives owned by this study.

#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkTypeface.h>
#include <include/pathops/SkPathOps.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/Hatches.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/brush/Lines.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/Feed.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Placers.h>
#include <sigilcompose/kit/Plate.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/kit/Strokes.h>
#include <sigilcompose/testing/Checks.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Divisions.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilgeometry/path/Frame.h>
#include <sigilgeometry/path/Numeric.h>
#include <sigilgeometry/path/Projection.h>
#include <sigilgeometry/path/Skia.h>
#include <sigilmaterial/core/Bank.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/kit/Grained.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmeasure/check/Check.h>
#include <sigilmotion/Animation.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Heading.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/kit/Theme.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace arrange = sigil::geometry::arrange;
namespace sketch = sigil::sketch;
namespace field = sigil::material::field;
namespace mat = sigil::material;
namespace motion = sigil::motion;
namespace matkit = sigil::material::kit;
namespace measure = sigil::measure;
namespace patterns = sigil::material::pattern;
namespace path = sigil::geometry::path;
namespace shapes = sigil::geometry::shapes;
namespace weave = sigil::weave;

namespace skia = sigil::material::skia;

using namespace sigil::compose;
using namespace sigil::motion;
using sigil::material::skia::Paint;
using namespace std::chrono_literals;
namespace ch = choreograph;

namespace chaucer_astrolabe {}
using namespace chaucer_astrolabe;
namespace chaucer_astrolabe {

using sigil::compose::hexColor;  // 0xRRGGBB -> SkColor4f

// ---------------------------------------------------------------------------
// palette — luminance percentiles over the hue-masked brass of the 1466
// Strasbourg instrument. Brass has ONE colour and many lights; this is a
// ramp, not a set of hues, and that is the whole material problem here.

constexpr SkColor4f kCase = hexColor(0x1d222d);  // the vitrine, not the object
constexpr SkColor4f kBrassP2 = hexColor(0x4f360e);  // inside the rete's cutouts
constexpr SkColor4f kBrassP10 =
    hexColor(0x5c462f);  // plate seen through a cutout
constexpr SkColor4f kBrassP30 = hexColor(0x846a2d);  // the recessed plate
constexpr SkColor4f kBrassP50 = hexColor(0xa18643);  // the base tone
constexpr SkColor4f kBrassP70 = hexColor(0xc2a955);  // the rete's raised faces
constexpr SkColor4f kBrassP90 = hexColor(0xffdc8b);  // throne, top of the limb
constexpr SkColor4f kBrassP99 = hexColor(0xffffbd);  // rim highlights only
constexpr SkColor4f kGrooveDark = hexColor(0x3a2a10);
constexpr SkColor4f kGrooveLite = hexColor(0xf2dfa0);
constexpr SkColor4f kVerdigris = hexColor(0x2f5a44);
constexpr SkColor4f kVellum = hexColor(0xefe6d2);
constexpr SkColor4f kInk = hexColor(0x241c15);
constexpr SkColor4f kRubric = hexColor(0x8c2f22);
constexpr SkColor4f kTrace = hexColor(0x2f6f9c);  // the sketch talking, only

// ---------------------------------------------------------------------------
// canvas & the instrument's frame

constexpr float kW = 2400, kH = 1600;
constexpr float kCx = 622, kCy = 774;
constexpr float kR = 470.0f;            // the Tropic of Capricorn, in px
constexpr float kMaterR = 1.155f * kR;  // 542.85
constexpr float kD = 3.14159265358979f / 180.0f;

/** THE MATH FRAME AS A UNIT MAP: x right, y UP, one unit = the Tropic of
 *  Capricorn's radius. `yScale = -1` IS "y counts up the page", which is
 *  the whole of what an astrolabe's plate is drawn in — every declination,
 *  every almucantar centre and every star position on this sheet is a
 *  number in units of R, and negating each of them at the call site is
 *  what the flip is there to stop.
 *
 *  Two origins, one frame: the canvas, and the plate box's own corner. */
const path::Grid kMathCanvas{
    .scale = kR, .yScale = -1.0f, .origin = {kCx, kCy}};
const path::Grid kMathPlate{.scale = kR, .yScale = -1.0f, .origin = {kR, kR}};

inline SkPoint MC(float mx, float my) { return kMathCanvas.at({mx, my}); }
inline SkPoint PL(float mx, float my) { return kMathPlate.at({mx, my}); }

// ---------------------------------------------------------------------------
// The two constants, and everything that follows from them

constexpr float kEps = 23.0f + 50.0f / 60.0f;  // Chaucer I.17
constexpr float kPhi = 51.0f + 50.0f / 60.0f;  // Chaucer I.14
constexpr float kEpsTrue1326 = 23.526778f;     // IAU 1980 at T = -6.74 cy

const float kK = std::tan((90.0f - kEps) * 0.5f * kD);  // 0.65147737
const float kReq = kK;                                  // equator, in R
const float kRcan = kK * kK;                            // Cancer,  in R

/** THE PLATE, stated once: the sphere seen from the SOUTH POLE onto the
 *  plane of the equator, in units of the Tropic of Capricorn's radius,
 *  with the roll saying that right ascension is read off it as an ordinary
 *  angle from +x — which at a pole is a choice, since north is not a
 *  direction there. Every ring engraved below is this value and a circle
 *  it carries. */
const path::Projection kPlate{.scheme = path::Scheme::Stereographic,
                              .centre = {.lonDeg = 0, .latDeg = 90},
                              .scale = kReq * 0.5f,
                              .rollDeg = 90.0f};
/** The two directions the families of circles are struck about: the zenith
 *  of Oxford, on the meridian, and the pole of the ecliptic. */
const path::Spherical kZenith{.lonDeg = 90.0f, .latDeg = kPhi};
const path::Spherical kEclipticPole{.lonDeg = 270.0f, .latDeg = 90.0f - kEps};
const float kYzen = kPlate.at(kZenith).y;

/** A point at declination δ lands at radius R_eq·tan((90−δ)/2). */
inline float rOfDec(float decDeg) { return kPlate.radiusAt(90.0f - decDeg); }

inline SkPoint projRA(float decDeg, float raDeg) {
  return path::toSk(kPlate.at({.lonDeg = raDeg, .latDeg = decDeg}));
}
/** The same point read in HOUR ANGLE: ψ = 90° − H, so noon is at the top,
 *  WEST is +x and EAST is −x. East on the left is not a mistake: an
 *  astrolabe shows the sky from OUTSIDE the sphere. */
inline SkPoint proj(float decDeg, float hourAngleDeg) {
  return projRA(decDeg, 90.0f - hourAngleDeg);
}

// The horizon and the almucantars ("compowned by two and two", I.18): what
// stands an altitude h from the zenith. The ecliptic, internally tangent to
// both tropics, and Chaucer's ±6° of ecliptic latitude either side of it
// (I.21): what stands that far from the ecliptic's own pole.
inline float almCy(float h) {
  return kPlate.circleOf(kZenith, 90 - h)->centre.y;
}
inline float almR(float h) { return kPlate.circleOf(kZenith, 90 - h)->radius; }
inline float bandCy(float b) {
  return kPlate.circleOf(kEclipticPole, 90 - b)->centre.y;
}
inline float bandR(float b) {
  return kPlate.circleOf(kEclipticPole, 90 - b)->radius;
}
const float kEclCy = bandCy(0.0f);
const float kEclR = bandR(0.0f);

/** The azimuths: a coaxal family through the zenith and the nadir, each a
 *  great circle whose own pole stands on the horizon a quarter turn from
 *  it. A′ is measured from the PRIME VERTICAL, not from north — drawn from
 *  north the family misses by more than the plate's own radius, which the
 *  audit prints as a finding. */
inline path::PlaneCircle azimuth(float primeDeg) {
  return kPlate
      .circleOf(path::offsetFrom(kZenith, 180.0f - primeDeg, 90.0f), 90.0f)
      .value();
}

/** The semi-diurnal arc: how far west of the meridian a body of declination
 *  δ sets. 90° on the equator by definition, and that is invariant #3. */
inline float H0(float decDeg) {
  const float c = -std::tan(kPhi * kD) * std::tan(decDeg * kD);
  return std::acos(std::clamp(c, -1.0f, 1.0f)) / kD;
}
inline float seasonalStep(float decDeg) {
  return (360.0f - 2.0f * H0(decDeg)) / 12.0f;
}

/** The obliquity is one turn of the sphere about the line of the equinoxes.
 *  Right ascension comes back either side of the equinox rather than round
 *  from it, which is the reading the rete's rotation is measured in. */
const path::Rotation kFromEcliptic = path::Rotation::aboutX(kEps);
inline float sunDec(float lamDeg) { return kFromEcliptic({lamDeg, 0}).latDeg; }
inline float sunRA(float lamDeg) {
  return path::wrap(kFromEcliptic({lamDeg, 0}).lonDeg + 180.0f, 360.0f) -
         180.0f;
}

/** The k-th seasonal-hour line, as the medieval makers struck it: divide
 *  the below-horizon arc of each tropic into twelve, and swing a circle
 *  through the k-th division of Cancer, of the equator and of Capricorn.
 *
 *  It comes back with no circle when the three stand on one line — which
 *  is not a degenerate case to guard against but invariant #4: the sixth
 *  seasonal-hour line IS straight, because midnight is midnight at every
 *  declination. */
inline std::optional<path::PlaneCircle> seasonalLine(int k) {
  const SkPoint can = proj(kEps, H0(kEps) + k * seasonalStep(kEps));
  const SkPoint equ = proj(0.0f, H0(0.0f) + k * seasonalStep(0.0f));
  const SkPoint cap = proj(-kEps, H0(-kEps) + k * seasonalStep(-kEps));
  return path::circleThrough(path::fromSk(can), path::fromSk(equ),
                             path::fromSk(cap));
}

// ---------------------------------------------------------------------------
// The twelve zodiac cells. The projection is not uniform along the
// ecliptic ring, and this is the single most visible "computed, not drawn"
// feature of a real rete: Capricorn and Sagittarius are 2.26x wider on the
// ring than Cancer and Gemini.

const char* const kSigns[12] = {
    "ARIES", "TAVRVS",   "GEMINI",      "CANCER",      "LEO",      "VIRGO",
    "LIBRA", "SCORPIVS", "SAGITTARIVS", "CAPRICORNVS", "AQVARIVS", "PISCES"};

/** λ → the angle about the ECLIPTIC CIRCLE'S OWN centre. Getting this map
 *  wrong is what produces the gaps and overlaps test::coverage catches. */
inline float ringAngle(float lamDeg) {
  const SkPoint p = projRA(sunDec(lamDeg), sunRA(lamDeg));
  float a = std::atan2(p.fY - kEclCy, p.fX) / kD;
  return a < 0 ? a + 360.0f : a;
}

// The limb's 24 hour letters. RECONSTRUCTED, not Chaucer's: he reads
// the hour as a capital "X" and gets 9 a.m., but never says what the
// alphabet is. The medieval Latin alphabet without J, U and W is 23 letters
// with X in position 21; nine a.m. is 21 hours after noon; & was genuinely
// taught as a 24th letter. A lands on the first hour after noon, running
// clockwise at 15°/hour, and X falls at ψ = 135° — exactly where the
// geometry puts 9 a.m. A 1-in-23 coincidence otherwise.
const char* const kLetters[24] = {"A", "B", "C", "D", "E", "F", "G", "H",
                                  "I", "K", "L", "M", "N", "O", "P", "Q",
                                  "R", "S", "T", "V", "X", "Y", "Z", "&"};

// The rete's stars, precessed J2000 → 1326.0 by the IAU 1976 ζ/z/θ
// rotation (ζ = −4.3155°, z = −4.3055°, θ = −3.7543°; T = −6.74 cy). The
// whole sky has slid ~8.6° in right ascension since J2000, which is why a
// rete has a service life of a century or two before the stars need
// re-cutting. Alradif is Deneb, α Cyg — the Commons table on the redrawn
// Chaucer rete says δ Cephei, and the standard reading of al-ridf ("the
// follower") is Deneb; δ Cep is 4th magnitude and nobody puts it on a rete.
struct Star {
  const char* name;
  const char* modern;
  float ra1326, dec1326;
};
const std::array<Star, 12> kStars = {{
    {"ALKAB", "Hassaleh", 63.397f, 31.809f},
    {"ALNATH", "Elnath", 70.999f, 27.717f},
    {"ALGHVL", "Algol", 36.358f, 38.151f},
    {"ALHABOR", "Sirius", 93.767f, -16.224f},
    {"ALGOMEISA", "Procyon", 105.842f, 6.530f},
    {"K ALASAD", "Regulus", 142.986f, 15.135f},
    {"ALKAID", "Alkaid", 200.116f, 52.757f},
    {"ALRAMIH", "Arcturus", 206.007f, 22.436f},
    {"ALPHETA", "Alphecca", 226.578f, 29.123f},
    {"ALRADIF", "Deneb", 304.629f, 42.994f},
    {"MARKAB", "Markab", 337.846f, 11.634f},
    {"ALNASIR", "Alpheratz", 353.549f, 25.339f},
}};
// J2000, for the precession ghost: the same twelve, 674 years later.
const std::array<SkPoint, 12> kStars2000 = {{
    {74.248f, 33.166f},
    {81.573f, 28.608f},
    {47.042f, 40.956f},
    {101.287f, -16.716f},
    {114.826f, 5.225f},
    {152.093f, 11.967f},
    {206.885f, 49.313f},
    {213.915f, 19.182f},
    {233.672f, 26.715f},
    {310.358f, 45.280f},
    {346.190f, 15.205f},
    {2.097f, 29.090f},
}};

// Chaucer I.10 gives the month lengths himself, for the back's calendar ring.
const std::array<int, 12> kMonthDays = {31, 28, 31, 30, 31, 30,
                                        31, 31, 30, 31, 30, 31};
const char* const kMonths[12] = {"IAN", "FEB", "MAR", "APR", "MAI", "IVN",
                                 "IVL", "AVG", "SEP", "OCT", "NOV", "DEC"};

// ---------------------------------------------------------------------------
// the rete's skeleton — the graph the metal actually is
//
// A rete is ONE PIERCED SHEET. Every bar has to connect: a degree-1 endpoint
// that is not a star's tip is a spur, and a disconnected component is a piece
// that FALLS OUT OF THE INSTRUMENT. So the rete is built as a graph first and
// drawn from it second, and test::endpointDegrees audits the graph.

constexpr float kRingSk = 0.9790f;  // outer ring centreline (band .958–1.0)
constexpr float kBarW = 0.040f;     // I.21's bars, off MHS 45133
constexpr float kRingW = 0.042f;

enum class Host { Ring, Ecl, ArmN, ArmE, ArmS, ArmW };

struct Thorn {
  SkPoint root, tip, c1, c2;
  Host host;
  float rootParam;  // angle (deg) on a ring; distance in R along an arm
  int star;
};

// Two figure-local frames on the plate, in the MATH frame (0 = +x, angles
// increasing the way the plate's own longitudes do). Every other radius on
// this instrument is a projection and stays hand-written — rOfDec() is the
// artefact, not a coordinate system.
const path::Frame kEcliptic{
    .centre = {0, kEclCy}, .radius = kEclR, .zero = path::Zero::East};
const path::Frame kRing{
    .centre = {0, 0}, .radius = kRingSk, .zero = path::Zero::East};
/** The same convention read as ARC LENGTH: where a plate angle falls along
 *  `shapes::circle()`, which is the number ring typography rides on. */
const path::Frame kPlateAngles{.zero = path::Zero::East};

inline SkPoint eclPoint(float angDeg) { return kEcliptic.at(angDeg); }
inline SkPoint ringPoint(float angDeg) { return kRing.at(angDeg); }
inline SkPoint armEnd(Host h) {
  switch (h) {
    case Host::ArmN:
      return {0, kRingSk};
    case Host::ArmE:
      return {kRingSk, 0};
    case Host::ArmS:
      return {0, -kRingSk};
    default:
      return {-kRingSk, 0};
  }
}

struct Rete {
  std::vector<float> ringCuts;    // degrees, sorted
  std::vector<float> eclCuts;     // degrees about the ecliptic centre, sorted
  std::vector<float> armCuts[4];  // distance along each arm, sorted
  std::vector<Thorn> thorns;
};

/** Builds the graph: four arms out of the pin, the outer ring, the ecliptic
 *  ring, and twelve thorns each SPRINGING from its host tangentially (which
 *  is what a Gothic pointer does) and curving to the star. Every thorn root
 *  becomes a cut in its host, so the root is a genuine degree-3 junction
 *  rather than a spur welded onto the middle of a bar. */
inline Rete buildRete() {
  Rete r;
  // the cardinal junctions: pin, the ecliptic's four crossings, the rim
  const float ariesAng = ringAngle(0.0f);  // 23.833°, about kEclCy
  r.eclCuts = {90.0f, ariesAng, 180.0f - ariesAng, 270.0f};
  r.ringCuts = {0.0f, 90.0f, 180.0f, 270.0f};
  r.armCuts[0] = {0.0f, kRcan, kRingSk};  // N: pin, Cancer, rim
  r.armCuts[1] = {0.0f, kReq, kRingSk};   // E: pin, Aries, rim
  r.armCuts[2] = {0.0f, kRingSk};         // S: pin, rim
  r.armCuts[3] = {0.0f, kReq, kRingSk};   // W: pin, Libra, rim

  for (size_t i = 0; i < kStars.size(); ++i) {
    const SkPoint p = projRA(kStars[i].dec1326, kStars[i].ra1326);
    // nearest host
    Host host = Host::Ring;
    float best = std::abs(kRingSk - std::hypot(p.fX, p.fY));
    float param = std::atan2(p.fY, p.fX) / kD;
    const float de = std::abs(std::hypot(p.fX, p.fY - kEclCy) - kEclR);
    if (de < best) {
      host = Host::Ecl;
      best = de;
      param = std::atan2(p.fY - kEclCy, p.fX) / kD;
    }
    const Host arms[4] = {Host::ArmN, Host::ArmE, Host::ArmS, Host::ArmW};
    for (auto arm : arms) {
      const SkPoint e = armEnd(arm);
      const float len = kRingSk;
      const float t =
          std::clamp((p.fX * e.fX + p.fY * e.fY) / (len * len), 0.0f, 1.0f);
      const SkPoint q{e.fX * t, e.fY * t};
      const float d = std::hypot(p.fX - q.fX, p.fY - q.fY);
      if (d < best) {
        host = arm;
        best = d;
        param = t * len;
      }
    }
    // Sirius gets the outer ring whatever the arithmetic says: the 1326
    // maker gave it the biggest pointer on the instrument and a dog's head,
    // and reproducing that is the difference between "an astrolabe" and
    // "THIS astrolabe".
    if (i == 3) {
      host = Host::Ring;
      param = std::atan2(p.fY, p.fX) / kD;
    }

    // spring the root back along the host so even a star sitting ON its host
    // (Regulus is 0.002 R off the ecliptic — of course it is) gets a thorn
    const float springR = 0.155f;
    Thorn th;
    th.star = (int)i;
    th.host = host;
    th.tip = p;
    const float side = (i % 2 == 0) ? 1.0f : -1.0f;
    SkVector tangent{0, 0};
    if (host == Host::Ring) {
      const float d0 = param - side * springR / kRingSk / kD;
      th.rootParam = d0;
      th.root = ringPoint(d0);
      tangent = {-std::sin(d0 * kD) * side, std::cos(d0 * kD) * side};
    } else if (host == Host::Ecl) {
      const float d0 = param - side * springR / kEclR / kD;
      th.rootParam = d0;
      th.root = eclPoint(d0);
      tangent = {-std::sin(d0 * kD) * side, std::cos(d0 * kD) * side};
    } else {
      const SkPoint e = armEnd(host);
      const float d0 = std::clamp(param - springR, 0.10f, kRingSk - 0.04f);
      th.rootParam = d0;
      th.root = {e.fX * d0 / kRingSk, e.fY * d0 / kRingSk};
      tangent = {e.fX / kRingSk, e.fY / kRingSk};
    }
    const float L = std::hypot(th.tip.fX - th.root.fX, th.tip.fY - th.root.fY);
    const SkVector dir{(th.tip.fX - th.root.fX) / std::max(L, 1e-5f),
                       (th.tip.fY - th.root.fY) / std::max(L, 1e-5f)};
    th.c1 = {th.root.fX + tangent.fX * L * 0.62f,
             th.root.fY + tangent.fY * L * 0.62f};
    th.c2 = {th.tip.fX - dir.fX * L * 0.30f, th.tip.fY - dir.fY * L * 0.30f};
    r.thorns.push_back(th);

    if (host == Host::Ring)
      r.ringCuts.push_back(th.rootParam);
    else if (host == Host::Ecl)
      r.eclCuts.push_back(th.rootParam);
    else
      r.armCuts[(int)host - (int)Host::ArmN].push_back(th.rootParam);
  }
  auto norm = [](std::vector<float>& v) {
    for (float& a : v) a = std::fmod(std::fmod(a, 360.0f) + 360.0f, 360.0f);
    std::sort(v.begin(), v.end());
  };
  norm(r.ringCuts);
  norm(r.eclCuts);
  for (auto& v : r.armCuts) std::sort(v.begin(), v.end());
  return r;
}

// The skeleton as SkPaths in PLATE-LOCAL px — what endpointDegrees audits and
// what the bars are stroked from.
enum class Part { Arm, Ring, Ecl, Thorn };

struct Piece {
  SkPath path;
  Part kind = Part::Arm;
};

inline std::vector<Piece> retePieces(const Rete& r) {
  std::vector<Piece> out;
  auto arcOf = [&](float a0, float a1, bool ecliptic) {
    SkPathBuilder b;
    const int n = std::max(4, (int)(std::abs(a1 - a0) * 0.7f));
    for (int i = 0; i <= n; ++i) {
      const float a = arrange::along(a0, a1 - a0, (size_t)i, (size_t)n + 1,
                                     arrange::Turn::Open);
      const SkPoint m = ecliptic ? eclPoint(a) : ringPoint(a);
      const SkPoint p = PL(m.fX, m.fY);
      if (i == 0)
        b.moveTo(p);
      else
        b.lineTo(p);
    }
    out.push_back({b.detach(), ecliptic ? Part::Ecl : Part::Ring});
  };
  for (size_t i = 0; i < r.ringCuts.size(); ++i)
    arcOf(r.ringCuts[i],
          r.ringCuts[(i + 1) % r.ringCuts.size()] +
              (i + 1 == r.ringCuts.size() ? 360.0f : 0.0f),
          false);
  for (size_t i = 0; i < r.eclCuts.size(); ++i)
    arcOf(r.eclCuts[i],
          r.eclCuts[(i + 1) % r.eclCuts.size()] +
              (i + 1 == r.eclCuts.size() ? 360.0f : 0.0f),
          true);
  const Host arms[4] = {Host::ArmN, Host::ArmE, Host::ArmS, Host::ArmW};
  for (int a = 0; a < 4; ++a) {
    const SkPoint e = armEnd(arms[a]);
    for (size_t i = 0; i + 1 < r.armCuts[a].size(); ++i) {
      const float t0 = r.armCuts[a][i] / kRingSk,
                  t1 = r.armCuts[a][i + 1] / kRingSk;
      SkPathBuilder b;
      b.moveTo(PL(e.fX * t0, e.fY * t0));
      b.lineTo(PL(e.fX * t1, e.fY * t1));
      out.push_back({b.detach(), Part::Arm});
    }
  }
  // the Capricorn stub: the ecliptic reaches 1.0 R and the rim's centreline
  // is at 0.9825, which is exactly why on surviving retes the zodiac band
  // appears to FUSE with the outer ring near 0° Capricorn. Chaucer's stated
  // 12°-wide band would reach 1.1246 R — 12.5% outside the rete itself — so
  // the makers clipped it, and the fusion falls out for free.
  {
    SkPathBuilder b;
    b.moveTo(PL(0, -kRingSk));
    b.lineTo(PL(0, -1.0f));
    out.push_back({b.detach(), Part::Ecl});
  }
  for (const Thorn& t : r.thorns) {
    SkPathBuilder b;
    b.moveTo(PL(t.root.fX, t.root.fY));
    const SkPoint c1 = PL(t.c1.fX, t.c1.fY), c2 = PL(t.c2.fX, t.c2.fY),
                  tp = PL(t.tip.fX, t.tip.fY);
    b.cubicTo(c1, c2, tp);
    out.push_back({b.detach(), Part::Thorn});
  }
  return out;
}

// ---------------------------------------------------------------------------
// paint helpers

// A positional shorthand over the library's designated-init `textStyle()`:
// this plate has ONE type signature and ~140 call sites, and the library
// spells it as a designated-init aggregate precisely so a file like this can
// name its own four parameters over it.
inline weave::TextStyle type(sk_sp<SkTypeface> face, float size, SkColor4f c,
                             float tracking = 0) {
  return weave::textStyle(
      {.face = std::move(face), .size = size, .color = c, .track = tracking});
}

using motion::ramp;  // (startMs, durationMs) -> a Transition

/** THE ENGRAVED V-GROOVE, at this plate's two tones. An engraved line is
 *  not a stroke, it is a cut with a shadowed wall and a lit wall — a
 *  CROSS-SECTION — and `kit::groove` is that cut: a radial ramp concentric
 *  with the circle, so it is constant along the groove and varies across
 *  it. The alphas are how deep each family reads over the brass. */
inline PathFormat groove(float rad, float w, float darkA, float liteA) {
  return kit::groove(
      rad, w, SkColor4f{kGrooveDark.fR, kGrooveDark.fG, kGrooveDark.fB, darkA},
      SkColor4f{kGrooveLite.fR, kGrooveLite.fG, kGrooveLite.fB, liteA});
}

/** One engraved circle: centre and radius in R units of the math frame,
 *  positioned in the plate box. This function is called ~73 times, and each
 *  call is a distinct circle: every groove has its own centre and radius, so
 *  its gradient differs, and there is no instancing to be had here. */
inline Element cut(SkPoint mc, float mr, float w, float darkA, float liteA,
                   const std::string& key) {
  const float rad = mr * kR;
  return kit::disc(PL(mc.fX, mc.fY), rad)
      .key(key)
      .shape(shapes::circle())
      .fill(Fill::none())
      .stroke(groove(rad, w, darkA, liteA));
}

// ---------------------------------------------------------------------------
// The verification runs in DOUBLE. The plate is drawn in float because SkPoint
// is float, and the headline numbers of this study are 5.6e-16 R and
// 2.1e-14 degrees — quantities a float cannot hold an opinion about. So the
// projection exists twice: once for the canvas, once for the proof.

constexpr double kDD = 3.14159265358979323846 / 180.0;
const double kEpsD = 23.0 + 50.0 / 60.0;
const double kPhiD = 51.0 + 50.0 / 60.0;
// NOLINTBEGIN(bugprone-throwing-static-initialization): trigonometry of
// constants cannot throw
const double kReqD = std::tan((90.0 - kEpsD) * 0.5 * kDD);
const double kRcanD = kReqD * kReqD;
const double kEclCyD = (kRcanD - 1.0) * 0.5;
const double kEclRD = (kRcanD + 1.0) * 0.5;
const double kAzCyD = (kReqD * std::tan((90.0 - kPhiD) * 0.5 * kDD) -
                       kReqD * std::tan((90.0 + kPhiD) * 0.5 * kDD)) *
                      0.5;
const double kAzAD = (kReqD * std::tan((90.0 - kPhiD) * 0.5 * kDD) +
                      kReqD * std::tan((90.0 + kPhiD) * 0.5 * kDD)) *
                     0.5;
// NOLINTEND(bugprone-throwing-static-initialization)

inline double rOfDecD(double dec) {
  return kReqD * std::tan((90.0 - dec) * 0.5 * kDD);
}
struct P2 {
  double x = 0, y = 0;
};
inline P2 projD(double dec, double H) {
  const double psi = (90.0 - H) * kDD, r = rOfDecD(dec);
  return {r * std::cos(psi), r * std::sin(psi)};
}
inline double almCyD(double h) {
  return kReqD * std::cos(kPhiD * kDD) /
         (std::sin(kPhiD * kDD) + std::sin(h * kDD));
}
inline double almRD(double h) {
  return kReqD * std::cos(h * kDD) /
         (std::sin(kPhiD * kDD) + std::sin(h * kDD));
}
/** (A, h) → (δ, H): the honest route into the plate, which never touches a
 *  closed form and is therefore the only fair way to check one. */
inline void horizToEq(double A, double h, double* dec, double* H) {
  const double sd =
      std::sin(kPhiD * kDD) * std::sin(h * kDD) +
      std::cos(kPhiD * kDD) * std::cos(h * kDD) * std::cos(A * kDD);
  *dec = std::asin(std::clamp(sd, -1.0, 1.0)) / kDD;
  *H = std::atan2(
           -std::sin(A * kDD) * std::cos(h * kDD),
           std::sin(h * kDD) * std::cos(kPhiD * kDD) -
               std::cos(h * kDD) * std::sin(kPhiD * kDD) * std::cos(A * kDD)) /
       kDD;
}
struct FitD {
  double cx = 0, cy = 0, r = 0, res = 0;
};
inline FitD fitCircleD(const std::vector<P2>& p) {
  double m[3][4] = {};
  for (const P2& q : p) {
    const double w = q.x * q.x + q.y * q.y;
    const double row[3] = {2 * q.x, 2 * q.y, 1};
    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) m[i][j] += row[i] * row[j];
      m[i][3] += row[i] * w;
    }
  }
  for (int i = 0; i < 3; ++i) {
    int piv = i;
    for (int r2 = i + 1; r2 < 3; ++r2)
      if (std::abs(m[r2][i]) > std::abs(m[piv][i])) piv = r2;
    for (int j = 0; j < 4; ++j) std::swap(m[i][j], m[piv][j]);
    const double d = m[i][i];
    if (std::abs(d) < 1e-18) continue;
    for (int j = 0; j < 4; ++j) m[i][j] /= d;
    for (int r2 = 0; r2 < 3; ++r2) {
      if (r2 == i) continue;
      const double f = m[r2][i];
      for (int j = 0; j < 4; ++j) m[r2][j] -= f * m[i][j];
    }
  }
  FitD f;
  f.cx = m[0][3];
  f.cy = m[1][3];
  f.r = std::sqrt(std::max(0.0, m[2][3] + f.cx * f.cx + f.cy * f.cy));
  for (const P2& q : p)
    f.res = std::max(f.res, std::abs(std::hypot(q.x - f.cx, q.y - f.cy) - f.r));
  return f;
}

// --- the reading order, in seconds ------------------------------------------
constexpr float tGround = 0.00f;
constexpr float tMater = 0.15f;
constexpr float tTicks = 0.80f;
constexpr float tLetters = 1.05f;
constexpr float tTropics = 1.40f;
constexpr float tHorizon = 2.20f;
constexpr float tAlmu = 3.40f;
constexpr float tAzim = 5.20f;
constexpr float tHours = 6.20f;
constexpr float tRete = 7.40f;
constexpr float tPin = 9.00f;
constexpr float tDay = 10.00f;
constexpr float tYear = 18.00f;
constexpr float tChaucer = 21.00f;
// The declared still: tChaucer plus the 200 ms hold and the 900 ms swing the
// alidade takes to reach 25° 30′, which is the last motion in the state — so
// nothing on the canvas is moving at it.
constexpr float tStill = 22.10f;
constexpr float tLoop = 26.00f;

}  // namespace chaucer_astrolabe

// ===========================================================================
