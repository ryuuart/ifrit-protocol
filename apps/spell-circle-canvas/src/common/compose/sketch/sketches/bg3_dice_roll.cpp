// bg3_dice_roll.cpp — BALDUR'S GATE 3, THE DIALOGUE ABILITY-CHECK ROLL
// =============================================================================
// Baldur's Gate 3 (Larian Studios, 2023; PC/PS5/XSX), patch-era UI. The screen:
// the dice-roll overlay that takes the screen when a conversation demands a
// skill check. One oversized d20 tumbles and settles inside an ornamented
// roundel, the target DC hangs above it, and — the detail that dates this UI —
// THE MODIFIERS ARE LISTED BELOW THE DIE AND ADDED AFTER IT LANDS, not folded
// into the number beforehand. Larian changed that deliberately so the screen
// reads the way a table reads: roll first, then do the sums. So the motion
// here is a DEPENDENCY CHAIN, not a stagger — nothing in the column exists
// until the die is still.
//
// Canvas 1200 x 1200. Square because the composition is radial; a 16:9
// letterbox would be a lie about where the weight sits.
//
// -----------------------------------------------------------------------------
// SOURCE — fetched and read, not remembered
//
//   github.com/Norbyte/bg3se — BG3Extender/IdeHelpers/ExtIdeHelpers.lua,
//   35,855 lines, the script extender's generated annotation of the ENGINE'S
//   OWN TYPE SURFACE. Every enum, class and field quoted below was grepped out
//   of that file at the line number given. Nothing here is from a wiki.
//
//   The icosahedron is not from anywhere. It is three numbers.
//
// -----------------------------------------------------------------------------
// FIVE CORRECTIONS TO MY OWN BRIEF, ALL FROM THE ENGINE'S TYPES
//
//  1. THE SKILL LIST'S REAL ORDER IS THE ABILITY GROUPING, AND THE BRIEF
//     PRINTED THE DOCSTRING ORDER.  The brief quotes `SkillId` off the
//     `--- @alias` at :1374, which is alphabetical — Acrobatics, AnimalHandling,
//     Arcana … — because that is a Lua type annotation, sorted for an IDE's
//     completion list. It is not the engine's order. `Ext_Enums.SkillId`
//     (:33053) is the ordinal table the engine actually indexes by, and it is
//     GROUPED BY GOVERNING ABILITY:
//
//         Deception 0  Intimidation 1  Performance 2  Persuasion 3    CHARISMA
//         Acrobatics 4  SleightOfHand 5  Stealth 6                    DEXTERITY
//         Arcana 7  History 8  Investigation 9  Nature 10  Religion 11
//                                                                 INTELLIGENCE
//         Athletics 12                                             STRENGTH
//         AnimalHandling 13  Insight 14  Medicine 15  Perception 16
//         Survival 17                                                 WISDOM
//         Invalid 18  Sentinel 19
//
//     Five blocks, 4 + 3 + 5 + 1 + 5 = 18, and CONSTITUTION GOVERNS NOTHING —
//     which is 5e's own skill table baked into the integers, in 5e's own
//     ability order read backwards from Charisma. Persuasion is ordinal 3, the
//     last Charisma skill. The tick ladder down the left margin IS this table,
//     drawn at its true ordinals with its five blocks bracketed; the ornament
//     is the data, and it is data the brief's alphabetical list destroyed.
//
//  2. THE MODIFIER ROW HAS A STRUCT, AND THE BRIEF DID NOT NAME IT.
//     `ResolvedRollBonus` (:6129) is exactly one row of the column:
//
//         --- @class ResolvedRollBonus
//         --- @field Bonus int32
//         --- @field Description TranslatedString
//         --- @field DiceSize DiceSizeId
//         --- @field NumDice uint8
//         --- @field ResolvedRollBonus int32
//         --- @field SourceName TranslatedString
//
//     A row is a source name and a description carrying EITHER a flat `Bonus`
//     OR `NumDice` x `DiceSize` whose actual roll lands in `ResolvedRollBonus`.
//     That is the Guidance `+1d4 -> 3` row, typed, and it is why Guidance
//     prints two numbers where Proficiency prints one. Every row on this plate
//     prints its own fields in the right-hand gutter.
//
//  3. THE DISCARDED ADVANTAGE DIE IS A FIELD, NOT AN INFERENCE.  The brief
//     reconstructs the dimmed second d20 from the advantage rule. It did not
//     need to. `StatsRollResult` (:6708):
//
//         --- @field Critical RollCritical
//         --- @field CriticalThreshold uint32?
//         --- @field DiscardedDiceTotal int32
//         --- @field NaturalRoll int32
//         --- @field RollsCount RerollValue[]
//         --- @field Total int32
//
//     `DiscardedDiceTotal` rides alongside `NaturalRoll`, so the loser of an
//     advantage pair is a value the UI is HANDED, and drawing it dimmed is
//     reporting a field rather than illustrating a rule. Both are on the plate,
//     and `NaturalRoll` (12) and `Total` (20) are labelled as the two separate
//     numbers they are — which is the whole point of adding after the landing.
//
//  4. RollCritical's NUMERIC ORDER IS NOT ITS ALPHABETICAL ONE.  The alias at
//     :1342 reads `Fail|None|Success` and the brief copied it. The engine's
//     table (:31644) is `None = 0, Success = 1, Fail = 2`. The plate prints the
//     ordinals, because a state machine's order is part of the state machine.
//
//  5. DIALOGUE GRANTS ADVANTAGE AND PROFICIENCY BY TWO PARALLEL PATHS.  The
//     brief singles out `AdvantageContext.SourceDialogue` (ordinal 8, :24692) as
//     "literally the enum for this conversation granted you advantage", and it
//     is. But `ProficiencyBonusBoostType` (:1327) carries its own
//     `SourceDialogue` member, so a conversation can grant PROFICIENCY by a
//     separate boost path with a separate enum. Both are named on the plate.
//
//     (Also: `AbilityId` (:24592) is 1-based — None = 0, Strength = 1 …
//     Charisma = 6, Sentinel = 7 — not the bare six the brief lists.)
//
// -----------------------------------------------------------------------------
// THE NUMBERS
//
// DOCUMENTED (engine types, quoted above, plus published 5e):
//   every enum name, spelling and ordinal; the ResolvedRollBonus row schema;
//   NaturalRoll / DiscardedDiceTotal / Total as three separate fields; the
//   modifiers-after-the-roll behaviour; advantage = 2d20 keep highest;
//   ability modifier = floor((score - 10) / 2); the proficiency ladder
//   +2 (1-4) / +3 (5-8) / +4 (9-12) AND BG3'S LEVEL-12 CAP, so +4 is the
//   ceiling and +5 is never drawn; the 5-step dialogue DC ladder; the
//   icosahedron.
// RECONSTRUCTED: this specific check and its character, the palette, the type,
//   the bezel radii, the ornament, and the whole of the layout geometry.
//
//   PERSUASION                  DC 15        SkillId ordinal 3, Charisma block
//   d20 NaturalRoll              12
//   Charisma modifier           + 3          floor((16 - 10) / 2)
//   Proficiency                 + 2          level 4  ->  +2   (cap +4)
//   Guidance                  +1d4 -> 3      NumDice 1, DiceSize D4
//                             ---------
//   Total                        20          >= DC 15   ->   SUCCESS
//
// -----------------------------------------------------------------------------
// THE DIE IS A SOLID, AND IT IS DERIVED
//
// Twelve vertices at the cyclic permutations of (0, +-1, +-phi). Twenty faces
// found by taking every triple whose three pairwise distances are the edge
// length 2 — not a table, a search, so a wrong phi produces no faces at all
// rather than a wrong solid. Thirty edges fall out of the faces. Each face is
// oriented outward against its own centroid, rotated, projected orthographically
// and culled by normal . view <= 0.
//
// The read comes entirely from a THREE-TIER EDGE WEIGHT, because a 2D library
// has no shading to lean on:
//     an edge shared by TWO visible faces  -> interior, 1.1 px
//     an edge shared by ONE visible face   -> silhouette, 2.6 px
//     an edge shared by NO visible face    -> omitted entirely
// Nine or ten triangles survive, and their edges are not symmetric. That
// asymmetry is the only reason it reads as a solid rather than a badge.
//
// The pips are derived too: faces pair antipodally (centroid ~ -centroid), and
// a real d20 numbers opposite faces to sum to 21. So the settled front face is
// given 12 — the roll — its antipode gets 9, and the remaining nine pairs take
// (n, 21-n). Nothing is hand-placed.
//
// -----------------------------------------------------------------------------
// LINE VOCABULARY — gilt on vellum, where a lone 1 px hairline is a mistake
//
//  * THE BEZEL IS A 20-GON, one flat per face of the die, and it carries the
//    whole border vocabulary at twenty vertices: `weightedCorners` on the outer
//    ring so the rule thickens where it turns, `gappedRule` on the inner so it
//    stops short at every flat, `brackets` as a twenty-tick ladder that indexes
//    the die's twenty faces, and a `PatternBrush` whose CORNER tile is a gilt
//    fleuron on the bisector with side tiles run between. See the CORNER ANGLE
//    note below — all four of those need to be told what a corner is.
//  * `lines::heavyHairHeavy` for the principal rule and `lines::dottedCore` for
//    the secondary — shipped helpers, not hand-built Rails.
//  * A hand-built `Rails` set on the DC plate with one rail's `dashPhase` slid
//    against its neighbours, which is what makes a doubled rule read as
//    ENGRAVED rather than printed twice.
//  * `lines::radialHatch` as the rosette behind the die and `lines::concentric`
//    inside the bezel band, both clipped to their own annulus.
//  * `Hatch` with `spacingBinding` AND `angleBinding` for the outcome wash:
//    as the sum lands, the success field hatches in behind the total, pitch
//    driven 14 px -> 3 px and angle swinging 12 degrees. Two bound Outputs on
//    one decoration, and the right idiom because the value being encoded is
//    CONFIDENCE, not a quantity.
//  * `brushes::Ribbon` with a calligraphic nib for the flourishes off the DC
//    plate — variable width is the whole point of a nib.
//  * `shapes::notched` on the DC plate so it reads as a hung sign;
//    `shapes::chamfered` on the portrait tablet and the outcome banner.
//
// -----------------------------------------------------------------------------
// THE CORNER ANGLE — the predicted break, confirmed, and what it cost
//
// `brackets`, `gappedRule` and `weightedCorners` all route through
// `detail::cornerWindows(src, radius, keep, angleDeg)`, and `cornerDistances`
// only records a vertex whose per-sample tangent break EXCEEDS that threshold.
// The default is 30 degrees. A regular 20-gon turns 360/20 = 18 degrees per
// vertex, and cos(18) = 0.9511 is NOT below cos(30) = 0.8660 — so at the
// default the bezel finds ZERO corners, brackets draw nothing, and the gapped
// rule degenerates to the whole contour. Blank ornament from an API doing
// exactly what it was told. Confirmed here at 30 and fixed at 12, where
// cos(18) = 0.9511 < cos(12) = 0.9781 and all twenty land.
//
// Every corner-bearing decoration on this plate therefore passes ~12 degrees
// EXPLICITLY, and `brushes::PatternBrush` needs it separately — its own
// `cornerAngleDeg` field defaults to 35, higher still.
//
// The default is deliberately not adaptive, and having lived with it I agree:
// the scan steps 2 px, so a rounded corner of radius 14 turns ~8.5 degrees per
// sample against this polygon's 18 — less than 2x apart — and any default low
// enough to catch the 20-gon would invent corners on every rounded rect in the
// corpus. Report at the bottom of this file.
//
// -----------------------------------------------------------------------------

