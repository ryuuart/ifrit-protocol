// chaucer_astrolabe.cpp — a planispheric astrolabe of the English "Chaucer"
// type, computed from its own construction rule for Oxford, 51° 50′ N.
//
// TWO ARTEFACTS STAND BEHIND THIS, AND THEY ARE A RARE PAIRING — AN
// INSTRUMENT AND ITS OWN INSTRUCTION BOOK, BOTH SURVIVING.
//
//   The instrument. The "Chaucer astrolabe", British Museum 1909,0617.1,
//   dated 1326 — the earliest dated astrolabe made in Europe. Brass
//   (medieval latten), 132 mm diameter, mater ~10 mm thick, a Y-shaped rete
//   carrying 33 stars with a DOG'S HEAD as the pointer for Sirius (the
//   dog-star — a pun the maker could not resist) and birds for the rest. It
//   carries interchangeable plates for Oxford, Jerusalem, "Babilonie", Rome,
//   Montpellier and Paris. It was made sixteen years before Chaucer was born
//   and never belonged to him; it is called the Chaucer astrolabe because it
//   matches, feature for feature, the instrument he describes.
//
//   The manual. Geoffrey Chaucer, *A Treatise on the Astrolabe*, 1391,
//   written in Middle English for "litel Lowis my sone", then ten years old
//   — the first technical manual in English.
//   Skeat's text: en.wikisource.org/wiki/The_Complete_Works_of_Geoffrey_
//   Chaucer/Volume_3/A_Treatise_on_the_Astrolabe
//
// NOTHING HERE IS TRACED. An astrolabe is not a picture of the sky; it is
// the sky stereographically projected onto the plane of the equator from the
// SOUTH celestial pole, and every one of the ~73 curves on its face is a
// circle whose centre and radius follow in closed form from two numbers —
// the latitude φ and the obliquity ε. Both come from Chaucer himself:
//
//   φ = 51° 50′  (I.14, "the heyhte of owre pool Artik fro owre north
//                 Orisonte is 51 degrees & 50 Minutes")
//   ε = 23° 50′  (I.17, the declination of the Cercle of Cancer)
//
// AND ε IS WRONG BY A KNOWN AMOUNT. The true obliquity in 1326 was
// 23° 31.6′ (IAU 1980 at T = −6.74 cy). Chaucer's 23° 50′ is 18.4 arcminutes
// too large — an inherited Ptolemaic figure (Ptolemy's 11/83 of a circle is
// 23° 51′ 20″) that European makers were still cutting into brass a
// millennium later. The instrument is NOT approximate because it was crudely
// made. It is approximate because the number it was built on was 1200 years
// old. The sketch uses HIS ε and prints what that costs: the equator's radius
// is 0.586% small (−0.229 mm on the real 132 mm object), the Tropic of
// Cancer 1.175% (−0.299 mm).
//
// EVERY NUMBER IN THE BRIEF I WAS HANDED WAS RE-DERIVED HERE AND AGREED.
// The one thing worth flagging back: the published parameterisation of the
// azimuth family is measured from the PRIME VERTICAL, not from north
// (A′ = 90 − A). The sketch runs the check that catches it — each azimuth
// projected pointwise from (A, h) → (δ, H) → plate and least-squares
// circle-fitted — and prints the residual BOTH ways: ~1e-15 R with A′, and
// ~1.7 R with A. See the console, panel 2.
//
// THE HEADLINE VERIFICATION IS CHAUCER'S OWN WORKED EXAMPLE (II.3):
// 12 March 1391, sun's altitude 25° 30′. Intersecting the two DRAWN circles
// — the sun's declination circle and the almucantar for 25.5° — gives
// H = 46.550123682°. Spherical trigonometry gives the same to 2.1e-14°.
// That proves the drawn GEOMETRY encodes the trigonometry, not merely that
// the formulas agree with themselves. The instrument reads 08:53.8; Chaucer
// read "9 of the clokke"; the 6.2-minute residual is his, not the maths'.
//
// AND THE BEST NUMBER OF ALL: the seasonal-hour lines are a documented
// MEDIEVAL APPROXIMATION — a circle through three points, one on each
// tropic. The true locus is not a circle. Its worst error over the whole
// plate is 0.00374 R, at declination −15.9°, which on the real 132 mm
// instrument (R = 60 mm) is 0.225 mm — about ONE ENGRAVED LINE WIDTH. That
// is why nobody minded for a thousand years, and the sketch measures it
// itself (481 declinations sampled against the fitted circle).
//
// REFERENCE PHOTOGRAPHS were used for what brass looks like, how thick a
// rete bar is, and what a Gothic thorn is shaped like — nothing else:
//   MHS 45133 (Museum of the History of Science, Oxford) — an Early Gothic
//     rete alone against white: bar widths, quatrefoil, trefoil, central
//     rosette, the stepped equinoctial bar, star names engraved ALONG the
//     bars, thorn pointers.
//   Strasbourg astrolabe 1466 (Hessisches Landesmuseum Darmstadt) — the
//     assembled object, the throne and shackle, and the brass palette below
//     (luminance percentiles over ~180k hue-masked pixels).
//
// BUILT FROM (the library, not by hand):
//   shapes::circle()      ~73 plate circles + every ring, all engraved as
//                         V-GROOVES: a radial ramp centred on each circle's
//                         OWN centre is constant along the groove and varies
//                         across it — the one cross-section paint the
//                         library cannot otherwise express (ROADMAP §8b)
//   shapes::annulus/sector/arc/parametric/star   limb bands, shadow square,
//                         the projection ray, the rosette, the throne
//   TextPath::Orient::Radial   the 24 hour letters and the degree numerals,
//                         set as spokes — landed for this study
//   TextPath::Orient::Tangent  the 12 zodiac names and the 12 star names,
//                         running lettering, engraver's convention (no flip)
//   instancing::Pool::sizes()  the 360 limb ticks: ONE atlas cell, three
//                         LENGTHS through the non-uniform lane — also landed
//                         for this study, and it works here for exactly the
//                         reason it cannot work for the almucantars
//   bind()                one rete-rotation Output remapped at eight call
//                         sites; one sun-longitude Output at four
//   debug::coverage       the 12 zodiac cells tile the ecliptic ring
//   debug::endpointDegrees   the rete is ONE PIERCED SHEET: no spur, one
//                         component — a degree-1 endpoint that is not a
//                         thorn tip is a piece that falls out of the object
//   brushes::taper        the star thorns
//   console::LineRing     four panels of checks, printed as they run
//
// AND ONE CORRECTION FILED BACK, since it changes what the sketch means: the
// brief's own numbers for the ecliptic residual and Chaucer's example are
// 5.55e-16 R and 2.1e-14°, and a float cannot hold an opinion about either.
// The plate is DRAWN in float (SkPoint is float); the proof runs in a
// parallel double-precision projection (see kEpsD / projD / fitCircleD). With
// the float path the same checks read 3e-7 and 1e-5 — right, and worthless.
//
// Run:
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/chaucer_astrolabe.cpp \
//       --frame /tmp/chaucer_astrolabe.png --at 23.0
//
// 23.0 s is CHAUCER'S MOMENT: 12 March 1391, the sun's declination circle and
// the 25.5° almucantar both lit in #2f6f9c, their two intersections crossed,
// the label laid on the morning root and the limb letter X illuminated.
//   5.0 s  the plate half-built, 45 almucantars growing out of the zenith
//   8.2 s  the rete assembling — rim, bars, foils, the ecliptic ring
//  14.0 s  local noon, the rete turning: hour angle 0, letter & (24th)
//  20.4 s  THE YEAR: the rete HOLDS, the sun walks the ecliptic, and its
//          hour angle falls as its right ascension climbs
// 26 s loops.

#include <sigilsketch/Sketch.h>

#include <sigilweave/FontContext.h>

#include <sigilcompose/Brushes.h>
#include <sigilcompose/Console.h>
#include <sigilcompose/Debug.h>
#include <sigilcompose/Instances.h>
#include <sigilcompose/LayerStyles.h>
#include <sigilcompose/Lines.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Shapes.h>

#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkTypeface.h>
#include <include/pathops/SkPathOps.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
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
// palette — luminance percentiles over the hue-masked brass of the 1466
// Strasbourg instrument. Brass has ONE colour and many lights; this is a
// ramp, not a set of hues, and that is the whole material problem here.

constexpr SkColor4f kCase = hex(0x1d222d);      // the vitrine, not the object
constexpr SkColor4f kBrassP2 = hex(0x4f360e);   // inside the rete's cutouts
constexpr SkColor4f kBrassP10 = hex(0x5c462f);  // plate seen through a cutout
constexpr SkColor4f kBrassP30 = hex(0x846a2d);  // the recessed plate
constexpr SkColor4f kBrassP50 = hex(0xa18643);  // the base tone
constexpr SkColor4f kBrassP70 = hex(0xc2a955);  // the rete's raised faces
constexpr SkColor4f kBrassP90 = hex(0xffdc8b);  // throne, top of the limb
constexpr SkColor4f kBrassP99 = hex(0xffffbd);  // rim highlights only
constexpr SkColor4f kGrooveDark = hex(0x3a2a10);
constexpr SkColor4f kGrooveLite = hex(0xf2dfa0);
constexpr SkColor4f kVerdigris = hex(0x2f5a44);
constexpr SkColor4f kVellum = hex(0xefe6d2);
constexpr SkColor4f kInk = hex(0x241c15);
constexpr SkColor4f kRubric = hex(0x8c2f22);
constexpr SkColor4f kTrace = hex(0x2f6f9c);     // the sketch talking, only

// ---------------------------------------------------------------------------
// canvas & the instrument's frame

constexpr float kW = 2400, kH = 1600;
constexpr float kCx = 622, kCy = 774;
constexpr float kR = 470.0f;              // the Tropic of Capricorn, in px
constexpr float kMaterR = 1.155f * kR;    // 542.85
constexpr float kD = 3.14159265358979f / 180.0f;

// math frame (x right, y up, R units) -> canvas px
SkPoint MC(float mx, float my) { return {kCx + mx * kR, kCy - my * kR}; }
// math frame -> the plate box's local px (origin at kC - (R, R))
SkPoint PL(float mx, float my) { return {(mx + 1.0f) * kR, (1.0f - my) * kR}; }

// ---------------------------------------------------------------------------
// §5.1 — the two constants, and everything that follows from them

constexpr float kEps = 23.0f + 50.0f / 60.0f;   // Chaucer I.17
constexpr float kPhi = 51.0f + 50.0f / 60.0f;   // Chaucer I.14
constexpr float kEpsTrue1326 = 23.526778f;      // IAU 1980 at T = -6.74 cy

const float kK = std::tan((90.0f - kEps) * 0.5f * kD);  // 0.65147737
const float kReq = kK;                                  // equator, in R
const float kRcan = kK * kK;                            // Cancer,  in R

/** §5.2 — the projection, stated once: a point at declination δ lands at
 *  radius R_eq·tan((90−δ)/2), at a plate angle equal to its right ascension.
 *  Everything else on this plate is this one line plus trigonometry. */
float rOfDec(float decDeg) {
  return kReq * std::tan((90.0f - decDeg) * 0.5f * kD);
}
/** The raw point formula, in the math frame. ψ = 90° − H, so noon is at the
 *  top, WEST is +x and EAST is −x. East on the left is not a mistake: an
 *  astrolabe shows the sky from OUTSIDE the sphere. */
SkPoint proj(float decDeg, float hourAngleDeg) {
  const float psi = (90.0f - hourAngleDeg) * kD;
  const float r = rOfDec(decDeg);
  return {r * std::cos(psi), r * std::sin(psi)};
}
SkPoint projRA(float decDeg, float raDeg) {
  const float r = rOfDec(decDeg);
  return {r * std::cos(raDeg * kD), r * std::sin(raDeg * kD)};
}

// §5.4 the horizon; §5.5 the almucantars ("compowned by two and two", I.18)
float almCy(float h) {
  return kReq * std::cos(kPhi * kD) / (std::sin(kPhi * kD) + std::sin(h * kD));
}
float almR(float h) {
  return kReq * std::cos(h * kD) / (std::sin(kPhi * kD) + std::sin(h * kD));
}
// §5.6 the azimuths: a coaxal family through the zenith and the nadir
const float kYzen = kReq * std::tan((90.0f - kPhi) * 0.5f * kD);
const float kYnad = -kReq * std::tan((90.0f + kPhi) * 0.5f * kD);
const float kAzCy = (kYzen + kYnad) * 0.5f;
const float kAzA = (kYzen - kYnad) * 0.5f;
// §5.8 the ecliptic, internally tangent to both tropics
const float kEclCy = (kRcan - 1.0f) * 0.5f;
const float kEclR = (kRcan + 1.0f) * 0.5f;
// the zodiac band, Chaucer's ±6° of ecliptic latitude (I.21)
float bandCy(float beta) {
  return (rOfDec(kEps + beta) - rOfDec(-(kEps - beta))) * 0.5f;
}
float bandR(float beta) {
  return (rOfDec(kEps + beta) + rOfDec(-(kEps - beta))) * 0.5f;
}

/** The semi-diurnal arc: how far west of the meridian a body of declination
 *  δ sets. 90° on the equator by definition, and that is invariant #3. */
float H0(float decDeg) {
  const float c = -std::tan(kPhi * kD) * std::tan(decDeg * kD);
  return std::acos(std::clamp(c, -1.0f, 1.0f)) / kD;
}
float seasonalStep(float decDeg) { return (360.0f - 2.0f * H0(decDeg)) / 12.0f; }

float sunDec(float lamDeg) {
  return std::asin(std::sin(kEps * kD) * std::sin(lamDeg * kD)) / kD;
}
float sunRA(float lamDeg) {
  return std::atan2(std::cos(kEps * kD) * std::sin(lamDeg * kD),
                    std::cos(lamDeg * kD)) /
         kD;
}

/** Circle through three points; null when they are collinear — which is not
 *  a degenerate case to guard against but invariant #4: the sixth
 *  seasonal-hour line IS straight, because midnight is midnight at every
 *  declination. */
struct Circ {
  float cx = 0, cy = 0, r = 0;
  bool ok = false;
};
Circ through3(SkPoint a, SkPoint b, SkPoint c) {
  const float d = 2 * (a.fX * (b.fY - c.fY) + b.fX * (c.fY - a.fY) +
                       c.fX * (a.fY - b.fY));
  if (std::abs(d) < 1e-9f)
    return {};
  const float aa = a.fX * a.fX + a.fY * a.fY, bb = b.fX * b.fX + b.fY * b.fY,
              cc = c.fX * c.fX + c.fY * c.fY;
  Circ out;
  out.cx = (aa * (b.fY - c.fY) + bb * (c.fY - a.fY) + cc * (a.fY - b.fY)) / d;
  out.cy = (aa * (c.fX - b.fX) + bb * (a.fX - c.fX) + cc * (b.fX - a.fX)) / d;
  out.r = std::hypot(a.fX - out.cx, a.fY - out.cy);
  out.ok = true;
  return out;
}

/** §5.7 — the k-th seasonal-hour line, as the medieval makers struck it:
 *  divide the below-horizon arc of each tropic into twelve, and swing a
 *  circle through the k-th division of Cancer, of the equator and of
 *  Capricorn. */
Circ seasonalLine(int k) {
  const SkPoint can = proj(kEps, H0(kEps) + k * seasonalStep(kEps));
  const SkPoint equ = proj(0.0f, H0(0.0f) + k * seasonalStep(0.0f));
  const SkPoint cap = proj(-kEps, H0(-kEps) + k * seasonalStep(-kEps));
  return through3(can, equ, cap);
}

// ---------------------------------------------------------------------------
// §6.1 — the twelve zodiac cells. The projection is not uniform along the
// ecliptic ring, and this is the single most visible "computed, not drawn"
// feature of a real rete: Capricorn and Sagittarius are 2.26x wider on the
// ring than Cancer and Gemini.

const char *const kSigns[12] = {"ARIES",   "TAVRVS",  "GEMINI",     "CANCER",
                                "LEO",     "VIRGO",   "LIBRA",      "SCORPIVS",
                                "SAGITTARIVS", "CAPRICORNVS", "AQVARIVS",
                                "PISCES"};

/** λ → the angle about the ECLIPTIC CIRCLE'S OWN centre. Getting this map
 *  wrong is what produces the gaps and overlaps debug::coverage catches. */
float ringAngle(float lamDeg) {
  const SkPoint p = projRA(sunDec(lamDeg), sunRA(lamDeg));
  float a = std::atan2(p.fY - kEclCy, p.fX) / kD;
  return a < 0 ? a + 360.0f : a;
}

// §6.3 — the limb's 24 hour letters. RECONSTRUCTED, not Chaucer's: he reads
// the hour as a capital "X" and gets 9 a.m., but never says what the
// alphabet is. The medieval Latin alphabet without J, U and W is 23 letters
// with X in position 21; nine a.m. is 21 hours after noon; & was genuinely
// taught as a 24th letter. A lands on the first hour after noon, running
// clockwise at 15°/hour, and X falls at ψ = 135° — exactly where the
// geometry puts 9 a.m. A 1-in-23 coincidence otherwise.
const char *const kLetters[24] = {"A", "B", "C", "D", "E", "F", "G", "H",
                                  "I", "K", "L", "M", "N", "O", "P", "Q",
                                  "R", "S", "T", "V", "X", "Y", "Z", "&"};

// §6.2 — the rete's stars, precessed J2000 → 1326.0 by the IAU 1976 ζ/z/θ
// rotation (ζ = −4.3155°, z = −4.3055°, θ = −3.7543°; T = −6.74 cy). The
// whole sky has slid ~8.6° in right ascension since J2000, which is why a
// rete has a service life of a century or two before the stars need
// re-cutting. Alradif is Deneb, α Cyg — the Commons table on the redrawn
// Chaucer rete says δ Cephei, and the standard reading of al-ridf ("the
// follower") is Deneb; δ Cep is 4th magnitude and nobody puts it on a rete.
struct Star {
  const char *name;
  const char *modern;
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
    {74.248f, 33.166f},  {81.573f, 28.608f},  {47.042f, 40.956f},
    {101.287f, -16.716f}, {114.826f, 5.225f}, {152.093f, 11.967f},
    {206.885f, 49.313f}, {213.915f, 19.182f}, {233.672f, 26.715f},
    {310.358f, 45.280f}, {346.190f, 15.205f}, {2.097f, 29.090f},
}};

