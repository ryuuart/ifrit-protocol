/** @file
 * vagrant_story_target — VAGRANT STORY, THE BATTLE-MODE TARGETING SCREEN
 *
 * Vagrant Story (Square Product Development Division 4 / Matsuno, 2000),
 * PlayStation. Battle mode, the instant the attack button is tapped:
 * world time freezes, a wireframe REACH SPHERE blooms around Ashley, and
 * every body part of the enemy inside it becomes selectable, each with
 * its own HP, its own armour and its own hit percentage. The HP / MP /
 * RISK gauges sit hard in the bottom-left corner; the selected limb's
 * card hangs beside the enemy.
 *
 * THE SCREEN IS A 3D SCENE WITH A BITMAP OVERLAY OVER IT, and this is a
 * SET for exactly that reason. Ashley, the Dullahan and the ground are
 * lit bodies; the reach sphere is real wire in real space, so it reads
 * by intersecting geometry rather than by being drawn to look as though
 * it did; and the overlay is a compose tree painted into a texture and
 * hung on one unlit quad that fills the frustum. Nothing about the
 * overlay knows it is on a quad and nothing about the quad knows it is
 * an overlay — the seam is an ordinary `material::Texture` in a
 * base-colour slot.
 *
 * THE TWO GRIDS ARE REAL AND THEY SURVIVE THE MOVE. Vagrant Story
 * displays at a 512x240 hi-res framebuffer but computes polygon
 * coordinates in 320x240 regardless, so it draws 3D geometry on a
 * 320-wide grid and UI text on a 512-wide one in the same frame. The
 * canvas is 1280x960 — 4x the polygon grid, 2.5x the text grid
 * horizontally and 4x vertically — and the overlay's type is baked
 * aliased and presented at an integer scale, which is what a 512-grid
 * bitmap face is.
 *
 * SOURCE — fetched and read, not remembered
 *
 * github.com/ser-pounce/rood-reverse, a byte-matching Vagrant Story
 * decompilation: src/BATTLE/BATTLE.PRG/146C.{h,c} and
 * assets/MENU/MENU4.PRG/status.yaml.
 *
 * THE SIX-LIMB MODEL IS A STRUCT. 146C.h:410-420 declares
 * vs_battle_uiEquipment_limb — hp, maxHp, agilityDefenseBonus,
 * nameIndex, chainEvasion, four damage-type resistances, eight
 * affinities and an armour piece at offset 0x18 — and 146C.h:551 carries
 * limbs[6] inside vs_battle_actor2 at offset 0x388. Six parts, each with
 * its own everything.
 *
 * THE HIT PERCENTAGE IS _getAgilityDifference, 146C.c:7110-7160,
 * transcribed into accuracy() below: both sides sum agility over
 * accessory, armour, weapon and shield; both are multiplied by
 * (100 - risk)/100 in integer arithmetic; the difference plus 100 is
 * clamped below at 0 and away from 255. _doesAttackHit (146C.c:7294)
 * then clamps to 100 — and at :7334, AFTER that clamp, adds 10 when the
 * source is actor 0. Ashley is actor 0, so he silently gets +10% to hit
 * that the printed number does not include, and a printed 100 is a true
 * 110: rand(100) < 110 can never fail. Both numbers are on the overlay,
 * side by side, which is the one thing this study exists to show.
 *
 * RISK. _getRiskModifier (146C.c:7265) is rate = ((risk + 150) * 100) /
 * 256, so RISK 0 already reads 58 and the rate reaches 100 at risk 106.
 * RISK also multiplies BOTH sides' agility by (100 - risk)/100, so
 * raising your own RISK costs you exactly risk% of your accuracy.
 * _getChainEvasionModifier (146C.c:7276) is (255 - chainEvasion) * 100 /
 * 255, per limb, off a byte.
 *
 * THE SPHERE AND THE WEDGE ARE BIT FIELDS. 146C.h:540-546 packs
 * enemyClass:3, reach:5 and currentAttackShapeAngle:5. The reach is a
 * 5-bit radius and the attack shape is a 5-bit angle over 32 divisions,
 * 11.25 degrees per step. enemyClass:3 is the six classes MENU9's
 * statHeaders.yaml confirms: human, beast, undead, phantom, dragon,
 * evil.
 *
 * TRUTHFUL TEXT. status.yaml gives hp / mp / risk, the condition tiers
 * critical / damaged / wounded / good / excellent, and the ten defence
 * rows physical, air, fire, earth, water, light, dark, blunt, edged,
 * piercing.
 *
 * RECONSTRUCTED: the HUD's rectangles, the figures' proportions, the
 * palette, the face, the humanoid part naming (the struct carries only a
 * nameIndex; HEAD/BODY/R.ARM/L.ARM/R.LEG/L.LEG is the conventional
 * reading) and every stat value. The decompilation has not matched the
 * render code, so no pixel here comes from it — only the arithmetic.
 *
 * THE CAMERA DOES NOT MOVE, and that is the subject rather than a
 * simplification: the screen this reconstructs is the frame in which
 * world time has stopped. What moves is the reach sphere, which turns,
 * and the marker ring on the selected limb, which pulses.
 *
 * EDIT THESE FIRST
 *   kRisk — Ashley's RISK. It moves the gauge, the printed hit number
 *     and the true one at once, because the formula reads it twice.
 *   kSelected — which of the six limbs is targeted (0..5).
 *   kReach — the 5-bit reach field; the sphere's radius is 24 units per
 *     step of it.
 *   kHudScale — the integer scale the baked bitmap face is presented at.
 */

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/kit/PixelType.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilcompose/typography/Type.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilsketch/set/Set.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilworld/kit/Kit.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace sketch = sigil::sketch;
namespace world = sigil::world;
namespace material = sigil::material;
namespace compose = sigil::compose;
namespace weave = sigil::weave;