#include <sigilsketch/Sketch.h>

#include <sigilcompose/Brushes.h>
#include <sigilcompose/Decorations.h>
#include <sigilcompose/Lines.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Shapes.h>
#include <sigilcompose/Util.h>

#include <sigilweave/ports/SystemFontManager.h>

#include <include/core/SkCanvas.h>
#include <include/core/SkFont.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace sigil::compose;
using namespace sigil::compose::util;
using namespace std::chrono_literals;
namespace weave = sigil::weave;

namespace bg3 {

// ---------------------------------------------------------------- the plate
constexpr float kW = 1200.0f;
constexpr float kH = 1200.0f;

/** Everything is placed on radii from here. */
constexpr float kCx = 600.0f;
constexpr float kCy = 520.0f;

constexpr float kBezelOuter = 330.0f; ///< 20-gon circumradius
constexpr float kBezelInner = 268.0f;
constexpr float kDieRadius = 174.0f;  ///< projected circumradius of the solid

/** 360/20 = 18 degrees per vertex. Pass 0.6x that; see the header. */
constexpr float kCornerAngle = 12.0f;

// ------------------------------------------------------------------ palette
// Illuminated-manuscript register.
constexpr SkColor4f kVellum{0.910f, 0.863f, 0.753f, 1.0f};   // #E8DCC0
constexpr SkColor4f kVellumDeep{0.796f, 0.733f, 0.604f, 1.0f};
constexpr SkColor4f kInk{0.141f, 0.110f, 0.078f, 1.0f};      // #241C14
constexpr SkColor4f kGilt{0.788f, 0.635f, 0.153f, 1.0f};     // #C9A227
constexpr SkColor4f kGiltDark{0.549f, 0.420f, 0.082f, 1.0f}; // #8C6B15
constexpr SkColor4f kViridian{0.184f, 0.365f, 0.290f, 1.0f}; // #2F5D4A
constexpr SkColor4f kOxblood{0.431f, 0.122f, 0.133f, 1.0f};  // #6E1F22
constexpr SkColor4f kAdvantage{0.306f, 0.604f, 0.306f, 1.0f};// #4E9A4E
constexpr SkColor4f kBone{0.871f, 0.824f, 0.706f, 1.0f};     // #DED2B4

inline SkColor4f alpha(SkColor4f c, float a) { return {c.fR, c.fG, c.fB, a}; }
inline Fill ink(float a = 1.0f) { return Fill::color(alpha(kInk, a)); }
inline Fill gilt(float a = 1.0f) { return Fill::color(alpha(kGilt, a)); }
inline Fill giltDark(float a = 1.0f) { return Fill::color(alpha(kGiltDark, a)); }

inline std::u8string u8(const std::string &s) {
  return std::u8string(reinterpret_cast<const char8_t *>(s.c_str()), s.size());
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
  const char *sourceName;   ///< SourceName    TranslatedString
  const char *description;  ///< Description   TranslatedString
  int bonus;                ///< Bonus         int32
  int numDice;              ///< NumDice       uint8
  const char *diceSize;     ///< DiceSize      DiceSizeId
  int resolved;             ///< ResolvedRollBonus int32
  double landsAt;           ///< s, after the die is still
};

// Modifiers arrive AFTER the roll, at 110 ms apart, starting at 1.45 s.
inline const std::array<Bonus, 3> kBonuses{{
    {"Charisma", "Ability modifier  (Charisma 16)", 3, 0, "", 3, 1.45},
    {"Proficiency", "ProficiencyBonus  (level 4)", 2, 0, "", 2, 1.56},
    {"Guidance", "Cantrip  Guidance", 0, 1, "D4", 3, 1.67},
}};

constexpr double kSettleAt = 1.45;  ///< the die is still
constexpr double kOutcomeAt = 1.95; ///< the last row has counted in

/** `Ext_Enums.SkillId` (:33053) at its true ordinals — the engine's order,
 *  which is the governing-ability grouping. See correction 1. */
struct Skill {
  int ordinal;
  const char *name;
};
inline const std::array<Skill, 18> kSkills{{
    {0, "Deception"}, {1, "Intimidation"}, {2, "Performance"},
    {3, "Persuasion"},
    {4, "Acrobatics"}, {5, "SleightOfHand"}, {6, "Stealth"},
    {7, "Arcana"}, {8, "History"}, {9, "Investigation"}, {10, "Nature"},
    {11, "Religion"},
    {12, "Athletics"},
    {13, "AnimalHandling"}, {14, "Insight"}, {15, "Medicine"},
    {16, "Perception"}, {17, "Survival"},
}};
/** The five blocks, as first-ordinal / count / AbilityId name+ordinal. */
struct AbilityBlock {
  int first, count, abilityOrdinal;
  const char *ability;
};
inline const std::array<AbilityBlock, 5> kBlocks{{
    {0, 4, 6, "CHARISMA"},
    {4, 3, 2, "DEXTERITY"},
    {7, 5, 4, "INTELLIGENCE"},
    {12, 1, 1, "STRENGTH"},
    {13, 5, 5, "WISDOM"},
}};
constexpr int kActiveSkill = 3; ///< Persuasion

// ------------------------------------------------------------ the solid
constexpr float kPhi = 1.618033988749895f;

struct V3 {
  float x = 0, y = 0, z = 0;
};
inline V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline V3 cross(V3 a, V3 b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float len(V3 a) { return std::sqrt(dot(a, a)); }
inline V3 norm(V3 a) {
  const float l = len(a);
  return l > 1e-6f ? V3{a.x / l, a.y / l, a.z / l} : a;
}

/** Twelve vertices: the cyclic permutations of (0, +-1, +-phi). */
inline std::vector<V3> icosaVertices() {
  std::vector<V3> v;
  v.reserve(12);
  for (int sa : {-1, 1})
    for (int sb : {-1, 1}) {
      const float a = (float)sa, b = kPhi * (float)sb;
      v.push_back({0, a, b});
      v.push_back({a, b, 0});
      v.push_back({b, 0, a});
    }
  return v;
}

struct Solid {
  std::vector<V3> verts;                    // 12
  std::vector<std::array<int, 3>> faces;    // 20, wound outward
  std::vector<V3> centroid, normal;         // per face
  std::vector<int> pip;                     // face -> 1..20
  std::vector<std::array<int, 2>> edges;    // 30
  std::vector<std::array<int, 2>> edgeFace; // 30, the two faces on each edge
};

/** Faces by SEARCH, not by table: every triple whose three pairwise distances
 *  are the edge length 2. A wrong phi yields no faces at all. */
inline Solid buildSolid() {
  Solid s;
  s.verts = icosaVertices();
  const int n = (int)s.verts.size();
  auto d2 = [&](int i, int j) {
    const V3 d = s.verts[(size_t)i] - s.verts[(size_t)j];
    return dot(d, d);
  };
  for (int i = 0; i < n; ++i)
    for (int j = i + 1; j < n; ++j) {
      if (std::abs(d2(i, j) - 4.0f) > 1e-3f)
        continue;
      for (int k = j + 1; k < n; ++k) {
        if (std::abs(d2(j, k) - 4.0f) > 1e-3f ||
            std::abs(d2(i, k) - 4.0f) > 1e-3f)
          continue;
        std::array<int, 3> f{i, j, k};
        const V3 &a = s.verts[(size_t)i], &b = s.verts[(size_t)j],
                 &c = s.verts[(size_t)k];
        V3 cen{(a.x + b.x + c.x) / 3, (a.y + b.y + c.y) / 3,
               (a.z + b.z + c.z) / 3};
        V3 nrm = cross(b - a, c - a);
        if (dot(nrm, cen) < 0) { // wind outward
          std::swap(f[1], f[2]);
          nrm = {-nrm.x, -nrm.y, -nrm.z};
        }
        s.faces.push_back(f);
        s.centroid.push_back(cen);
        s.normal.push_back(norm(nrm));
      }
    }
  // Thirty edges, each with the two faces that share it.
  const int nf = (int)s.faces.size();
  for (int f = 0; f < nf; ++f)
    for (int e = 0; e < 3; ++e) {
      int a = s.faces[(size_t)f][(size_t)e];
      int b = s.faces[(size_t)f][(size_t)((e + 1) % 3)];
      if (a > b)
        std::swap(a, b);
      int found = -1;
      for (int q = 0; q < (int)s.edges.size(); ++q)
        if (s.edges[(size_t)q][0] == a && s.edges[(size_t)q][1] == b)
          found = q;
      if (found < 0) {
        s.edges.push_back({a, b});
        s.edgeFace.push_back({f, -1});
      } else {
        s.edgeFace[(size_t)found][1] = f;
      }
    }
  s.pip.assign((size_t)nf, 0);
  return s;
}

inline V3 rotate(V3 p, float ax, float ay, float az) {
  float c = std::cos(ax), sn = std::sin(ax);
  V3 q{p.x, p.y * c - p.z * sn, p.y * sn + p.z * c};
  c = std::cos(ay);
  sn = std::sin(ay);
  V3 r{q.x * c + q.z * sn, q.y, -q.x * sn + q.z * c};
  c = std::cos(az);
  sn = std::sin(az);
  return {r.x * c - r.y * sn, r.x * sn + r.y * c, r.z};
}

/** Number the faces so opposites sum to 21 and the settled front face is the
 *  roll. Antipodes are found by centroid, not assumed. */
inline void numberFaces(Solid &s, float ax, float ay, float az, int frontPip) {
  const int nf = (int)s.faces.size();
  std::vector<int> anti((size_t)nf, -1);
  for (int i = 0; i < nf; ++i)
    for (int j = 0; j < nf; ++j) {
      const V3 &a = s.centroid[(size_t)i];
      const V3 &b = s.centroid[(size_t)j];
      if (std::abs(a.x + b.x) < 1e-3f && std::abs(a.y + b.y) < 1e-3f &&
          std::abs(a.z + b.z) < 1e-3f)
        anti[(size_t)i] = j;
    }
  int front = 0;
  float best = -2.0f;
  for (int i = 0; i < nf; ++i) {
    const V3 r = rotate(s.normal[(size_t)i], ax, ay, az);
    if (r.z > best) {
      best = r.z;
      front = i;
    }
  }
  std::fill(s.pip.begin(), s.pip.end(), 0);
  s.pip[(size_t)front] = frontPip;
  if (anti[(size_t)front] >= 0)
    s.pip[(size_t)anti[(size_t)front]] = 21 - frontPip;
  int next = 1;
  for (int i = 0; i < nf; ++i) {
    if (s.pip[(size_t)i] != 0)
      continue;
    while (next == frontPip || next == 21 - frontPip || next > 20)
      ++next;
    s.pip[(size_t)i] = next;
    if (anti[(size_t)i] >= 0)
      s.pip[(size_t)anti[(size_t)i]] = 21 - next;
    ++next;
  }
}

/** The settle attitude: rotate a chosen face normal onto +z so ONE face is
 *  square to the viewer and carries the roll — then tilt ~10 deg off that
 *  axis, because a perfectly face-on icosahedron projects to a symmetric
 *  figure and a symmetric figure reads as a badge. The tilt is what makes
 *  the surviving edges unequal. */
inline void settleAttitude(const Solid &s, float &tx, float &ty, float &tz) {
  const V3 n = s.normal.empty() ? V3{0, 0, 1} : s.normal[0];
  const float h = std::sqrt(n.y * n.y + n.z * n.z);
  tx = std::atan2(n.y, n.z) + 0.175f; // align, then tilt 10 deg
  ty = -std::atan2(n.x, h) - 0.125f;  // …and 7 deg
  tz = 0.085f;
}

/** The tumble: a fast spin decelerating into the settle, then two decaying
 *  bounces. One ramp cannot shape this. Settled by kSettleAt. */
inline void tumbleAngles(double t, float kAx, float kAy, float kAz, float &ax,
                         float &ay, float &az) {
  const float u = (float)std::clamp(t / 1.10, 0.0, 1.0);
  const float e = 1.0f - std::pow(1.0f - u, 5.0f); // easeOutQuint
  const float remain = 1.0f - e;
  constexpr float kTau = 6.2831853f;
  ax = kAx + remain * 2.25f * kTau;
  ay = kAy + remain * 1.50f * kTau;
  az = kAz + remain * 0.75f * kTau;
  if (t > 0.95) { // two decaying bounces, dead by 1.45
    const float w = (float)(t - 0.95);
    const float b = 0.17f * std::exp(-9.0f * w) * std::sin(w * 34.0f);
    ax += b;
    ay += b * 0.7f;
    az += b * 1.3f;
  }
  if (t > kSettleAt) { // a breath, so the plate is never frozen
    const float w = (float)(t - kSettleAt);
    ax += 0.010f * std::sin(w * 0.8f);
    ay += 0.013f * std::sin(w * 0.55f + 1.1f);
  }
}

} // namespace bg3

// =============================================================================

struct Bg3DiceRoll : sigil::compose::sketch::Sketch {
  // ---- bound Outputs (all paint-only) -------------------------------------
  choreograph::Output<float> bezelSpin{0};
  choreograph::Output<float> rosetteSpin{0};
  choreograph::Output<float> hatchSpacing{14.0f};
  choreograph::Output<float> hatchAngle{12.0f};
  choreograph::Output<float> outcomeInk{0};

