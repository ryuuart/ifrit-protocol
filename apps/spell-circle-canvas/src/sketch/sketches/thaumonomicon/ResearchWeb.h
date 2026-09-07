#pragma once

/** @file
 * THE RESEARCH WEB, transcribed: Thaumcraft 4's `alchemy.json`, all 22
 * entries and the 24 edges between them, plus the reconstructed save
 * state the browser is opened on.
 *
 * It stands beside the sketch rather than inside it because it is a
 * CATALOGUE — a data set with a provenance (`alchemy.json`, `en_us.lang`
 * for the titles, `ConfigResearch.java` for the flags) that decides the
 * plate byte for byte, and that nothing about the drawing has an opinion
 * on. A resource read at run time would be wrong for the same reason:
 * these bytes ARE the picture, where a resource is something the picture
 * is drawn over.
 */

#include <include/core/SkColor.h>

#include <cstdint>
#include <string_view>

namespace thaum {

// THE GRAPH — alchemy.json, all 22 entries, verbatim.

enum Meta : uint8_t {
  kPlain = 0,
  kRound = 1,
  kHex = 2,
  kSpiky = 4,
  kHidden = 8,
  kReverse = 16
};
enum State : uint8_t { kComplete, kUnlockable, kLocked };

// fields are grouped by what they belong to, not by size
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
struct Node {
  const char* key;
  int col, row;
  uint8_t meta;
  int icon;  // index into the reconstructed glyph set
  State state;
  const char* title;  // en_us.lang research.<KEY>.title
  uint8_t warp;       // max stage warp — drives drawForbidden()
  bool flagResearch;  // EnumResearchFlag.RESEARCH -> the UV(176,16) badge
  bool flagPage;      // EnumResearchFlag.PAGE     -> the UV(208,16) badge
};

// Icon glyph ids (reconstructed art; the names are the mod's own texture
// names from alchemy.json's `icons`).
enum Glyph {
  gAspect,
  gAlumentum,
  gIngot,
  gCluster,
  gTallow,
  gBucket,
  gBottle,
  gSalts,
  gSoap,
  gSpa,
  gSmelter,
  gJar,
  gTube,
  gSmelterThaum,
  gSmelterVoid,
  gSmelterAux,
  gVent,
  gCentrifuge,
  gThaumatorium,
  gInput,
  gUrn,
  gSprayer,
};

// The save state is RECONSTRUCTED — a mid-game player who has just finished
// the thaumium smelter and can see, but not yet take, the automation branch.
// It is chosen so the one hovered node's tooltip is self-consistent with the
// graph: THAUMATORIUM needs CENTRIFUGE, and CENTRIFUGE is not complete.
constexpr Node kNodes[] = {
    {"BASEALCHEMY", 0, 0, kRound | kHidden, gAspect, kComplete, "Basic Alchemy",
     0, false, false},
    {"ALUMENTUM", 2, -1, kPlain, gAlumentum, kComplete, "Alumentum", 0, false,
     false},
    {"METALLURGY", 2, 2, kPlain, gIngot, kComplete, "Alchemical Metallurgy", 0,
     false, false},
    {"METALPURIFICATION", 2, 4, kPlain, gCluster, kUnlockable,
     "Metal Purification", 0, false, false},
    {"HEDGEALCHEMY", -2, 0, kPlain, gTallow, kComplete, "Hedge Alchemy", 0,
     false, false},
    {"LIQUIDDEATH", -4, 2, kPlain, gBucket, kComplete, "Liquid Death", 3, false,
     true},
    {"BOTTLETAINT", -6, 0, kPlain, gBottle, kUnlockable, "Bottled Taint", 2,
     true, false},
    {"BATHSALTS", -4, -2, kHidden, gSalts, kComplete, "Purifying Bath Salts", 0,
     false, false},
    {"SANESOAP", -3, -4, kPlain, gSoap, kUnlockable, "Sanity Soap", 0, false,
     false},
    {"ARCANESPA", -5, -4, kPlain, gSpa, kUnlockable, "Arcane Spa", 0, false,
     false},
    {"ESSENTIASMELTER", 4, 0, kSpiky, gSmelter, kComplete, "Essentia Smelting",
     0, false, false},
    {"WARDEDJARS", 4, -2, kRound, gJar, kComplete, "Warded Jars & labels", 0,
     false, false},
    {"TUBES", 6, -2, kPlain, gTube, kComplete, "Essentia Tubes", 0, false,
     false},
    {"ESSENTIASMELTERTHAUMIUM", 8, 0, kPlain, gSmelterThaum, kComplete,
     "Thaumium Essentia Smelter", 0, true, true},
    {"ESSENTIASMELTERVOID", 12, 0, kPlain, gSmelterVoid, kLocked,
     "Void Metal Essentia Smelter", 0, false, false},
    {"IMPROVEDSMELTING", 5, -4, kPlain, gSmelterAux, kLocked,
     "Improved Essentia Distillation (part 1)", 0, false, false},
    {"IMPROVEDSMELTING2", 3, -4, kPlain, gVent, kLocked,
     "Improved Essentia Distillation (part 2)", 0, false, false},
    {"CENTRIFUGE", 7, -4, kPlain, gCentrifuge, kUnlockable,
     "Essentia Centrifuge", 0, false, false},
    {"THAUMATORIUM", 10, -2, kSpiky, gThaumatorium, kLocked,
     "Alchemical Automation", 0, false, false},
    {"ESSENTIATRANSPORT", 12, -2, kPlain, gInput, kLocked,
     "Advanced Essentia Transport", 0, false, false},
    {"EVERFULLURN", -2, 2, kPlain, gUrn, kComplete, "Everfull Urn", 0, false,
     false},
    {"POTIONSPRAYER", -1, -2, kReverse, gSprayer, kUnlockable, "Potion Sprayer",
     0, false, false},
};
constexpr int kNodeCount = (int)(sizeof(kNodes) / sizeof(kNodes[0]));

/** THE TABLE IS SMALL AND THE COMPARISON IS A VIEW. Twenty-two entries
 *  is a linear scan either way; what a `std::string` per comparison adds
 *  is an allocation per node per edge per pass, which is the whole cost
 *  of the lookup. */
inline const Node& nodeByKey(std::string_view k) {
  for (const Node& n : kNodes)
    if (n.key == k) return n;
  return kNodes[0];
}
inline int indexByKey(std::string_view k) {
  for (int i = 0; i < kNodeCount; ++i)
    if (kNodes[(size_t)i].key == k) return i;
  return 0;
}

/** The four edge tiers of genResearchBackgroundZoomable:550-571 — the colour
 *  the white tile art is MULTIPLIED by, the z it is drawn at, and whether it
 *  carries an arrowhead. Only three of the four occur in this save: nothing in
 *  ALCHEMY has an unknown sibling, so the 0.1875 tier never fires. */
enum EdgeTier : uint8_t {
  kParentKnown,    // 0.6,0.6,0.6  z=3  arrow
  kParentUnknown,  // 0.2,0.2,0.2  z=2  arrow
  kSiblingKnown,   // 0.3,0.3,0.4  z=1  no arrow
};
struct Edge {
  const char* child;
  const char* parent;
  EdgeTier tier;
  bool flipped;  // source.hasMeta(REVERSE)
};

// Parent edges are drawn only when the parent is in the same category, and are
// SUPPRESSED when the parent lists the child as a sibling (:545) — which is why
// WARDEDJARS<-ESSENTIASMELTER appears once, as a sibling edge, and not twice.
// Parents outside ALCHEMY (UNLOCKALCHEMY, f_toomuchflux, BELLOWS, INFUSION,
// BASEELDRITCH) draw nothing; a `~` parent (POTIONSPRAYER's ~TUBES) is
// explicitly skipped at :547.
constexpr Edge kEdges[] = {
    {"ALUMENTUM", "BASEALCHEMY", kParentKnown, false},
    {"METALLURGY", "BASEALCHEMY", kParentKnown, false},
    {"HEDGEALCHEMY", "BASEALCHEMY", kParentKnown, false},
    {"METALPURIFICATION", "METALLURGY", kParentKnown, false},
    {"LIQUIDDEATH", "HEDGEALCHEMY", kParentKnown, false},
    {"BOTTLETAINT", "HEDGEALCHEMY", kParentKnown, false},
    {"BATHSALTS", "HEDGEALCHEMY", kParentKnown, false},
    {"EVERFULLURN", "HEDGEALCHEMY", kParentKnown, false},
    {"POTIONSPRAYER", "HEDGEALCHEMY", kParentKnown, true},
    {"SANESOAP", "BATHSALTS", kParentKnown, false},
    {"ARCANESPA", "BATHSALTS", kParentKnown, false},
    {"ESSENTIASMELTER", "ALUMENTUM", kParentKnown, false},
    {"ESSENTIASMELTER", "METALLURGY", kParentKnown, false},
    {"WARDEDJARS", "ESSENTIASMELTER", kSiblingKnown, false},
    {"TUBES", "WARDEDJARS", kParentKnown, false},
    {"IMPROVEDSMELTING", "TUBES", kParentKnown, false},
    {"CENTRIFUGE", "TUBES", kParentKnown, false},
    {"ESSENTIASMELTERTHAUMIUM", "TUBES", kParentKnown, false},
    {"ESSENTIASMELTERTHAUMIUM", "METALLURGY", kParentKnown, false},
    {"IMPROVEDSMELTING2", "IMPROVEDSMELTING", kParentUnknown, false},
    {"THAUMATORIUM", "CENTRIFUGE", kParentUnknown, false},
    {"THAUMATORIUM", "ESSENTIASMELTERTHAUMIUM", kParentKnown, false},
    {"ESSENTIATRANSPORT", "THAUMATORIUM", kParentUnknown, false},
    {"ESSENTIASMELTERVOID", "ESSENTIASMELTERTHAUMIUM", kParentKnown, false},
};
constexpr int kEdgeCount = (int)(sizeof(kEdges) / sizeof(kEdges[0]));

}  // namespace thaum