using namespace sigil::world;

namespace vs {

namespace gm = ::sigil::geometry::mesh;
namespace ck = ::sigil::compose::kit;

constexpr float kPi = 3.14159265358979f;
constexpr float kDeg = kPi / 180.0f;

// ---------------------------------------------------------------------------
// THE ARITHMETIC. Every number below is the decompiled function, integer
// division and clamp order included.

// fields are grouped by what they belong to, not by size
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
struct Limb {
  const char* name;
  short hp, maxHp;
  signed char agilityDefenseBonus;
  unsigned char chainEvasion;
  const char* armour;
  short types[4];       // blunt / edged / piercing / +1
  short affinities[8];  // air fire earth water light dark +2
  /** status.yaml's first defence row. It is NOT in the limb struct —
   *  physical defence lives on the ARMOUR, which the limb owns at offset
   *  0x18 — so it is carried alongside rather than faked into types[3]. */
  short physical;
};

// RECONSTRUCTED values in a DOCUMENTED struct. Part naming is the
// conventional humanoid reading of limbs[0..5]; the struct itself carries
// only a nameIndex.
constexpr Limb kLimbs[6] = {
    {"HEAD",
     88,
     120,
     6,
     40,
     "HOUNSKULL",
     {12, -8, 20, 0},
     {4, -6, 0, 2, 8, -10, 0, 0},
     18},
    {"BODY",
     210,
     260,
     2,
     18,
     "CUIRASS",
     {24, 10, -6, 0},
     {0, 6, 8, 0, 2, -4, 0, 0},
     31},
    {"R.ARM",
     96,
     140,
     -3,
     92,
     "VAMBRACE",
     {6, 4, 2, 0},
     {-2, 0, 4, 6, 0, 0, 0, 0},
     14},
    {"L.ARM",
     132,
     140,
     1,
     55,
     "GAUNTLET",
     {8, 12, -4, 0},
     {2, 4, 0, 0, -6, 2, 0, 0},
     16},
    {"R.LEG",
     148,
     180,
     0,
     30,
     "GREAVE",
     {14, 6, 8, 0},
     {0, 2, 10, 4, 0, 0, 0, 0},
     21},
    {"L.LEG",
     180,
     180,
     4,
     12,
     "SABATON",
     {16, 2, 12, 0},
     {6, 0, 6, 0, 0, -2, 0, 0},
     23},
};

/** status.yaml's condition tiers. */
constexpr const char* kCondition[5] = {"CRITICAL", "DAMAGED", "WOUNDED", "GOOD",
                                       "EXCELLENT"};

/** Ashley and the Dullahan, in the fields the formula actually reads. */
struct Actor {
  int agility;
  int accessoryAgi;
  int armourAgiSum;  // SUM(limbs[0..5].armor.currentAgility)
  int weaponAgi, shieldAgi;
  bool drawn;  // vs_battle_actors[..]->unk20 & 1
};
constexpr Actor kAshley{42, 3, -7, 2, -4, true};
constexpr Actor kEnemy{30, 0, 5, 0, 0, false};

inline int actorAgi(const Actor& a) {
  int v = a.agility + a.accessoryAgi + a.armourAgiSum;
  if (a.drawn) v += a.weaponAgi + a.shieldAgi;
  return v;
}

/** _getAgilityDifference, 146C.c:7110 — variance and gems left at zero so
 *  the printed number is the deterministic part the HUD shows. */
inline int agilityDifference(int part, float riskSrc, float riskTgt) {
  const int rs = (int)std::lround(riskSrc), rt = (int)std::lround(riskTgt);
  int src = actorAgi(kAshley) * (100 - rs) / 100;
  int tgt = actorAgi(kEnemy) + kLimbs[part].agilityDefenseBonus;
  tgt = tgt * (100 - rt) / 100;
  int d = src - tgt + 100;
  if (d < 0)
    d = 0;
  else if (d == 255)
    d = 254;
  return d;
}
/** What the HUD prints: _doesAttackHit's clamp to 100, before the cheat. */
inline int printedHit(int part, float risk) {
  return std::min(agilityDifference(part, risk, 0.0f), 100);
}
/** What actually rolls: 146C.c:7334 adds 10 for actorId 0 AFTER the clamp. */
inline int trueHit(int part, float risk) { return printedHit(part, risk) + 10; }
/** _getRiskModifier, 146C.c:7265. */
inline int riskRate(float risk) {
  const int r = ((int)std::lround(risk) + 150) * 100 / 256;
  return r == 255 ? 254 : r;
}
/** _getChainEvasionModifier, 146C.c:7276. */
inline int chainEvasionRate(int part) {
  const int r = (255 - kLimbs[part].chainEvasion) * 100 / 255;
  return r == 255 ? 254 : r;
}
inline const char* conditionOf(int hp, int maxHp) {
  const float f = maxHp > 0 ? (float)hp / (float)maxHp : 0.0f;
  if (f >= 0.999f) return kCondition[4];
  if (f >= 0.70f) return kCondition[3];
  if (f >= 0.45f) return kCondition[2];
  if (f >= 0.20f) return kCondition[1];
  return kCondition[0];
}

// The five-bit fields, as data.
constexpr int kReach = 9;                     // u_int reach : 5
constexpr int kAttackShapeAngle = 12;         // currentAttackShapeAngle : 5
constexpr float kAngleStep = 360.0f / 32.0f;  // 11.25 deg per step
constexpr int kEnemyClass = 2;                // u_int enemyClass : 3 -> undead
constexpr const char* kClassNames[6] = {"HUMAN",   "BEAST",  "UNDEAD",
                                        "PHANTOM", "DRAGON", "EVIL"};
constexpr const char* kWeaponName = "KATANA";
constexpr int kSelected = 2;  // R.ARM
constexpr float kRisk = 34.0f;
constexpr int kAshleyHp = 268, kAshleyMaxHp = 340;
constexpr int kAshleyMp = 91, kAshleyMaxMp = 160;

// ---------------------------------------------------------------------------
// THE SET. World units are centimetres and Ashley is 176 of them.

/** The reach field as a radius: 24 units per step of the 5-bit value, so
 *  the sphere reaches past the Dullahan and not past the room. */
constexpr float kSphereRadius = (float)kReach * 24.0f;
/** Where the two of them stand. Ashley is left and near, the Dullahan
 *  right and far, and the gap between them is inside the sphere — which
 *  is the whole condition the screen exists to report. */
constexpr glm::vec3 kAshleyAt{-142.0f, 0.0f, 46.0f};
constexpr glm::vec3 kEnemyAt{-16.0f, 0.0f, -70.0f};
constexpr float kChest = 104.0f;

/** The palette. Bone type over cold slate, the reach wire cyan, RISK
 *  amber running to blood. */
constexpr SkColor4f kBone{0.910f, 0.894f, 0.847f, 1.0f};
constexpr SkColor4f kCyan{0.498f, 0.847f, 0.910f, 1.0f};
constexpr SkColor4f kAmber{0.847f, 0.628f, 0.220f, 1.0f};
constexpr SkColor4f kBlood{0.659f, 0.094f, 0.125f, 1.0f};

/** A wire of the reach sphere: a torus thin enough to be a line at this
 *  distance, unlit so the room's emitters do not shade it. A wire is its
 *  own light in the reference too — the sphere is drawn, not lit. */
material::Material wire(float alpha, float glow) {
  return material::kit::unlit(
      {.baseColor = {kCyan.fR, kCyan.fG, kCyan.fB, alpha},
       .emissive = {kCyan.fR, kCyan.fG, kCyan.fB, 1.0f},
       .emissiveStrength = glow});
}

/** WHAT A LIMB IS IN SPACE: where its centre sits relative to the
 *  figure's feet and how big it is. Six entries, in the struct's own
 *  order, so the body on screen and the six cards on the overlay are one
 *  list read twice. */
struct Part {
  glm::vec3 at;
  glm::vec3 radii;
  float exponent;
};
constexpr std::array<Part, 6> kParts = {
    Part{{0.0f, 158.0f, 0.0f}, {15.0f, 18.0f, 15.0f}, 2.4f},   // HEAD
    Part{{0.0f, 108.0f, 0.0f}, {24.0f, 34.0f, 16.0f}, 3.0f},   // BODY
    Part{{-27.0f, 110.0f, 4.0f}, {9.0f, 32.0f, 9.0f}, 2.0f},   // R.ARM
    Part{{27.0f, 110.0f, 4.0f}, {9.0f, 32.0f, 9.0f}, 2.0f},    // L.ARM
    Part{{-13.0f, 38.0f, 0.0f}, {11.0f, 38.0f, 11.0f}, 2.0f},  // R.LEG
    Part{{13.0f, 38.0f, 0.0f}, {11.0f, 38.0f, 11.0f}, 2.0f}};  // L.LEG

/** A figure: the six parts of limbs[6], as six bodies. Nothing here is a
 *  character rig — the point is that the thing the cards address and the
 *  thing on screen are the same six entries. */
Element figure(const std::string& prefix, glm::vec3 at, float facingDeg,
               float scale, const material::Material& skin, float selectedPulse,
               int selected) {
  Element body = Element().key(prefix).at(at).rotateY(facingDeg).scale(scale);
  for (size_t i = 0; i < kParts.size(); ++i) {
    const Part& p = kParts[i];
    Element limb = Element()
                       .key(prefix + "-" + kLimbs[i].name)
                       .at(p.at)
                       .mesh(gm::superellipsoid(p.radii, p.exponent, 20, 14))
                       .fill(skin)
                       .tag("limb");
    body.child(std::move(limb));
    if ((int)i != selected) continue;
    // THE MARKER RING: a ring lying flat round the limb, pulsing. Flat
    // because the figure it hangs on is turned to face its opponent and a
    // ring standing upright inside that turn is seen edge-on; a
    // horizontal one reads the same from every bearing. It is the only
    // thing in the scene that moves, because the screen this
    // reconstructs is the one in which nothing else does.
    const float r =
        std::max(p.radii.x, p.radii.z) + 42.0f + 9.0f * selectedPulse;
    body.child(Element()
                   .key(prefix + "-mark")
                   .at(p.at)
                   .mesh(gm::torus(r, 3.2f, 36, 6))
                   .fill(wire(1.0f, 3.4f))
                   .tag("mark"));
  }
  return body;
}

/** THE REACH SPHERE: eight meridians and five latitude rings of real
 *  wire round Ashley's chest, turning. Every ring is one torus, so the
 *  near arc and the far arc of a wire are the same body seen from two
 *  sides and the sphere reads as a sphere without a hidden-line pass. */
Element reachSphere(float seconds) {
  Element sphere =
      Element().key("reach").at(kAshleyAt + glm::vec3{0.0f, kChest, 0.0f});
  sphere.rotateY(seconds * 26.0f);
  constexpr int kMeridians = 8;
  for (int i = 0; i < kMeridians; ++i) {
    // The ring stands upright in its OWN node and the bearing is its
    // parent's, because a node holds one rotation per axis and applies
    // them in a fixed order — asking one node for both would turn a ring
    // that is already upright about the axis it is upright around, and
    // every meridian would land on the same one.
    Element bearing = Element()
                          .key("meridian" + std::to_string(i))
                          .rotateY((float)i * 180.0f / (float)kMeridians);
    bearing.child(Element()
                      .key("ring")
                      .rotateX(90.0f)
                      .mesh(gm::torus(kSphereRadius, 1.1f, 64, 5))
                      .fill(wire(0.34f, 1.5f))
                      .tag("wire"));
    sphere.child(std::move(bearing));
  }
  constexpr int kLatitudes = 5;
  for (int i = 0; i < kLatitudes; ++i) {
    const float lat = ((float)i - 2.0f) * 26.0f * kDeg;
    sphere.child(
        Element()
            .key("latitude" + std::to_string(i))
            .at({0.0f, kSphereRadius * std::sin(lat), 0.0f})
            .mesh(gm::torus(kSphereRadius * std::cos(lat), 1.1f, 64, 5))
            .fill(wire(i == 2 ? 0.62f : 0.30f, i == 2 ? 2.4f : 1.5f))
            .tag("wire"));
  }
  return sphere;
}

/** THE ATTACK WEDGE, which is the other 5-bit field: the sphere's 32
 *  divisions drawn as ticks round its equator, the ones inside the
 *  current attack shape long and bright and the rest short, laid on
 *  the floor under the sphere. It is a sibling of the sphere rather than
 *  a child, because the wedge is fixed to Ashley's facing while the
 *  wires turn. */
Element attackLadder() {
  Element ladder =
      Element().key("ladder").at(kAshleyAt + glm::vec3{0.0f, 6.0f, 0.0f});
  for (int i = 0; i < 32; ++i) {
    const float a = (float)i * kAngleStep * kDeg;
    const bool inWedge = i < kAttackShapeAngle;
    const float len = inWedge ? 34.0f : 15.0f;
    Element bearing = Element()
                          .key("tick" + std::to_string(i))
                          .at({std::cos(a) * kSphereRadius, 0.0f,
                               -std::sin(a) * kSphereRadius})
                          .rotateY(-(float)i * kAngleStep);
    bearing.child(
        Element()
            .key("bar")
            .rotateX(-90.0f)
            .mesh(gm::quad(4.0f, len))
            .fill(wire(inWedge ? 0.92f : 0.34f, inWedge ? 2.8f : 1.1f))
            .tag("tick"));
    ladder.child(std::move(bearing));
  }
  return ladder;
}

// ---------------------------------------------------------------------------
// THE OVERLAY. A compose tree at exactly the canvas's pixels, painted
// into a texture and hung on one quad that fills the frustum.

constexpr int kHudW = 1280, kHudH = 960;
/** The integer scale the baked face is presented at — trap 4 of the
 *  bitmap bake, and the whole reason the type reads as a 512-grid face. */
constexpr float kHudScale = 3.0f;
/** Vagrant Story's overlay drops a hard black shadow one text-grid pixel
 *  down and right, which at this scale is this many canvas pixels. */
constexpr SkVector kShadow{kHudScale, kHudScale};

/** A run of the baked face, placed at the text grid's own step: 2.5 px
 *  horizontally, 4 px vertically. */
compose::Element run(const ck::Mask& mask, float x, float y, SkColor4f colour) {
  compose::Element e = ck::masked(mask, {.colour = colour,
                                         .scale = kHudScale,
                                         .shadowOffset = kShadow,
                                         .shadowMul = 0.0f});
  e.absolute()
      .left(std::round(x / 2.5f) * 2.5f)
      .top(std::round(y / 4.0f) * 4.0f);
  return e;
}

/** A plate the overlay's type sits on: translucent slate with one pale
 *  hairline round it, which is what every Vagrant Story panel is. */
compose::Element plate(float x, float y, float w, float h, float alpha) {
  compose::Element e =
      compose::box()
          .width(w)
          .height(h)
          .fill(SkColor4f{0.043f, 0.055f, 0.098f, alpha})
          .stroke(compose::decorations::border(
              2.0f,
              compose::Fill::color({kBone.fR, kBone.fG, kBone.fB, 0.55f})));
  e.absolute().left(x).top(y);
  return e;
}

/** A gauge: a frame, a filled bar and nothing else. The reference's are
 *  heavy and sit hard in the corner. */
compose::Element gauge(float x, float y, float w, float h, float fraction,
                       SkColor4f colour) {
  compose::Element frame =
      compose::box()
          .width(w)
          .height(h)
          .fill(SkColor4f{0.031f, 0.039f, 0.071f, 0.86f})
          .stroke(compose::decorations::border(
              2.0f,
              compose::Fill::color({kBone.fR, kBone.fG, kBone.fB, 0.72f})));
  compose::Element fill =
      compose::box()
          .width(std::max(0.0f, std::min(1.0f, fraction)) * (w - 8.0f))
          .height(h - 8.0f)
          .fill(colour);
  fill.absolute().left(4.0f).top(4.0f);
  frame.child(std::move(fill));
  frame.absolute().left(x).top(y);
  return frame;
}

}  // namespace vs