  // ---- state that changes CONTENT (re-describe) ---------------------------
  double clock = 0;
  float totalAnim = (float)bg3::kNaturalRoll;
  int shownTotal = bg3::kNaturalRoll;
  int rowsMounted = 0;
  bool outcomeMounted = false;

  bg3::Solid solid;
  sk_sp<SkTypeface> serif, mono;
  float ax = 0, ay = 0, az = 0;
  float tx = 0, ty = 0, tz = 0; ///< the settle attitude

  // ------------------------------------------------------------------- type
  Element label(const std::string &s, float x, float y, float size,
                SkColor4f col, float track = 0.0f, bool useMono = false) const {
    weave::TextStyle st;
    st.shaping.typeface = useMono ? mono : serif;
    st.shaping.fontSize = size;
    st.shaping.letterSpacing = track;
    st.paint.foreground.setColor4f(col, nullptr);
    return box().left(x).top(y).child(text(bg3::u8(s), st));
  }
  /** Right-aligned, since a numeral column must align on its units digit and
   *  Yoga is not the skeleton here. `right` is in the PARENT's space, so the
   *  parent's width has to be named — pinning `right` against the canvas
   *  width inside a 528 px row is the bug this parameter exists to stop. */
  Element labelR(const std::string &s, float right, float y, float size,
                 SkColor4f col, bool useMono = false,
                 float parentWidth = bg3::kW) const {
    weave::TextStyle st;
    st.shaping.typeface = useMono ? mono : serif;
    st.shaping.fontSize = size;
    st.paint.foreground.setColor4f(col, nullptr);
    return box()
        .right(parentWidth - right)
        .top(y)
        .child(text(bg3::u8(s), st));
  }

  /** A bare rule as its own tiny node — a stroke wants a box the size of the
   *  stroke, never the canvas. */
  static Element rule(float x, float y, float w, Decoration d,
                      float h = 1.0f) {
    return box().left(x).top(y).width(w).height(h).foreground(
        std::move(d));
  }