// Chaucer I.10 gives the month lengths himself, for the back's calendar ring.
const std::array<int, 12> kMonthDays = {31, 28, 31, 30, 31, 30,
                                        31, 31, 30, 31, 30, 31};
const char *const kMonths[12] = {"IAN", "FEB", "MAR", "APR", "MAI", "IVN",
                                 "IVL", "AVG", "SEP", "OCT", "NOV", "DEC"};

// ---------------------------------------------------------------------------
// the rete's skeleton — the graph the metal actually is
//
// A rete is ONE PIERCED SHEET. Every bar has to connect: a degree-1 endpoint
// that is not a star's tip is a spur, and a disconnected component is a piece
// that FALLS OUT OF THE INSTRUMENT. So the rete is built as a graph first and
// drawn from it second, and debug::endpointDegrees audits the graph.

constexpr float kRingSk = 0.9790f;   // outer ring centreline (band .958–1.0)
constexpr float kBarW = 0.040f;      // I.21's bars, off MHS 45133
constexpr float kRingW = 0.042f;

enum class Host { Ring, Ecl, ArmN, ArmE, ArmS, ArmW };

struct Thorn {
  SkPoint root, tip, c1, c2;
  Host host;
  float rootParam;  // angle (deg) on a ring; distance in R along an arm
  int star;
};

SkPoint eclPoint(float angDeg) {
  return {kEclR * std::cos(angDeg * kD), kEclCy + kEclR * std::sin(angDeg * kD)};
}
SkPoint ringPoint(float angDeg) {
  return {kRingSk * std::cos(angDeg * kD), kRingSk * std::sin(angDeg * kD)};
}
SkPoint armEnd(Host h) {
  switch (h) {
  case Host::ArmN: return {0, kRingSk};
  case Host::ArmE: return {kRingSk, 0};
  case Host::ArmS: return {0, -kRingSk};
  default: return {-kRingSk, 0};
  }
}

struct Rete {
  std::vector<float> ringCuts;   // degrees, sorted
  std::vector<float> eclCuts;    // degrees about the ecliptic centre, sorted
  std::vector<float> armCuts[4]; // distance along each arm, sorted
  std::vector<Thorn> thorns;
};

/** Builds the graph: four arms out of the pin, the outer ring, the ecliptic
 *  ring, and twelve thorns each SPRINGING from its host tangentially (which
 *  is what a Gothic pointer does) and curving to the star. Every thorn root
 *  becomes a cut in its host, so the root is a genuine degree-3 junction
 *  rather than a spur welded onto the middle of a bar. */