namespace {

struct VagrantStoryTarget final : sketch::Set {
  weave::FontContext* fonts = nullptr;
  std::shared_ptr<compose::TextureScene> overlay;
  compose::Element retained;
  float lastSeconds = -1.0f;
  /** The camera. It is a member because the overlay's quad has to be put
   *  where the frustum is, and a set whose camera is declared once is a
   *  set whose overlay is a fixed rectangle rather than a guess. */
  world::Camera lens;

  void setup(sketch::SetContext& ctx) override {
    ctx.canvas(vs::kHudW, vs::kHudH);
    ctx.background({0.016f, 0.019f, 0.031f, 1.0f});
    ctx.captureAt(2.2);
    fonts = &ctx.fonts;

    lens.eye = {-108.0f, 334.0f, 988.0f};
    lens.target = {-24.0f, 104.0f, 0.0f};
    lens.up = {0.0f, 1.0f, 0.0f};
    lens.fovYDeg = 38.0f;
    lens.zNear = 4.0f;
    lens.zFar = 4096.0f;
    ctx.camera(lens);
    // THE OVERLAY IS DESCRIBED ONCE. The screen this reconstructs is the
    // one in which world time has stopped, so nothing on the overlay is a
    // function of the clock and re-describing it per frame would be
    // paying for a picture that cannot change.
    retained = hud();
  }