  // -------------------------------------------------------------- the solid
  /** The die, projected honestly. Immediate-mode: 20 faces and 30 edges is
   *  cheap, and it changes every frame. */
  Element die(float radius, float opacity, bool numerals, float spin) const {
    const float boxSize = radius * 2.28f;
    return custom([this, radius, opacity, numerals, spin](
                      SkCanvas &c, const PaintContext &ctx) {
      const float cx = ctx.size.width() * 0.5f;
      const float cy = ctx.size.height() * 0.5f;
      const float s = radius / 1.9021130f; // circumradius sqrt(1 + phi^2)

      std::array<SkPoint, 12> proj{};
      for (int i = 0; i < 12; ++i) {
        const bg3::V3 r =
            bg3::rotate(solid.verts[(size_t)i], ax, ay + spin, az);
        proj[(size_t)i] = {cx + r.x * s, cy - r.y * s};
      }
      const int nf = (int)solid.faces.size();
      std::vector<float> nz((size_t)nf);
      std::vector<bool> vis((size_t)nf);
      for (int f = 0; f < nf; ++f) {
        const bg3::V3 r =
            bg3::rotate(solid.normal[(size_t)f], ax, ay + spin, az);
        nz[(size_t)f] = r.z;
        vis[(size_t)f] = r.z > 0.0f;
      }

      // Carved bone: the visible faces as one filled body, shaded only by a
      // flat gradient the node already carries. No per-face tone — the read
      // is the edges.
      SkPaint body;
      body.setAntiAlias(true);
      body.setStyle(SkPaint::kFill_Style);
      SkPathBuilder solidBody;
      for (int f = 0; f < nf; ++f) {
        if (!vis[(size_t)f])
          continue;
        const auto &t = solid.faces[(size_t)f];
        solidBody.moveTo(proj[(size_t)t[0]]);
        solidBody.lineTo(proj[(size_t)t[1]]);
        solidBody.lineTo(proj[(size_t)t[2]]);
        solidBody.close();
      }
      const SkPath bodyPath = solidBody.detach();
      // Each visible face gets its own tone from its own normal — the only
      // shading, and it is derived from the geometry rather than painted.
      for (int f = 0; f < nf; ++f) {
        if (!vis[(size_t)f])
          continue;
        const auto &t = solid.faces[(size_t)f];
        const float k = 0.72f + 0.28f * nz[(size_t)f];
        body.setColor4f({bg3::kBone.fR * k, bg3::kBone.fG * k,
                         bg3::kBone.fB * k, opacity},
                        nullptr);
        SkPathBuilder tri;
        tri.moveTo(proj[(size_t)t[0]]);
        tri.lineTo(proj[(size_t)t[1]]);
        tri.lineTo(proj[(size_t)t[2]]);
        tri.close();
        c.drawPath(tri.detach(), body);
      }

      // THE THREE-TIER EDGE WEIGHT.
      SkPaint edge;
      edge.setAntiAlias(true);
      edge.setStyle(SkPaint::kStroke_Style);
      edge.setStrokeCap(SkPaint::kRound_Cap);
      SkPathBuilder interior, silhouette;
      for (size_t e = 0; e < solid.edges.size(); ++e) {
        const int f0 = solid.edgeFace[e][0], f1 = solid.edgeFace[e][1];
        const int seen = (f0 >= 0 && vis[(size_t)f0] ? 1 : 0) +
                         (f1 >= 0 && vis[(size_t)f1] ? 1 : 0);
        if (seen == 0)
          continue; // back edge: omitted entirely
        SkPathBuilder &into = seen == 2 ? interior : silhouette;
        into.moveTo(proj[(size_t)solid.edges[e][0]]);
        into.lineTo(proj[(size_t)solid.edges[e][1]]);
      }
      edge.setStrokeWidth(1.1f);
      edge.setColor4f(bg3::alpha(bg3::kGiltDark, opacity * 0.75f), nullptr);
      c.drawPath(interior.detach(), edge);
      edge.setStrokeWidth(2.6f);
      edge.setColor4f(bg3::alpha(bg3::kInk, opacity), nullptr);
      c.drawPath(silhouette.detach(), edge);

      if (!numerals || !serif)
        return;
      // Ink-filled numerals at each visible face's centroid, scaled by that
      // face's own foreshortening and dropped when it turns too far away.
      SkPaint glyph;
      glyph.setAntiAlias(true);
      glyph.setColor4f(bg3::alpha(bg3::kInk, opacity), nullptr);
      for (int f = 0; f < nf; ++f) {
        if (nz[(size_t)f] < 0.34f)
          continue;
        const bg3::V3 r =
            bg3::rotate(solid.centroid[(size_t)f], ax, ay + spin, az);
        const float fx = cx + r.x * s, fy = cy - r.y * s;
        const float fs = radius * 0.30f * (0.55f + 0.45f * nz[(size_t)f]);
        SkFont font(serif, fs);
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%d", solid.pip[(size_t)f]);
        const float w =
            font.measureText(buf, std::strlen(buf), SkTextEncoding::kUTF8);
        glyph.setAlphaf(opacity * (0.35f + 0.65f * nz[(size_t)f]));
        c.drawString(buf, fx - w * 0.5f, fy + fs * 0.34f, font, glyph);
      }
    })
        .left(bg3::kCx - boxSize * 0.5f)
        .top(bg3::kCy - boxSize * 0.5f)
        .width(boxSize)
        .height(boxSize)
        .cache(Cache::None);
  }

  // ------------------------------------------------------------- the bezel
  /** The 20-gon, carrying the whole border vocabulary at twenty vertices.
   *  Static content on a rotating node: baked once, blitted spinning. */
  Element bezel() const {
    const float o = bg3::kBezelOuter, i = bg3::kBezelInner;
    const float a = bg3::kCornerAngle;

    // The gilt fleuron that sits ON each vertex, on the bisector.
    brushes::PatternBrush ornament;
    ornament.side = box()
                        .width(13)
                        .height(7)
                        .outline(shapes::polygon(4, 45.0f))
                        .foreground(stroke(0.9f, bg3::giltDark(0.85f)));
    ornament.corner = box()
                          .width(22)
                          .height(22)
                          .outline(shapes::star(4, 0.34f, 0.14f))
                          .fill(bg3::alpha(bg3::kGilt, 0.92f))
                          .foreground(stroke(0.8f, bg3::ink(0.55f)));
    ornament.advance = 21.0f;
    ornament.cornerLength = 26.0f;
    ornament.reach = 26.0f;
    // PatternBrush has its OWN threshold, and its default is 35 — higher
    // than Border's 30. It needs telling separately.
    ornament.cornerAngleDeg = a;
    ornament.cornerAlign = brushes::PatternBrush::CornerAlign::Bisector;

    Element ring =
        stack()
            .left(bg3::kCx - o)
            .top(bg3::kCy - o)
            .width(o * 2)
            .height(o * 2)
            // The outer 20-gon: the rule THICKENS at each of the twenty
            // vertices — the brass-rule move.
            .child(box()
                       .inset(0)
                       .outline(shapes::polygon(20))
                       .foreground(decorations::weightedCorners(
                           1.5f, 5.5f, bg3::gilt(), 20.0f, 0.0f, a)))
            // A twenty-tick bracket ladder just inside it: one tick per face
            // of the die, so the ornament IS the index.
            .child(box()
                       .inset(17)
                       .outline(shapes::polygon(20))
                       .foreground(decorations::brackets(2.4f, bg3::ink(0.7f),
                                                         15.0f, 0.0f, a)))
            // The illuminated border proper: fleuron on every vertex.
            .child(box()
                       .inset(36)
                       .outline(shapes::polygon(20))
                       .stroke(ornament))
            // The inner 20-gon: stops short at every flat, so vellum
            // breathes between the two rules.
            .child(box()
                       .inset(o - i)
                       .outline(shapes::polygon(20))
                       .foreground(decorations::gappedRule(2.4f, bg3::ink(0.85f),
                                                           32.0f, 0.0f, a))
                       .foreground(decorations::brackets(3.4f, bg3::gilt(),
                                                         11.0f, 0.0f, a)));
    return ring.rotate(&bezelSpin)
        .transformOrigin(0.5f, 0.5f)
        .cache(Cache::Texture);
  }

  /** The band the bezel's rules sit on: a filled 20-gon carrying the
   *  concentric rules, with the rosette disc laid over its middle — so
   *  `lines::concentric` shows only in the band, which is where a bezel's
   *  turned rings belong. */
  Element bezelBand() const {
    const float o = bg3::kBezelOuter;
    return box()
        .left(bg3::kCx - o)
        .top(bg3::kCy - o)
        .width(o * 2)
        .height(o * 2)
        .outline(shapes::polygon(20))
        // Light enough that the rules read as ornament ON vellum rather than
        // ink on a brown ring — this is gilt, not bronze.
        .fill(linearGradient({0, 0}, {o * 1.4f, o * 2},
                             {bg3::alpha(bg3::kGilt, 0.16f),
                              bg3::alpha(bg3::kGiltDark, 0.09f),
                              bg3::alpha(bg3::kGilt, 0.14f)},
                             {0.0f, 0.55f, 1.0f}))
        // OVERLAY, not background: `background()` paints BENEATH the fill
        // (the CSS box-shadow slot), so turned rings put there vanish under
        // any opaque surface. `overlay()` is over the fill, under children.
        .overlay(lines::concentric(bg3::giltDark(0.42f), 5, 0.8f))
        // No drop shadow here: `background()` is UNDER the fill, and this
        // fill is 14% alpha — so the blurred silhouette showed straight
        // through and turned a gilt band into a mud-olive ring.
        .cache(Cache::Texture);
  }

