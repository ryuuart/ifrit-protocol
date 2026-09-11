#pragma once

#include <include/core/SkCanvas.h>
#include <include/core/SkFont.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/Hatches.h>
#include <sigilcompose/brush/Lines.h>
#include <sigilcompose/brush/Rails.h>
#include <sigilcompose/brush/Ribbons.h>
#include <sigilcompose/brush/Stamps.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/kit/Strokes.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Faces.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmotion/Animation.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <glm/gtc/matrix_transform.hpp>
#include <optional>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace mskia = sigil::material::skia;
namespace mesh = sigil::geometry::mesh;
namespace shapes = sigil::geometry::shapes;
namespace weave = sigil::weave;

using namespace sigil::compose;
using namespace sigil::motion;
using namespace std::chrono_literals;

namespace bg3 {

// ---------------------------------------------------------------- the plate
constexpr float kW = 1200.0f;
/** The artefact's own height. Everything on the overlay is derived from
 *  it, so the caption band below can grow the canvas without moving
 *  anything on the plate. */
constexpr float kH = 1200.0f;
constexpr float kBandH = 118.0f;
constexpr float kCanvasH = kH + kBandH;

/** Everything is placed on radii from here. */
constexpr float kCx = 600.0f;
constexpr float kCy = 520.0f;

constexpr float kBezelOuter = 330.0f;  ///< 20-gon circumradius
constexpr float kBezelInner = 268.0f;
constexpr float kDieRadius = 174.0f;  ///< projected circumradius of the solid

/** 360/20 = 18 degrees per vertex. Pass 0.6x that; see the header. */
constexpr float kCornerAngle = 12.0f;

// ------------------------------------------------------------------ palette
// Illuminated-manuscript register.
// THE OVERLAY IS DARK. Baldur's Gate 3 drops a near-black scrim with a
// heavy vignette over the scene and lights the roundel and the die out of
// it: the roll is a thing happening in front of the world, not a page
// laid over it. This sheet was parchment, and every other difference from
// the reference followed from that one — a gilt band on cream is
// ornament, and the same band on black is metal.
//
// The inversion is TWO CONSTANTS, because every call site names the
// ground and the ink rather than a colour: the ground goes near-black and
// the ink goes bone. The gilt does not move, the die's own stone does not
// move, and the three status colours are lifted to where they read on
// black rather than re-chosen.
constexpr SkColor4f kVellum{0.043f, 0.037f, 0.031f, 1.0f};  // the scrim
constexpr SkColor4f kVellumDeep{0.086f, 0.071f, 0.055f, 1.0f};
constexpr SkColor4f kInk{0.878f, 0.839f, 0.745f, 1.0f};       // bone type
constexpr SkColor4f kGilt{0.788f, 0.635f, 0.153f, 1.0f};      // #C9A227
constexpr SkColor4f kGiltDark{0.549f, 0.420f, 0.082f, 1.0f};  // #8C6B15
constexpr SkColor4f kViridian{0.353f, 0.678f, 0.541f, 1.0f};
constexpr SkColor4f kOxblood{0.816f, 0.290f, 0.290f, 1.0f};
constexpr SkColor4f kAdvantage{0.435f, 0.827f, 0.435f, 1.0f};
constexpr SkColor4f kBone{0.871f, 0.824f, 0.706f, 1.0f};  // #DED2B4

inline Fill ink(float a = 1.0f) {
  return Fill::color(mskia::withAlpha(kInk, a));
}
inline Fill gilt(float a = 1.0f) {
  return Fill::color(mskia::withAlpha(kGilt, a));
}
inline Fill giltDark(float a = 1.0f) {
  return Fill::color(mskia::withAlpha(kGiltDark, a));
}

inline std::u8string u8(const std::string& s) {
  return std::u8string(reinterpret_cast<const char8_t*>(s.c_str()), s.size());
}

// ------------------------------------------------------- the roll, as typed
// StatsRollResult (:6708) — three separate fields, and the plate keeps them
// separate because that IS the behaviour being studied.
constexpr int kNaturalRoll = 12;        ///< StatsRollResult.NaturalRoll
constexpr int kDiscardedDiceTotal = 7;  ///< StatsRollResult.DiscardedDiceTotal
constexpr int kDC = 15;                 ///< the 5-step dialogue ladder
constexpr int kFinalTotal = 20;         ///< StatsRollResult.Total

/** One `ResolvedRollBonus` (:6129). `numDice`/`diceSize` non-zero means the
 *  row rolled for its own value and `resolved` is what it got. */
struct Bonus {
  const char* sourceName;   ///< SourceName    TranslatedString
  const char* description;  ///< Description   TranslatedString
  int bonus;                ///< Bonus         int32
  int numDice;              ///< NumDice       uint8
  const char* diceSize;     ///< DiceSize      DiceSizeId
  int resolved;             ///< ResolvedRollBonus int32
  double landsAt;           ///< s, after the die is still
};

// Modifiers arrive AFTER the roll, at 110 ms apart, starting at 1.45 s.
inline const std::array<Bonus, 3> kBonuses{{
    {"Charisma", "Ability modifier  (Charisma 16)", 3, 0, "", 3, 1.45},
    {"Proficiency", "ProficiencyBonus  (level 4)", 2, 0, "", 2, 1.56},
    {"Guidance", "Cantrip  Guidance", 0, 1, "D4", 3, 1.67},
}};

constexpr double kSettleAt = 1.45;   ///< the die is still
constexpr double kOutcomeAt = 1.95;  ///< the last row has counted in

/** `Ext_Enums.SkillId` (:33053) at its true ordinals — the engine's order,
 *  which is the governing-ability grouping, not alphabetical. */
struct Skill {
  int ordinal;
  const char* name;
};
inline const std::array<Skill, 18> kSkills{{
    {0, "Deception"},
    {1, "Intimidation"},
    {2, "Performance"},
    {3, "Persuasion"},
    {4, "Acrobatics"},
    {5, "SleightOfHand"},
    {6, "Stealth"},
    {7, "Arcana"},
    {8, "History"},
    {9, "Investigation"},
    {10, "Nature"},
    {11, "Religion"},
    {12, "Athletics"},
    {13, "AnimalHandling"},
    {14, "Insight"},
    {15, "Medicine"},
    {16, "Perception"},
    {17, "Survival"},
}};
/** The five blocks, as first-ordinal / count / AbilityId name+ordinal. */
struct AbilityBlock {
  int first, count, abilityOrdinal;
  const char* ability;
};
inline const std::array<AbilityBlock, 5> kBlocks{{
    {0, 4, 6, "CHARISMA"},
    {4, 3, 2, "DEXTERITY"},
    {7, 5, 4, "INTELLIGENCE"},
    {12, 1, 1, "STRENGTH"},
    {13, 5, 5, "WISDOM"},
}};
constexpr int kActiveSkill = 3;  ///< Persuasion

// ------------------------------------------------------------ the solid

// THE SOLID IS THE GEOMETRY KIT'S, in its welded form: twelve corners
// shared by twenty faces, so an edge is a pair of corner indices and the
// two faces that meet on it are found once rather than searched for by
// position. What is this study's own is the NUMBERING — a real d20 pairs
// opposite faces to sum to 21 — and the attitude the die settles at.
using V3 = glm::vec3;

/** Which of the twenty lands square to the viewer. One face of a regular
 *  solid is every face, so naming it makes the settle a declaration
 *  rather than a consequence of the order the faces came in. */
constexpr int kSettledFace = 0;

/** The die: the solid, what each face looks like from outside, the pip
 *  each one carries, and the edge list the three-tier edge weight reads.
 *  Each of an icosahedron's faces is one triangle, so face f is triangle
 *  f and its three corners are the triangle's three indices. */
struct Die {
  mesh::Mesh cage;
  std::vector<V3> normal, centroid;  // per face
  std::vector<int> pip;              // face -> 1..20
  std::vector<mesh::Edge> edges;     // 30, each with the two faces on it
  size_t faces() const { return normal.size(); }
  std::array<int, 3> corners(size_t face) const {
    return {(int)cage.indices[face * 3], (int)cage.indices[face * 3 + 1],
            (int)cage.indices[face * 3 + 2]};
  }
};

/** Number the faces so opposites sum to 21 and the face that settles
 *  square to the viewer carries the roll. The pairing is the solid's
 *  own — `opposedFace` measures it from the centroids — so nothing is
 *  hand-placed and nothing assumes an order. */
inline void numberFaces(Die& d, int settledFace, int frontPip) {
  const size_t n = d.faces();
  std::vector<int> across(n, -1);
  for (size_t f = 0; f < n; ++f)
    if (const std::optional<size_t> other = mesh::opposedFace(d.cage, f))
      across[f] = (int)*other;
  d.pip.assign(n, 0);
  d.pip[(size_t)settledFace] = frontPip;
  if (across[(size_t)settledFace] >= 0)
    d.pip[(size_t)across[(size_t)settledFace]] = 21 - frontPip;
  int next = 1;
  for (size_t f = 0; f < n; ++f) {
    if (d.pip[f] != 0) continue;
    while (next == frontPip || next == 21 - frontPip || next > 20) ++next;
    d.pip[f] = next;
    if (across[f] >= 0) d.pip[(size_t)across[f]] = 21 - next;
    ++next;
  }
}

inline Die buildDie(int settledFace, int frontPip) {
  Die d;
  d.cage = mesh::platonic(mesh::Platonic::Icosahedron,
                          {.circumradius = 1.0f, .sharedVertices = true});
  const size_t faces = mesh::faceCount(d.cage);
  for (size_t f = 0; f < faces; ++f) {
    d.normal.push_back(mesh::faceNormal(d.cage, f));
    d.centroid.push_back(mesh::faceCentroid(d.cage, f));
  }
  d.edges = mesh::edges(d.cage);
  numberFaces(d, settledFace, frontPip);
  return d;
}

/** An attitude as three turns, applied about x then y then z. */
inline glm::mat3 turn(float ax, float ay, float az) {
  glm::mat4 m(1.0f);
  m = glm::rotate(m, az, {0, 0, 1});
  m = glm::rotate(m, ay, {0, 1, 0});
  m = glm::rotate(m, ax, {1, 0, 0});
  return glm::mat3(m);
}

/** The settle attitude: the pose that turns the rolled face square to
 *  the viewer, then a tilt off that axis — because a perfectly face-on
 *  icosahedron projects to a symmetric figure and a symmetric figure
 *  reads as a badge. The tilt is what makes the surviving edges
 *  unequal. */
inline glm::mat3 settleAttitude(const Die& d, int face) {
  return turn(0.175f, -0.125f, 0.085f) *
         glm::mat3(mesh::faceUp(d.cage, (size_t)face, {0, 0, 1}));
}

/** The tumble: a fast spin decelerating into the settle, then two decaying
 *  bounces. One ramp cannot shape this. Settled by kSettleAt. */
inline void tumbleAngles(double t, float& ax, float& ay, float& az) {
  const float u = (float)std::clamp(t / 1.10, 0.0, 1.0);
  const float e = 1.0f - std::pow(1.0f - u, 5.0f);  // easeOutQuint
  const float remain = 1.0f - e;
  constexpr float kTau = 6.2831853f;
  ax = remain * 2.25f * kTau;
  ay = remain * 1.50f * kTau;
  az = remain * 0.75f * kTau;
  if (t > 0.95) {  // two decaying bounces, dead by 1.45
    const float w = (float)(t - 0.95);
    const float b = 0.17f * std::exp(-9.0f * w) * std::sin(w * 34.0f);
    ax += b;
    ay += b * 0.7f;
    az += b * 1.3f;
  }
  // AND THEN IT STOPS. A die that keeps breathing after it has landed is
  // a scene with no settled state, so a still of it is a still of a
  // moment nothing declared — and the one thing this study is about is
  // what happens AFTER the die lands. Past kSettleAt the three turns are
  // zero and the attitude is the settle attitude and nothing else.
}

}  // namespace bg3

// =============================================================================