  /** The overlay's own tree, described fresh each frame — it is a few
   *  dozen absolute nodes and the scene behind it reconciles them, so a
   *  frame in which nothing moved costs a comparison. */
  compose::Element hud() {
    using namespace vs;
    const Limb& L = kLimbs[kSelected];
    const weave::TextStyle title = compose::type({.size = 13.0f,
                                                  .color = {1, 1, 1, 1},
                                                  .track = 0.0f,
                                                  .condense = 0.92f,
                                                  .aliased = true,
                                                  .antiAlias = false});
    const weave::TextStyle body = compose::type({.size = 9.0f,
                                                 .color = {1, 1, 1, 1},
                                                 .track = 0.0f,
                                                 .condense = 0.95f,
                                                 .aliased = true,
                                                 .antiAlias = false});

    compose::Element root =
        compose::box().width((float)kHudW).height((float)kHudH);

    auto label = [&](std::string_view s, float x, float y, SkColor4f c,
                     bool large) {
      const std::u8string u8(reinterpret_cast<const char8_t*>(s.data()),
                             s.size());
      root.child(run(ck::bakeRun(u8, *fonts, large ? title : body), x, y, c));
    };

    // THE GAUGES, hard in the bottom-left corner, which is where Vagrant
    // Story puts them and is the composition's own anchor. Three rows of
    // one pitch, so the block reads as one instrument.
    constexpr float kRow = 68.0f, kRow0 = 716.0f;
    root.child(plate(34.0f, 698.0f, 452.0f, 226.0f, 0.72f));
    label("HP", 56.0f, kRow0, kBone, false);
    root.child(gauge(156.0f, kRow0 - 4.0f, 276.0f, 28.0f,
                     (float)kAshleyHp / (float)kAshleyMaxHp,
                     {0.42f, 0.78f, 0.45f, 1.0f}));
    label(std::to_string(kAshleyHp) + "/" + std::to_string(kAshleyMaxHp),
          156.0f, kRow0 + 32.0f, kBone, false);
    label("MP", 56.0f, kRow0 + kRow, kBone, false);
    root.child(gauge(156.0f, kRow0 + kRow - 4.0f, 276.0f, 28.0f,
                     (float)kAshleyMp / (float)kAshleyMaxMp,
                     {0.36f, 0.58f, 0.92f, 1.0f}));
    label(std::to_string(kAshleyMp) + "/" + std::to_string(kAshleyMaxMp),
          156.0f, kRow0 + kRow + 32.0f, kBone, false);
    label("RISK", 56.0f, kRow0 + 2.0f * kRow, kAmber, false);
    root.child(gauge(156.0f, kRow0 + 2.0f * kRow - 4.0f, 276.0f, 28.0f,
                     kRisk / 100.0f, kRisk >= 50.0f ? kBlood : kAmber));
    label("RATE " + std::to_string(riskRate(kRisk)), 156.0f,
          kRow0 + 2.0f * kRow + 32.0f, kAmber, false);

    // THE TARGET CARD, beside the enemy: the selected limb, its armour,
    // its condition, its chain evasion, and the two hit numbers — the
    // printed one and the one that actually rolls.
    root.child(plate(800.0f, 160.0f, 452.0f, 384.0f, 0.78f));
    label(std::string("TARGET  ") + L.name, 822.0f, 180.0f, kCyan, true);
    label(std::string("DULLAHAN  ") + kClassNames[kEnemyClass], 822.0f, 216.0f,
          kBone, false);
    label(L.armour, 822.0f, 242.0f, kBone, false);
    label("HP " + std::to_string(L.hp) + "/" + std::to_string(L.maxHp) + "  " +
              conditionOf(L.hp, L.maxHp),
          822.0f, 268.0f, kBone, false);
    label("PHYS " + std::to_string(L.physical) + "  EVA " +
              std::to_string(chainEvasionRate(kSelected)) + "%  BYTE " +
              std::to_string((int)L.chainEvasion),
          822.0f, 294.0f, kBone, false);
    label("PRINTED", 822.0f, 330.0f, kBone, false);
    label(std::to_string(printedHit(kSelected, kRisk)) + "%", 822.0f, 352.0f,
          kBone, true);
    label("TRUE", 1032.0f, 330.0f, kAmber, false);
    label(std::to_string(trueHit(kSelected, kRisk)) + "%", 1032.0f, 352.0f,
          kAmber, true);
    label("ACTOR 0 TAKES +10", 822.0f, 396.0f, kAmber, false);
    label("AFTER THE CLAMP", 822.0f, 420.0f, kAmber, false);
    label(std::string("WEAPON ") + kWeaponName, 822.0f, 456.0f, kBone, false);
    label("REACH " + std::to_string(kReach) + "   ANGLE " +
              std::to_string(kAttackShapeAngle),
          822.0f, 480.0f, kBone, false);
    label("32 DIVISIONS, 11.25 DEG", 822.0f, 504.0f, kBone, false);

    // THE SIX LIMBS as one strip along the bottom — the struct's own
    // order, with the selected one picked out.
    root.child(plate(514.0f, 806.0f, 730.0f, 94.0f, 0.66f));
    for (int i = 0; i < 6; ++i) {
      const float x = 534.0f + (float)i * 119.0f;
      const bool sel = i == kSelected;
      label(kLimbs[i].name, x, 824.0f, sel ? kCyan : kBone, false);
      label(
          std::to_string(kLimbs[i].hp) + "/" + std::to_string(kLimbs[i].maxHp),
          x, 856.0f, sel ? kCyan : kBone, false);
    }
    return root;
  }