  /** The rosette: a radial fan clipped to the annulus between the two
   *  20-gons, counter-rotating under the die. */
  Element rosette() const {
    const float r = bg3::kBezelInner - 6.0f;
    return box()
        .left(bg3::kCx - r)
        .top(bg3::kCy - r)
        .width(r * 2)
        .height(r * 2)
        .outline(shapes::polygon(20, 9.0f))
        // Opaque, so it masks the band's concentric rules down to the band.
        .fill(radialGradient({r, r}, r,
                             {bg3::kVellum, bg3::alpha(bg3::kVellumDeep, 1.0f)}))
        .overlay(lines::radialHatch(bg3::giltDark(0.34f), 60, 0.6f))
        .rotate(&rosetteSpin)
        .transformOrigin(0.5f, 0.5f)
        .cache(Cache::Texture);
  }

  // -------------------------------------------------------- the DC plate
  Element dcPlate() const {
    // A hand-built Rails set: one rail's dashPhase slid against its
    // neighbours is what makes a doubled rule read as ENGRAVED.
    lines::Rails engraved = lines::rails({
        {.offset = -4.0f, .width = 1.8f, .fill = bg3::gilt()},
        {.offset = 0.0f,
         .width = 0.9f,
         .fill = bg3::giltDark(),
         .dash = {3.0f, 5.0f}},
        {.offset = 4.0f,
         .width = 1.8f,
         .fill = bg3::gilt(),
         .dash = {9.0f, 9.0f},
         .dashPhase = 4.5f},
    });

    // Two flourishes springing off the plate — a calligraphic nib, so the
    // width varies; a constant-width stroke would look printed.
    //
    // NOTE: `shapes::parametric` samples f over UNIT coordinates — ±1 spans
    // the box, it is not px. Returning px here sent the curve off-canvas as
    // one enormous gold arc through the whole composition. Documented at the
    // call site; still the easiest thing in Shapes.h to get wrong.
    auto flourish = [&](bool mirror) {
      brushes::Ribbon nib;
      nib.fill = bg3::gilt(0.9f);
      nib.widthStart = 5.6f;
      nib.widthEnd = 0.6f;
      nib.nibAngleDeg = mirror ? 118.0f : 62.0f;
      nib.nibContrast = 0.18f;
      const float dir = mirror ? -1.0f : 1.0f;
      return box()
          .left(mirror ? 18.0f : 388.0f)
          .top(26.0f)
          .width(118)
          .height(62)
          .outline(shapes::parametric(
              [dir](float t) {
                // Unit space: a hooked spur that curls back on itself.
                const float u = t * 3.14159f;
                return SkPoint{dir * (-0.92f + 1.72f * t),
                               -0.55f * std::sin(u) * std::cos(u * 0.62f) +
                                   0.30f * t};
              },
              0.0f, 1.0f, 96))
          .background(nib);
    };

    return stack()
        .left(bg3::kCx - 262.0f)
        .top(112.0f)
        .width(524)
        .height(150)
        .child(box()
                   .left(148)
                   .top(0)
                   .width(228)
                   .height(118)
                   .outline(shapes::notched(30.0f, 13.0f, shapes::Corner::All))
                   .fill(bg3::alpha(bg3::kVellumDeep, 0.96f))
                   .background(shadow({0.1f, 0.07f, 0.04f, 0.30f}, {0, 4}, 12))
                   .style(decorations::doubleBorder(
                       decorations::border(1.9f, bg3::ink(0.9f), 5.0f),
                       decorations::border(0.7f, bg3::giltDark(), 10.0f))))
        .child(flourish(false))
        .child(flourish(true))
        // The hanger: the plate reads as a hung sign, so it hangs from a rule.
        .child(rule(166.0f, 118.0f, 192.0f, engraved, 1.0f))
        // The plate box (148..376) and its hanger (166..358) are both centred
        // on local 262; the three runs were each placed by eye and none of
        // them was — D C sat 16 px left of centre and the numeral 8.5 right,
        // so the two stacked runs disagreed by 24.5 px on a symmetric sign.
        // Offsets below are measured ink centres, not advances: D C carries a
        // trailing 7 px letterspace and the caption 1.2.
        .child(label("D C", 148.0f + 90.0f, 10.0f, 21.0f,
                     bg3::alpha(bg3::kInk, 0.72f), 7.0f))
        .child(label(std::to_string(bg3::kDC), 148.0f + 91.5f, 26.0f, 42.0f,
                     bg3::kInk))
        // The caption lives INSIDE the plate: below it the plate deliberately
        // bleeds over the bezel's top flat, and type on the band is unreadable.
        .child(label("SkillCheck \xc2\xb7 StatsRollType", 148.0f + 26.5f, 82.0f,
                     9.5f, bg3::alpha(bg3::kInk, 0.5f), 1.2f, true));
  }

  // ------------------------------------------------------- the skill ladder
  /** Correction 1, drawn: the engine's ordinal table IS the ability grouping.
   *  Eighteen ticks at their true ordinals, five blocks bracketed, the active
   *  skill marked. Ornament that is data. */
  Element skillLadder() const {
    constexpr float kX = 74.0f;   // the tick ladder's spine
    constexpr float kTop = 286.0f;
    constexpr float kPitch = 25.0f;
    constexpr float kBlockGap = 27.0f; ///< the header's OWN row

    // The ordinal is the true index; the gap between blocks is drawn, so the
    // grouping the ordinals encode is visible rather than asserted.
    auto yOf = [](int ordinal) {
      int blockIndex = 0;
      for (const auto &b : bg3::kBlocks)
        if (ordinal >= b.first + b.count)
          ++blockIndex;
      return kTop + (float)ordinal * kPitch + (float)blockIndex * kBlockGap;
    };

    Element g = stack().left(0).top(0).width(bg3::kW).height(bg3::kH);
    const float yLast = yOf(17);
    g.child(rule(kX, kTop - 12.0f, 1.2f,
                 lines::hatch(bg3::ink(0.42f), 4.0f, 1.2f, 90.0f),
                 yLast - kTop + 26.0f));

    for (const auto &blk : bg3::kBlocks) {
      const float y0 = yOf(blk.first);
      const float y1 = yOf(blk.first + blk.count - 1);
      // A bracket that only exists at the block's ends.
      g.child(box()
                  .left(kX - 14.0f)
                  .top(y0 - 6.0f)
                  .width(14.0f)
                  .height(y1 - y0 + 12.0f)
                  .outline(shapes::chamfered(5.0f, shapes::Corner::AntiDiagonal))
                  .foreground(decorations::brackets(1.6f, bg3::giltDark(),
                                                    11.0f, 0.0f, 24.0f)));
      // The header sits in the gap ABOVE its block, on its own row.
      g.child(label(blk.ability, kX + 22.0f, y0 - 21.0f, 9.0f,
                    bg3::alpha(bg3::kGiltDark, 0.95f), 2.4f, true));
      g.child(label("AbilityId " + std::to_string(blk.abilityOrdinal),
                    kX + 128.0f, y0 - 20.0f, 7.5f,
                    bg3::alpha(bg3::kInk, 0.34f), 0.8f, true));
    }

    for (const auto &sk : bg3::kSkills) {
      const float y = yOf(sk.ordinal);
      const bool live = sk.ordinal == bg3::kActiveSkill;
      g.child(rule(kX, y, live ? 26.0f : 15.0f,
                   stroke(live ? 2.4f : 1.0f,
                          live ? bg3::gilt() : bg3::ink(0.55f)),
                   live ? 2.4f : 1.0f));
      g.child(label(sk.name, kX + (live ? 34.0f : 22.0f), y - 8.0f,
                    live ? 15.0f : 11.5f,
                    live ? bg3::kInk : bg3::alpha(bg3::kInk, 0.58f),
                    live ? 1.6f : 0.6f));
      g.child(labelR(std::to_string(sk.ordinal), kX - 19.0f, y - 6.0f, 9.5f,
                     bg3::alpha(bg3::kInk, live ? 0.85f : 0.34f), true));
    }
    g.child(label("Ext_Enums.SkillId  :33053", kX - 14.0f, yLast + 26.0f, 8.5f,
                  bg3::alpha(bg3::kInk, 0.45f), 1.0f, true));
    g.child(label("ordinals ARE the governing-ability grouping", kX - 14.0f,
                  yLast + 39.0f, 8.5f, bg3::alpha(bg3::kInk, 0.34f), 0.6f,
                  true));
    return g.cache(Cache::Texture);
  }

