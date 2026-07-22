#pragma once
// Passive-tree geometry lifted from Path of Exile's public tree export
// (grindinggear/skilltree-export data.json): group centres, the
// orbitRadii/skillsPerOrbit ladder, orbitIndex -> angle, node kinds, and
// which links are same-group/same-orbit ARCS rather than straight spokes.
// Generated, then frozen — this is a study fixture, not a live import.
//
// Constants from that export, verbatim:
//   orbitRadii     [0, 82, 162, 335, 493, 662, 846]
//   skillsPerOrbit [1, 6, 16, 16, 40, 72, 72]
//
// The cluster is every node within 1500 tree units of one keystone that
// is still connected to it (39 of them), transformed
// into the 900x640 stage with a uniform scale (0.34928) so the orbit
// proportions survive. Node names and stat wording are ours; the export
// carries its own and we are not reproducing it.

#include <cstdint>

namespace compose_gallery::skill_tree_data {

enum class Kind : uint8_t { Minor, Notable, Keystone, Jewel, Mastery };
enum class State : uint8_t { Unallocated, CanAllocate, Allocated };

struct Node {
  float x, y;        ///< scene space (900x640)
  uint8_t group;     ///< index into kGroups
  uint8_t orbit;     ///< index into the orbitRadii ladder
  Kind kind;
  State state;
  const char *name;  ///< empty for anonymous minors
};

struct Group {
  float x, y;            ///< scene-space centre
  float radius[7];       ///< occupied orbit radii, scene space, 0 = unused
};

struct Edge {
  uint8_t a, b;      ///< indices into kNodes
  bool arc;          ///< same group + same orbit: follows the circle
  uint8_t group;     ///< arc centre (index into kGroups), 0xFF for spokes
};

/** One tooltip's worth of copy for the named nodes. */
struct Detail {
  const char *name;
  const char *kind;
  const char *stats[3];   ///< nullptr-terminated
  const char *flavour;    ///< keystones only
};

inline constexpr Group kGroups[] = {
    {728.13f, 110.28f, {0.00f, 0.00f, 56.58f, 0.00f, 0.00f, 0.00f, 0.00f}},
    {491.23f, 145.47f, {0.00f, 0.00f, 56.58f, 0.00f, 0.00f, 0.00f, 0.00f}},
    {699.83f, 278.62f, {0.00f, 0.00f, 56.58f, 0.00f, 0.00f, 0.00f, 0.00f}},
    {227.70f, 335.93f, {0.00f, 0.00f, 0.00f, 117.01f, 0.00f, 0.00f, 0.00f}},
    {491.23f, 335.93f, {0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f}},
    {699.83f, 431.97f, {0.00f, 0.00f, 56.58f, 0.00f, 0.00f, 0.00f, 0.00f}},
    {409.57f, 415.24f, {0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f}},
    {491.23f, 525.42f, {0.00f, 0.00f, 56.58f, 0.00f, 0.00f, 0.00f, 0.00f}},
};

inline constexpr Node kNodes[] = {
    {706.47f, 58.00f, 0, 2, Kind::Notable, State::Unallocated, "Kindled Reserve"},
    {780.40f, 88.62f, 0, 2, Kind::Minor, State::Unallocated, "Fire Damage"},
    {491.23f, 88.89f, 1, 2, Kind::Notable, State::Unallocated, "Cinderpath"},
    {728.13f, 110.28f, 0, 0, Kind::Mastery, State::Unallocated, "Ember Mastery"},
    {675.85f, 131.93f, 0, 2, Kind::Minor, State::CanAllocate, "Life"},
    {434.64f, 145.47f, 1, 2, Kind::Minor, State::Unallocated, "Resistance"},
    {491.23f, 145.47f, 1, 0, Kind::Mastery, State::Unallocated, "Ember Mastery"},
    {547.81f, 145.47f, 1, 2, Kind::Minor, State::Unallocated, "Burning"},
    {749.78f, 162.55f, 0, 2, Kind::Minor, State::Unallocated, "Armour"},
    {491.23f, 202.06f, 1, 2, Kind::Minor, State::CanAllocate, "Energy Shield"},
    {678.18f, 226.35f, 2, 2, Kind::Minor, State::Unallocated, "Elemental Damage"},
    {721.49f, 226.35f, 2, 2, Kind::Minor, State::Unallocated, "Cast Speed"},
    {182.92f, 227.83f, 3, 3, Kind::Notable, State::Allocated, "Ashen Discipline"},
    {272.48f, 227.83f, 3, 3, Kind::Minor, State::Allocated, "Fire Damage"},
    {643.25f, 278.62f, 2, 2, Kind::Minor, State::CanAllocate, "Life"},
    {699.83f, 278.62f, 2, 0, Kind::Mastery, State::Unallocated, "Ember Mastery"},
    {756.42f, 278.62f, 2, 2, Kind::Notable, State::Unallocated, "Wickwright"},
    {119.60f, 291.16f, 3, 3, Kind::Minor, State::CanAllocate, "Resistance"},
    {335.80f, 291.16f, 3, 3, Kind::Minor, State::Allocated, "Burning"},
    {678.18f, 330.90f, 2, 2, Kind::Minor, State::Unallocated, "Armour"},
    {721.49f, 330.90f, 2, 2, Kind::Minor, State::Unallocated, "Energy Shield"},
    {227.70f, 335.93f, 3, 0, Kind::Mastery, State::CanAllocate, "Ember Mastery"},
    {344.71f, 335.93f, 3, 3, Kind::Minor, State::Allocated, "Elemental Damage"},
    {491.23f, 335.93f, 4, 0, Kind::Minor, State::Allocated, "Cast Speed"},
    {119.60f, 380.71f, 3, 3, Kind::Minor, State::Unallocated, "Fire Damage"},
    {335.80f, 380.71f, 3, 3, Kind::Minor, State::CanAllocate, "Life"},
    {647.56f, 410.32f, 5, 2, Kind::Minor, State::CanAllocate, "Resistance"},
    {409.57f, 415.24f, 6, 0, Kind::Keystone, State::Allocated, "EMBERWARDEN"},
    {699.83f, 431.97f, 5, 0, Kind::Mastery, State::Unallocated, "Ember Mastery"},
    {756.42f, 431.97f, 5, 2, Kind::Notable, State::Unallocated, "Tinderheart"},
    {182.92f, 444.04f, 3, 3, Kind::Notable, State::Unallocated, "Long Watch"},
    {272.48f, 444.04f, 3, 3, Kind::Minor, State::Unallocated, "Burning"},
    {647.56f, 453.63f, 5, 2, Kind::Minor, State::Unallocated, "Armour"},
    {491.23f, 468.83f, 7, 2, Kind::Minor, State::CanAllocate, "Energy Shield"},
    {543.50f, 503.76f, 7, 2, Kind::Minor, State::Unallocated, "Elemental Damage"},
    {434.64f, 525.42f, 7, 2, Kind::Minor, State::Unallocated, "Cast Speed"},
    {491.23f, 525.42f, 7, 0, Kind::Mastery, State::Unallocated, "Ember Mastery"},
    {543.50f, 547.07f, 7, 2, Kind::Minor, State::Unallocated, "Fire Damage"},
    {491.23f, 582.00f, 7, 2, Kind::Notable, State::Unallocated, "Sootbound"},
};
inline constexpr int kNodeCount = (int)(sizeof(kNodes) / sizeof(kNodes[0]));

inline constexpr Edge kEdges[] = {
    {0, 3, false, 0xFF},
    {0, 4, true, 0},
    {1, 8, true, 0},
    {2, 7, true, 1},
    {2, 6, false, 0xFF},
    {2, 5, true, 1},
    {4, 23, false, 0xFF},
    {4, 8, true, 0},
    {5, 9, true, 1},
    {7, 9, true, 1},
    {9, 23, false, 0xFF},
    {10, 14, true, 2},
    {10, 11, true, 2},
    {11, 16, true, 2},
    {12, 21, false, 0xFF},
    {12, 17, true, 3},
    {12, 13, true, 3},
    {13, 18, true, 3},
    {14, 19, true, 2},
    {14, 23, false, 0xFF},
    {15, 16, false, 0xFF},
    {16, 20, true, 2},
    {17, 24, true, 3},
    {18, 22, true, 3},
    {19, 20, true, 2},
    {21, 30, false, 0xFF},
    {22, 25, true, 3},
    {22, 23, false, 0xFF},
    {23, 27, false, 0xFF},
    {23, 33, false, 0xFF},
    {23, 26, false, 0xFF},
    {24, 30, true, 3},
    {25, 31, true, 3},
    {26, 32, true, 5},
    {28, 29, false, 0xFF},
    {29, 32, true, 5},
    {30, 31, true, 3},
    {33, 35, true, 7},
    {33, 34, true, 7},
    {34, 37, true, 7},
    {35, 38, true, 7},
    {36, 38, false, 0xFF},
    {37, 38, true, 7},
};
inline constexpr int kEdgeCount = (int)(sizeof(kEdges) / sizeof(kEdges[0]));

inline constexpr Detail kDetails[] = {
    {"EMBERWARDEN", "KEYSTONE", {"Fire damage you take is dealt over 4 seconds", "You cannot be Ignited", nullptr}, "The first flame asked only to be carried."},
    {"HOLLOW COVENANT", "KEYSTONE", {"Life recovery applies to Energy Shield instead", "Maximum Life is halved", nullptr}, "Trade the vessel; keep the flame."},
    {"STILLPOINT", "KEYSTONE", {"Skills cost no Mana while stationary", "You cannot regenerate Mana while moving", nullptr}, "The world turns. You need not."},
    {"Kindled Reserve", "NOTABLE", {"18% increased Fire Damage", "+12 to maximum Life", nullptr}, nullptr},
    {"Cinderpath", "NOTABLE", {"20% increased Damage over Time", "10% increased Area of Effect", nullptr}, nullptr},
    {"Ashen Discipline", "NOTABLE", {"+16% to Fire Resistance", "6% increased maximum Life", nullptr}, nullptr},
    {"Wickwright", "NOTABLE", {"25% increased Ignite Duration", "12% increased Burning Damage", nullptr}, nullptr},
    {"Tinderheart", "NOTABLE", {"+20 to maximum Life", "8% increased Attack Speed while on Low Life", nullptr}, nullptr},
    {"Long Watch", "NOTABLE", {"+8% to all Elemental Resistances", "15% increased Stun Threshold", nullptr}, nullptr},
    {"Sootbound", "NOTABLE", {"30% increased Armour", "5% additional Physical Damage Reduction", nullptr}, nullptr},
    {"Emberglass", "NOTABLE", {"+40 to maximum Energy Shield", "10% increased Energy Shield Recharge Rate", nullptr}, nullptr},
};
inline constexpr int kDetailCount = (int)(sizeof(kDetails) / sizeof(kDetails[0]));

} // namespace compose_gallery::skill_tree_data