  /** The overlay's quad: it stands one metre in front of the eye and is
   *  exactly as wide and as tall as the frustum is there, so a texture
   *  pixel and a plate pixel are the same pixel. */
  Element overlayQuad(material::Texture texture) {
    const glm::vec3 forward = glm::normalize(lens.target - lens.eye);
    constexpr float kAt = 100.0f;
    const float h = 2.0f * kAt * std::tan(lens.fovYDeg * 0.5f * vs::kDeg);
    const float w = h * (float)vs::kHudW / (float)vs::kHudH;
    const glm::vec3 at = lens.eye + forward * kAt;
    material::Material surface =
        material::kit::unlit({.baseColor = {1, 1, 1, 1}});
    surface.child(material::kit::kBaseColorSlot, std::move(texture));
    return Element()
        .key("overlay")
        .transform(
            ::sigil::geometry::mesh::camera::faceCamera(lens.eye, at, lens.up))
        .mesh(vs::gm::quad(w, h))
        .fill(std::move(surface))
        .tag("overlay");
  }

  world::Frame describe(float seconds) override {
    using namespace vs;
    Element scene = Element().key("battle");

    // The floor of the Iron Maiden: one dark flag, lit, so the figures
    // and the sphere have something to stand on and cast their values
    // against.
    scene.child(Element()
                    .key("floor")
                    .at({0.0f, 0.0f, 0.0f})
                    .rotateX(-90.0f)
                    .mesh(gm::quad(1800.0f, 1800.0f))
                    .fill(material::kit::surface(
                        {.baseColor = {0.112f, 0.104f, 0.116f, 1.0f},
                         .roughness = 0.9f}))
                    .tag("ground"));

    // The chamber's far wall. Without it the upper half of the frame is
    // the clear colour, and a wireframe read against nothing is a
    // wireframe with no depth in it.
    scene.child(Element()
                    .key("wall")
                    .at({0.0f, 300.0f, -900.0f})
                    .mesh(gm::quad(2400.0f, 1200.0f))
                    .fill(material::kit::surface(
                        {.baseColor = {0.088f, 0.086f, 0.104f, 1.0f},
                         .roughness = 0.95f}))
                    .tag("ground"));

    // A cold key from behind the enemy and a warm fill at Ashley's back:
    // the dungeon's own two-source reading, which is what separates the
    // two figures without a rim pass.
    scene.child(Element().key("key").light(light::sun(
        {-0.42f, -0.74f, -0.52f}, {0.62f, 0.72f, 1.00f, 1.0f}, 0.92f)));
    scene.child(Element().key("torch").light(
        light::point({-330.0f, 240.0f, 300.0f}, {1.00f, 0.58f, 0.26f, 1.0f},
                     1.60f, 1100.0f)));
    scene.child(Element().key("bounce").light(light::sun(
        {0.34f, -0.42f, 0.84f}, {0.44f, 0.50f, 0.66f, 1.0f}, 0.34f)));

    scene.child(
        figure("ashley", kAshleyAt, 72.0f, 1.0f,
               material::kit::surface({.baseColor = {0.62f, 0.60f, 0.55f, 1.0f},
                                       .roughness = 0.44f}),
               0.0f, -1));
    const float pulse = 0.5f + 0.5f * std::sin(seconds * 4.2f);
    scene.child(
        figure("dullahan", kEnemyAt, -104.0f, 1.34f,
               material::kit::surface({.baseColor = {0.22f, 0.24f, 0.30f, 1.0f},
                                       .roughness = 0.28f}),
               pulse, kSelected));

    scene.child(reachSphere(seconds));
    scene.child(attackLadder());

    if (!overlay || seconds <= lastSeconds)
      overlay = compose::TextureScene::make({kHudW, kHudH}, *fonts);
    lastSeconds = seconds;
    overlay->render(retained, (double)seconds);
    scene.child(overlayQuad(overlay->texture()));

    return Frame(std::move(scene));
  }
};

}  // namespace

SIGIL_SKETCH(VagrantStoryTarget, "Study \xc2\xb7 Game UI",
             "Vagrant Story's battle-mode targeting screen as what it is "
             "\xe2\x80\x94 a lit 3D scene with a wireframe reach sphere in it "
             "and a baked bitmap overlay on one quad over the frustum")