  // ------------------------------------------------------ the modifier column
  /** Straight, because the reference stacks them — but DOCKED ON A RADIUS,
   *  each row's leader tick running back to the bezel's lower-right flat, so
   *  the column participates in the radial composition without pretending
   *  BG3 fans them on an arc. */
  Element modifierColumn() const {
    constexpr float kX = 648.0f;
    constexpr float kTop = 866.0f;
    constexpr float kPitch = 50.0f;
    constexpr float kRight = 1076.0f;
    constexpr float kRowW = kRight - kX + 112.0f;

    Element col = box()
                      .left(0)
                      .top(0)
                      .width(bg3::kW)
                      .height(bg3::kH)
                      .staggerChildren(110ms);

    for (int i = 0; i < rowsMounted && i < (int)bg3::kBonuses.size(); ++i) {
      const auto &b = bg3::kBonuses[(size_t)i];
      const float y = kTop + (float)i * kPitch;
      const std::string amount =
          b.numDice > 0
              ? "+" + std::to_string(b.numDice) + std::string(b.diceSize) +
                    " \xe2\x86\x92 " + std::to_string(b.resolved)
              : "+" + std::to_string(b.bonus);

      // The row is its OWN node and the entrance binds on IT — a bound
      // opacity is a saveLayer the size of the node's box, so the box is
      // the row, never the canvas.
      Element row =
          stack()
              .left(kX - 96.0f)
              .top(y - 10.0f)
              .width(kRowW)
              .height(44.0f)
              .opacity(withFrom(0.0f, 1.0f, {260ms, choreograph::easeOutQuad}))
              .translateX(withFrom(18.0f, 0.0f,
                                   {300ms, choreograph::easeOutQuad}))
              // The leader tick, running back toward the bezel.
              .child(rule(0.0f, 20.0f, 84.0f,
                          lines::heavyHairHeavy(1.4f, 0.5f,
                                                bg3::giltDark(0.8f), 2.6f),
                          1.0f))
              .child(label(b.sourceName, 96.0f, 2.0f, 21.0f, bg3::kInk, 0.8f))
              .child(label(b.description, 96.0f, 26.0f, 9.5f,
                           bg3::alpha(bg3::kInk, 0.45f), 0.7f, true))
              .child(labelR(amount, kRowW - 16.0f, 0.0f,
                            b.numDice > 0 ? 20.0f : 24.0f, bg3::kInk, false,
                            kRowW));
      col.child(std::move(row));
    }
    return col;
  }

  /** The total: NaturalRoll and Total are separate fields, so they are
   *  separate numbers on the plate, and the second visibly accumulates. */
  Element totalBlock() const {
    constexpr float kX = 648.0f;
    constexpr float kY = 1024.0f;
    constexpr float kRight = 1076.0f;
    Element g = stack().left(0).top(0).width(bg3::kW).height(bg3::kH);
    g.child(rule(kX + 96.0f, kY - 12.0f, kRight - kX - 96.0f + 16.0f,
                 lines::heavyHairHeavy(2.0f, 0.7f, bg3::ink(0.85f), 4.0f),
                 1.0f));
    g.child(label("Total", kX + 96.0f, kY + 4.0f, 22.0f,
                  bg3::alpha(bg3::kInk, 0.8f), 3.0f));
    g.child(label("StatsRollResult.Total", kX + 96.0f, kY + 32.0f, 9.0f,
                  bg3::alpha(bg3::kInk, 0.42f), 0.8f, true));
    g.child(labelR(std::to_string(shownTotal), kRight, kY - 14.0f, 56.0f,
                   bg3::kInk));
    return g;
  }

  /** The outcome. `Hatch` with BOTH bindings — pitch and angle — on a box
   *  the size of the banner, never the canvas. */
  Element outcome() const {
    if (!outcomeMounted)
      return box().left(0).top(0).width(1).height(1);
    lines::Hatch wash;
    wash.strokeFill = Fill::color(bg3::alpha(bg3::kViridian, 0.34f));
    wash.width = 1.5f;
    wash.spacingBinding = &hatchSpacing;
    wash.angleBinding = &hatchAngle;
    return stack()
        .left(636.0f)
        .top(1094.0f)
        .width(452.0f)
        .height(64.0f)
        .outline(shapes::chamfered(14.0f, shapes::Corner::Diagonal))
        .fill(bg3::alpha(bg3::kViridian, 0.10f))
        .overlay(wash)
        .foreground(decorations::weightedCorners(1.2f, 3.4f, bg3::gilt(), 16.0f,
                                                 0.0f, 30.0f))
        .child(label("SUCCESS", 34.0f, 12.0f, 30.0f, bg3::kViridian, 8.0f))
        .child(label("RollCritical.None 0  \xc2\xb7  Total 20 \xe2\x89\xa5 DC 15",
                     34.0f, 44.0f, 9.0f, bg3::alpha(bg3::kInk, 0.55f), 0.9f,
                     true))
        .opacity(withFrom(0.0f, 1.0f, {380ms, choreograph::easeOutQuad}));
  }

  // ------------------------------------------------------------- marginalia
  Element portrait() const {
    constexpr float cx = 372.0f, cy = 742.0f, r = 64.0f;
    return stack()
        .left(cx - r - 16.0f)
        .top(cy - r - 16.0f)
        .width(r * 2 + 32.0f)
        .height(r * 2 + 32.0f)
        .outline(shapes::chamfered(10.0f))
        .fill(bg3::alpha(bg3::kVellumDeep, 0.95f))
        .background(shadow({0.1f, 0.07f, 0.04f, 0.32f}, {0, 3}, 10))
        .style(
            decorations::doubleBorder(decorations::border(1.8f, bg3::ink(0.9f)),
                                      decorations::border(0.7f,
                                                          bg3::giltDark(), 5.0f)))
        .child(box()
                   .left(16.0f)
                   .top(16.0f)
                   .width(r * 2)
                   .height(r * 2)
                   .outline(shapes::polygon(20, 9.0f))
                   .fill(radialGradient({r, r * 0.8f}, r * 1.25f,
                                        {bg3::alpha(bg3::kVellum, 1.0f),
                                         bg3::alpha(bg3::kGiltDark, 0.55f)}))
                   .overlay(lines::concentric(bg3::giltDark(0.45f), 4, 0.6f))
                   .overlay(lines::radialHatch(bg3::giltDark(0.3f), 20, 0.5f))
                   .foreground(decorations::brackets(2.0f, bg3::ink(0.8f),
                                                     10.0f, 0.0f,
                                                     bg3::kCornerAngle)))
        .cache(Cache::Texture);
  }

  /** BG3's dialogue DCs sit on a 5-step ladder. Drawn as a scale in the right
   *  gutter with BOTH the target and the achieved total on it, so
   *  `Total >= DC` is a distance you can see rather than a claim in a
   *  caption — and it balances the skill ladder on the left. */
  Element dcLadder() const {
    constexpr float kX = 1102.0f;
    constexpr float kBottom = 762.0f;
    constexpr float kStep = 68.0f; // per 5 points of DC
    auto yOf = [](float dc) { return kBottom - (dc - 5.0f) / 5.0f * kStep; };

    Element g = stack().left(0).top(0).width(bg3::kW).height(bg3::kH);
    g.child(rule(kX, yOf(30.0f) - 14.0f, 1.2f,
                 lines::hatch(bg3::ink(0.42f), 4.0f, 1.2f, 90.0f),
                 kBottom - yOf(30.0f) + 28.0f));
    g.child(label("DC LADDER", kX - 30.0f, yOf(30.0f) - 34.0f, 9.0f,
                  bg3::alpha(bg3::kInk, 0.5f), 2.2f, true));

    for (int dc = 5; dc <= 30; dc += 5) {
      const float y = yOf((float)dc);
      const bool target = dc == bg3::kDC;
      g.child(rule(kX - (target ? 20.0f : 11.0f), y, target ? 20.0f : 11.0f,
                   stroke(target ? 2.6f : 1.0f,
                          target ? bg3::gilt() : bg3::ink(0.5f)),
                   target ? 2.6f : 1.0f));
      g.child(label(std::to_string(dc), kX + 8.0f, y - 8.0f,
                    target ? 15.0f : 11.5f,
                    target ? bg3::kInk : bg3::alpha(bg3::kInk, 0.55f),
                    target ? 1.4f : 0.6f));
    }
    // The margin actually cleared: DC 15 -> Total 20, five points.
    const float yDC = yOf((float)bg3::kDC);
    const float yTotal = yOf((float)bg3::kFinalTotal);
    g.child(box()
                .left(kX - 34.0f)
                .top(yTotal)
                .width(20.0f)
                .height(yDC - yTotal)
                .outline(shapes::chamfered(5.0f, shapes::Corner::AntiDiagonal))
                .fill(bg3::alpha(bg3::kViridian, 0.14f))
                .foreground(decorations::brackets(
                    1.8f, Fill::color(bg3::kViridian), 9.0f, 0.0f, 24.0f)));
    g.child(rule(kX - 34.0f, yTotal, 34.0f,
                 stroke(2.6f, Fill::color(bg3::kViridian)), 2.6f));
    g.child(label("TOTAL", kX + 8.0f, yTotal - 30.0f, 9.0f,
                  bg3::alpha(bg3::kViridian, 0.95f), 1.8f, true));
    g.child(label("+5", kX - 62.0f, (yDC + yTotal) * 0.5f - 7.0f, 11.0f,
                  bg3::alpha(bg3::kViridian, 0.9f), 0.6f, true));
    g.child(label("DC", kX - 52.0f, yDC - 7.0f, 9.0f,
                  bg3::alpha(bg3::kGiltDark, 0.95f), 1.4f, true));
    return g.cache(Cache::Texture);
  }