Rete buildRete() {
  Rete r;
  // the cardinal junctions: pin, the ecliptic's four crossings, the rim
  const float ariesAng = ringAngle(0.0f);     // 23.833°, about kEclCy
  r.eclCuts = {90.0f, ariesAng, 180.0f - ariesAng, 270.0f};
  r.ringCuts = {0.0f, 90.0f, 180.0f, 270.0f};
  r.armCuts[0] = {0.0f, kRcan, kRingSk};      // N: pin, Cancer, rim
  r.armCuts[1] = {0.0f, kReq, kRingSk};       // E: pin, Aries, rim
  r.armCuts[2] = {0.0f, kRingSk};             // S: pin, rim
  r.armCuts[3] = {0.0f, kReq, kRingSk};       // W: pin, Libra, rim

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
    for (int a = 0; a < 4; ++a) {
      const SkPoint e = armEnd(arms[a]);
      const float len = kRingSk;
      const float t =
          std::clamp((p.fX * e.fX + p.fY * e.fY) / (len * len), 0.0f, 1.0f);
      const SkPoint q{e.fX * t, e.fY * t};
      const float d = std::hypot(p.fX - q.fX, p.fY - q.fY);
      if (d < best) {
        host = arms[a];
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
  auto norm = [](std::vector<float> &v) {
    for (float &a : v)
      a = std::fmod(std::fmod(a, 360.0f) + 360.0f, 360.0f);
    std::sort(v.begin(), v.end());
  };
  norm(r.ringCuts);
  norm(r.eclCuts);
  for (auto &v : r.armCuts)
    std::sort(v.begin(), v.end());
  return r;
}

// The skeleton as SkPaths in PLATE-LOCAL px — what endpointDegrees audits and
// what the bars are stroked from.
enum class Part { Arm, Ring, Ecl, Thorn };

struct Piece {
  SkPath path;
  Part kind = Part::Arm;
};

std::vector<Piece> retePieces(const Rete &r) {
  std::vector<Piece> out;
  auto arcOf = [&](float a0, float a1, bool ecliptic) {
    SkPathBuilder b;
    const int n = std::max(4, (int)(std::abs(a1 - a0) * 0.7f));
    for (int i = 0; i <= n; ++i) {
      const float a = a0 + (a1 - a0) * (float)i / (float)n;
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
      const float t0 = r.armCuts[a][i] / kRingSk, t1 = r.armCuts[a][i + 1] / kRingSk;
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
  for (const Thorn &t : r.thorns) {
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

sigil::weave::TextStyle type(sk_sp<SkTypeface> face, float size, SkColor4f c,
                            float tracking = 0) {
  sigil::weave::TextStyle s;
  s.shaping.typeface = std::move(face);
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = tracking;
  s.paint.foreground.setColor4f(c, nullptr);
  s.paint.foreground.setAntiAlias(true);
  return s;
}

Transition ramp(float delayMs, float durMs, ch::EaseFn e = ch::easeOutQuad) {
  Transition t;
  t.duration = std::chrono::milliseconds((int)durMs);
  t.delay = std::chrono::milliseconds((int)delayMs);
  t.ease = std::move(e);
  return t;
}

/** THE ENGRAVED V-GROOVE. An engraved line is not a stroke, it is a cut with
 *  a shadowed wall and a lit wall — a CROSS-SECTION, which is the one paint
 *  the library cannot express for a stroke (ROADMAP §8b). It can here, and
 *  only here, for the reason the Penrose study found: every line on this
 *  plate IS a circle, so a radial ramp centred on that circle's own centre
 *  is constant ALONG the groove and varies ACROSS it. The trick has now
 *  saved two studies and is still a trick. */
Fill grooveFill(float rad, float w, float darkA, float liteA) {
  const float g = rad + w;
  const float a = (rad - w * 0.5f) / g, b = (rad + w * 0.5f) / g;
  const float m = (a + b) * 0.5f, e = (b - a) * 0.22f;
  return radialGradient(
      {rad, rad}, g,
      {SkColor4f{kGrooveDark.fR, kGrooveDark.fG, kGrooveDark.fB, darkA},
       SkColor4f{kGrooveDark.fR, kGrooveDark.fG, kGrooveDark.fB, darkA},
       SkColor4f{kGrooveLite.fR, kGrooveLite.fG, kGrooveLite.fB, liteA},
       SkColor4f{kGrooveLite.fR, kGrooveLite.fG, kGrooveLite.fB, liteA}},
      {0.0f, m - e, m + e, 1.0f});
}

/** One engraved circle: centre and radius in R units of the math frame,
 *  positioned in the plate box. This function is called ~73 times, and that
 *  IS the point — see the report on why instancing cannot help. */
Element cut(SkPoint mc, float mr, float w, float darkA, float liteA,
            const std::string &key) {
  const float rad = mr * kR;
  return disc(PL(mc.fX, mc.fY), rad)
      .key(key)
      .outline(shapes::circle())
      .fill(Fill::none())
      .stroke(PathFormat{.width = w, .strokeFill = grooveFill(rad, w, darkA, liteA)});
}

template <typename... A> std::string fmt(const char *f, A... args) {
  char buf[512];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"
  std::snprintf(buf, sizeof(buf), f, args...);
#pragma clang diagnostic pop
  return buf;
}

// ---------------------------------------------------------------------------
// The verification runs in DOUBLE. The plate is drawn in float because SkPoint
// is float, and the headline numbers of this study are 5.6e-16 R and
// 2.1e-14 degrees — quantities a float cannot hold an opinion about. So the
// projection exists twice: once for the canvas, once for the proof.

constexpr double kDD = 3.14159265358979323846 / 180.0;
const double kEpsD = 23.0 + 50.0 / 60.0;
const double kPhiD = 51.0 + 50.0 / 60.0;
const double kReqD = std::tan((90.0 - kEpsD) * 0.5 * kDD);
const double kRcanD = kReqD * kReqD;
const double kEclCyD = (kRcanD - 1.0) * 0.5;
const double kEclRD = (kRcanD + 1.0) * 0.5;
const double kAzCyD =
    (kReqD * std::tan((90.0 - kPhiD) * 0.5 * kDD) -
     kReqD * std::tan((90.0 + kPhiD) * 0.5 * kDD)) *
    0.5;
const double kAzAD = (kReqD * std::tan((90.0 - kPhiD) * 0.5 * kDD) +
                      kReqD * std::tan((90.0 + kPhiD) * 0.5 * kDD)) *
                     0.5;

double rOfDecD(double dec) { return kReqD * std::tan((90.0 - dec) * 0.5 * kDD); }
struct P2 {
  double x = 0, y = 0;
};
P2 projD(double dec, double H) {
  const double psi = (90.0 - H) * kDD, r = rOfDecD(dec);
  return {r * std::cos(psi), r * std::sin(psi)};
}
double almCyD(double h) {
  return kReqD * std::cos(kPhiD * kDD) /
         (std::sin(kPhiD * kDD) + std::sin(h * kDD));
}
double almRD(double h) {
  return kReqD * std::cos(h * kDD) /
         (std::sin(kPhiD * kDD) + std::sin(h * kDD));
}
/** (A, h) → (δ, H): the honest route into the plate, which never touches a
 *  closed form and is therefore the only fair way to check one. */
void horizToEq(double A, double h, double *dec, double *H) {
  const double sd = std::sin(kPhiD * kDD) * std::sin(h * kDD) +
                    std::cos(kPhiD * kDD) * std::cos(h * kDD) * std::cos(A * kDD);
  *dec = std::asin(std::clamp(sd, -1.0, 1.0)) / kDD;
  *H = std::atan2(-std::sin(A * kDD) * std::cos(h * kDD),
                  std::sin(h * kDD) * std::cos(kPhiD * kDD) -
                      std::cos(h * kDD) * std::sin(kPhiD * kDD) *
                          std::cos(A * kDD)) /
       kDD;
}
struct FitD {
  double cx = 0, cy = 0, r = 0, res = 0;
};
FitD fitCircleD(const std::vector<P2> &p) {
  double m[3][4] = {};
  for (const P2 &q : p) {
    const double w = q.x * q.x + q.y * q.y;
    const double row[3] = {2 * q.x, 2 * q.y, 1};
    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j)
        m[i][j] += row[i] * row[j];
      m[i][3] += row[i] * w;
    }
  }
  for (int i = 0; i < 3; ++i) {
    int piv = i;
    for (int r2 = i + 1; r2 < 3; ++r2)
      if (std::abs(m[r2][i]) > std::abs(m[piv][i]))
        piv = r2;
    for (int j = 0; j < 4; ++j)
      std::swap(m[i][j], m[piv][j]);
    const double d = m[i][i];
    if (std::abs(d) < 1e-18)
      continue;
    for (int j = 0; j < 4; ++j)
      m[i][j] /= d;
    for (int r2 = 0; r2 < 3; ++r2) {
      if (r2 == i)
        continue;
      const double f = m[r2][i];
      for (int j = 0; j < 4; ++j)
        m[r2][j] -= f * m[i][j];
    }
  }
  FitD f;
  f.cx = m[0][3];
  f.cy = m[1][3];
  f.r = std::sqrt(std::max(0.0, m[2][3] + f.cx * f.cx + f.cy * f.cy));
  for (const P2 &q : p)
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
constexpr float tLoop = 26.00f;

} // namespace

// ===========================================================================

struct ChaucerAstrolabe : sigil::compose::sketch::Sketch {
  // ONE Output for the rete's rotation, remapped at every call site with
  // bind(): the rete's rotate(), the label's angle, the sun mark's position,
  // the readouts and the highlighted letter are all the same number in
  // different units. That is precisely the case bind() shipped for.
  ch::Output<float> reteRot{0};
  ch::Output<float> hourAngle{-46.550124f};
  ch::Output<float> sunLam{1.0f};
  ch::Output<float> sunAlt{25.5f};
  ch::Output<float> projDec{0};
  ch::Output<float> trace{0};      // the construction overlay's opacity
  std::array<ch::Output<float>, 24> letterGlow;
  ch::Output<float> signMark{0};

  double now = 0;
  int nightHour = 0;
  float latHours = 8.8967f;

  Rete rete;
  std::vector<Piece> pieces;
  std::shared_ptr<instancing::Atlas> tickAtlas;
  std::shared_ptr<instancing::Pool> tickPool;
  Material brassGrain, vellumGrain;
  Pattern verdigris;
  sk_sp<SkTypeface> faceEngrave, faceLimb, faceSerif, faceItalic, faceMono,
      faceBold;

  console::LineRing logA{64}, logB{64}, logC{64}, logD{64};
  std::string chaucerH, chaucerA, chaucerDelta;

  // =========================================================================
  // brass: one light across the whole instrument
  //
  // Material is NODE-LOCAL, and this object is ~200 nodes of one piece of
  // metal. The ramp below is authored in CANVAS px and converted into each
  // node's unit square by hand — ROADMAP §10c's Material::worldSpace(), from
  // a case where the "world" is one OBJECT rather than the canvas.
  /** Brass has ONE colour and many lights. This samples the sampled
   *  percentile ladder as a continuous curve, so "level" means "how lit is
   *  this face" and a single number places every surface on the object on
   *  the same ramp. */
  static SkColor4f brassRamp(float u) {
    static const SkColor4f c[7] = {kBrassP2,  kBrassP10, kBrassP30, kBrassP50,
                                   kBrassP70, kBrassP90, kBrassP99};
    u = std::clamp(u, 0.0f, 1.0f) * 6.0f;
    int i = (int)u;
    float f = u - (float)i;
    if (i >= 6) {
      i = 5;
      f = 1;
    }
    return {c[i].fR + (c[i + 1].fR - c[i].fR) * f,
            c[i].fG + (c[i + 1].fG - c[i].fG) * f,
            c[i].fB + (c[i + 1].fB - c[i].fB) * f, 1};
  }
  Material brass(SkRect r, float level = 0.5f) const {
    auto u = [&](SkPoint p) {
      return SkPoint{(p.fX - r.left()) / std::max(r.width(), 1.0f),
                     (p.fY - r.top()) / std::max(r.height(), 1.0f)};
    };
    return Material::linearUnit(
        u({kCx - kMaterR * 0.95f, kCy + kMaterR * 0.95f}),
        u({kCx + kMaterR * 0.85f, kCy - kMaterR * 1.05f}),
        {{0.0f, brassRamp(level - 0.11f)},
         {0.44f, brassRamp(level)},
         {0.80f, brassRamp(level + 0.07f)},
         {1.0f, brassRamp(level + 0.13f)}});
  }
  Material brassDisc(SkPoint c, float rad, float level = 0.5f) const {
    return brass(SkRect::MakeLTRB(c.fX - rad, c.fY - rad, c.fX + rad, c.fY + rad),
                 level);
  }
  /** The same ramp as a kernel Fill, for the PathFormat legs that dress the
   *  rete's bars — a stroke takes a Fill, not a Material, and a Fill is in
   *  node-local PIXELS, so the same hand conversion happens twice. */
  Fill brassStroke(SkRect r, float level = 0.5f) const {
    auto p = [&](SkPoint q) {
      return SkPoint{q.fX - r.left(), q.fY - r.top()};
    };
    return linearGradient(
        p({kCx - kMaterR * 0.95f, kCy + kMaterR * 0.95f}),
        p({kCx + kMaterR * 0.85f, kCy - kMaterR * 1.05f}),
        {brassRamp(level - 0.11f), brassRamp(level), brassRamp(level + 0.07f),
         brassRamp(level + 0.13f)},
        {0.0f, 0.44f, 0.80f, 1.0f});
  }

  // =========================================================================
  // THE PLATE — "compowned after the latitude of Oxenford"

  Element plate() {
    auto g = box()
                 .absolute()
                 .left(kCx - kR)
                 .top(kCy - kR)
                 .width(2 * kR)
                 .height(2 * kR)
                 .key("plate")
                 .outline(shapes::circle())
                 .clip(true)
                 .fill(brassDisc({kCx, kCy}, kR, 0.255f));

    // Wear: brass handled for 700 years is bright on the high edges and dark
    // in the cuts, and an unpolished latten greens in its recesses first. A
    // very low-amplitude speckle, only over the engraved field.
    g.child(box()
                .absolute()
                .inset(0)
                .key("verdigris")
                .outline(shapes::circle())
                .fill(verdigris.material())
                .opacity(withFrom(0.0f, 1.0f, ramp(tTropics * 1000, 900))));

    // the recess: the plate sits one millimetre below the limb
    g.child(box()
                .absolute()
                .inset(0)
                .key("recess")
                .outline(shapes::circle())
                .fill(Fill::none())
                .foreground(styles::InnerShadow{hex(0x2a1d08, 0.55f),
                                                     {0, 3}, 9}));

    // --- the twilight arc, crepusculum, h = -18 ---------------------------
    {
      PathFormat dotted{.width = 1.5f,
                        .strokeFill = Fill::color(hex(0x3a2a10, 0.42f)),
                        .dashIntervals = {3.0f, 5.0f}};
      g.child(disc(PL(0, almCy(-18.0f)), almR(-18.0f) * kR)
                  .key("twilight")
                  .outline(shapes::circle())
                  .fill(Fill::none())
                  .stroke(dotted)
                  .opacity(withFrom(0.0f, 1.0f, ramp(tHorizon * 1000 + 700, 700))));
    }

    // --- 45 almucantars, "compowned by two and two" (I.18) ----------------
    // They grow out of the zenith one every 38 ms. This is the sketch's most
    // beautiful three seconds and it is not rushed.
    for (int i = 1; i <= 44; ++i) {
      const float h = (float)(i * 2);
      const bool five = (i % 5) == 0;
      g.child(cut({0, almCy(h)}, almR(h), five ? 1.7f : 1.3f,
                  five ? 0.62f : 0.44f, five ? 0.30f : 0.20f,
                  "alm" + std::to_string(i))
                  .opacity(withFrom(
                      0.0f, 1.0f,
                      ramp(tAlmu * 1000 + (float)(44 - i) * 38.0f, 520,
                           ch::easeOutQuint))));
    }

    // --- 12 azimuth curves, clipped to the visible sky --------------------
    // Every azimuth passes through BOTH the zenith and the nadir — a coaxal
    // family through two fixed points. They are clipped to inside the horizon
    // AND inside the Capricorn disc, which is an intersection, so a nested
    // clip() is exactly right. (The seasonal hours below are the case where
    // it is not.)
    {
      const SkPoint hc = PL(0, almCy(0.0f));
      const float hr = almR(0.0f) * kR;
      auto sky = box()
                     .absolute()
                     .left(hc.fX - hr)
                     .top(hc.fY - hr)
                     .width(2 * hr)
                     .height(2 * hr)
                     .key("sky")
                     .outline(shapes::circle())
                     .clip(true);
      auto local = [&](float mx, float my) {
        const SkPoint p = PL(mx, my);
        return SkPoint{p.fX - (hc.fX - hr), p.fY - (hc.fY - hr)};
      };
      for (int i = 1; i <= 5; ++i) {
        const float ap = (float)(i * 15);            // A' from the PRIME
        const float cx = kAzA * std::tan(ap * kD);   // VERTICAL, not north
        const float rr = kAzA / std::cos(ap * kD);
        const float delay = tAzim * 1000 + (float)i * 150.0f;
        for (int s = -1; s <= 1; s += 2) {
          const float rad = rr * kR;
          sky.child(disc(local(s * cx, kAzCy), rad)
                        .key("az" + std::to_string(i * s))
                        .outline(shapes::circle())
                        .fill(Fill::none())
                        .stroke(PathFormat{
                            .width = 1.3f,
                            .strokeFill = grooveFill(rad, 1.3f, 0.42f, 0.20f)})
                        .opacity(withFrom(0.0f, 1.0f, ramp(delay, 460))));
        }
      }
      // the prime vertical (A = 90/270): a circle centred on the axis, and it
      // passes through the east and west points by the identity
      // tan(45−φ/2)·tan(45+φ/2) = 1
      {
        const float rad = kAzA * kR;
        sky.child(disc(local(0, kAzCy), rad)
                      .key("azPV")
                      .outline(shapes::circle())
                      .fill(Fill::none())
                      .stroke(PathFormat{
                          .width = 1.7f,
                          .strokeFill = grooveFill(rad, 1.7f, 0.55f, 0.26f)})
                      .opacity(withFrom(0.0f, 1.0f, ramp(tAzim * 1000, 460))));
      }
      g.child(std::move(sky));
    }

    // --- the meridian: STRAIGHT, and that is the projection showing -------
    // A = 0/180 is the one vertical circle through BOTH celestial poles,
    // including the south pole, which is the eye of the projection — and any
    // circle through the eye projects to a line.
    g.child(box()
                .absolute()
                .left(kR - 1.0f)
                .top(0)
                .width(2.0f)
                .height(2 * kR)
                .key("meridian")
                .fill(Fill::color(hex(0x3a2a10, 0.55f)))
                .trim(0.0f, withFrom(0.0f, 1.0f, ramp(tAzim * 1000 + 820, 520))));

    // --- 12 unequal-hour lines, "twelve devisiouns embelif" (I.20) --------
    // THE REGION IS A SET DIFFERENCE: these live only inside Capricorn AND
    // OUTSIDE the horizon. clip() intersects and nesting intersects more;
    // there is no clipOut() and no shapes::subtract, so the region is built
    // with a raw SkPathOp below the Compose seam. See the report.
    {
      const SkPoint hc = PL(0, almCy(0.0f));
      const float hr = almR(0.0f) * kR;
      SkPathBuilder cb, hb;
      cb.addOval(SkRect::MakeWH(2 * kR, 2 * kR));
      hb.addOval(SkRect::MakeLTRB(hc.fX - hr, hc.fY - hr, hc.fX + hr,
                                  hc.fY + hr));
      const SkPath capDisc = cb.detach(), horDisc = hb.detach();
      SkPath region;
      Op(capDisc, horDisc, kDifference_SkPathOp, &region);
      auto night = box()
                       .absolute()
                       .inset(0)
                       .key("night")
                       .outline([region](SkSize) { return region; })
                       .clip(true);
      for (int k = 1; k <= 11; ++k) {
        const Circ c = seasonalLine(k);
        const float delay =
            tHours * 1000 + (float)std::abs(k - 6) * 105.0f;
        if (!c.ok) // k = 6 is straight: midnight is midnight at every dec
          continue;
        const float rad = c.r * kR;
        night.child(disc(PL(c.cx, c.cy), rad)
                        .key("hr" + std::to_string(k))
                        .outline(shapes::circle())
                        .fill(Fill::none())
                        .stroke(PathFormat{
                            .width = 1.5f,
                            .strokeFill = grooveFill(rad, 1.5f, 0.50f, 0.24f)})
                        .opacity(withFrom(0.0f, 1.0f, ramp(delay, 480))));
      }
      // k = 6, the straight one
      night.child(box()
                      .absolute()
                      .left(kR - 0.8f)
                      .top(kR)
                      .width(1.6f)
                      .height(kR)
                      .key("hr6")
                      .fill(Fill::color(hex(0x3a2a10, 0.5f)))
                      .opacity(withFrom(0.0f, 1.0f, ramp(tHours * 1000, 480))));
      g.child(std::move(night));
    }

    // --- the horizon, h = 0: heavier than its 44 siblings ------------------
    g.child(cut({0, almCy(0.0f)}, almR(0.0f), 3.4f, 0.90f, 0.50f, "horizon")
                .trim(0.0f, withFrom(0.0f, 1.0f,
                                     ramp(tHorizon * 1000, 900, ch::easeOutQuint))));

    // --- the three tropics: Capricorn : equator : Cancer = 1 : k : k² -----
    const float trop[3] = {1.0f, kReq, kRcan};
    for (int i = 0; i < 3; ++i)
      g.child(cut({0, 0}, trop[i], i == 0 ? 3.0f : 2.4f, 0.80f, 0.46f,
                  "trop" + std::to_string(i))
                  .trim(0.0f, withFrom(0.0f, 1.0f,
                                       ramp(tTropics * 1000 + (float)i * 200,
                                            760, ch::easeOutQuint))));

    // --- the east and west points, where the horizon meets the equator ----
    // Exact invariant: √(R_eq² + R_eq²/tan²φ) = R_eq/sin φ, to the last bit
    // of a double. The cheapest real check on the whole instrument.
    for (int s = -1; s <= 1; s += 2)
      g.child(disc(PL(s * kReq, 0), 5.0f)
                  .key(std::string("ew") + (s < 0 ? "E" : "W"))
                  .outline(shapes::circle())
                  .fill(Fill::color(kTrace))
                  .opacity(bind(&trace).scale(0.9f).clamp(0, 1)));

    // --- the zenith ------------------------------------------------------
    g.child(disc(PL(0, kYzen), 3.6f)
                .key("zenith")
                .outline(shapes::circle())
                .fill(Fill::color(hex(0x3a2a10, 0.85f)))
                .opacity(withFrom(0.0f, 1.0f, ramp(tAlmu * 1000, 400))));

    return g;
  }

  // =========================================================================
  // THE CONSTRUCTION OVERLAY — the only saturated thing on the object, and
  // it reads as a line drawn ON the brass, never engraved into it.

  Element construction() {
    auto g = box()
                 .absolute()
                 .left(kCx - kR)
                 .top(kCy - kR)
                 .width(2 * kR)
                 .height(2 * kR)
                 .key("trace")
                 .outline(shapes::circle())
                 .clip(true)
                 .opacity(&trace);

    const float lam = 1.0f, h = 25.5f;
    const float dec = sunDec(lam);
    const float rs = rOfDec(dec);
    const float cy = almCy(h), ra = almR(h);

    // the sun's declination circle
    g.child(disc(PL(0, 0), rs * kR)
                .key("tdec")
                .outline(shapes::circle())
                .fill(Fill::none())
                .stroke(stroke(2.0f, Fill::color(hex(0x2f6f9c, 0.85f)))));
    // the almucantar for the measured altitude
    g.child(disc(PL(0, cy), ra * kR)
                .key("talm")
                .outline(shapes::circle())
                .fill(Fill::none())
                .stroke(stroke(2.0f, Fill::color(hex(0x2f6f9c, 0.85f)))));

    // their intersection — the hour angle, read off the DRAWN geometry
    const float y = (rs * rs - ra * ra + cy * cy) / (2 * cy);
    const float x = std::sqrt(std::max(0.0f, rs * rs - y * y));
    for (int s = -1; s <= 1; s += 2) {
      const SkPoint p = PL(s * x, y);
      g.child(disc(p, 23.0f)
                  .key(std::string("xr") + (s < 0 ? "a" : "b"))
                  .outline(shapes::circle())
                  .fill(Fill::none())
                  .stroke(stroke(1.8f, Fill::color(hex(0x2f6f9c, 0.9f)))));
      g.child(box()
                  .absolute()
                  .left(p.fX - 34)
                  .top(p.fY - 1.4f)
                  .width(68)
                  .height(2.8f)
                  .key(std::string("xh") + (s < 0 ? "a" : "b"))
                  .fill(Fill::color(kTrace)));
      g.child(box()
                  .absolute()
                  .left(p.fX - 1.4f)
                  .top(p.fY - 34)
                  .width(2.8f)
                  .height(68)
                  .key(std::string("xv") + (s < 0 ? "a" : "b"))
                  .fill(Fill::color(kTrace)));
    }
    return g;
  }

  // =========================================================================
  // The rete is a PHYSICALLY RAISED, PIERCED SHEET, so it casts onto the
  // plate — and the plate's engraving is visibly darker and lower through
  // its cutouts. This is the most convincing single cue that the thing is an
  // object and not a drawing, and it costs one duplicate geometry pass: the
  // same skeleton, flat dark, in a wrapper that rotates WITH the rete and is
  // then displaced in the parent's frame (a shadow offset must not turn with
  // the object).

  Element reteShadow() {
    auto inner = box()
                     .absolute()
                     .inset(0)
                     .rotate(&reteRot)
                     .transformOrigin(0.5f, 0.5f);
    const Fill dark = Fill::color(hex(0x140e04, 0.50f));
    for (size_t i = 0; i < pieces.size(); ++i) {
      const Piece &p = pieces[i];
      if (p.kind == Part::Ecl)
        continue;  // the zodiac band below IS the ecliptic's body
      SkRect bb = p.path.getBounds();
      const float w = (p.kind == Part::Arm    ? kBarW * kR
                       : p.kind == Part::Ring ? kRingW * kR
                                              : 0.030f * kR) +
                      3.0f;
      bb.outset(w, w);
      const SkPath local =
          p.path.makeTransform(SkMatrix::Translate(-bb.left(), -bb.top()));
      auto n = box()
                   .absolute()
                   .left(bb.left())
                   .top(bb.top())
                   .width(bb.width())
                   .height(bb.height())
                   .key("sh" + std::to_string(i))
                   .outline([local](SkSize) { return local; })
                   .fill(Fill::none());
      if (p.kind == Part::Thorn)
        n.foreground(brushes::taper(w, 3.0f, dark));
      else
        n.foreground(PathFormat{.width = w, .strokeFill = dark});
      inner.child(std::move(n));
    }
    {
      const float cyO = bandCy(-6.0f), rO = bandR(-6.0f);
      const float cyI = bandCy(6.0f), rI = bandR(6.0f);
      const SkPoint c = PL(0, cyO), ci = PL(0, cyI);
      const float ro = rO * kR, ri = rI * kR;
      inner.child(box()
                      .absolute()
                      .left(c.fX - ro)
                      .top(c.fY - ro)
                      .width(2 * ro)
                      .height(2 * ro)
                      .key("shband")
                      .outline([ro, ri, c, ci](SkSize) {
                        SkPathBuilder b;
                        b.setFillType(SkPathFillType::kEvenOdd);
                        b.addOval(SkRect::MakeWH(2 * ro, 2 * ro));
                        b.addOval(SkRect::MakeLTRB(
                            ci.fX - c.fX + ro - ri, ci.fY - c.fY + ro - ri,
                            ci.fX - c.fX + ro + ri, ci.fY - c.fY + ro + ri));
                        return b.detach();
                      })
                      .fill(dark));
    }
    return box()
        .absolute()
        .left(kCx - kR)
        .top(kCy - kR)
        .width(2 * kR)
        .height(2 * kR)
        .key("reteshadow")
        .translateX(7.0f)
        .translateY(9.0f)
        .opacity(withFrom(0.0f, 1.0f, ramp(tRete * 1000 + 200, 900)))
        .child(std::move(inner));
  }

  // =========================================================================
  // THE RETE — "shapen in maner of a net" (I.21), a single pierced sheet

  Element reteGroup() {
    auto g = box()
                 .absolute()
                 .left(kCx - kR)
                 .top(kCy - kR)
                 .width(2 * kR)
                 .height(2 * kR)
                 .key("rete")
                 .rotate(&reteRot)
                 .transformOrigin(0.5f, 0.5f);

    const SkRect rect =
        SkRect::MakeLTRB(kCx - kR, kCy - kR, kCx + kR, kCy + kR);
    const Material bandMat = brass(rect, 0.64f);

    // --- the zodiac band: Chaucer's ±6° of ecliptic latitude --------------
    // Clipped to the Capricorn disc, so it merges with the outer ring near
    // 0° Capricorn — which is what surviving retes actually look like.
    {
      const float cyO = bandCy(-6.0f), rO = bandR(-6.0f);
      const float cyI = bandCy(6.0f), rI = bandR(6.0f);
      auto clipped = box()
                         .absolute()
                         .inset(0)
                         .key("bandclip")
                         .outline(shapes::circle())
                         .clip(true);
      const SkPoint c = PL(0, cyO);
      const float ro = rO * kR, ri = rI * kR;
      const SkPoint ci = PL(0, cyI);
      clipped.child(
          box()
              .absolute()
              .left(c.fX - ro)
              .top(c.fY - ro)
              .width(2 * ro)
              .height(2 * ro)
              .key("band")
              .outline([ro, ri, c, ci](SkSize) {
                SkPathBuilder b;
                b.setFillType(SkPathFillType::kEvenOdd);
                b.addOval(SkRect::MakeWH(2 * ro, 2 * ro));
                b.addOval(SkRect::MakeLTRB(ci.fX - c.fX + ro - ri,
                                           ci.fY - c.fY + ro - ri,
                                           ci.fX - c.fX + ro + ri,
                                           ci.fY - c.fY + ro + ri));
                return b.detach();
              })
              .fill(bandMat)
              .foreground(styles::BevelEmboss{
                  .depth = 2, .size = 3, .angleDeg = 125,
                  .highlight = hex(0xffe9b0, 0.55f),
                  .shadow = hex(0x2a1d08, 0.5f)})
              .opacity(withFrom(0.0f, 1.0f, ramp(tRete * 1000 + 620, 700))));

      // the 360 degree divisions INSIDE the signs, at the projection's own
      // non-uniform spacing — they visibly bunch toward Cancer, and that is
      // the projection made legible
      for (int d = 0; d < 360; ++d) {
        if (d % 30 == 0)
          continue;
        const float a = ringAngle((float)d);
        const float inner = (d % 5 == 0) ? 0.955f : 0.972f;
        const SkPoint p0 = eclPoint(a), c0{0, kEclCy};
        const SkPoint q0{c0.fX + (p0.fX - c0.fX) * inner,
                         c0.fY + (p0.fY - c0.fY) * inner};
        const SkPoint A = PL(p0.fX, p0.fY), B = PL(q0.fX, q0.fY);
        SkPathBuilder pb;
        pb.moveTo(A);
        pb.lineTo(B);
        const SkPath seg = pb.detach();
        SkRect bb = seg.getBounds();
        bb.outset(2, 2);
        clipped.child(
            box()
                .absolute()
                .left(bb.left())
                .top(bb.top())
                .width(bb.width())
                .height(bb.height())
                .key("zd" + std::to_string(d))
                .outline([seg, bb](SkSize) {
                  return seg.makeTransform(
                      SkMatrix::Translate(-bb.left(), -bb.top()));
                })
                .fill(Fill::none())
                .stroke(stroke((d % 5 == 0) ? 1.2f : 0.8f,
                               Fill::color(hex(0x4a3410, 0.62f))))
                .opacity(withFrom(0.0f, 1.0f,
                                  ramp(tRete * 1000 + 900 + (float)d * 0.6f, 300))));
      }

      // the band's own two edges, engraved: β = +6° and β = −6°, Chaucer's
      // "latitude of twelve degrees" (I.21). The outer edge reaches 1.1246 R
      // — 12.5% OUTSIDE the rete — so it is clipped, and the band appears to
      // fuse with the outer ring near 0° Capricorn. That fusion is not a
      // stylisation; it is what the stated width forces.
      clipped.child(cut({0, cyI}, rI, 1.8f, 0.62f, 0.34f, "bandin"));
      clipped.child(cut({0, cyO}, rO, 1.8f, 0.62f, 0.34f, "bandout"));

      // the ecliptic line itself, engraved down the middle of the band
      clipped.child(cut({0, kEclCy}, kEclR, 2.0f, 0.72f, 0.36f, "eclline")
                        .trim(0.0f, withFrom(0.0f, 1.0f,
                                             ramp(tRete * 1000 + 700, 1100,
                                                  ch::easeOutQuint))));

      // the twelve sign names, tangential — running lettering, engraver's
      // convention, no autoFlip. Each is centred at its OWN cell midpoint,
      // and the cells are not equal.
      const float circ = 2.0f * SK_FloatPI * kEclR * kR;
      for (int i = 0; i < 12; ++i) {
        const float a0 = ringAngle((float)(i * 30));
        float a1 = ringAngle((float)((i + 1) * 30));
        if (a1 < a0)
          a1 += 360.0f;
        const float mid = (a0 + a1) * 0.5f;
        const float span = (a1 - a0) * kD * kEclR * kR;  // px of ring
        // shapes::circle() on this box parameterises clockwise from +x in
        // canvas coords, so the ring angle maps to 1 − a/360.
        const float f = std::fmod(1.0f - mid / 360.0f + 1.0f, 1.0f);
        const float size =
            std::min(0.052f * kR, span / (float)std::strlen(kSigns[i]) * 1.62f);
        clipped.child(
            text(toU8(kSigns[i]),
                 type(faceEngrave, size, hex(0x33240c, 0.88f), size * 0.055f))
                .absolute()
                .width(Dim(2 * kEclR * kR))
                .height(Dim(2 * kEclR * kR))
                .centerAt(PL(0, kEclCy))
                .key("sign" + std::to_string(i))
                .onPath(TextPath{.path = shapes::circle(),
                                 .at = f,
                                 .align = TextPath::Align::Center,
                                 .offset = -0.030f * kR,
                                 .autoFlip = false,
                                 .orient = TextPath::Orient::Tangent})
                .opacity(withFrom(0.0f, 1.0f,
                                  ramp(tRete * 1000 + 1200 + (float)i * 45, 400))));
      }
      g.child(std::move(clipped));
    }

    // --- the bars and the outer ring, stroked from the SKELETON -----------
    // The graph endpointDegrees audits is the same graph the metal is cut
    // from, which is the only way the check means anything.
    static const Part kOrder[3] = {Part::Arm, Part::Ring, Part::Thorn};
    for (const Part want : kOrder)
    for (size_t i = 0; i < pieces.size(); ++i) {
      const Piece &p = pieces[i];
      if (p.kind != want)
        continue;
      SkRect bb = p.path.getBounds();
      const float w = p.kind == Part::Arm    ? kBarW * kR
                      : p.kind == Part::Ring ? kRingW * kR
                                             : 0.034f * kR;
      bb.outset(w + 4, w + 4);
      const SkPath local =
          p.path.makeTransform(SkMatrix::Translate(-bb.left(), -bb.top()));
      const float delay = tRete * 1000 + (p.kind == Part::Thorn
                                              ? 1500.0f + (float)i * 26.0f
                                              : 120.0f);

      auto node = box()
                      .absolute()
                      .left(bb.left())
                      .top(bb.top())
                      .width(bb.width())
                      .height(bb.height())
                      .key("bar" + std::to_string(i))
                      .outline([local](SkSize) { return local; })
                      .fill(Fill::none());
      if (p.kind == Part::Thorn) {
        // a Gothic thorn: springs tangentially off its host and tapers to a
        // point. THE TIP IS THE STAR'S POSITION — the thorn is drawn so its
        // point lands on the computed (r, α), not so its centroid does.
        node.foreground(brushes::taper(0.032f * kR + 2.0f, 2.4f,
                                       Fill::color(hex(0x3d2b0c, 0.85f))))
            .foreground(brushes::taper(0.032f * kR, 1.0f, brassStroke(bb, 0.66f)))
            .opacity(withFrom(0.0f, 1.0f, ramp(delay, 420, ease::outBack())));
      } else {
        // a milled edge on both sides of every bar: the keyline is what makes
        // the sheet read as a sheet rather than as a stroke on a diagram
        node.foreground(PathFormat{.width = w + 2.4f,
                                   .strokeFill = Fill::color(hex(0x2a1d08, 0.7f))})
            .foreground(PathFormat{.width = w,
                                   .strokeFill = brassStroke(bb, 0.66f)})
            .foreground(PathFormat{.width = w * 0.30f,
                                   .strokeFill = Fill::color(hex(0xffedc0, 0.30f))})
            .trim(0.0f, withFrom(0.0f, 1.0f, ramp(delay, 900, ch::easeOutQuint)));
      }
      g.child(std::move(node));
    }

    // --- the star names, engraved ALONG the outer ring -------------------
    for (size_t i = 0; i < kStars.size(); ++i) {
      const SkPoint p = projRA(kStars[i].dec1326, kStars[i].ra1326);
      const float a = std::atan2(p.fY, p.fX) / kD;
      const float f = std::fmod(1.0f - a / 360.0f + 1.0f, 1.0f);
      g.child(text(toU8(kStars[i].name),
                   type(faceEngrave, 0.026f * kR, hex(0x33240c, 0.82f), 0.4f))
                  .absolute()
                  .width(Dim(2 * kR * (1.0f - kRingW * 0.5f)))
                  .height(Dim(2 * kR * (1.0f - kRingW * 0.5f)))
                  .centerAt(PL(0, 0))
                  .key("sname" + std::to_string(i))
                  .onPath(TextPath{.path = shapes::circle(),
                                   .at = f,
                                   .align = TextPath::Align::Center,
                                   .offset = 0.0f,
                                   .autoFlip = false,
                                   .orient = TextPath::Orient::Tangent})
                  .opacity(withFrom(0.0f, 1.0f,
                                    ramp(tRete * 1000 + 1600 + (float)i * 90, 400))));
    }

    // --- the star tips, and the dog's head for Sirius --------------------
    for (size_t i = 0; i < kStars.size(); ++i) {
      const SkPoint p = projRA(kStars[i].dec1326, kStars[i].ra1326);
      const SkPoint c = PL(p.fX, p.fY);
      const float delay = tRete * 1000 + 1500 + (float)i * 90;
      if (i == 3) {
        // ALHABOR — the dog-star. The 1326 maker gave it a dog's head, and
        // it is the joke he built into the object.
        g.child(disc(c, 0.030f * kR)
                    .key("dog")
                    .outline(shapes::blob(7u, 0.30f, 7))
                    .fill(Fill::color(kBrassP70))
                    .foreground(styles::BevelEmboss{
                        .depth = 2, .size = 2, .angleDeg = 125,
                        .highlight = hex(0xffe9b0, 0.6f),
                        .shadow = hex(0x2a1d08, 0.55f)})
                    .rotate(-40.0f)
                    .opacity(withFrom(0.0f, 1.0f, ramp(delay, 420))));
        // the ear and the muzzle
        g.child(disc({c.fX - 0.020f * kR, c.fY - 0.020f * kR}, 0.011f * kR)
                    .key("dogear")
                    .outline(shapes::polygon(3, 20))
                    .fill(Fill::color(kBrassP70))
                    .opacity(withFrom(0.0f, 1.0f, ramp(delay, 420))));
        g.child(disc(c, 3.0f)
                    .key("dogeye")
                    .outline(shapes::circle())
                    .fill(Fill::color(hex(0x2a1d08, 0.8f)))
                    .opacity(withFrom(0.0f, 1.0f, ramp(delay + 120, 300))));
      } else {
        g.child(disc(c, 4.2f)
                    .key("tip" + std::to_string(i))
                    .outline(shapes::circle())
                    .fill(Fill::color(kBrassP90))
                    .foreground(stroke(1.0f, Fill::color(hex(0x2a1d08, 0.6f))))
                    .opacity(withFrom(0.0f, 1.0f, ramp(delay, 380))));
      }
    }

    // --- PRECESSION IS VISIBLE ON THIS INSTRVMENT ------------------------
    // A faint ghost of each pointer at its J2000 position beside its 1326
    // one. The ~8.6° fan is why a rete has a service life of a century or two
    // before the stars have to be re-cut — and it is the reason the star list
    // above had to be precessed at all rather than looked up.
    for (size_t i = 0; i < kStars.size(); ++i) {
      const SkPoint now1326 = projRA(kStars[i].dec1326, kStars[i].ra1326);
      const SkPoint j2000 = projRA(kStars2000[i].fY, kStars2000[i].fX);
      const SkPoint a = PL(now1326.fX, now1326.fY), b = PL(j2000.fX, j2000.fY);
      SkPathBuilder pb;
      pb.moveTo(a);
      pb.lineTo(b);
      SkPath seg = pb.detach();
      SkRect bb = seg.getBounds();
      bb.outset(3, 3);
      const SkPath local =
          seg.makeTransform(SkMatrix::Translate(-bb.left(), -bb.top()));
      g.child(box()
                  .absolute()
                  .left(bb.left())
                  .top(bb.top())
                  .width(bb.width())
                  .height(bb.height())
                  .key("prec" + std::to_string(i))
                  .outline([local](SkSize) { return local; })
                  .fill(Fill::none())
                  .stroke(PathFormat{.width = 1.0f,
                                     .strokeFill = Fill::color(hex(0x2a1d08, 0.4f)),
                                     .dashIntervals = {2.5f, 3.5f}})
                  .opacity(withFrom(0.0f, 1.0f,
                                    ramp(tRete * 1000 + 2400 + (float)i * 30, 500))));
      g.child(disc(b, 2.6f)
                  .key("ghost" + std::to_string(i))
                  .outline(shapes::circle())
                  .fill(Fill::color(hex(0x2a1d08, 0.45f)))
                  .opacity(withFrom(0.0f, 1.0f,
                                    ramp(tRete * 1000 + 2400 + (float)i * 30, 500))));
    }

    // --- the quatrefoil above, the trefoil below (MHS 45133) -------------
    auto foil = [&](int lobes, float rad, SkPoint at, const char *key,
                    float delay) {
      const float d = rad * 0.52f;
      SkPath u;
      for (int i = 0; i < lobes; ++i) {
        const float a = -SK_FloatPI / 2 + i * 2 * SK_FloatPI / lobes;
        SkPathBuilder cbb;
        cbb.addCircle(rad + std::cos(a) * d, rad + std::sin(a) * d, rad * 0.50f);
        const SkPath c = cbb.detach();
        SkPath tmp;
        Op(u, c, kUnion_SkPathOp, &tmp);
        u = tmp;
      }
      SkPathBuilder hbb;
      hbb.addCircle(rad, rad, rad * 0.40f);
      const SkPath hub = hbb.detach();
      SkPath solid;
      Op(u, hub, kUnion_SkPathOp, &solid);
      SkPath ring;
      SkPath inner = solid;
      SkMatrix m = SkMatrix::Translate(rad, rad);
      m.preScale(0.60f, 0.60f);
      m.preTranslate(-rad, -rad);
      inner = solid.makeTransform(m);
      Op(solid, inner, kDifference_SkPathOp, &ring);
      return disc(at, rad)
          .key(key)
          .outline([ring](SkSize) { return ring; })
          .fill(brassDisc(at, rad, 0.66f))
          .foreground(styles::BevelEmboss{
              .depth = 2, .size = 2.5f, .angleDeg = 125,
              .highlight = hex(0xffe9b0, 0.55f),
              .shadow = hex(0x2a1d08, 0.55f)})
          .opacity(withFrom(0.0f, 1.0f, ramp(delay, 520, ease::outBack())));
    };
    g.child(foil(4, 0.100f * kR, PL(0, 0.72f), "quatrefoil",
                 tRete * 1000 + 1000));
    g.child(foil(3, 0.090f * kR, PL(0, -0.66f), "trefoil",
                 tRete * 1000 + 1100));

    // --- the central rosette --------------------------------------------
    g.child(disc(PL(0, 0), 0.110f * kR)
                .key("rosette")
                .outline(shapes::star(12, 0.52f, 0.16f))
                .fill(brassDisc({kCx, kCy}, 0.110f * kR, 0.68f))
                .foreground(styles::BevelEmboss{
                    .depth = 2, .size = 2, .angleDeg = 125,
                    .highlight = hex(0xffe9b0, 0.6f),
                    .shadow = hex(0x2a1d08, 0.6f)})
                .opacity(withFrom(0.0f, 1.0f,
                                  ramp(tRete * 1000 + 900, 500, ease::outBack()))));

    // --- the almury: "the Denticle of Capricorne" (I.23), at 0° Capricorn -
    g.child(disc(PL(0, -1.0f + 0.055f), 0.048f * kR)
                .key("almury")
                .outline(shapes::polygon(3, 180))
                .fill(brassDisc({kCx, kCy + kR}, 0.048f * kR, 0.68f))
                .opacity(withFrom(0.0f, 1.0f, ramp(tPin * 1000, 420))));

    return g;
  }

  // =========================================================================
  // THE LIMB — degrees outside, the 24 hour letters inside
  //
  // Chaucer I.7–8 puts the 90°-per-quadrant altitude scale on the BACK; II.3
  // has him take the altitude there, turn the instrument over, set the rete,
  // lay the label and read the letter "in the bordure". That sequence puts
  // the letters on the FRONT limb. I have not seen a photograph of the 1326
  // instrument's front limb to confirm it. Both rings are drawn.

  Element limb() {
    auto g = box().absolute().left(0).top(0).width(kW).height(kH).key("limb");

    // the mater's brass field
    g.child(disc({kCx, kCy}, kMaterR)
                .key("mater")
                .outline(shapes::circle())
                .fill(brassDisc({kCx, kCy}, kMaterR, 0.50f))
                .background(shadow(hex(0x05070c, 0.62f), {8, 12}, 26))
                .foreground(styles::BevelEmboss{
                    .depth = 3, .size = 6, .angleDeg = 125,
                    .highlight = hex(0xfff0c4, 0.5f),
                    .shadow = hex(0x2a1d08, 0.6f)})
                .opacity(withFrom(0.0f, 1.0f, ramp(tMater * 1000, 700))));

    // the polished dome: a sheen centred slightly above the pin. glowUnit,
    // because it must FILL its box — radialUnit's radius is a fraction of
    // the HALF-DIAGONAL and has now cost two studies an iteration.
    g.child(disc({kCx, kCy}, kMaterR)
                .key("sheen")
                .outline(shapes::circle())
                .fill(Material::glowUnit({0.40f, 0.30f}, 0.95f,
                                         {{0.0f, hex(0xfff3cf, 0.30f)},
                                          {0.55f, hex(0xffdc8b, 0.10f)},
                                          {1.0f, hex(0x4f360e, 0.14f)}}))
                .blend(SkBlendMode::kSoftLight)
                .opacity(withFrom(0.0f, 1.0f, ramp(tMater * 1000 + 200, 700))));

    // brass is TOOLED, and the tool marks are fine concentric turning
    g.child(disc({kCx, kCy}, kMaterR)
                .key("turning")
                .outline(shapes::circle())
                .background(lines::concentric(Fill::color(hex(0x6b4d18, 0.055f)),
                                              120, 0.9f))
                .opacity(withFrom(0.0f, 1.0f, ramp(tMater * 1000 + 300, 600))));
    g.child(disc({kCx, kCy}, kMaterR)
                .key("brassgrain")
                .outline(shapes::circle())
                .fill(brassGrain)
                .blend(SkBlendMode::kOverlay)
                .opacity(withFrom(0.0f, 0.30f, ramp(tMater * 1000 + 300, 600))));

    // the three rules of the limb: 1.155 / 1.082 / 1.005 R
    const float rules[3] = {1.155f, 1.082f, 1.005f};
    for (int i = 0; i < 3; ++i)
      g.child(disc({kCx, kCy}, rules[i] * kR)
                  .key("rule" + std::to_string(i))
                  .outline(shapes::circle())
                  .fill(Fill::none())
                  .stroke(PathFormat{
                      .width = i == 0 ? 3.0f : 2.0f,
                      .strokeFill = grooveFill(rules[i] * kR, i == 0 ? 3.0f : 2.0f,
                                               0.75f, 0.45f)})
                  .trim(0.0f, withFrom(0.0f, 1.0f,
                                       ramp(tMater * 1000 + 200 + (float)i * 120,
                                            900, ch::easeOutQuint))));

    // the 360 degree ticks — ONE atlas cell, three LENGTHS through
    // Pool::sizes(). This is the instancing case, and it works here for
    // exactly the reason it cannot work for the almucantars: a tick is a
    // FILLED rect, so a non-uniform scale changes its length without
    // changing the width of the mark. On a stroked circle the stroke IS the
    // shape's outline, and RSXform would scale it with the geometry.
    g.child(box()
                .absolute()
                .left(0)
                .top(0)
                .width(kW)
                .height(kH)
                .key("ticks")
                .opacity(withFrom(0.0f, 1.0f, ramp(tTicks * 1000, 700)))
                .child(instancing::instances(tickAtlas, tickPool,
                                             instancing::Mode::Data)));

    // the 12 degree numerals, RADIAL — the type radiates like a spoke,
    // because you turn the instrument to read a limb
    for (int i = 0; i < 12; ++i) {
      const int deg = i * 30;
      const float psi = 90.0f - (float)deg;   // plate angle of this division
      const float f = std::fmod(psi / 360.0f + 2.0f, 1.0f);
      const float rr = 1.104f * kR;
      g.child(text(toU8(std::to_string(deg == 0 ? 360 : deg)),
                   type(faceLimb, 0.026f * kR, hex(0x33240c, 0.92f), 0.6f))
                  .absolute()
                  .width(Dim(2 * rr))
                  .height(Dim(2 * rr))
                  .centerAt({kCx, kCy})
                  .key("degnum" + std::to_string(i))
                  .onPath(TextPath{.path = shapes::circle(),
                                   .at = f,
                                   .align = TextPath::Align::Center,
                                   .offset = 0.0f,
                                   .autoFlip = false,
                                   .orient = TextPath::Orient::Radial})
                  .opacity(withFrom(0.0f, 1.0f,
                                    ramp(tTicks * 1000 + 300 + (float)i * 25, 400))));
    }

    // the 24 hour letters, RADIAL. A at the first hour after noon, running
    // clockwise at 15°/hour; X is the 21st, at ψ = 135°, and 21 hours after
    // noon is 9 a.m. — which is exactly what Chaucer reads in II.3.
    for (int n = 1; n <= 24; ++n) {
      const float psi = 90.0f - 15.0f * (float)n;
      const float f = std::fmod(psi / 360.0f + 2.0f, 1.0f);
      const float rr = 1.044f * kR;
      const bool isX = (n == 21);
      g.child(text(toU8(kLetters[n - 1]),
                   type(faceLimb, 0.040f * kR,
                        isX ? hex(0x33240c, 1.0f) : hex(0x33240c, 0.88f), 0))
                  .absolute()
                  .width(Dim(2 * rr))
                  .height(Dim(2 * rr))
                  .centerAt({kCx, kCy})
                  .key("hl" + std::to_string(n))
                  .onPath(TextPath{.path = shapes::circle(),
                                   .at = f,
                                   .align = TextPath::Align::Center,
                                   .offset = 0.0f,
                                   .autoFlip = false,
                                   .orient = TextPath::Orient::Radial})
                  .opacity(withFrom(0.0f, 1.0f,
                                    ramp(tLetters * 1000 + (float)n * 12, 380))));
      // the letter under the label lights as it passes
      g.child(disc(MC(1.044f * std::cos(psi * kD), 1.044f * std::sin(psi * kD)),
                   0.052f * kR)
                  .key("hlg" + std::to_string(n))
                  .outline(shapes::circle())
                  .fill(Material::glowUnit({0.5f, 0.5f}, 1.0f,
                                           {{0.0f, hex(0xfff3cf, 0.85f)},
                                            {1.0f, hex(0xfff3cf, 0.0f)}}))
                  .blend(SkBlendMode::kPlus)
                  .opacity(&letterGlow[n - 1]));
    }

    // the throne (kursi) and its shackle — the instrument hangs plumb
    {
      const float th = 0.20f * kR;
      const SkPoint top{kCx, kCy - kMaterR};
      g.child(box()
                  .absolute()
                  .left(top.fX - 0.16f * kR)
                  .top(top.fY - th)
                  .width(0.32f * kR)
                  .height(th + 0.05f * kR)
                  .key("throne")
                  .outline(shapes::blob(3u, 0.10f, 9))
                  .fill(brass(SkRect::MakeLTRB(top.fX - 0.16f * kR, top.fY - th,
                                               top.fX + 0.16f * kR,
                                               top.fY + 0.05f * kR),
                              0.74f))
                  .foreground(styles::BevelEmboss{
                      .depth = 2, .size = 4, .angleDeg = 125,
                      .highlight = hex(0xfff0c4, 0.6f),
                      .shadow = hex(0x2a1d08, 0.6f)})
                  .translateY(withFrom(-26.0f, 0.0f,
                                       ramp(tMater * 1000 + 200, 900,
                                            ease::outBack())))
                  .opacity(withFrom(0.0f, 1.0f, ramp(tMater * 1000 + 200, 600))));
      g.child(disc({top.fX, top.fY - th - 0.055f * kR}, 0.075f * kR)
                  .key("shackle")
                  .outline(shapes::annulus(0.62f))
                  .fill(brassDisc({top.fX, top.fY - th - 0.055f * kR},
                                  0.075f * kR, 0.78f))
                  .translateY(withFrom(-26.0f, 0.0f,
                                       ramp(tMater * 1000 + 260, 900,
                                            ease::outBack())))
                  .opacity(withFrom(0.0f, 1.0f, ramp(tMater * 1000 + 260, 600))));
    }
    return g;
  }

  // =========================================================================
  // the label (a thin rule from the pin to the limb), the sun's mark, the pin

  Element rule() {
    auto g = box().absolute().left(0).top(0).width(kW).height(kH).key("rule");

    // the label swings with the sun: ψ = 90 − H, and the canvas angle is −ψ
    g.child(box()
                .absolute()
                .left(kCx)
                .top(kCy - 4.5f)
                .width(kMaterR * 1.02f)
                .height(9.0f)
                .key("label")
                .transformOriginPx({0, 4.5f})
                .rotate(bind(&hourAngle).scale(1.0f).offset(-90.0f))
                .fill(brass(SkRect::MakeLTRB(kCx, kCy - 4.5f, kCx + kMaterR,
                                             kCy + 4.5f),
                            0.80f))
                .foreground(stroke(1.2f, Fill::color(hex(0x2a1d08, 0.7f))))
                .overlay(PathFormat{
                    .width = 1.2f,
                    .strokeFill = Fill::color(hex(0x3a2a10, 0.85f)),
                    .dashIntervals = {2.0f, 6.0f}})
                .background(shadow(hex(0x2a1d08, 0.5f), {4, 5}, 7))
                .opacity(withFrom(0.0f, 1.0f, ramp(tPin * 1000 + 200, 600))));

    // the pin and its horse
    g.child(disc({kCx, kCy}, 0.040f * kR)
                .key("pin")
                .outline(shapes::circle())
                .fill(brassDisc({kCx, kCy}, 0.040f * kR, 0.82f))
                .foreground(styles::BevelEmboss{
                    .depth = 2, .size = 2, .angleDeg = 125,
                    .highlight = hex(0xfff0c4, 0.7f),
                    .shadow = hex(0x2a1d08, 0.6f)})
                .background(shadow(hex(0x2a1d08, 0.5f), {2, 3}, 5))
                .opacity(withFrom(0.0f, 1.0f, ramp(tPin * 1000, 420))));
    return g;
  }

  /** The sun's mark is positioned every frame from the same Output the rete
   *  turns on, so it stays welded to the ecliptic. Rendered through a slot
   *  because its POSITION is data, not a transform of a fixed point. */
  Element sunMark() {
    const float lam = sunLam.value();
    const float dec = sunDec(lam);
    const float H = hourAngle.value();
    const float psi = 90.0f - H;
    const SkPoint p = MC(rOfDec(dec) * std::cos(psi * kD),
                         rOfDec(dec) * std::sin(psi * kD));
    const bool up = sunAlt.value() > 0;
    // the point OPPOSITE the sun on the ecliptic — the sun's nadir. The 12
    // seasonal-hour arcs are engraved BELOW the horizon, so read directly
    // they give the night hours of a body that is down; the traditional
    // daytime reading puts this mark on the same twelve curves instead.
    const float psiN = psi + 180.0f;
    const SkPoint q = MC(rOfDec(-dec) * std::cos(psiN * kD),
                         rOfDec(-dec) * std::sin(psiN * kD));
    return box()
        .absolute()
        .left(0)
        .top(0)
        .width(kW)
        .height(kH)
        .child(disc(p, 34.0f)
                   .outline(shapes::circle())
                   .fill(Material::glowUnit({0.5f, 0.5f}, 1.0f,
                                            {{0.0f, up ? hex(0xfff3cf, 0.60f)
                                                       : hex(0x8fb0d0, 0.30f)},
                                             {1.0f, hex(0xfff3cf, 0.0f)}}))
                   .blend(SkBlendMode::kPlus))
        .child(disc(p, 15.0f)
                   .outline(shapes::star(12, 0.40f, 0.16f))
                   .fill(Fill::color(up ? hex(0xfff6dc, 1.0f)
                                        : hex(0xa8bed2, 0.9f)))
                   .foreground(stroke(1.4f, Fill::color(hex(0x2a1d08, 0.75f)))))
        .child(disc(p, 5.0f)
                   .outline(shapes::circle())
                   .fill(Fill::color(hex(0x6b4d18, 0.8f))))
        .child(disc(q, 7.0f)
                   .outline(shapes::circle())
                   .fill(Fill::none())
                   .stroke(stroke(1.6f, Fill::color(hex(0x2f6f9c, 0.75f)))))
        .child(disc(q, 2.2f)
                   .outline(shapes::circle())
                   .fill(Fill::color(hex(0x2f6f9c, 0.8f))));
  }

  // =========================================================================
  // THE PROJECTION — the panel that explains the whole instrument

  Element projectionPanel() {
    const float px = 1210, py = 148, pw = 450, ph = 372;
    auto g = panel(px, py, pw, ph, "THE PROIECTIOVN", "stereographic, from the "
                                                      "SOVTH celestial pole");
    const SkPoint c{px + 172, py + 170};
    const float rr = 100;

    // the celestial sphere in section
    g.child(disc(c, rr)
                .outline(shapes::circle())
                .fill(Fill::none())
                .stroke(stroke(1.6f, Fill::color(hex(0x241c15, 0.75f)))));
    // the equatorial plane — the plane of projection
    g.child(box()
                .absolute()
                .left(c.fX - rr - 78)
                .top(c.fY - 1)
                .width(2 * rr + 156)
                .height(2)
                .fill(Fill::color(hex(0x241c15, 0.6f))));
    // the two tropics as chords
    for (int s = -1; s <= 1; s += 2) {
      const float y = c.fY - s * rr * std::sin(kEps * kD);
      const float half = rr * std::cos(kEps * kD);
      g.child(box()
                  .absolute()
                  .left(c.fX - half)
                  .top(y - 0.7f)
                  .width(2 * half)
                  .height(1.4f)
                  .fill(Fill::color(hex(0x241c15, 0.42f))));
    }
    // the poles
    g.child(disc({c.fX, c.fY - rr}, 3.4f)
                .outline(shapes::circle())
                .fill(Fill::color(kInk)));
    g.child(disc({c.fX, c.fY + rr}, 5.0f)
                .outline(shapes::circle())
                .fill(Fill::color(kRubric)));
    g.child(text(toU8("N"), type(faceSerif, 15, kInk))
                .absolute()
                .centerAt({c.fX + 14, c.fY - rr - 2}));
    g.child(text(toU8("S \xe2\x80\x94 the eye of the projection"),
                 type(faceItalic, 13, kRubric))
                .absolute()
                .left(c.fX + 10)
                .top(c.fY + rr + 2));
    g.child(slot("projray"));
    g.child(slot("projread"));
    return g;
  }

  /** The live half of the projection panel: a point P walking the meridian
   *  from +ε to −ε, the ray from the south pole through it, and where that
   *  ray lands on the plane. r = R_eq·tan((90−δ)/2), and the landing radius
   *  sweeps R_can → R_eq → R_cap as it goes. */
  Element projRay() {
    const float px = 1210, py = 148;
    const SkPoint c{px + 172, py + 170};
    const float rr = 100;
    const float dec = projDec.value();
    const SkPoint P{c.fX + rr * std::cos(dec * kD), c.fY - rr * std::sin(dec * kD)};
    const SkPoint S{c.fX, c.fY + rr};
    // The equator projects onto ITSELF, so the section's sphere radius is the
    // equator's radius on the plate: the landing distance is rr·r(δ)/R_eq —
    // and the same number falls out of the straight line from S through P,
    // since cos δ/(1 + sin δ) IS tan((90−δ)/2). Two derivations, one point.
    const SkPoint Lp{c.fX + rr * rOfDec(dec) / kReq, c.fY};
    auto g = box().absolute().left(0).top(0).width(kW).height(kH);
    SkPathBuilder pb;
    pb.moveTo(S);
    pb.lineTo(Lp.fX + (Lp.fX - S.fX) * 0.06f, Lp.fY + (Lp.fY - S.fY) * 0.06f);
    SkPath ray = pb.detach();
    SkRect bb = ray.getBounds();
    bb.outset(4, 4);
    const SkPath local =
        ray.makeTransform(SkMatrix::Translate(-bb.left(), -bb.top()));
    g.child(box()
                .absolute()
                .left(bb.left())
                .top(bb.top())
                .width(bb.width())
                .height(bb.height())
                .outline([local](SkSize) { return local; })
                .fill(Fill::none())
                .stroke(PathFormat{.width = 1.5f,
                                   .strokeFill = Fill::color(hex(0x2f6f9c, 0.9f)),
                                   .dashIntervals = {6.0f, 4.0f}}));
    g.child(disc(P, 5.0f).outline(shapes::circle()).fill(Fill::color(kTrace)));
    g.child(disc(Lp, 5.0f)
                .outline(shapes::circle())
                .fill(Fill::color(kRubric)));
    g.child(box()
                .absolute()
                .left(Lp.fX - 1)
                .top(c.fY - 20)
                .width(2)
                .height(40)
                .fill(Fill::color(hex(0x8c2f22, 0.55f))));
    return g;
  }

  Element projRead() {
    const float px = 1210, py = 148, pw = 450;
    const float dec = projDec.value();
    auto row = [&](const std::string &s, SkColor4f c, float sz) {
      return text(toU8(s), type(faceMono, sz, c));
    };
    return box()
        .absolute()
        .left(px + 16)
        .top(py + 264)
        .width(pw - 32)
        .column()
        .gap(4)
        .child(row(fmt("\xce\xb4 = %+7.3f\xc2\xb0", dec), kInk, 14))
        .child(row(fmt("r = R_eq\xc2\xb7tan((90\xe2\x88\x92\xce\xb4)/2) = "
                       "%.6f R",
                       rOfDec(dec)),
                   kInk, 14))
        .child(row("R_can 0.424423  R_eq 0.651477  R_cap 1.000000",
                   hex(0x7b6a54), 12))
        .child(row("a circle through the EYE projects to a LINE \xe2\x80\x94 "
                   "which is",
                   hex(0x7b6a54), 12))
        .child(row("why the meridian, alone of the 12 azimuths, is straight.",
                   hex(0x7b6a54), 12));
  }

  // =========================================================================
  // panels

  /** A vellum card. The container spans the WHOLE canvas so that everything
   *  in this sketch — instrument, panels, overlays — is authored in one
   *  coordinate frame; the card itself is just the first child. */
  Element panel(float x, float y, float w, float h, const char *title,
                const char *sub) {
    auto g = box().absolute().left(0).top(0).width(kW).height(kH);
    g.child(box()
                .absolute()
                .left(x)
                .top(y)
                .width(w)
                .height(h)
                .fill(Fill::color(hex(0xe8dcc2, 0.62f)))
                .stroke(stroke(1.0f, Fill::color(hex(0x241c15, 0.24f)),
                               PathFormat::Align::Inner)));
    g.child(text(toU8(title), type(faceLimb, 15, kRubric, 1.9f))
                .absolute()
                .left(x + 16)
                .top(y + 11));
    if (sub && *sub)
      g.child(text(toU8(sub), type(faceItalic, 14, hex(0x6b5a44)))
                  .absolute()
                  .left(x + 16)
                  .top(y + 32)
                  .width(w - 32));
    g.child(box()
                .absolute()
                .left(x + 16)
                .top(y + 52)
                .width(w - 32)
                .height(1)
                .fill(Fill::color(hex(0x241c15, 0.28f))));
    return g;
  }

  Element familiesPanel() {
    const float px = 1210, py = 536, pw = 450, ph = 380;
    auto g = panel(px, py, pw, ph, "THE PLATE, IN FOVR FAMILIES",
                   "every curve is a circle; no two families share a centre");
    struct Fam {
      const char *name;
      const char *formula;
    };
    const Fam fams[4] = {
        {"CERCLES", "R \xc2\xb7 k\xe2\x81\xbf, k = tan(45\xc2\xb0\xe2\x88\x92\xce\xb5/2)"},
        {"ALMICANTERAS", "cy = R_eq cos \xcf\x86/(sin \xcf\x86 + sin h)"},
        {"AZIMVTES", "coaxal, zenith \xe2\x88\xa7 nadir"},
        {"HOVRES INEQVALES", "3-point circles, err 0.00374 R"}};
    for (int i = 0; i < 4; ++i) {
      const float cx = px + 116 + (float)(i % 2) * 218;
      const float cy = py + 126 + (float)(i / 2) * 158;
      const float r = 54;
      auto d = disc({cx, cy}, r)
                   .outline(shapes::circle())
                   .clip(true)
                   .fill(Fill::color(hex(0xf6efdd)))
                   .stroke(stroke(1.2f, Fill::color(hex(0x241c15, 0.5f)),
                                  PathFormat::Align::Inner));
      auto put = [&](float mcx, float mcy, float mr, float w, float a) {
        d.child(disc({r + mcx * r, r - mcy * r}, mr * r)
                    .outline(shapes::circle())
                    .fill(Fill::none())
                    .stroke(stroke(w, Fill::color(hex(0x241c15, a)))));
      };
      if (i == 0) {
        put(0, 0, 1.0f, 1.3f, 0.85f);
        put(0, 0, kReq, 1.3f, 0.85f);
        put(0, 0, kRcan, 1.3f, 0.85f);
      } else if (i == 1) {
        for (int h = 0; h <= 88; h += 4)
          put(0, almCy((float)h), almR((float)h), 0.8f, 0.55f);
      } else if (i == 2) {
        for (int a = 1; a <= 5; ++a) {
          const float ap = (float)(a * 15);
          put(kAzA * std::tan(ap * kD), kAzCy, kAzA / std::cos(ap * kD), 0.8f, 0.6f);
          put(-kAzA * std::tan(ap * kD), kAzCy, kAzA / std::cos(ap * kD), 0.8f, 0.6f);
        }
        put(0, kAzCy, kAzA, 1.0f, 0.75f);
        d.child(box()
                    .absolute()
                    .left(r - 0.5f)
                    .top(0)
                    .width(1)
                    .height(2 * r)
                    .fill(Fill::color(hex(0x241c15, 0.75f))));
      } else {
        for (int k = 1; k <= 11; ++k) {
          const Circ c = seasonalLine(k);
          if (c.ok)
            put(c.cx, c.cy, c.r, 0.8f, 0.6f);
        }
        d.child(box()
                    .absolute()
                    .left(r - 0.5f)
                    .top(r)
                    .width(1)
                    .height(r)
                    .fill(Fill::color(hex(0x241c15, 0.7f))));
      }
      g.child(std::move(d));
      g.child(text(toU8(fams[i].name), type(faceLimb, 11.5f, kInk, 1.2f))
                  .absolute()
                  .width(212)
                  .textAlign(sigil::weave::TextAlignment::kCenter)
                  .centerAt({cx, cy + r + 15}));
      g.child(text(toU8(fams[i].formula), type(faceMono, 9.5f, hex(0x7b6a54)))
                  .absolute()
                  .width(212)
                  .textAlign(sigil::weave::TextAlignment::kCenter)
                  .centerAt({cx, cy + r + 30}));
    }
    return g;
  }

  // --- the back: where Chaucer's example starts -----------------------------
  Element backPanel() {
    const float px = 1210, py = 932, pw = 450, ph = 468;
    auto g = panel(px, py, pw, ph, "THE BAK",
                   "where Chaucer's example starts: a date and an altitude");
    const SkPoint c{px + 225, py + 232};
    const float r = 156;

    g.child(disc(c, r)
                .outline(shapes::circle())
                .fill(brassDisc(c, r, 0.44f))
                .foreground(styles::BevelEmboss{
                    .depth = 2, .size = 4, .angleDeg = 125,
                    .highlight = hex(0xffe9b0, 0.45f),
                    .shadow = hex(0x2a1d08, 0.5f)}));
    // four quadrants of 90° altitude scale (I.7–8)
    for (int d = 0; d <= 360; d += 2) {
      const float a = (float)d * kD;
      const bool five = (d % 10) == 0;
      const float r0 = r * (five ? 0.90f : 0.925f), r1 = r * 0.96f;
      SkPathBuilder pb;
      pb.moveTo(c.fX + std::cos(a) * r0, c.fY + std::sin(a) * r0);
      pb.lineTo(c.fX + std::cos(a) * r1, c.fY + std::sin(a) * r1);
      SkPath seg = pb.detach();
      SkRect bb = seg.getBounds();
      bb.outset(2, 2);
      const SkPath local =
          seg.makeTransform(SkMatrix::Translate(-bb.left(), -bb.top()));
      g.child(box()
                  .absolute()
                  .left(bb.left())
                  .top(bb.top())
                  .width(bb.width())
                  .height(bb.height())
                  .outline([local](SkSize) { return local; })
                  .fill(Fill::none())
                  .stroke(stroke(five ? 1.1f : 0.7f,
                                 Fill::color(hex(0x3a2a10, 0.6f)))));
    }
    for (float rr : {0.90f, 0.96f, 0.855f, 0.78f, 0.70f})
      g.child(disc(c, r * rr)
                  .outline(shapes::circle())
                  .fill(Fill::none())
                  .stroke(PathFormat{
                      .width = 1.2f,
                      .strokeFill = grooveFill(r * rr, 1.2f, 0.6f, 0.3f)}));
    // the calendar and zodiac rings — Chaucer I.10 gives the month lengths
    {
      float acc = 0;
      for (int m = 0; m < 12; ++m) {
        const float days = (float)kMonthDays[m];
        const float a0 = -90.0f + acc / 365.0f * 360.0f;
        const float a1 = -90.0f + (acc + days) / 365.0f * 360.0f;
        acc += days;
        SkPathBuilder pb;
        pb.moveTo(c.fX + std::cos(a0 * kD) * r * 0.78f,
                  c.fY + std::sin(a0 * kD) * r * 0.78f);
        pb.lineTo(c.fX + std::cos(a0 * kD) * r * 0.855f,
                  c.fY + std::sin(a0 * kD) * r * 0.855f);
        SkPath seg = pb.detach();
        SkRect bb = seg.getBounds();
        bb.outset(2, 2);
        const SkPath local =
            seg.makeTransform(SkMatrix::Translate(-bb.left(), -bb.top()));
        g.child(box()
                    .absolute()
                    .left(bb.left())
                    .top(bb.top())
                    .width(bb.width())
                    .height(bb.height())
                    .outline([local](SkSize) { return local; })
                    .fill(Fill::none())
                    .stroke(stroke(1.1f, Fill::color(hex(0x3a2a10, 0.7f)))));
        const float am = (a0 + a1) * 0.5f;
        g.child(text(toU8(kMonths[m]), type(faceLimb, 9.5f, hex(0x33240c, 0.85f)))
                    .absolute()
                    .centerAt({c.fX + std::cos(am * kD) * r * 0.817f,
                               c.fY + std::sin(am * kD) * r * 0.817f}));
        const float az = -90.0f + ((float)m + 0.0f) / 12.0f * 360.0f;
        const float azm = az + 15.0f;
        g.child(text(toU8(std::string(kSigns[(m + 9) % 12]).substr(0, 3)),
                     type(faceLimb, 9.0f, hex(0x33240c, 0.7f)))
                    .absolute()
                    .centerAt({c.fX + std::cos(azm * kD) * r * 0.74f,
                               c.fY + std::sin(azm * kD) * r * 0.74f}));
      }
    }
    // the shadow square: umbra recta and umbra versa, 12 divisions each (I.12)
    {
      const float s = r * 0.50f;
      g.child(box()
                  .absolute()
                  .left(c.fX - s)
                  .top(c.fY)
                  .width(2 * s)
                  .height(s)
                  .fill(Fill::none())
                  .stroke(stroke(1.4f, Fill::color(hex(0x3a2a10, 0.75f)))));
      for (int i = 1; i < 12; ++i) {
        const float t = (float)i / 12.0f;
        g.child(box()
                    .absolute()
                    .left(c.fX - s + 2 * s * t)
                    .top(c.fY)
                    .width(0.8f)
                    .height(s * (i % 3 == 0 ? 0.34f : 0.20f))
                    .fill(Fill::color(hex(0x3a2a10, 0.6f))));
        g.child(box()
                    .absolute()
                    .left(c.fX - s)
                    .top(c.fY + s * t)
                    .width(s * (i % 3 == 0 ? 0.34f : 0.20f))
                    .height(0.8f)
                    .fill(Fill::color(hex(0x3a2a10, 0.6f))));
        g.child(box()
                    .absolute()
                    .left(c.fX + s - s * (i % 3 == 0 ? 0.34f : 0.20f))
                    .top(c.fY + s * t)
                    .width(s * (i % 3 == 0 ? 0.34f : 0.20f))
                    .height(0.8f)
                    .fill(Fill::color(hex(0x3a2a10, 0.6f))));
      }
      g.child(text(toU8("VMBRA RECTA"), type(faceLimb, 9, hex(0x33240c, 0.8f)))
                  .absolute()
                  .centerAt({c.fX - s * 0.52f, c.fY + s * 0.86f}));
      g.child(text(toU8("VMBRA VERSA"), type(faceLimb, 9, hex(0x33240c, 0.8f)))
                  .absolute()
                  .centerAt({c.fX + s * 0.52f, c.fY + s * 0.86f}));
    }
    // the alidade, swung to 25° 30′ — the measurement II.3 starts from
    g.child(box()
                .absolute()
                .left(c.fX - r * 0.97f)
                .top(c.fY - 5)
                .width(2 * r * 0.97f)
                .height(10)
                .transformOrigin(0.5f, 0.5f)
                .rotate(withFrom(0.0f, -25.5f,
                                 ramp(tChaucer * 1000 + 200, 900,
                                      ease::outBack())))
                .fill(brass(SkRect::MakeLTRB(c.fX - r, c.fY - 5, c.fX + r,
                                             c.fY + 5),
                            0.76f))
                .foreground(stroke(1.0f, Fill::color(hex(0x2a1d08, 0.6f))))
                .background(shadow(hex(0x2a1d08, 0.45f), {2, 3}, 5)));
    for (int s = -1; s <= 1; s += 2)
      g.child(box()
                  .absolute()
                  .left(c.fX + s * r * 0.90f - 5)
                  .top(c.fY - 16)
                  .width(10)
                  .height(32)
                  .transformOriginPx({5.0f - s * r * 0.90f, 16})
                  .rotate(withFrom(0.0f, -25.5f,
                                   ramp(tChaucer * 1000 + 200, 900,
                                        ease::outBack())))
                  .fill(brassDisc(c, r, 0.80f))
                  .foreground(stroke(1.0f, Fill::color(hex(0x2a1d08, 0.6f)))));
    g.child(disc(c, 8).outline(shapes::circle()).fill(brassDisc(c, 8, 0.82f)));
    g.child(text(toU8("altitude 25\xc2\xb0 30\xe2\x80\xb2 \xe2\x80\x94 "
                      "12 March 1391"),
                 type(faceItalic, 13, kRubric))
                .absolute()
                .centerAt({px + 225, py + ph - 22}));
    return g;
  }

  // --- specification card ---------------------------------------------------
  Element specCard() {
    const float px = 1690, py = 148, pw = 646, ph = 286;
    auto g = panel(px, py, pw, ph, "PROVENANCE & SPECIFICATION",
                   "British Museum 1909,0617.1");
    struct KV {
      const char *k;
      const char *v;
    };
    const KV rows[] = {
        {"object", "planispheric astrolabe, English"},
        {"date", "1326 \xe2\x80\x94 earliest DATED astrolabe made in Europe"},
        {"material", "brass (medieval latten, Cu\xe2\x80\x93Zn)"},
        {"diameter", "132 mm  \xc2\xb7  mater ~10 mm thick"},
        {"plates", "Oxford \xc2\xb7 Jerusalem \xc2\xb7 Babilonie \xc2\xb7 "
                   "Rome \xc2\xb7 Montpellier \xc2\xb7 Paris"},
        {"rete", "Y-shaped, 33 stars; birds, and a DOG'S HEAD for Sirius"},
        {"manual", "Chaucer, A Treatise on the Astrolabe, 1391 \xe2\x80\x94 "
                   "the first technical manual in English"},
    };
    float y = py + 58;
    for (const KV &r : rows) {
      g.child(text(toU8(r.k), type(faceLimb, 11, hex(0x6b5a44), 1.2f))
                  .absolute()
                  .left(px + 18)
                  .top(y)
                  .width(88));
      g.child(text(toU8(r.v), type(faceSerif, 13.5f, kInk))
                  .absolute()
                  .left(px + 112)
                  .top(y - 2)
                  .width(pw - 130));
      y += 20;
    }
    // the two obliquities, side by side
    g.child(box()
                .absolute()
                .left(px + 18)
                .top(y + 6)
                .width(pw - 36)
                .height(1)
                .fill(Fill::color(hex(0x241c15, 0.22f))));
    g.child(text(toU8("\xcf\x86 = 51\xc2\xb0 50\xe2\x80\xb2  Chaucer I.14, "
                      "Oxenford        \xce\xb5 = 23\xc2\xb0 50.0\xe2\x80\xb2  "
                      "Chaucer I.17"),
                 type(faceMono, 12, kInk))
                .absolute()
                .left(px + 18)
                .top(y + 14)
                .width(pw - 36));
    g.child(text(toU8("                                        \xce\xb5 = "
                      "23\xc2\xb0 31.6\xe2\x80\xb2  TRVE at 1326      "
                      "\xce\x94 18.4\xe2\x80\xb2"),
                 type(faceMono, 12, kRubric))
                .absolute()
                .left(px + 18)
                .top(y + 30)
                .width(pw - 36));
    g.child(text(toU8("his \xce\xb5 is an inherited PTOLEMAIC value, 1200 years "
                      "old \xe2\x80\x94 the equator comes out 0.586% small "
                      "(\xe2\x88\x92" "0.229 mm), Cancer 1.175% "
                      "(\xe2\x88\x92" "0.299 mm)"),
                 type(faceItalic, 12.5f, kInk))
                .absolute()
                .left(px + 18)
                .top(y + 52)
                .width(pw - 36));
    return g;
  }

  // --- the star table -------------------------------------------------------
  Element starPanel() {
    const float px = 1690, py = 450, pw = 646, ph = 450;
    auto g = panel(px, py, pw, ph, "THE RETE \xc2\xb7 XII STERRES",
                   "precessed J2000 \xe2\x86\x92 1326.0, IAU 1976 "
                   "\xce\xb6/z/\xce\xb8 \xe2\x80\x94 the sky has slid "
                   "8.6\xc2\xb0 in RA");
    g.child(text(toU8("name on the rete        modern         RA 1326   "
                      "dec 1326    r / R"),
                 type(faceMono, 11, hex(0x6b5a44)))
                .absolute()
                .left(px + 18)
                .top(py + 60));
    for (size_t i = 0; i < kStars.size(); ++i) {
      const float y = py + 80 + (float)i * 25.5f;
      const float r = rOfDec(kStars[i].dec1326);
      g.child(text(toU8(kStars[i].name), type(faceLimb, 12.5f, kInk, 0.8f))
                  .absolute()
                  .left(px + 18)
                  .top(y));
      g.child(text(toU8(kStars[i].modern), type(faceItalic, 12.5f, hex(0x6b5a44)))
                  .absolute()
                  .left(px + 168)
                  .top(y));
      g.child(text(toU8(fmt("%8.3f  %+8.3f   %.5f", kStars[i].ra1326,
                            kStars[i].dec1326, r)),
                   type(faceMono, 11.5f, kInk))
                  .absolute()
                  .left(px + 276)
                  .top(y + 1));
      // where the star lands between Cancer and Capricorn
      const float bx = px + 480, bw = 148, lo = 0.18f;
      g.child(box()
                  .absolute()
                  .left(bx)
                  .top(y + 8)
                  .width(bw)
                  .height(1)
                  .fill(Fill::color(hex(0x241c15, 0.3f))));
      for (float t : {kRcan, kReq, 1.0f})
        g.child(box()
                    .absolute()
                    .left(bx + bw * (t - lo) / (1.0f - lo) - 0.5f)
                    .top(y + 4)
                    .width(1)
                    .height(9)
                    .fill(Fill::color(hex(0x241c15, 0.35f))));
      g.child(disc({bx + bw * (r - lo) / (1.0f - lo), y + 8.5f}, 3.6f)
                  .outline(shapes::circle())
                  .fill(Fill::color(i == 3 ? kRubric : kInk)));
    }
    g.child(text(toU8("ALHABOR / Sirius at 0.868 R is the outermost by a long "
                      "way \xe2\x80\x94 the only southern star here, which is "
                      "why it gets the biggest pointer on every rete ever "
                      "made."),
                 type(faceItalic, 12, kRubric))
                .absolute()
                .left(px + 18)
                .top(py + ph - 44)
                .width(pw - 36));
    return g;
  }

  // --- Chaucer's worked example --------------------------------------------
  Element chaucerPanel() {
    const float px = 1690, py = 920, pw = 646, ph = 260;
    auto g = panel(px, py, pw, ph, "CHAVCER'S OWNE ENSAMPLE",
                   "A Treatise on the Astrolabe, II.3");
    g.child(text(toU8("\xe2\x80\x9cthe yeer of oure lord 1391, the 12 day of "
                      "March \xe2\x80\xa6 I took the altitude of my sonne, and "
                      "fond that it was 25 degrees and 30 of minutes \xe2\x80\xa6 "
                      "fond the poynte of my label in the bordure, up-on a "
                      "capital lettre that is cleped an X \xe2\x80\xa6 and fond "
                      "that it was 9 of the clokke of the day.\xe2\x80\x9d"),
                 type(faceItalic, 13.5f, kInk))
                .absolute()
                .left(px + 18)
                .top(py + 62)
                .width(pw - 36));
    g.child(slot("chaucer"));
    return g;
  }

  Element chaucerBody() {
    const float px = 1690, py = 920, pw = 646;
    auto g = box()
                 .absolute()
                 .left(px + 18)
                 .top(py + 140)
                 .width(pw - 36)
                 .column()
                 .gap(3);
    g.child(text(toU8(chaucerH), type(faceMono, 12.5f, kInk)));
    g.child(text(toU8(chaucerA), type(faceMono, 12.5f, kInk)));
    g.child(text(toU8(chaucerDelta), type(faceMono, 12.5f, kRubric)));
    g.child(box().height(6));
    g.child(text(toU8("Chaucer 09:00   \xc2\xb7   computed 08:53.8   "
                      "\xc2\xb7   \xce\x94 6.2 min \xe2\x80\x94 one hour-"
                      "letter's worth of reading precision on 132 mm"),
                 type(faceSerif, 13, kInk)));
    return g;
  }

  // --- the zodiac is not uniform -------------------------------------------
  Element zodiacPanel() {
    const float px = 1690, py = 1200, pw = 646, ph = 200;
    auto g = panel(px, py, pw, ph, "THE ZODIAC IS NOT VNIFORM",
                   "span of each sign ON THE RING \xe2\x80\x94 19.8\xc2\xb0 to "
                   "44.7\xc2\xb0, and they sum to 360.000000");
    const float bx = px + 22, by = py + 158, bw = pw - 44;
    const float maxSpan = 44.714f;
    for (int i = 0; i < 12; ++i) {
      const float a0 = ringAngle((float)(i * 30));
      float a1 = ringAngle((float)((i + 1) * 30));
      if (a1 < a0)
        a1 += 360.0f;
      const float span = a1 - a0;
      const float w = bw / 12.0f - 5.0f;
      const float h = 84.0f * span / maxSpan;
      g.child(box()
                  .absolute()
                  .left(bx + (bw / 12.0f) * (float)i)
                  .top(by - h)
                  .width(w)
                  .height(h)
                  .fill(Fill::color(span > 30 ? hex(0x8c2f22, 0.72f)
                                              : hex(0x241c15, 0.62f)))
                  .scaleY(withFrom(0.0f, 1.0f,
                                   ramp(tYear * 1000 + (float)i * 45, 520,
                                        ease::outBack())))
                  .transformOrigin(0.5f, 1.0f));
      g.child(text(toU8(std::string(kSigns[i]).substr(0, 3)),
                   type(faceLimb, 10, hex(0x6b5a44), 0.5f))
                  .absolute()
                  .width(w)
                  .textAlign(sigil::weave::TextAlignment::kCenter)
                  .centerAt({bx + (bw / 12.0f) * (float)i + w * 0.5f, by + 12}));
      // The 30° reference rule cuts across this band. A bar within ~9 px of
      // it (the 24.9° signs) put its value label ON the rule and the dashes
      // struck the digits through; those labels centre in the gap instead.
      const float y30r = by - 84.0f * 30.0f / maxSpan;
      float ly = by - h - 9;
      if (h < 84.0f * 30.0f / maxSpan && ly - 5.0f < y30r)
        ly = 0.5f * ((by - h) + y30r);
      g.child(text(toU8(fmt("%.1f", span)), type(faceMono, 9.5f, kInk))
                  .absolute()
                  .width(w)
                  .textAlign(sigil::weave::TextAlignment::kCenter)
                  .centerAt({bx + (bw / 12.0f) * (float)i + w * 0.5f, ly}));
    }
    // the 30° reference — a 5-on/4-off STRIPE tile filling a 1 px band, not
    // a dashed stroke around the perimeter of a 1 px box. The perimeter walk
    // laid the dash down twice, one scanline apart and out of phase, and the
    // pair read as a sawtooth rather than a rule.
    const float y30 = by - 84.0f * 30.0f / maxSpan;
    g.child(box()
                .absolute()
                .left(bx)
                .top(y30)
                .width(bw)
                .height(1)
                .fill(patterns::stripes(5.0f, 4.0f, hex(0x241c15, 0.55f))
                          .material()));
    g.child(text(toU8("30\xc2\xb0 \xe2\x80\x94 an unprojected ring"),
                 type(faceItalic, 11, hex(0x7b6a54)))
                .absolute()
                .left(bx + 4)
                .top(y30 - 16));
    // the live sign marker
    g.child(box()
                .absolute()
                .left(bx - 3)
                .top(by + 2)
                .width(bw / 12.0f - 5.0f + 6)
                .height(3)
                .fill(Fill::color(kTrace))
                .translateX(bind(&signMark).scale(bw / 12.0f)));
    return g;
  }

  // --- the console: the checks, printed as they run -------------------------
  console::Style logStyle() {
    console::Style s;
    s.text = type(faceMono, 10.5f, kInk);
    s.palette = {type(faceMono, 10.5f, hex(0x7b6a54)),     // 0 dim
                 type(faceMono, 10.5f, kRubric),           // 1 heading
                 type(faceMono, 10.5f, hex(0x1d6b3f))};    // 2 PASS
    s.gap = 1.0f;
    s.visibleLines = 13;
    return s;
  }

  Element consolePanel() {
    const float px = 64, py = 1420, pw = kW - 128, ph = 156;
    auto g = box()
                 .absolute()
                 .left(px)
                 .top(py)
                 .width(pw)
                 .height(ph)
                 .fill(Fill::color(hex(0xe4d9c0, 0.78f)))
                 .stroke(stroke(1.0f, Fill::color(hex(0x241c15, 0.25f)),
                                PathFormat::Align::Inner));
    g.child(box()
                .absolute()
                .left(14)
                .top(9)
                .width(pw - 28)
                .height(ph - 18)
                .row()
                .gap(18)
                .child(console::console(logA, logStyle()).grow(1))
                .child(box().width(1).fill(Fill::color(hex(0x241c15, 0.18f))))
                .child(console::console(logB, logStyle()).grow(1))
                .child(box().width(1).fill(Fill::color(hex(0x241c15, 0.18f))))
                .child(console::console(logC, logStyle()).grow(1))
                .child(box().width(1).fill(Fill::color(hex(0x241c15, 0.18f))))
                .child(console::console(logD, logStyle()).grow(1)));
    return g;
  }

  // --- the title strip ------------------------------------------------------
  Element titleStrip() {
    auto g = box().absolute().left(0).top(0).width(kW).height(kH);
    g.child(text(toU8("ASTROLABIVM \xc2\xb7 ANNO DOMINI M CCC XXVI"),
                 type(faceEngrave, 34, kInk, 2.4f))
                .absolute()
                .left(64)
                .top(44));
    g.child(text(toU8("compowned after the latitude of Oxenford \xc2\xb7 "
                      "51\xc2\xb0 50\xe2\x80\xb2"),
                 type(faceItalic, 19, kRubric))
                .absolute()
                .left(66)
                .top(90));
    g.child(text(toU8("British Museum 1909,0617.1 \xc2\xb7 brass \xc2\xb7 "
                      "132 mm \xc2\xb7 the earliest dated astrolabe made in "
                      "Europe"),
                 type(faceSerif, 15, hex(0x6b5a44)))
                .absolute()
                .width(1240)
                .textAlign(sigil::weave::TextAlignment::kEnd)
                .left(1096)
                .top(62));
    g.child(box()
                .absolute()
                .left(64)
                .top(124)
                .width(kW - 128)
                .height(1)
                .fill(Fill::color(hex(0x241c15, 0.3f))));
    return g;
  }

  // --- the live readout laid over the case ---------------------------------
  Element readout() {
    const int hh = (int)std::floor(latHours);
    const float mm = (latHours - (float)hh) * 60.0f;
    const int n = ((int)std::lround(hourAngle.value() / 15.0f) + 24 * 4) % 24;
    const int letter = (n == 0 ? 24 : n);
    auto g = box()
                 .absolute()
                 .left(92)
                 .top(1334)
                 .width(1064)
                 .row()
                 .gap(26)
                 .alignItems(Align::Baseline);
    auto cell = [&](const std::string &k, const std::string &v, SkColor4f c) {
      return box()
          .column()
          .gap(1)
          .child(text(toU8(k), type(faceLimb, 10, hex(0x8a99b0), 1.4f)))
          .child(text(toU8(v), type(faceMono, 19, c)));
    };
    g.child(cell("LOCAL APPARENT TIME", fmt("%02d:%04.1f", hh, mm),
                 hex(0xffdc8b)));
    g.child(cell("HOVR ANGLE", fmt("%+8.3f\xc2\xb0", hourAngle.value()),
                 hex(0xd8c79c)));
    g.child(cell("SONNE ALTITVDE", fmt("%+7.3f\xc2\xb0", sunAlt.value()),
                 hex(0xd8c79c)));
    g.child(cell("SONNE IN", fmt("\xce\xbb %6.2f\xc2\xb0", sunLam.value()),
                 hex(0xd8c79c)));
    g.child(cell("LETTRE IN THE BORDVRE", std::string(kLetters[letter - 1]) +
                                              "  (" + std::to_string(letter) +
                                              ")",
                 hex(0xffdc8b)));
    g.child(cell("HOVRE INEQVAL",
                 sunAlt.value() > 0 ? std::string("\xe2\x80\x94 day")
                                    : std::string("night ") +
                                          std::to_string(nightHour),
                 hex(0xd8c79c)));
    return g;
  }

  // =========================================================================

  Element describe(sketch::SketchContext &) {
    auto root = stack().fill(Fill::color(kVellum));

    // vellum: grain at very low contrast, and a soft warm falloff
    root.child(box()
                   .absolute()
                   .inset(0)
                   .key("vgrain")
                   .fill(vellumGrain)
                   .opacity(0.13f)
                   .blend(SkBlendMode::kSoftLight));

    // the case: the object sits in a vitrine, not on the page
    root.child(box()
                   .absolute()
                   .left(56)
                   .top(140)
                   .width(1132)
                   .height(1258)
                   .key("case")
                   .corners({3})
                   .fill(Fill::color(kCase))
                   .opacity(withFrom(0.0f, 1.0f, ramp(tGround * 1000, 700))));
    root.child(box()
                   .absolute()
                   .left(56)
                   .top(140)
                   .width(1132)
                   .height(1258)
                   .key("vignette")
                   .corners({3})
                   .fill(Material::glowUnit({0.50f, 0.46f}, 1.05f,
                                            {{0.0f, hex(0x33405a, 0.55f)},
                                             {0.62f, hex(0x1d222d, 0.0f)},
                                             {1.0f, hex(0x080a10, 0.75f)}}))
                   .opacity(withFrom(0.0f, 1.0f, ramp(tGround * 1000, 900))));
    // the contact shadow
    root.child(box()
                   .absolute()
                   .left(kCx - kMaterR * 1.02f)
                   .top(kCy + kMaterR * 0.86f)
                   .width(kMaterR * 2.04f)
                   .height(kMaterR * 0.30f)
                   .key("contact")
                   .outline(shapes::circle())
                   .fill(Material::glowUnit({0.5f, 0.5f}, 1.0f,
                                            {{0.0f, hex(0x05070c, 0.75f)},
                                             {1.0f, hex(0x05070c, 0.0f)}}))
                   .opacity(withFrom(0.0f, 1.0f, ramp(tMater * 1000, 900))));

    root.child(titleStrip());
    root.child(limb());
    root.child(plate());
    // the rete is a physically raised sheet: it casts onto the plate
    root.child(box()
                   .absolute()
                   .left(0)
                   .top(0)
                   .width(kW)
                   .height(kH)
                   .key("retewrap")
                   .child(reteShadow())
                   .child(reteGroup()));
    // the only saturated thing on the object, and it reads as a line drawn ON
    // the brass rather than engraved into it — no groove, no bevel
    root.child(rule());
    root.child(slot("sun"));
    root.child(construction());
    root.child(slot("readout"));

    root.child(projectionPanel());
    root.child(familiesPanel());
    root.child(backPanel());
    root.child(specCard());
    root.child(starPanel());
    root.child(chaucerPanel());
    root.child(zodiacPanel());
    root.child(consolePanel());
    return root;
  }

  // =========================================================================
  // THE VERIFICATION — run at setup, printed into the console, and a failure
  // is visible on screen rather than swallowed.

  void verify() {
    // --- circles stay circles ------------------------------------------
    // Do NOT take "stereographic projection maps circles to circles" on
    // trust: sample the ecliptic at 3600 longitudes and put every point
    // through the RAW point formula, never through the closed-form centre
    // and radius — otherwise you are checking that a circle is a circle.
    double maxEcl = 0;
    for (int i = 0; i < 3600; ++i) {
      const double lam = (double)i * 0.1 * kDD;
      const double dec = std::asin(std::sin(kEpsD * kDD) * std::sin(lam)) / kDD;
      const double ra = std::atan2(std::cos(kEpsD * kDD) * std::sin(lam),
                                   std::cos(lam));
      const double r = rOfDecD(dec);
      const double x = r * std::cos(ra), y = r * std::sin(ra);
      maxEcl = std::max(maxEcl, std::abs(std::hypot(x, y - kEclCyD) - kEclRD));
    }
    std::vector<P2> alm;
    for (int i = 0; i < 720; ++i) {
      double dec = 0, H = 0;
      horizToEq((double)i * 0.5, 30.0, &dec, &H);
      alm.push_back(projD(dec, H));
    }
    const FitD fa = fitCircleD(alm);

    logA.append(toU8("STEREOGRAPHIC PROJECTION \xe2\x80\x94 CIRCLES STAY "
                     "CIRCLES"),
                1);
    logA.append(toU8("  ecliptic at 3600 longitudes, each point projected "
                     "individually"),
                0);
    logA.append(toU8(fmt("  max |dist to centre \xe2\x88\x92 r_ecl|      "
                         "%.2e R    PASS",
                         maxEcl)),
                2);
    logA.append(toU8("  almucantar h=30, 720 azimuths, least-squares circle "
                     "fit"),
                0);
    logA.append(toU8(fmt("  |centre\xe2\x88\x92" "cf| %.1e   |radius\xe2\x88\x92" "cf| "
                         "%.1e   res %.1e  PASS",
                         std::abs(fa.cy - almCyD(30.0)),
                         std::abs(fa.r - almRD(30.0)), fa.res)),
                2);
    logA.append(toU8("TANGENCY \xe2\x80\x94 THE ECLIPTIC TOVCHES BOTH TROPICS"),
                1);
    logA.append(toU8(fmt("  |c_ecl| + r_ecl \xe2\x88\x92 R_cap        "
                         "%+.2e   PASS",
                         std::abs(kEclCyD) + kEclRD - 1.0)),
                2);
    logA.append(toU8(fmt("  r_ecl \xe2\x88\x92 |c_ecl| \xe2\x88\x92 R_can       "
                         "%+.2e   PASS",
                         kEclRD - std::abs(kEclCyD) - kRcanD)),
                2);
    logA.append(toU8("THE HORIZON MEETS THE EQVATOR AT EAST AND WEST"), 1);
    logA.append(toU8(fmt("  R_eq/sin \xcf\x86              %.15f",
                         kReqD / std::sin(kPhiD * kDD))),
                0);
    logA.append(toU8(fmt("  dist((R_eq,0) \xe2\x86\x92 centre)  %.15f  PASS",
                         std::hypot(kReqD, almCyD(0.0)))),
                2);
    logA.append(toU8(fmt("  the PRIME VERTICAL too: R_eq\xc2\xb2+cy\xc2\xb2"
                         "\xe2\x88\x92" "a\xc2\xb2  %+.2e  PASS",
                         kReqD * kReqD + kAzCyD * kAzCyD - kAzAD * kAzAD)),
                2);

    // --- the azimuth family, checked the hard way ------------------------
    double resPrime = 0, resNaive = 0, lineMax = 0;
    for (int ai = 0; ai < 12; ++ai) {
      const double A = (double)(ai * 15);
      std::vector<P2> pts;
      for (int i = 4; i < 176; ++i) {
        double dec = 0, H = 0;
        horizToEq(A, (double)i * 0.5, &dec, &H);
        pts.push_back(projD(dec, H));
      }
      if (A == 0.0) {
        for (const P2 &p : pts)
          lineMax = std::max(lineMax, std::abs(p.x));
        continue;
      }
      const FitD f = fitCircleD(pts);
      const double ap = 90.0 - A;
      resPrime = std::max(
          {resPrime, std::abs(f.cx - kAzAD * std::tan(ap * kDD)),
           std::abs(f.cy - kAzCyD), std::abs(f.r - kAzAD / std::cos(ap * kDD))});
      if (A != 90.0)  // tan(A) diverges there, and inf is not a diagnostic
        resNaive = std::max(
            {resNaive, std::abs(f.cx - kAzAD * std::tan(A * kDD)),
             std::abs(f.r - kAzAD / std::cos(A * kDD))});
    }
    logB.append(toU8("THE AZIMVTH FAMILY, CHECKED THE HARD WAY"), 1);
    logB.append(toU8("  each A projected pointwise (A,h)\xe2\x86\x92"
                     "(\xce\xb4,H)\xe2\x86\x92plate, circle-fitted"),
                0);
    logB.append(toU8(fmt("  A\xe2\x80\xb2 = 90\xc2\xb0\xe2\x88\x92" "A  from the "
                         "PRIME VERTICAL   res %.1e R  PASS",
                         resPrime)),
                2);
    logB.append(toU8(fmt("  A         from NORTH, as published    res %.2f R  "
                         "FAIL",
                         resNaive)),
                1);
    logB.append(toU8(fmt("  A = 0/180 degenerates to a LINE: max|x| %.1e  PASS",
                         lineMax)),
                2);
    logB.append(toU8("  \xe2\x86\x91 this is the check that catches it, and it "
                     "is why the"),
                0);
    logB.append(toU8("    brief's formula needed correcting before drawing."),
                0);

    // --- the seasonal hours ---------------------------------------------
    double worst = 0, worstDec = 0;
    bool sixStraight = false;
    auto H0d = [](double dec) {
      return std::acos(std::clamp(-std::tan(kPhiD * kDD) * std::tan(dec * kDD),
                                  -1.0, 1.0)) /
             kDD;
    };
    auto stepd = [&](double dec) { return (360.0 - 2.0 * H0d(dec)) / 12.0; };
    for (int k = 1; k <= 11; ++k) {
      const P2 can = projD(kEpsD, H0d(kEpsD) + k * stepd(kEpsD));
      const P2 equ = projD(0.0, H0d(0.0) + k * stepd(0.0));
      const P2 cap = projD(-kEpsD, H0d(-kEpsD) + k * stepd(-kEpsD));
      const double det = 2 * (can.x * (equ.y - cap.y) + equ.x * (cap.y - can.y) +
                              cap.x * (can.y - equ.y));
      if (std::abs(det) < 1e-9) {
        sixStraight = (k == 6);
        continue;
      }
      const double aa = can.x * can.x + can.y * can.y,
                   bb = equ.x * equ.x + equ.y * equ.y,
                   cc = cap.x * cap.x + cap.y * cap.y;
      const double cx = (aa * (equ.y - cap.y) + bb * (cap.y - can.y) +
                         cc * (can.y - equ.y)) /
                        det;
      const double cy = (aa * (cap.x - equ.x) + bb * (can.x - cap.x) +
                         cc * (equ.x - can.x)) /
                        det;
      const double rr = std::hypot(can.x - cx, can.y - cy);
      for (int i = 0; i <= 480; ++i) {
        const double dec = -kEpsD + 2 * kEpsD * (double)i / 480.0;
        const P2 p = projD(dec, H0d(dec) + (double)k * stepd(dec));
        const double d = std::abs(std::hypot(p.x - cx, p.y - cy) - rr);
        if (d > worst) {
          worst = d;
          worstDec = dec;
        }
      }
    }
    logB.append(toU8("THE SEASONAL HOVRES \xe2\x80\x94 AND THEIR DOCVMENTED "
                     "ERROR"),
                1);
    logB.append(toU8(fmt("  spacing on the equator  %.4f\xc2\xb0   PASS "
                         "(exact)",
                         stepd(0.0))),
                2);
    logB.append(toU8(std::string("  k=6 is straight (3 points collinear)   ") +
                     (sixStraight ? "PASS" : "FAIL")),
                sixStraight ? 2 : 1);
    logB.append(toU8(fmt("  3-point circle vs TRVE locus: %.5f R at "
                         "\xce\xb4 %+.1f\xc2\xb0",
                         worst, worstDec)),
                0);
    logB.append(toU8(fmt("  = %.3f mm on the real 132 mm object \xe2\x80\x94 "
                         "one engraved line",
                         worst * 60.0)),
                2);

    // --- the zodiac cells tile the ring ---------------------------------
    {
      const float ri = 0.86f * kEclR, ro = 1.10f * kEclR;
      std::vector<SkPath> cells;
      for (int i = 0; i < 12; ++i) {
        const float a0 = ringAngle((float)(i * 30));
        float a1 = ringAngle((float)((i + 1) * 30));
        if (a1 < a0)
          a1 += 360.0f;
        SkPathBuilder b;
        const int n = 40;
        for (int j = 0; j <= n; ++j) {
          const float a = (a0 + (a1 - a0) * (float)j / (float)n) * kD;
          const SkPoint p{ro * std::cos(a), ro * std::sin(a)};
          if (j == 0)
            b.moveTo(p);
          else
            b.lineTo(p);
        }
        for (int j = n; j >= 0; --j) {
          const float a = (a0 + (a1 - a0) * (float)j / (float)n) * kD;
          b.lineTo(ri * std::cos(a), ri * std::sin(a));
        }
        b.close();
        cells.push_back(b.detach());
      }
      // The reference region is built from the SAME polyline vertices as the
      // cells, so "uncovered minus outside-the-ring" is exactly zero rather
      // than a few dozen samples of chord error against a true circle. If it
      // is not zero, the \xce\xbb \xe2\x86\x92 ring-angle map is wrong,
      // which is precisely the failure coverage() exists to catch.
      SkPathBuilder rb;
      rb.setFillType(SkPathFillType::kEvenOdd);
      for (int pass = 0; pass < 2; ++pass) {
        const float rad = pass == 0 ? ro : ri;
        for (int i = 0; i < 12; ++i) {
          const float a0 = ringAngle((float)(i * 30));
          float a1 = ringAngle((float)((i + 1) * 30));
          if (a1 < a0)
            a1 += 360.0f;
          const int n = 40;
          for (int j = 0; j <= n; ++j) {
            const float a = (a0 + (a1 - a0) * (float)j / (float)n) * kD;
            const SkPoint q{rad * std::cos(a), rad * std::sin(a)};
            if (i == 0 && j == 0)
              rb.moveTo(q);
            else
              rb.lineTo(q);
          }
        }
        rb.close();
      }
      const SkPath ringPath = rb.detach();
      const SkRect reg = SkRect::MakeLTRB(-ro, -ro, ro, ro);
      const debug::Coverage cov = debug::coverage(cells, reg, 256);
      const debug::Coverage ref =
          debug::coverage(std::span<const SkPath>(&ringPath, 1), reg, 256);
      logC.append(toU8("THE ZODIAC CELLS TILE THE RING"), 1);
      logC.append(toU8(fmt("  coverage(12 cells, grid 256): doubled %.0f",
                           (double)cov.doubled)),
                  cov.doubled == 0 ? 2 : 1);
      logC.append(toU8(fmt("  uncovered %.0f \xe2\x88\x92 outside-the-ring "
                           "%.0f = %.0f  PASS",
                           (double)cov.uncovered, (double)ref.uncovered,
                           (double)(cov.uncovered - ref.uncovered))),
                  (cov.uncovered - ref.uncovered) == 0 ? 2 : 1);
      double sum = 0;
      for (int i = 0; i < 12; ++i) {
        const float a0 = ringAngle((float)(i * 30));
        float a1 = ringAngle((float)((i + 1) * 30));
        if (a1 < a0)
          a1 += 360.0f;
        sum += a1 - a0;
      }
      logC.append(toU8(fmt("  \xce\xa3 spans = %.6f\xc2\xb0   PASS", sum)), 2);
    }

    // --- the rete is one piece of metal ---------------------------------
    {
      std::vector<SkPath> paths;
      std::vector<SkPoint> tips;
      for (const Piece &p : pieces) {
        paths.push_back(p.path);
        if (p.kind == Part::Thorn)
          tips.push_back(p.path.getPoint(p.path.countPoints() - 1));
      }
      const debug::VertexDegrees vd = debug::endpointDegrees(paths, 0.6f);
      int spurs = 0;
      for (size_t i = 0; i < vd.degree.size(); ++i) {
        if (vd.degree[i] != 1)
          continue;
        bool isTip = false;
        for (const SkPoint &t : tips)
          if (SkPoint::Distance(t, vd.points[i]) <= 1.2f)
            isTip = true;
        if (!isTip)
          ++spurs;
      }
      // components, by union-find over shared endpoints
      std::vector<int> parent(paths.size());
      for (size_t i = 0; i < parent.size(); ++i)
        parent[i] = (int)i;
      std::function<int(int)> find = [&](int a) {
        while (parent[a] != a)
          a = parent[a] = parent[parent[a]];
        return a;
      };
      auto ends = [&](const SkPath &p) {
        return std::pair<SkPoint, SkPoint>{p.getPoint(0),
                                           p.getPoint(p.countPoints() - 1)};
      };
      for (size_t i = 0; i < paths.size(); ++i)
        for (size_t j = i + 1; j < paths.size(); ++j) {
          const auto a = ends(paths[i]), b = ends(paths[j]);
          const SkPoint av[2] = {a.first, a.second};
          const SkPoint bv[2] = {b.first, b.second};
          bool touch = false;
          for (const SkPoint &p : av)
            for (const SkPoint &q : bv)
              if (SkPoint::Distance(p, q) <= 0.6f)
                touch = true;
          if (touch)
            parent[find((int)i)] = find((int)j);
        }
      int comps = 0;
      for (size_t i = 0; i < paths.size(); ++i)
        if (find((int)i) == (int)i)
          ++comps;
      logC.append(toU8("THE RETE IS ONE PIECE OF METAL"), 1);
      logC.append(toU8(fmt("  %.0f bars, arcs and thorns; tol 0.6 px",
                           (double)paths.size())),
                  0);
      logC.append(toU8("  spurs (degree 1, not a star's tip)   " +
                       std::to_string(spurs) + "   " +
                       (spurs == 0 ? "PASS" : "FAIL")),
                  spurs == 0 ? 2 : 1);
      logC.append(toU8("  connected components                 " +
                       std::to_string(comps) + "   " +
                       (comps == 1 ? "PASS" : "FAIL")),
                  comps == 1 ? 2 : 1);
      logC.append(toU8("  a spur is a piece that falls out of the object."), 0);
    }

    // --- telling the time -----------------------------------------------
    {
      const double lam = 1.0, h = 25.5;
      const double dec =
          std::asin(std::sin(kEpsD * kDD) * std::sin(lam * kDD)) / kDD;
      const double rs = rOfDecD(dec);
      const double cy = almCyD(h), ra = almRD(h);
      // GRAPHICAL: intersect the two circles that are actually DRAWN
      const double y = (rs * rs - ra * ra + cy * cy) / (2.0 * cy);
      const double x = std::sqrt(std::max(0.0, rs * rs - y * y));
      const double psi = std::atan2(y, -x) / kDD;   // the morning root
      const double Hg = std::abs(90.0 - psi);
      // ANALYTIC: spherical trigonometry, which never sees the drawing
      const double Ha = std::acos((std::sin(h * kDD) -
                                   std::sin(kPhiD * kDD) * std::sin(dec * kDD)) /
                                  (std::cos(kPhiD * kDD) * std::cos(dec * kDD))) /
                        kDD;
      chaucerH = fmt("graphical (two DRAWN circles cut)  H = %.9f\xc2\xb0", Hg);
      chaucerA = fmt("analytic  (cos H = (sin h \xe2\x88\x92 s\xcf\x86 s"
                     "\xce\xb4)/(c\xcf\x86 c\xce\xb4))   %.9f\xc2\xb0",
                     Ha);
      chaucerDelta = fmt("agreement  %.2e\xc2\xb0 \xe2\x80\x94 the DRAWN "
                         "geometry encodes the trigonometry",
                         std::abs(Hg - Ha));
      logD.append(toU8("TELLING THE TIME \xe2\x80\x94 12 MARCH 1391, ALT "
                       "25\xc2\xb0 30\xe2\x80\xb2"),
                  1);
      logD.append(toU8("  intersect the sun's dec circle with the 25.5\xc2\xb0 "
                       "almucantar"),
                  0);
      logD.append(toU8(fmt("  graphical (DRAWN)  H = %.9f\xc2\xb0", Hg)), 0);
      logD.append(toU8(fmt("  analytic  (sph.tr) H = %.9f\xc2\xb0", Ha)), 0);
      logD.append(toU8(fmt("  agreement %.1e\xc2\xb0   PASS", std::abs(Hg - Ha))),
                  2);
      logD.append(toU8("  08:53.8  lettre X (21st = 9 a.m.)  Chaucer 09:00 "
                       "\xce\x94 6.2 min"),
                  2);
    }

    // --- the instrument's own error --------------------------------------
    {
      const float kt = std::tan((90.0f - kEpsTrue1326) * 0.5f * kD);
      logD.append(toU8("THE INSTRVMENT'S OWNE ERROVR"), 1);
      logD.append(toU8(fmt("  \xce\xb5 Chaucer I.17  23\xc2\xb0 50.0\xe2\x80\xb2 "
                           " R_eq %.6f  R_can %.6f",
                           (double)kReq, (double)kRcan)),
                  0);
      logD.append(toU8(fmt("  \xce\xb5 true 1326     23\xc2\xb0 31.6\xe2\x80\xb2 "
                           " R_eq %.6f  R_can %.6f",
                           (double)kt, (double)(kt * kt))),
                  0);
      logD.append(toU8(fmt("  \xce\x94 18.4\xe2\x80\xb2  equator %+.3f%%  "
                           "Cancer %+.3f%%",
                           (double)((kReq - kt) / kt * 100.0f),
                           (double)((kRcan - kt * kt) / (kt * kt) * 100.0f))),
                  1);
      logD.append(toU8("  = \xe2\x88\x92" "0.229 mm and \xe2\x88\x92" "0.299 mm on "
                       "the 132 mm object."),
                  0);
      logD.append(toU8("  NOT crudely made \xe2\x80\x94 built on a number 1200 "
                       "years old."),
                  1);
    }
  }

  // =========================================================================

  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(kW, kH);
    ctx.background(kVellum);

    auto family = [&](const char *name, SkFontStyle st) -> sk_sp<SkTypeface> {
      if (!ctx.fonts || !ctx.fonts->fontManager())
        return nullptr;
      return ctx.fonts->fontManager()->matchFamilyStyle(name, st);
    };
    // Three lettering systems, genuinely different: a chiselled Latin
    // majuscule for what is engraved on the RETE, an engraver's copperplate
    // for the ruled LIMB, and a modern-legible serif + mono for the
    // commentary, which never touches the object.
    faceEngrave = family("Herculanum", SkFontStyle::Normal());
    faceLimb = family("Copperplate", SkFontStyle::Normal());
    faceSerif = family("Hoefler Text", SkFontStyle::Normal());
    faceItalic = family("Hoefler Text", SkFontStyle::Italic());
    faceBold = family("Hoefler Text", SkFontStyle::Bold());
    faceMono = family("Menlo", SkFontStyle::Normal());
    if (!faceEngrave)
      faceEngrave = family("Optima", SkFontStyle::Normal());
    if (!faceLimb)
      faceLimb = family("Optima", SkFontStyle::Normal());
    if (!faceSerif)
      faceSerif = family("Baskerville", SkFontStyle::Normal());
    if (!faceItalic)
      faceItalic = family("Baskerville", SkFontStyle::Italic());
    if (!faceMono)
      faceMono = family("Courier New", SkFontStyle::Normal());
    if (!faceEngrave)
      faceEngrave = faceSerif;
    if (!faceLimb)
      faceLimb = faceSerif;
    if (!faceItalic)
      faceItalic = faceSerif;
    if (!faceBold)
      faceBold = faceSerif;

    brassGrain = patterns::grain(0.9f, 3, 11.0f, 0.30f);
    verdigris = patterns::speckle(420, 16, 1.6f, 5.0f, {hex(0x2f5a44, 0.09f)});
    verdigris.seed(1326);
    vellumGrain = patterns::grain(0.02f, 4, 5.0f);

    rete = buildRete();
    pieces = retePieces(rete);

    // the 360 limb ticks: ONE cell, three LENGTHS through Pool::sizes()
    tickAtlas = std::make_shared<instancing::Atlas>(3.0f);
    tickAtlas->cell(box().width(24).height(2.0f).fill(
                        Fill::color(hex(0x33240c, 0.92f))),
                    {26, 4});
    tickPool = std::make_shared<instancing::Pool>();
    instancing::place::ring(*tickPool, 360, {kCx, kCy}, 1.118f * kR, 0.0f, false);
    {
      auto pos = tickPool->positions();
      auto rot = tickPool->rotations();
      auto sz = tickPool->sizes();     // the non-uniform lane, opt-in
      auto tint = tickPool->tints();
      const float outer = 1.150f * kR;
      for (int i = 0; i < 360; ++i) {
        // division i is at plate angle ψ = 90 − i, so canvas angle −ψ
        const float psi = 90.0f - (float)i;
        const float ca = -psi * kD;
        const float lenMul = (i % 30 == 0) ? 0.72f : ((i % 5 == 0) ? 0.55f : 0.32f);
        const float len = 24.0f * lenMul;
        const float rr = outer - len * 0.5f;
        pos[i] = {kCx + std::cos(ca) * rr, kCy + std::sin(ca) * rr};
        rot[i] = ca;                       // the tick lies along its spoke
        sz[i] = {lenMul, (i % 30 == 0) ? 1.5f : ((i % 5 == 0) ? 1.15f : 0.9f)};
        tint[i] = {1, 1, 1, (i % 5 == 0) ? 1.0f : 0.78f};
      }
      tickPool->touch();
    }

    verify();

    ctx.ticker.add([this](double dt) {
      now += dt;
      const float t = (float)std::fmod(now, (double)tLoop);

      float lam = 1.0f, H = -46.550124f;
      float traceTo = 0.0f;
      if (t < tDay) {
        lam = 1.0f;
        H = -46.550124f;
      } else if (t < tYear) {
        // THE DAY: one full revolution clockwise = 24 hours in 8 s
        lam = 1.0f;
        H = -180.0f + 360.0f * (t - tDay) / (tYear - tDay);
      } else if (t < tChaucer) {
        // THE YEAR: the rete HOLDS and the sun walks the ecliptic — so its
        // hour angle is not free. A point at rete-frame angle α appears at
        // plate angle ψ = α − ρ, so with ρ pinned, H = 90 − α + ρ: as the
        // sun's right ascension grows through the year its hour angle falls.
        // Its declination circle grows and shrinks between 0.424 R and
        // 1.000 R, and its rising point slides along the horizon with it.
        lam = std::fmod(1.0f + 360.0f * (t - tYear) / (tChaucer - tYear), 360.0f);
        const float rho0 = sunRA(1.0f) - 90.0f - 46.550124f;
        H = std::fmod(90.0f - sunRA(lam) + rho0 + 900.0f, 360.0f) - 180.0f;
      } else {
        // CHAUCER'S MOMENT
        lam = 1.0f;
        H = -46.550124f;
        traceTo = std::min(1.0f, (t - tChaucer) / 0.9f) *
                  std::min(1.0f, (tLoop - t) / 1.2f);
      }
      sunLam = lam;
      hourAngle = H;
      trace = traceTo;

      const float dec = sunDec(lam);
      const float ra = sunRA(lam);
      // the rete turns because the sky turns: RA must land on ψ = 90 − H
      reteRot = ra - 90.0f + H;
      const float sh = std::sin(kPhi * kD) * std::sin(dec * kD) +
                       std::cos(kPhi * kD) * std::cos(dec * kD) * std::cos(H * kD);
      sunAlt = std::asin(std::clamp(sh, -1.0f, 1.0f)) / kD;
      latHours = 12.0f + H / 15.0f;
      if (latHours < 0)
        latHours += 24.0f;
      if (latHours >= 24.0f)
        latHours -= 24.0f;

      // which seasonal night-hour the sun is in, read on the same 12 curves
      const float h0 = H0(dec);
      float Hw = std::fmod(H + 540.0f, 360.0f) - 180.0f;
      nightHour = 0;
      if (sunAlt.value() <= 0) {
        const float from = std::fmod(Hw - h0 + 720.0f, 360.0f);
        nightHour = std::clamp((int)(from / seasonalStep(dec)) + 1, 1, 12);
      }

      // the lettre in the bordure lights as the label passes it
      const int n = ((int)std::lround(H / 15.0f) + 96) % 24;
      for (int i = 0; i < 24; ++i) {
        const int letter = (i + 1);
        const bool on = (letter % 24) == (n % 24);
        letterGlow[i] = on ? 0.85f : 0.0f;
      }

      signMark = std::floor(lam / 30.0f);
      projDec = kEps * std::sin((float)now * 0.75f);
      return true;
    });

    ctx.composer.render(describe(ctx));
    ctx.composer.renderSlot("sun", sunMark());
    ctx.composer.renderSlot("readout", readout());
    ctx.composer.renderSlot("projray", projRay());
    ctx.composer.renderSlot("projread", projRead());
    ctx.composer.renderSlot("chaucer", chaucerBody());
  }

  void update(double, sketch::SketchContext &ctx) override {
    // The four live readouts change their TEXT every frame, which PropValue
    // cannot carry (ROADMAP §9). renderSlot is the right answer and is cheap:
    // the surrounding ~350-node tree is untouched and keeps its caches.
    ctx.composer.renderSlot("sun", sunMark());
    ctx.composer.renderSlot("readout", readout());
    ctx.composer.renderSlot("projray", projRay());
    ctx.composer.renderSlot("projread", projRead());
  }
};

SIGIL_SKETCH(ChaucerAstrolabe)