  Element marginalia() const {
    Element g = stack().left(0).top(0).width(bg3::kW).height(bg3::kH);
    // Registration marks: brackets on a chamfered frame that bleeds.
    g.child(box()
                .left(26)
                .top(26)
                .width(bg3::kW - 52)
                .height(bg3::kH - 52)
                .outline(shapes::chamfered(26.0f))
                .foreground(decorations::brackets(1.4f, bg3::ink(0.42f), 30.0f,
                                                  0.0f, 24.0f))
                .foreground(decorations::gappedRule(0.6f, bg3::giltDark(0.55f),
                                                    46.0f, 8.0f, 24.0f)));
    g.child(label("BALDUR\xe2\x80\x99S GATE 3  \xc2\xb7  DIALOGUE ABILITY CHECK",
                  56.0f, 44.0f, 13.0f, bg3::alpha(bg3::kInk, 0.62f), 3.4f));
    g.child(label("AdvantageContext.SourceDialogue  8", 56.0f, 64.0f, 9.0f,
                  bg3::alpha(bg3::kGiltDark, 0.9f), 1.2f, true));
    g.child(labelR("PLATE I", bg3::kW - 56.0f, 44.0f, 11.0f,
                   bg3::alpha(bg3::kInk, 0.5f), true));
    g.child(labelR("Norbyte/bg3se \xc2\xb7 ExtIdeHelpers.lua \xc2\xb7 35,855 lines",
                   bg3::kW - 56.0f, 60.0f, 8.5f, bg3::alpha(bg3::kInk, 0.36f),
                   true));

    // The advantage note, on the die's upper-left radius where the dimmed
    // die sits.
    // In the top-left margin, clear of both the ladder and the bezel, with a
    // dotted leader running down to the die it describes.
    constexpr float kAx2 = 56.0f, kAy2 = 138.0f;
    g.child(label("ADVANTAGE", kAx2, kAy2, 12.0f,
                  bg3::alpha(bg3::kAdvantage, 0.95f), 3.2f));
    g.child(label("AdvantageBoostType 0", kAx2, kAy2 + 18.0f, 8.0f,
                  bg3::alpha(bg3::kInk, 0.42f), 0.8f, true));
    g.child(label("AdvantageContext.SourceDialogue  8", kAx2, kAy2 + 29.0f,
                  8.0f, bg3::alpha(bg3::kInk, 0.42f), 0.8f, true));
    g.child(rule(kAx2, kAy2 + 46.0f, 188.0f,
                 stroke(1.2f, Fill::color(bg3::alpha(bg3::kAdvantage, 0.75f))),
                 1.2f));
    g.child(label("2d20 keep highest \xc2\xb7 DiscardedDiceTotal  " +
                      std::to_string(bg3::kDiscardedDiceTotal),
                  kAx2, kAy2 + 51.0f, 9.0f, bg3::alpha(bg3::kInk, 0.55f), 0.8f,
                  true));
    // The leader to the discarded die is NOT drawn here — see
    // advantageLeader(). Marginalia paints under the roundel, and the rosette
    // is opaque by design, so a leader laid down at this point loses its last
    // 29% at the rosette's rim and ends in mid-air on the gilt band.

    // The footer: four lines, each on its own baseline, clear of the outcome
    // banner's left edge at x = 636.
    constexpr float kFootX = 56.0f;
    constexpr float kFootTop = bg3::kH - 112.0f;
    constexpr float kFootLead = 15.0f;
    g.child(rule(kFootX, kFootTop - 12.0f, 470.0f,
                 lines::cased(0.7f, bg3::ink(0.34f), 3.0f), 1.0f));
    const char *foot[] = {
        "PROFICIENCY  +2 (1\xe2\x80\x93" "4)   +3 (5\xe2\x80\x93" "8)   "
        "+4 (9\xe2\x80\x93" "12)  \xe2\x97\x82 BG3 caps at level 12",
        "RollCritical  None 0 \xc2\xb7 Success 1 \xc2\xb7 Fail 2      "
        "DiceSizeId  D20 = 5",
        "ResolvedRollBonus{SourceName, Description, NumDice, DiceSize, Bonus}",
        "modifiers are added AFTER the die lands",
    };
    for (int i = 0; i < 4; ++i)
      g.child(label(foot[i], kFootX, kFootTop + (float)i * kFootLead, 9.5f,
                    i == 3 ? bg3::alpha(bg3::kGiltDark, 0.9f)
                           : bg3::alpha(bg3::kInk, 0.45f),
                    1.1f, true));

    // NaturalRoll — the single most important number on the plate before the
    // modifiers accumulate, so it gets the margin to itself: a leader running
    // out of the bezel to a label and a numeral nothing crosses.
    constexpr float kNx = 962.0f, kNy = 246.0f;
    g.child(rule(kNx - 74.0f, kNy + 44.0f, 74.0f,
                 lines::dottedCore(1.5f, 2.4f, bg3::giltDark(0.95f), 3.6f,
                                   7.0f),
                 1.0f));
    g.child(label("NaturalRoll", kNx, kNy, 12.0f,
                  bg3::alpha(bg3::kInk, 0.62f), 2.6f));
    g.child(label("StatsRollResult", kNx, kNy + 16.0f, 8.0f,
                  bg3::alpha(bg3::kInk, 0.38f), 0.8f, true));
    g.child(label(std::to_string(bg3::kNaturalRoll), kNx, kNy + 28.0f, 52.0f,
                  bg3::kInk));
    g.child(rule(kNx, kNy + 92.0f, 118.0f,
                 lines::heavyHairHeavy(1.8f, 0.6f, bg3::gilt(), 3.4f), 1.0f));
    g.child(label("before modifiers", kNx, kNy + 98.0f, 8.5f,
                  bg3::alpha(bg3::kGiltDark, 0.85f), 1.0f, true));
    return g.cache(Cache::Texture);
  }

  /** The vellum, edge to edge, with its stain. A gradient, because a gradient
   *  lowers to a SIMD blitter and an SkSL mix chain does not.
   *
   *  It lives on its OWN cached child rather than on the root's fill: the
   *  root has animated children, so the root is live-paint forever, and a
   *  fill sitting on it re-ran the gradient over all 1.44 MP every frame —
   *  4.09 ms of the budget for a picture that never changes. Same pixels,
   *  one blit. */
  Element ground() const {
    return box()
        .inset(0)
        .width(bg3::kW)
        .height(bg3::kH)
        .fill(linearGradient({0, 0}, {bg3::kW * 0.7f, bg3::kH},
                             {bg3::kVellum, bg3::kVellumDeep,
                              {0.741f, 0.667f, 0.529f, 1.0f}},
                             {0.0f, 0.62f, 1.0f}))
        .cache(Cache::Texture);
  }

  /** The outermost ornamental ring, running off all four edges. */
  Element outerRing() const {
    return box()
        .left(bg3::kCx - 700.0f)
        .top(bg3::kCy - 660.0f)
        .width(1400)
        .height(1400)
        .outline(shapes::polygon(40, 4.5f))
        .style(decorations::doubleBorder(
            decorations::border(2.6f, bg3::giltDark(0.42f)),
            decorations::border(0.9f, bg3::ink(0.3f), 16.0f)))
        .background(lines::concentric(bg3::giltDark(0.10f), 3, 1.0f))
        .cache(Cache::Texture);
  }

  /** The discarded die of the advantage pair: behind, up-left, rotated,
   *  dimmed. DiscardedDiceTotal is a field, not an inference. */
  Element discardedDie() const {
    return box()
        .left(0)
        .top(0)
        .width(bg3::kW)
        .height(bg3::kH)
        .translateX(-40.0f)
        .translateY(-40.0f)
        .rotate(-23.0f)
        .transformOriginPx({bg3::kCx, bg3::kCy})
        .child(die(bg3::kDieRadius * 0.86f, 0.45f, false, 1.9f));
  }

  /** The leader from the ADVANTAGE block to the discarded die. It has to be
   *  laid down AFTER the roundel — the rosette is opaque, and its rim sits at
   *  t = 0.71 of this curve, so drawn with the rest of the marginalia the
   *  callout stopped short of the thing it calls out. */
  Element advantageLeader() const {
    constexpr float kAx2 = 56.0f, kAy2 = 138.0f;
    return box()
        .left(kAx2 + 238.0f)
        .top(kAy2 + 64.0f)
        .width(232.0f)
        .height(196.0f)
        .outline(shapes::parametric(
            [](float t) {
              return SkPoint{-1.0f + 2.0f * t, -1.0f + 2.0f * t * t};
            },
            0.0f, 1.0f, 48))
        .background(stroke(1.0f, Fill::color(bg3::alpha(bg3::kInk, 0.32f))));
  }

  /** The winner, punching in on a keyframe path: one ramp cannot shape a
   *  landing. */
  Element heroDie() const {
    return box()
        .left(0)
        .top(0)
        .width(bg3::kW)
        .height(bg3::kH)
        .transformOriginPx({bg3::kCx, bg3::kCy})
        .scale(withKeyframes<float>({{0ms, 0.80f},
                                     {1100ms, 1.0f},
                                     {1210ms, 1.075f},
                                     {1300ms, 0.975f},
                                     {1390ms, 1.02f},
                                     {1450ms, 1.0f}}))
        .child(die(bg3::kDieRadius, 1.0f, true, 0.0f));
  }

  // ---------------------------------------------------------------- describe
  /** Only three things here change with STATE — the column, the total and
   *  the outcome. Everything else is behind `memo(0, …)`, whose body runs
   *  only when its props change, i.e. never.
   *
   *  That matters more than it looks. The counter re-describes on every
   *  integer it passes through (12 → 20), so `describe()` runs ~10 times
   *  during the reveal; without memo each run rebuilt the ornament from
   *  scratch, and `PatternBrush` keeps its baked-tile cache IN THE BRUSH
   *  VALUE — a freshly constructed brush gets an EMPTY one, so all twenty
   *  fleurons plus every side tile were re-baked on each pass. That is what
   *  put p99 at 21.68 ms across the reveal window while the settled frame
   *  measured 9.49. Memoised, the reveal costs what the still frame does. */
  Element describe(sketch::SketchContext &ctx) {
    (void)ctx;
    auto once = [](auto fn) { return memo(0, [fn](const int &) { return fn(); }); };
    return stack()
        .child(once([this] { return ground(); }))
        .child(once([this] { return outerRing(); }))
        .child(once([this] { return marginalia(); }))
        .child(once([this] { return skillLadder(); }))
        .child(once([this] { return dcLadder(); }))
        .child(once([this] { return bezelBand(); }))
        .child(once([this] { return rosette(); }))
        .child(once([this] { return discardedDie(); }))
        .child(once([this] { return advantageLeader(); }))
        .child(once([this] { return bezel(); }))
        .child(once([this] { return portrait(); }))
        .child(once([this] { return heroDie(); }))
        .child(once([this] { return dcPlate(); }))
        .child(modifierColumn())
        .child(totalBlock())
        .child(outcome());
  }

  // ------------------------------------------------------------------- setup
  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(bg3::kW, bg3::kH);
    ctx.background(bg3::kVellum);

    sk_sp<SkFontMgr> mgr = weave::ports::systemFontManager();
    auto pick = [&](std::initializer_list<const char *> names) {
      for (const char *n : names)
        if (sk_sp<SkTypeface> f = mgr->matchFamilyStyle(n, SkFontStyle::Normal()))
          return f;
      return mgr->matchFamilyStyle(nullptr, SkFontStyle::Normal());
    };
    // BG3 sets a humanist old-style with tall caps; these are the closest
    // faces present on the host.
    serif = pick({"Baskerville", "Palatino", "Hoefler Text", "Georgia",
                  "Times New Roman", "Helvetica"});
    mono = pick({"Menlo", "SF Mono", "Monaco", "Courier New", "Helvetica"});

    solid = bg3::buildSolid();
    bg3::settleAttitude(solid, tx, ty, tz);
    // The pips are assigned AT the settled attitude, so the face that ends up
    // square to the viewer is the one that carries the roll — and its
    // antipode takes 21 - roll, like a real die.
    bg3::numberFaces(solid, tx, ty, tz, bg3::kNaturalRoll);
    bg3::tumbleAngles(0.0, tx, ty, tz, ax, ay, az);

    ctx.ticker.add([this](double dt) {
      clock += dt;
      bg3::tumbleAngles(clock, tx, ty, tz, ax, ay, az);
      bezelSpin = (float)std::fmod(clock * 1.5, 360.0);      // 1.5 deg/s
      rosetteSpin = (float)std::fmod(-clock * 0.42, 360.0);  // counter

      // The total accumulates as each row lands — retargeted, so the number
      // visibly climbs rather than stepping.
      int target = bg3::kNaturalRoll;
      for (const auto &b : bg3::kBonuses)
        if (clock >= b.landsAt)
          target += b.numDice > 0 ? b.resolved : b.bonus;
      totalAnim += ((float)target - totalAnim) *
                   (1.0f - std::exp(-(float)dt * 13.0f));

      if (clock >= bg3::kOutcomeAt) {
        const float u =
            (float)std::clamp((clock - bg3::kOutcomeAt) / 0.38, 0.0, 1.0);
        const float e = 1.0f - (1.0f - u) * (1.0f - u);
        hatchSpacing = 14.0f - 11.0f * e; // 14 px -> 3 px
        hatchAngle = 12.0f + 12.0f * e;   // a 12 degree swing
        outcomeInk = e;
      }
      return true;
    });

    ctx.composer.render(describe(ctx));
  }

  /** Content changes — the digits, and the rows' MOUNT, which is what makes
   *  the column a dependency chain instead of a stagger. */
  void update(double elapsed, sketch::SketchContext &ctx) override {
    (void)elapsed;
    bool dirty = false;
    const int rounded = (int)std::lround(totalAnim);
    if (rounded != shownTotal) {
      shownTotal = rounded;
      dirty = true;
    }
    // Rows do not EXIST until the die is still; staggerChildren cascades
    // their entrances from that mount.
    if (clock >= bg3::kSettleAt && rowsMounted < (int)bg3::kBonuses.size()) {
      rowsMounted = (int)bg3::kBonuses.size();
      dirty = true;
    }
    if (!outcomeMounted && clock >= bg3::kOutcomeAt) {
      outcomeMounted = true;
      dirty = true;
    }
    if (dirty)
      ctx.composer.render(describe(ctx));
  }
};

SIGIL_SKETCH(Bg3DiceRoll)

// -----------------------------------------------------------------------------
// REPORT — the corner default, from the author who lived with it
//
// The break is real and reproduces exactly as derived: a 20-gon turns 18 deg per
// vertex, cos(18) = 0.9511 is not below cos(30) = 0.8660, so `brackets`,
// `gappedRule` and `weightedCorners` all found ZERO corners on this bezel at
// the default and the ornament rendered blank. At 12 deg all twenty land.
//
// Should the default scale with detected vertex count? NO, and I changed my
// mind while building this. The threshold's job is to tell a VERTEX from a
// finely-sampled CURVE, and the scan steps 2 px — so a rounded corner of
// radius 14 turns ~8.5 deg per sample against this polygon's 18. Less than 2x
// apart. Any default low enough to catch the 20-gon invents corners on every
// rounded rect in the corpus, and a bracket set that quietly grows four extra
// arms is far harder to notice than one that draws nothing. The diagnostic
// that now prints the sharpest break it DID see, plus the number to pass, is
// the right fix and it is what got me to 12 without a bisection.
//
// What was genuinely missing was reachability, and it is now fixed: the three
// helpers took `(width, fill, arm, inset)` with no way to pass an angle, so the
// documented remedy could only be had by abandoning the helper and writing a
// `Border{}` aggregate by hand. They now take a trailing `angleDeg` and this
// file uses it throughout.
//
// One asymmetry remains: `brushes::PatternBrush::cornerAngleDeg` defaults to
// 35, five degrees HIGHER than `Border::cornerAngleDeg`'s 30, so a frame whose
// rules and whose corner tiles are both on the same polygon needs two different
// numbers passed to two differently-shaped APIs to describe one fact about one
// shape. Both are set to 12 here.
//
// MEASURED, at 30 deg, on this bezel — the whole ornament, gone:
//   * all twenty brackets (the tick ladder indexing the die's faces)
//   * the corner weighting on the outer rule, which flattened to one uniform
//     hairline (Weighted with no corners = gappedRule over the whole contour)
//   * the inner gapped rule, which ran CONTINUOUS instead of stopping at each
//     flat — the header's documented degenerate case, seen in the wild
//   * all twenty PatternBrush fleurons, leaving only the side tiles
// and the library printed, correctly and to the exact degree:
//   "no corner cleared the 30.0 threshold, but the sharpest tangent break on
//    this contour is 18.0 — ... Pass a smaller angleDeg, e.g. 11f."
//
// -----------------------------------------------------------------------------
// TWO OTHER THINGS THIS PLATE PAID FOR
//
// 1. `background()` IS UNDER THE FILL, AND THAT IS EASY TO MISREAD AS A BUG.
//    It is the CSS box-shadow slot, documented as such. But the natural
//    sentence for a texture is "give this node a hatch background", and doing
//    that to a node with an opaque fill renders NOTHING — the rosette's
//    60-spoke fan and the portrait's rings both vanished this way. The slot
//    that means "over the surface, under the children" is `overlay()`. The
//    same mistake in reverse cost the bezel band its colour: a `background`
//    drop shadow under a 14%-alpha fill shows straight THROUGH the fill, and
//    turned a gilt band into mud-olive.
//
// 2. A RE-DESCRIBE THROWS AWAY EVERY `PatternBrush` TILE CACHE.  The header of
//    `PatternBrush` says the baked-tile cache lives in the brush VALUE and a
//    constructed brush gets an empty one. The consequence only shows up under
//    a counter: this plate re-describes on every integer the total passes
//    through, so `describe()` ran ~10 times during the reveal, and each run
//    built a fresh brush and re-baked twenty corner tiles plus every side
//    tile. The settled frame measured 9.49 ms while the reveal window measured
//    21.68 — a FAIL that a capture at the settled phase would never have
//    shown. Wrapping the static subtrees in `memo(0, …)` took the reveal to
//    8.41 ms with identical pixels. Bench the window your motion happens in,
//    not just the frame you ship.
//
// PERF, 1200 x 1200, Release:  p50 7.56 ms / p99 8.10 ms settled;
//                              p50 7.69 ms / p99 8.41 ms across the reveal.
//   Dominant: the two rotating baked textures (bezel 660x660 at 1.89 ms, the
//   rosette 524x524 at 1.15 ms) — that is the resample cost of `.rotate()` on
//   a `Cache::Texture` node, and it is the price of the brief's 1.5 deg/s
//   ring, paid deliberately. `bakeScale` was NOT used: it softens gilt
//   hairlines and the study is about hairlines.
// -----------------------------------------------------------------------------
