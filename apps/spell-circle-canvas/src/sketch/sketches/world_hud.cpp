/** @file
 * world_hud — an action-RPG HUD at Voxygen's own dimensions, over the
 * kind of world it was drawn for.
 *
 * The action-RPG HUD study, grounded in a real one: Veloren's
 * (github.com/veloren/veloren, voxygen/src/hud/). Every dimension and
 * every colour below is read out of that source rather than invented —
 * which is the point, because a HUD's proportions ARE its design.
 *
 * IT IS A SET, and the world behind it is the reason. A HUD is not a
 * page; it is a layer whose whole job is to stay legible over something
 * else moving underneath, and a HUD photographed over an empty gradient
 * is a picture of half the problem. So the frame carries a voxel valley
 * — Veloren's own terrain form — lit by a sun, and the HUD is a compose
 * tree painted into a texture and hung on one unlit quad that exactly
 * fills the frustum. The seam is an ordinary `material::Texture` in a
 * base-colour slot: nothing in the HUD knows it is on a quad.
 *
 * From voxygen/src/hud/skillbar.rs:
 *   health frame 484x24, content 480x18       energy frame 323x16,
 *   content 319x10                            poise frame 323x16,
 *   tick 3x10                                 hotbar slot 40x40 in a
 *   42x42 frame, ten of them between the two mouse slots
 *   selected-exp chip 34x38, hung off slot10
 * From voxygen/src/hud/mod.rs (the palette, verbatim Rgba -> hex):
 *   HP #54A100   LOW_HP #ED9608   CRITICAL_HP #C9302B   STAMINA #4A9EBF
 *   XP #9669AB   POISE #B30099    POISE_TICK #B3E600    ENEMY_HP #ED1A4A
 *   BUFF #10B01F DEBUFF #C9302B   quality ladder LOW #999999 /
 *   COMMON #C9FFFF / MODERATE #10B01F / HIGH #2E52E6 / EPIC #944AED /
 *   LEGENDARY #EBC200 / ARTIFACT #BD3D1C
 *
 * The HUD's own pixels are Veloren's unscaled, and the plate is twice
 * that, so the tree is laid out at 900x640 and presented at 2x into an
 * 1800x1280 texture. Scaling the dimensions instead would lose the one
 * thing the study is for.
 *
 * What it exercises: a bar STACK whose widths carry meaning (the health
 * bar is 1.5x the energy bar because Veloren says so), the decay ghost
 * Veloren paints in QUALITY_EPIC behind lost maximum health, the low-HP
 * animation, a twelve-slot hotbar through instances() with per-slot
 * cooldown sweeps, a framed minimap with a compass rose over generated
 * terrain, buff/debuff pips with drain rings, the loot scroller's
 * quality-coloured feed, and an enemy nameplate.
 *
 * Veloren's own art is hand-painted wood and bone. Nothing here is an
 * image: the frames are ramps under noise inside a bevel, the minimap's
 * terrain is mpattern::noise thresholded into bands, the item glyphs are
 * paths, and the valley behind is one merged mesh of voxel columns whose
 * colour is a vertex lane.
 *
 * THE COMBAT LOOP IS A FUNCTION OF THE SCENE TIME, in `driveTo` — which
 * a set requires and which is also the honest shape for it: every bar on
 * the HUD reads one clock, so there is one place that says what second 6
 * of the fight looks like.
 *
 * EDIT THESE FIRST
 *   driveTo — the 9 s combat loop; every bar and sweep on the HUD.
 *   kEye / kLook — where the camera stands over the valley.
 *   kColumns / kColumnSize — how coarse the voxel terrain is.
 */

#include <include/core/SkPathBuilder.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/brush/PixelStyles.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilcompose/core/Instances.h>
#include <sigilcompose/brush/Adaptors.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilweave/style/Type.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilmaterial/field/Field.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilsketch/set/Set.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilworld/kit/Kit.h>

#include <cmath>
#include <cstdint>
#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace shapes = sigil::geometry::shapes;
namespace mpattern = sigil::material::pattern;
namespace field = sigil::material::field;
namespace world = sigil::world;
namespace material = sigil::material;
namespace compose = sigil::compose;
namespace weave = sigil::weave;
namespace mskia = sigil::material::skia;
namespace path = sigil::geometry::path;
namespace motion = sigil::motion;

using namespace sigil::compose;
using sigil::material::skia::Paint;
using sigil::compose::toU8;
using namespace std::chrono_literals;

namespace {
/** The canvas this piece was drawn against, which is also the default a
 *  sketch gets when it declares none. */
constexpr SkSize kSceneSize = {900, 640};

namespace worldhud {

constexpr float kW = kSceneSize.fWidth, kH = kSceneSize.fHeight;

// voxygen/src/hud/mod.rs, verbatim.
constexpr SkColor4f kHp = hex(0x54A100);
constexpr SkColor4f kLowHp = hex(0xED9608);
constexpr SkColor4f kCritHp = hex(0xC9302B);
constexpr SkColor4f kStamina = hex(0x4A9EBF);
constexpr SkColor4f kXp = hex(0x9669AB);
constexpr SkColor4f kPoise = hex(0xB30099);
constexpr SkColor4f kPoiseTick = hex(0xB3E600);
constexpr SkColor4f kEnemyHp = hex(0xED1A4A);
constexpr SkColor4f kBuff = hex(0x10B01F);
constexpr SkColor4f kDebuff = hex(0xC9302B);
constexpr SkColor4f kQualityLow = hex(0x999999);
constexpr SkColor4f kQualityCommon = hex(0xC9FFFF);
constexpr SkColor4f kQualityModerate = hex(0x10B01F);
constexpr SkColor4f kQualityHigh = hex(0x2E52E6);
constexpr SkColor4f kQualityEpic = hex(0x944AED);
constexpr SkColor4f kQualityLegendary = hex(0xEBC200);
constexpr SkColor4f kQualityArtifact = hex(0xBD3D1C);

// The frame material: Veloren's UI is carved bone over dark wood.
constexpr SkColor4f kBoneHi = hex(0xD8CBA8);
constexpr SkColor4f kBone = hex(0xA2947A);
constexpr SkColor4f kBoneLo = hex(0x584E3D);
constexpr SkColor4f kWood = hex(0x2A2118);
constexpr SkColor4f kWoodLo = hex(0x160F0A);
constexpr SkColor4f kTrack = hex(0x0B0906);
constexpr SkColor4f kInk = hex(0xEDE6D4);
constexpr SkColor4f kInkDim = hex(0x8C8271);

// skillbar.rs dimensions, unscaled — the stage is wide enough to take
// them, and scaling them would be the one thing that loses the study.
constexpr float kHealthW = 484, kHealthH = 24;
constexpr float kHealthInnerW = 480, kHealthInnerH = 18;
constexpr float kEnergyW = 323, kEnergyH = 16;
constexpr float kEnergyInnerW = 319, kEnergyInnerH = 10;
constexpr float kSlot = 40, kSlotFrame = 42, kSlotGap = 2;
constexpr int kSlotCount = 12;  // M1 + ten numbered + M2
constexpr float kBarX = (kW - kHealthW) * 0.5f;
constexpr float kBarY = 486;
constexpr float kEnergyY = 514;
constexpr float kPoiseY = 534;
constexpr float kSlotsY = 556;
constexpr float kSlotsW = kSlotCount * kSlotFrame + (kSlotCount - 1) * kSlotGap;
constexpr float kSlotsX = (kW - kSlotsW) * 0.5f;

inline sigil::weave::TextStyle type(float size, SkColor4f color,
                                    float tracking = 0, float weight = 0) {
  sigil::weave::TextStyle s = sigil::weave::textStyle(
      {.size = size, .color = color, .track = tracking, .weight = weight});
  // Veloren draws every HUD string twice: black underneath, then the
  // colour on top. At 10px over terrain that is the whole legibility
  // budget, so it is not optional.
  sigil::weave::PaintLayer shade;
  shade.paint.setColor4f({0, 0, 0, 0.9f}, nullptr);
  shade.paint.setAntiAlias(true);
  shade.offset = {1, 1};
  s.paint.addUnderlay(shade);
  return s;
}

/** A sunk track: the black slot a bar's content sits in. */
inline Element track(float w, float h) {
  return box()
      .width(Dim(w))
      .height(Dim(h))
      .fill(Paint::solid(kTrack))
      .foreground(styles::InnerShadow{{0, 0, 0, 0.85f}, {0, 2}, 3});
}

/** The carved bone frame Veloren hangs on everything. */
inline Element boneFrame(float w, float h, float radius = 3) {
  return box()
      .width(Dim(w))
      .height(Dim(h))
      .corners({radius})
      .fill(Paint::linear(
          {0, 0}, {0, h}, {{0.0f, kBoneHi}, {0.45f, kBone}, {1.0f, kBoneLo}}))
      // The grain: Veloren's frames are carved, and a ramp with no noise
      // in it is a plastic one. It rides UNDER the bevel, so the carve
      // reads through the highlight rather than over it.
      .child(box()
                 .inset(0)
                 .corners({radius})
                 .fill(Paint::recipe(field::noise(0.36f, 3, 1.0f)))
                 .opacity(0.38f)
                 .blend(SkBlendMode::kMultiply))
      .foreground(styles::BevelEmboss{
          1.6f, 2.4f, 120, {1, 1, 1, 0.35f}, {0, 0, 0, 0.65f}})
      .foreground(stroke(1.0f, Fill::color({0.05f, 0.04f, 0.03f, 0.9f}),
                         PathFormat::Align::Outer));
}

/** A bar: bone frame, sunk track, content, and Veloren's decay ghost —
 *  the QUALITY_EPIC band it paints over maximum health you have lost. */
inline Element bar(float frameW, float frameH, float innerW, float innerH,
                   float fraction, SkColor4f color, float decay = 0.0f) {
  const float padX = (frameW - innerW) * 0.5f;
  const float padY = (frameH - innerH) * 0.5f;
  Element e = boneFrame(frameW, frameH, 2)
                  .child(track(innerW, innerH).left(padX).top(padY));
  if (decay > 0.0f)
    e.child(box()
                .left(padX + innerW * (1.0f - decay))
                .top(padY)
                .width(Dim(innerW * decay))
                .height(Dim(innerH))
                .fill(Paint::solid({kQualityEpic.fR, kQualityEpic.fG,
                                       kQualityEpic.fB, 0.55f})));
  e.child(
      box()
          .left(padX)
          .top(padY)
          .width(Dim(innerW * fraction))
          .height(Dim(innerH))
          .fill(Paint::linear(
              {0, 0}, {0, innerH},
              {{0.0f,
                {std::min(1.0f, color.fR * 1.45f + 0.06f),
                 std::min(1.0f, color.fG * 1.45f + 0.06f),
                 std::min(1.0f, color.fB * 1.45f + 0.06f), 1}},
               {0.5f, color},
               {1.0f,
                {color.fR * 0.62f, color.fG * 0.62f, color.fB * 0.62f, 1}}})));
  return e;
}

/** Hotbar item glyphs — paths, so a slot never needs a sprite. */
enum class Glyph { Sword, Bow, Fire, Frost, Heal, Shield, Dash, Bomb };

inline std::function<SkPath(SkSize)> glyphPath(Glyph g) {
  return [g](SkSize s) {
    const float w = s.width(), h = s.height(), cx = w * 0.5f;
    SkPathBuilder b;
    switch (g) {
      case Glyph::Sword:
        b.moveTo(cx, h * 0.08f);
        b.lineTo(cx + w * 0.11f, h * 0.22f);
        b.lineTo(cx + w * 0.08f, h * 0.66f);
        b.lineTo(cx - w * 0.08f, h * 0.66f);
        b.lineTo(cx - w * 0.11f, h * 0.22f);
        b.close();
        b.addRect(SkRect::MakeXYWH(w * 0.20f, h * 0.66f, w * 0.60f, h * 0.06f));
        b.addRect(
            SkRect::MakeXYWH(cx - w * 0.05f, h * 0.72f, w * 0.10f, h * 0.20f));
        break;
      case Glyph::Bow:
        b.addArc(SkRect::MakeXYWH(w * 0.20f, h * 0.10f, w * 0.62f, h * 0.80f),
                 120, 130);
        b.moveTo(w * 0.32f, h * 0.14f);
        b.lineTo(w * 0.32f, h * 0.86f);
        b.moveTo(w * 0.32f, h * 0.50f);
        b.lineTo(w * 0.82f, h * 0.50f);
        break;
      case Glyph::Fire:
        b.moveTo(cx, h * 0.08f);
        b.quadTo(w * 0.86f, h * 0.44f, w * 0.72f, h * 0.72f);
        b.quadTo(w * 0.60f, h * 0.94f, cx, h * 0.92f);
        b.quadTo(w * 0.40f, h * 0.94f, w * 0.28f, h * 0.72f);
        b.quadTo(w * 0.14f, h * 0.44f, cx, h * 0.08f);
        b.close();
        break;
      case Glyph::Frost:
        for (int i = 0; i < 3; ++i) {
          const float a = (float)i * 1.0471976f;
          const float dx = std::cos(a) * w * 0.36f,
                      dy = std::sin(a) * h * 0.36f;
          b.moveTo(cx - dx, h * 0.5f - dy);
          b.lineTo(cx + dx, h * 0.5f + dy);
        }
        break;
      case Glyph::Heal:
        b.addRect(
            SkRect::MakeXYWH(cx - w * 0.09f, h * 0.16f, w * 0.18f, h * 0.68f));
        b.addRect(SkRect::MakeXYWH(w * 0.16f, h * 0.41f, w * 0.68f, h * 0.18f));
        break;
      case Glyph::Shield:
        b.moveTo(w * 0.18f, h * 0.16f);
        b.lineTo(w * 0.82f, h * 0.16f);
        b.lineTo(w * 0.82f, h * 0.54f);
        b.quadTo(w * 0.82f, h * 0.82f, cx, h * 0.90f);
        b.quadTo(w * 0.18f, h * 0.82f, w * 0.18f, h * 0.54f);
        b.close();
        break;
      case Glyph::Dash:
        for (int i = 0; i < 3; ++i) {
          const float x = w * (0.22f + 0.22f * (float)i);
          b.moveTo(x, h * 0.24f);
          b.lineTo(x + w * 0.16f, h * 0.50f);
          b.lineTo(x, h * 0.76f);
        }
        break;
      case Glyph::Bomb:
        b.addCircle(cx, h * 0.60f, w * 0.28f);
        b.moveTo(cx + w * 0.10f, h * 0.34f);
        b.quadTo(cx + w * 0.34f, h * 0.16f, cx + w * 0.22f, h * 0.06f);
        break;
    }
    return b.detach();
  };
}

// ---------------------------------------------------------------------------
// THE VALLEY. One merged mesh of voxel columns, coloured by a vertex
// lane, because a mesh is one body and one body is one depth-sorted draw:
// a thousand separate columns would be a thousand bodies sorted against
// each other by their centres, which is where a painter's order shows.

namespace gm = ::sigil::geometry::mesh;

constexpr int kColumns = 44;          ///< columns per side
constexpr float kColumnSize = 62.0f;  ///< how wide one column is
constexpr float kStep = 46.0f;        ///< the height quantum
constexpr float kWaterLevel = 1.0f * kStep;

/** Where the camera stands over the valley, and what it looks at. */
constexpr glm::vec3 kEye{40.0f, 690.0f, 1420.0f};
constexpr glm::vec3 kLook{0.0f, 150.0f, -320.0f};

/** A deterministic value in [0,1) from a pair of integers — the terrain
 *  has to be the same valley on every machine and in every run. */
inline float hash2(int x, int z) {
  uint32_t h = (uint32_t)x * 374761393u ^ (uint32_t)z * 668265263u;
  h = (h ^ (h >> 13U)) * 1274126177u;
  return (float)((h ^ (h >> 16U)) & 0xFFFFFFu) / 16777216.0f;
}

/** One axis-aligned box, 24 vertices and 12 triangles with flat normals
 *  and one colour in the vertex lane. */
inline void addBox(gm::Mesh& out, glm::vec3 lo, glm::vec3 hi, glm::vec4 tint) {
  static const glm::vec3 kNormals[6] = {{0, 0, 1},  {0, 0, -1}, {1, 0, 0},
                                        {-1, 0, 0}, {0, 1, 0},  {0, -1, 0}};
  const glm::vec3 c[8] = {{lo.x, lo.y, hi.z}, {hi.x, lo.y, hi.z},
                          {hi.x, hi.y, hi.z}, {lo.x, hi.y, hi.z},
                          {lo.x, lo.y, lo.z}, {hi.x, lo.y, lo.z},
                          {hi.x, hi.y, lo.z}, {lo.x, hi.y, lo.z}};
  static const int kFace[6][4] = {{0, 1, 2, 3}, {5, 4, 7, 6}, {1, 5, 6, 2},
                                  {4, 0, 3, 7}, {3, 2, 6, 7}, {4, 5, 1, 0}};
  // Five faces, not six: the camera stands above the valley and the
  // underside of a column is a tenth of the triangles for nothing.
  for (int f = 0; f < 5; ++f) {
    const uint32_t base = (uint32_t)out.positions.size();
    // The top face carries the column's own colour; the sides take a
    // fraction of it, which is what makes a voxel field read as blocks
    // rather than as a surface.
    const float side = f == 4 ? 1.0f : 0.72f;
    for (int k = 0; k < 4; ++k) {
      out.positions.push_back(c[kFace[f][k]]);
      out.normals.push_back(kNormals[f]);
      out.uvs.emplace_back((float)(k == 1 || k == 2), (float)(k >= 2));
      out.colors.emplace_back(tint.r * side, tint.g * side, tint.b * side,
                              1.0f);
    }
    out.indices.insert(out.indices.end(),
                       {base, base + 1, base + 2, base, base + 2, base + 3});
  }
}

/** The terrain height at a column, in quanta. Two ridges crossed by a
 *  valley floor, roughened by the hash so no two columns of one band
 *  stand at the same height. */
inline int heightAt(int ix, int iz) {
  const float x = (float)ix / (float)kColumns - 0.5f;
  const float z = (float)iz / (float)kColumns - 0.5f;
  const float valley = 5.6f * (x * x * 8.0f);
  // The ridges are damped toward the middle, which is what cuts the
  // river channel: a valley floor is flat because the ridges do not
  // reach it, not because a second rule flattened it.
  const float bank = std::min(1.0f, std::abs(x) * 3.4f);
  const float ridge =
      bank * (1.9f * std::sin(z * 7.4f + 1.2f) + 1.3f * std::cos(x * 5.1f));
  const float grain = bank * hash2(ix, iz) * 1.15f;
  return (int)std::lround(0.4f + valley + ridge + grain);
}

/** The colour band a column's top stands in: water sand, valley grass,
 *  hillside, and bare rock above the tree line. */
inline glm::vec4 bandOf(int height, float grain) {
  const glm::vec4 water{0.09f, 0.26f, 0.33f, 1.0f};
  if (height <= 0) return water;
  const glm::vec4 sand{0.60f, 0.54f, 0.36f, 1.0f};
  const glm::vec4 grass{0.24f, 0.44f, 0.19f, 1.0f};
  const glm::vec4 slope{0.20f, 0.34f, 0.17f, 1.0f};
  const glm::vec4 rock{0.36f, 0.35f, 0.33f, 1.0f};
  glm::vec4 c = height <= 1   ? sand
                : height <= 3 ? grass
                : height <= 6 ? slope
                              : rock;
  const float j = 0.90f + 0.20f * grain;
  return {c.r * j, c.g * j, c.b * j, 1.0f};
}

/** The valley, plus the trees standing on it: one mesh. */
inline gm::Mesh valley() {
  gm::Mesh out;
  const float half = 0.5f * (float)kColumns * kColumnSize;
  for (int iz = 0; iz < kColumns; ++iz)
    for (int ix = 0; ix < kColumns; ++ix) {
      const int h = heightAt(ix, iz);
      const float x0 = (float)ix * kColumnSize - half;
      const float z0 = (float)iz * kColumnSize - half;
      // A column below the water line stands AT it: the river is voxel
      // water in the same mesh, not a translucent plane over the terrain
      // — one body is one depth-sorted draw, and two huge overlapping
      // bodies have no order a painter can be right about.
      const float top =
          (float)std::max(h, 0) * kStep + (h <= 0 ? kWaterLevel * 0.62f : 0.0f);
      addBox(out, {x0, top - kStep * 3.0f, z0},
             {x0 + kColumnSize, top, z0 + kColumnSize},
             bandOf(h, hash2(ix + 91, iz + 17)));
      // A tree on one column in fourteen, above the water and below the
      // rock: a trunk and two canopy blocks, which is the whole of what a
      // voxel tree is.
      if (h < 2 || h > 6 || hash2(ix + 7, iz + 41) > 0.07f) continue;
      const float cx = x0 + kColumnSize * 0.5f, cz = z0 + kColumnSize * 0.5f;
      addBox(out, {cx - 9.0f, top, cz - 9.0f},
             {cx + 9.0f, top + 78.0f, cz + 9.0f}, {0.26f, 0.18f, 0.11f, 1.0f});
      addBox(out, {cx - 44.0f, top + 62.0f, cz - 44.0f},
             {cx + 44.0f, top + 122.0f, cz + 44.0f},
             {0.13f, 0.30f, 0.13f, 1.0f});
    }
  return out;
}

}  // namespace worldhud

struct WorldHud final : sketch::Set {
  // Plain fractions in [0,1], not pixel widths. Every bar here is a
  // full-size fill whose growing edge is pinned with transformOrigin() and
  // whose extent is carried by scaleX, so these Outputs feed the transform
  // directly and none of them needs to know the bar's size.
  choreograph::Output<float> hp{0.62f}, energy{0.78f}, poise{0.55f};
  choreograph::Output<float> xp{0}, enemyHp{0.4f};
  choreograph::Output<float> lowPulse{0}, compass{0};
  std::array<choreograph::Output<float>, 4> cooldown{};

  std::shared_ptr<instancing::Atlas> slotAtlas;
  std::shared_ptr<instancing::Pool> slotPool;

  /** The HUD's own scene, asked of the session once and held by it: a
   *  scene standing on a device destroys the texture it painted into
   *  when it goes, and one asked for per frame would be one held per
   *  frame. */
  std::shared_ptr<compose::TextureScene> overlay;
  Element retained;
  worldhud::gm::camera::Camera lens;

  void setup(sketch::SetContext& ctx) override {
    namespace wh = worldhud;
    ctx.canvas((int)kSceneSize.fWidth, (int)kSceneSize.fHeight);
    ctx.captureAt(6.0);
    ctx.background({0.086f, 0.118f, 0.165f, 1.0f});
    overlay = ctx.textureScene({(int)kSceneSize.fWidth,
                                (int)kSceneSize.fHeight});

    lens.eye = wh::kEye;
    lens.target = wh::kLook;
    lens.up = {0.0f, 1.0f, 0.0f};
    lens.fovYDeg = 46.0f;
    lens.zNear = 8.0f;
    lens.zFar = 8192.0f;
    ctx.camera(lens);

    // The empty slot frame is one atlas cell stamped twelve times.
    slotAtlas = std::make_shared<instancing::Atlas>(2.0f);
    slotAtlas->cell(
        wh::boneFrame(wh::kSlotFrame, wh::kSlotFrame, 3)
            .child(wh::track(wh::kSlot - 4, wh::kSlot - 4).left(5).top(5)),
        {wh::kSlotFrame, wh::kSlotFrame});
    slotPool = std::make_shared<instancing::Pool>();
    for (int i = 0; i < wh::kSlotCount; ++i)
      slotPool->add(
          {i * (wh::kSlotFrame + wh::kSlotGap) + wh::kSlotFrame * 0.5f,
           wh::kSlotFrame * 0.5f});
    retained = hud();
  }

  /** THE COMBAT LOOP, as one function of the scene time: health drains,
   *  a heal lands, energy spends and regenerates, poise breaks and
   *  recovers, four abilities cool down on periods of their own. Every
   *  bar on the HUD reads this one clock, so there is one place that
   *  says what any second of the fight looks like. */
  void driveTo(double t) {
    const double cycle = std::fmod(t, 9.0);
    float h = 0.62f;
    if (cycle < 3.0)
      h = 0.62f - 0.42f * (float)(cycle / 3.0);
    else if (cycle < 3.5)
      h = 0.20f + 0.55f * (float)((cycle - 3.0) / 0.5);
    else
      h = 0.75f - 0.13f * (float)((cycle - 3.5) / 5.5);
    hp = h;
    // Veloren's hp_ani: below 20% the bar breathes.
    lowPulse = h < 0.25f ? 0.5f + 0.5f * (float)std::sin(t * 9.0) : 0.0f;
    energy = 0.35f + 0.45f * (float)(0.5 + 0.5 * std::sin(t * 0.9));
    poise = 0.30f + 0.60f * (float)(0.5 + 0.5 * std::sin(t * 0.55 + 1.7));
    compass = (float)std::fmod(t * 8.0, 360.0);
    xp = (float)std::fmod(t * 0.11, 1.0);
    // The boss bar drains on a sawtooth and must never reach zero: a
    // formulation that clamps at empty spends part of every cycle showing
    // a boss nameplate over an unfilled black slab.
    enemyHp = 0.85f - 0.77f * (float)std::fmod(t * 0.16, 1.0);
    for (size_t i = 0; i < cooldown.size(); ++i) {
      const double period = 2.4 + 0.9 * (double)i;
      // the dark cover keeps its top edge and its bottom edge rises
      cooldown[i] = 1.0f - (float)(std::fmod(t, period) / period);
    }
  }

  /** The overlay's quad: it stands a fixed distance in front of the eye
   *  and is exactly as wide and as tall as the frustum is there, so a
   *  texture pixel and a plate pixel are the same pixel. */
  world::Element overlayQuad(material::Texture texture) {
    const glm::vec3 forward = glm::normalize(lens.target - lens.eye);
    constexpr float kAt = 60.0f;
    const float h =
        2.0f * kAt * std::tan(lens.fovYDeg * 0.5f * 3.14159265358979f / 180.0f);
    const float w = h * kSceneSize.fWidth / kSceneSize.fHeight;
    const glm::vec3 at = lens.eye + forward * kAt;
    material::Material surface =
        material::kit::unlit({.baseColor = {1, 1, 1, 1}});
    surface.child(material::kit::kBaseColorSlot, std::move(texture));
    return world::Element()
        .key("overlay")
        .transform(
            ::sigil::geometry::mesh::camera::faceCamera(lens.eye, at, lens.up))
        .mesh(worldhud::gm::quad(w, h))
        .fill(std::move(surface))
        .tag("overlay");
  }

  world::Frame describe(float seconds) override {
    namespace wh = worldhud;
    driveTo((double)seconds);

    world::Element scene = world::Element().key("vale");
    scene.child(world::Element().key("sun").light(world::light::sun(
        {-0.44f, -0.78f, -0.44f}, {1.00f, 0.94f, 0.80f, 1.0f}, 1.05f)));
    scene.child(world::Element().key("sky").light(world::light::sun(
        {
            0.26f,
            0.52f,
            0.36f,
        },
        {0.42f, 0.56f, 0.78f, 1.0f}, 0.42f)));

    // The valley is cooked once and held: it is a function of nothing,
    // and rebuilding twenty thousand triangles per frame would be a
    // statement about the terrain rather than about the HUD.
    static const wh::gm::Mesh kValley = wh::valley();
    scene.child(world::Element()
                    .key("terrain")
                    .mesh(kValley)
                    .fill(material::kit::surface(
                        {.baseColor = {1, 1, 1, 1}, .roughness = 0.92f}))
                    .tag("terrain"));

    // THE HUD IS DESCRIBED ONCE. Every bar on it is a bound Output on a
    // retained node, so the frames after the first cost a reconcile
    // against a tree that did not change — re-describing a hundred nodes
    // per frame would be paying for the bindings twice.
    overlay->render(retained, (double)seconds);
    scene.child(overlayQuad(overlay->texture()));
    return world::Frame(std::move(scene));
  }

  // ------------------------------------------------------------------

  Element barStack() {
    namespace wh = worldhud;
    using namespace std::chrono_literals;
    Element stackEl = stack().inset(0);

    // health, with the decay ghost and the low-HP wash
    stackEl.child(box().left(wh::kBarX).top(wh::kBarY).child(
        wh::bar(wh::kHealthW, wh::kHealthH, wh::kHealthInnerW,
                wh::kHealthInnerH, 0.62f, wh::kHp, 0.14f)));
    // the live fill rides on top of the static frame so only IT repaints
    stackEl.child(box()
                      .left(wh::kBarX + 2)
                      .top(wh::kBarY + 3)
                      .width(Dim(wh::kHealthInnerW))
                      .height(Dim(wh::kHealthInnerH))
                      .transformOrigin(0.0f, 0.5f)
                      .scaleX(&hp)
                      .fill(Paint::linear({0, 0}, {0, wh::kHealthInnerH},
                                             {{0.0f, hex(0x7FE000)},
                                              {0.5f, wh::kHp},
                                              {1.0f, hex(0x2F5C00)}})));
    stackEl.child(box()
                      .left(wh::kBarX)
                      .top(wh::kBarY)
                      .width(Dim(wh::kHealthW))
                      .height(Dim(wh::kHealthH))
                      .corners({2})
                      .fill(Paint::solid({wh::kCritHp.fR, wh::kCritHp.fG,
                                             wh::kCritHp.fB, 0.55f}))
                      .opacity(&lowPulse)
                      .blend(SkBlendMode::kPlus));
    stackEl.child(text(toU8("640 / 1030"), wh::type(11, wh::kInk, 0.8f))
                      .left(wh::kBarX + wh::kHealthW * 0.5f - 30)
                      .top(wh::kBarY + 5));

    // energy
    const float ex = (wh::kW - wh::kEnergyW) * 0.5f;
    stackEl.child(
        box()
            .left(ex)
            .top(wh::kEnergyY)
            .child(wh::bar(wh::kEnergyW, wh::kEnergyH, wh::kEnergyInnerW,
                           wh::kEnergyInnerH, 1.0f, wh::kStamina)));
    stackEl.child(box()
                      .left(ex + 2)
                      .top(wh::kEnergyY + 3)
                      .width(Dim(wh::kEnergyInnerW))
                      .height(Dim(wh::kEnergyInnerH))
                      .transformOrigin(0.0f, 0.5f)
                      .scaleX(&energy)
                      .fill(Paint::solid(wh::kStamina)));

    // poise, with skillbar.rs's 3x10 ticks along it
    stackEl.child(
        box()
            .left(ex)
            .top(wh::kPoiseY)
            .child(wh::bar(wh::kEnergyW, wh::kEnergyH, wh::kEnergyInnerW,
                           wh::kEnergyInnerH, 1.0f, wh::kPoise)));
    stackEl.child(box()
                      .left(ex + 2)
                      .top(wh::kPoiseY + 3)
                      .width(Dim(wh::kEnergyInnerW))
                      .height(Dim(wh::kEnergyInnerH))
                      .transformOrigin(0.0f, 0.5f)
                      .scaleX(&poise)
                      .fill(Paint::solid(wh::kPoise)));
    // The ticks are a RAIL, not five boxes: one mark every sixth of the
    // bar, three wide and ten tall, declared once as the ladder it is.
    stackEl.child(
        box()
            .left(ex + 2)
            .top(wh::kPoiseY + 3)
            .width(Dim(wh::kEnergyInnerW))
            .height(Dim(wh::kEnergyInnerH))
            .foreground(styles::TickRail{.color = wh::kPoiseTick,
                                         .pitch = wh::kEnergyInnerW / 6.0f,
                                         .minor = 10.0f,
                                         .major = 10.0f,
                                         .width = 3.0f,
                                         .majorEvery = 0,
                                         .phase = 1.0f,
                                         .edge = path::Edge::Top}));
    return stackEl;
  }

  Element hotbar() {
    namespace wh = worldhud;
    using namespace std::chrono_literals;
    static const struct {
      const char* key;
      wh::Glyph glyph;
      bool filled;
    } kSlots[] = {
        {"M1", wh::Glyph::Sword, true}, {"1", wh::Glyph::Fire, true},
        {"2", wh::Glyph::Frost, true},  {"3", wh::Glyph::Heal, true},
        {"4", wh::Glyph::Dash, true},   {"5", wh::Glyph::Shield, true},
        {"6", wh::Glyph::Bomb, true},   {"7", wh::Glyph::Bow, false},
        {"8", wh::Glyph::Sword, false}, {"9", wh::Glyph::Fire, false},
        {"0", wh::Glyph::Frost, false}, {"M2", wh::Glyph::Bow, true},
    };
    Element rail = stack()
                       .left(wh::kSlotsX)
                       .top(wh::kSlotsY)
                       .width(Dim(wh::kSlotsW))
                       .height(Dim(wh::kSlotFrame));
    rail.child(instances(slotAtlas, slotPool));
    for (int i = 0; i < wh::kSlotCount; ++i) {
      const float x = i * (wh::kSlotFrame + wh::kSlotGap);
      if (kSlots[i].filled)
        rail.child(
            box()
                .left(x + 9)
                .top(9)
                .width(Dim(24.0f))
                .height(Dim(24.0f))
                .shape(wh::glyphPath(kSlots[i].glyph))
                .fill(Paint::linear(
                    {0, 0}, {0, 24}, {{0.0f, wh::kBoneHi}, {1.0f, wh::kBone}}))
                // several glyphs are line-only (frost, dash, bow):
                // a fill alone leaves them invisible
                .stroke(stroke(2.2f, Fill::color(wh::kBoneHi)))
                .stroke(stroke(3.4f, Fill::color({0.04f, 0.03f, 0.02f, 0.75f}),
                               PathFormat::Align::Outer)));
      // four of them are cooling down: the sweep Veloren draws as a dark
      // wipe over the icon
      if (i >= 1 && i <= 4)
        rail.child(box()
                       .left(x + 3)
                       .top(3)
                       .width(Dim(wh::kSlot - 4))
                       .height(Dim(wh::kSlot - 4))
                       .transformOrigin(0.5f, 0.0f)
                       .scaleY(&cooldown[(size_t)i - 1])
                       .fill(Paint::linear(
                           {0, 0}, {0, wh::kSlot - 4},
                           {{0.0f, {0.06f, 0.10f, 0.16f, 0.86f}},
                            {1.0f, {0.10f, 0.16f, 0.24f, 0.72f}}})));
      rail.child(text(toU8(kSlots[i].key), wh::type(9, wh::kInkDim, 0.6f))
                     .left(x + 4)
                     .top(wh::kSlotFrame - 13));
    }
    // the selected-exp chip skillbar.rs hangs off slot10
    rail.child(box()
                   .left(wh::kSlotsW + 3)
                   .top(2)
                   .width(Dim(34.0f))
                   .height(Dim(38.0f))
                   .child(worldhud::boneFrame(34, 38, 3).inset(0))
                   .child(box()
                              .left(3)
                              .top(20)
                              .width(Dim(28.0f))
                              .height(Dim(6.0f))
                              .fill(Paint::solid(worldhud::kTrack))
                              .child(box()
                                         .left(0)
                                         .top(0)
                                         .width(Dim(28.0f))
                                         .height(Dim(6.0f))
                                         .transformOrigin(0.0f, 0.5f)
                                         .scaleX(&xp)
                                         .fill(Paint::solid(worldhud::kXp))))
                   .child(text(toU8("34"), wh::type(13, wh::kInk, 0.4f, 640))
                              .left(9)
                              .top(3)));
    return rail;
  }

  /** The minimap: generated terrain under a bone ring, with a compass
   *  rose that counter-rotates and player/POI markers. */
  Element minimap() {
    namespace wh = worldhud;
    using namespace std::chrono_literals;
    constexpr float d = 168;
    return stack()
        .key("minimap")
        .right(28)
        .top(28)
        .width(Dim(d))
        .height(Dim(d))
        .opacity(animate(motion::from(0.0f).to(1.0f), {420ms}))
        .child(
            box()
                .inset(0)
                .corners({d * 0.5f})
                .clip()
                .fill(Paint::solid(hex(0x2E4A2A)))
                .child(box()
                           .inset(0)
                           .fill(Paint::recipe(field::noise(0.014f, 5, 3.0f)))
                           .opacity(0.85f)
                           .blend(SkBlendMode::kMultiply))
                // the height BANDS: three thresholds of one noise field,
                // which is how a world map reads as terrain rather than
                // as a texture
                .child(box()
                           .inset(0)
                           .fill(Paint::recipe(field::noise(0.030f, 4, 2.0f)))
                           .opacity(0.55f)
                           .blend(SkBlendMode::kOverlay))
                .child(box()
                           .inset(0)
                           .fill(Paint::recipe(field::noise(0.070f, 2, 5.0f)))
                           .opacity(0.30f)
                           .blend(SkBlendMode::kMultiply))
                .child(box().inset(0).fill(
                    Paint::radial({d * 0.5f, d * 0.5f}, d * 0.55f,
                                     {{0.0f, {0, 0, 0, 0}},
                                      {0.72f, {0, 0, 0, 0.25f}},
                                      {1.0f, {0, 0, 0, 0.75f}}})))
                // the rivers Veloren's world always has
                .child(box()
                           .inset(0)
                           .fill(Pattern(mpattern::stripes(
                                             2, 47,
                                             mskia::toColor(
                                                 hex(0x2F6FA8, 0.30f))))
                                     .material())
                           .rotate(24.0f)
                           .opacity(0.7f)))
        // THE COMPASS ROSE, turning under the frame. It is a rose and not
        // a cross: small, at the middle, eight points, with the four
        // cardinal arms longer than the four between them.
        .child(box()
                   .left(d * 0.5f - 23)
                   .top(d * 0.5f - 23)
                   .width(Dim(46.0f))
                   .height(Dim(46.0f))
                   .rotate(&compass)
                   .child(box()
                              .inset(0)
                              .shape(shapes::star(8, 0.34f))
                              .fill(Paint::solid({wh::kBoneHi.fR,
                                                     wh::kBoneHi.fG,
                                                     wh::kBoneHi.fB, 0.30f})))
                   .child(box()
                              .inset(9)
                              .shape(shapes::star(4, 0.22f))
                              .fill(Paint::solid({wh::kBoneHi.fR,
                                                     wh::kBoneHi.fG,
                                                     wh::kBoneHi.fB, 0.62f}))))
        .child(box()
                   .left(d * 0.5f - 4)
                   .top(d * 0.5f - 4)
                   .width(Dim(8.0f))
                   .height(Dim(8.0f))
                   .shape(shapes::polygon(3))
                   .fill(Paint::solid(hex(0xFFE9A8))))
        .child(box()
                   .left(d * 0.30f)
                   .top(d * 0.36f)
                   .width(Dim(6.0f))
                   .height(Dim(6.0f))
                   .corners({3})
                   .fill(Paint::solid(wh::kQualityLegendary)))
        .child(box()
                   .left(d * 0.68f)
                   .top(d * 0.62f)
                   .width(Dim(6.0f))
                   .height(Dim(6.0f))
                   .corners({3})
                   .fill(Paint::solid(wh::kEnemyHp)))
        // the ring
        .child(box()
                   .inset(0)
                   .corners({d * 0.5f})
                   .foreground(stroke(
                       5.0f,
                       linearGradient({0, 0}, {0, d},
                                      {wh::kBoneHi, wh::kBone, wh::kBoneLo}),
                       PathFormat::Align::Inner))
                   .foreground(
                       stroke(1.0f, Fill::color({0.05f, 0.04f, 0.03f, 0.9f}))))
        .child(text(toU8("N"), wh::type(11, wh::kInk, 1.0f, 640))
                   .left(d * 0.5f - 4)
                   .top(7))
        .child(box()
                   .row()
                   .left(0)
                   .right(0)
                   .bottom(-19)
                   .justify(Justify::Center)
                   .child(text(toU8("1204, -388"),
                               wh::type(10, wh::kInkDim, 1.2f))));
  }

  /** Buff and debuff pips with their drain rings — buffs.rs colours. */
  Element buffRow() {
    namespace wh = worldhud;
    using namespace std::chrono_literals;
    struct Pip {
      const char* label;
      SkColor4f color;
      float left;
    };
    // The ring is the outline itself, trimmed: a fraction of the way
    // round is a fraction of the buff left.
    auto drainRing = [](SkColor4f colour, float left) {
      PathFormat ring = stroke(2.6f, Fill::color(colour));
      ring.cap = SkPaint::kRound_Cap;
      ring.trimStart = 0.0f;
      ring.trimEnd = std::max(0.0f, std::min(1.0f, left));
      return ring;
    };
    static const Pip kPips[] = {
        {"REG", wh::kBuff, 0.72f},   {"HST", wh::kBuff, 0.35f},
        {"PRT", wh::kBuff, 0.88f},   {"BRN", wh::kDebuff, 0.51f},
        {"BLD", wh::kDebuff, 0.19f},
    };
    Element row = box()
                      .key("buffs")
                      .row()
                      .gap(6)
                      .left(28)
                      .top(28)
                      .zIndex(6)
                      .staggerChildren(70ms);
    for (const Pip& p : kPips)
      row.child(
          box()
              .width(Dim(30.0f))
              .height(Dim(30.0f))
              .corners({4})
              .opacity(animate(motion::from(0.0f).to(1.0f), {320ms}))
              .translateY(animate(motion::from(-10.0f).to(0.0f), {380ms}))
              .fill(Paint::linear(
                  {0, 0}, {0, 30},
                  {{0.0f, hex(0x2A2118)}, {1.0f, hex(0x120C08)}}))
              .foreground(stroke(1.4f, Fill::color({p.color.fR, p.color.fG,
                                                    p.color.fB, 0.28f})))
              // THE DRAIN RING: the same outline stroked again, trimmed
              // to what is left of the buff. One node, two decorations —
              // a trim window is per decoration, so the spent part and
              // the remaining part need no second element.
              .foreground(drainRing(p.color, p.left))
              .alignItems(Align::Center)
              .justify(Justify::Center)
              // the drain: a dark wipe from the bottom, under the label
              .child(box()
                         .left(0)
                         .bottom(0)
                         .width(Dim(30.0f))
                         .height(Dim(30.0f * (1.0f - p.left)))
                         .fill(Paint::solid({0, 0, 0, 0.62f}))
                         .zIndex(1))
              .child(text(toU8(p.label), wh::type(9, p.color, 0.6f, 640))
                         .zIndex(2)));
    return row;
  }

  /** The loot scroller: recent pickups, each in its quality colour. */
  Element lootFeed() {
    namespace wh = worldhud;
    using namespace std::chrono_literals;
    struct Line {
      const char* text;
      SkColor4f color;
    };
    static const Line kLines[] = {
        {"Sunsteel Greatsword", wh::kQualityLegendary},
        {"Cave Spider Silk x4", wh::kQualityLow},
        {"Velorite Fragment x2", wh::kQualityHigh},
        {"Rugged Hide", wh::kQualityModerate},
        {"Glowing Remains", wh::kQualityEpic},
    };
    Element feed = box()
                       .key("loot")
                       .column()
                       .gap(3)
                       .left(28)
                       .bottom(70)
                       .zIndex(6)
                       .staggerChildren(90ms);
    for (const Line& l : kLines)
      feed.child(box()
                     .row()
                     .alignItems(Align::Center)
                     .gap(7)
                     .opacity(animate(motion::from(0.0f).to(1.0f), {420ms}))
                     .translateX(animate(motion::from(-24.0f).to(0.0f), {480ms}))
                     .child(box()
                                .width(Dim(16.0f))
                                .height(Dim(16.0f))
                                .corners({2})
                                .fill(Paint::solid({l.color.fR * 0.28f,
                                                       l.color.fG * 0.28f,
                                                       l.color.fB * 0.28f, 1}))
                                .foreground(stroke(1.0f, Fill::color(l.color))))
                     .child(text(toU8(l.text), wh::type(11, l.color, 0.4f))));
    return feed;
  }

  /** The enemy nameplate: ENEMY_HP_COLOR over a black track. */
  Element targetPlate() {
    namespace wh = worldhud;
    using namespace std::chrono_literals;
    return box()
        .key("target")
        .column()
        .alignItems(Align::Center)
        .left(0)
        .right(0)
        .top(96)
        .zIndex(6)
        .opacity(animate(motion::from(0.0f).to(1.0f),
                         {360ms, &choreograph::easeOutQuad, 220ms}))
        .child(text(toU8("CAVE TROLL"), wh::type(15, wh::kInk, 1.6f, 640)))
        .child(text(toU8("Lv 27"), wh::type(10, wh::kInkDim, 1.4f))
                   .margin(0, 2, 0, 4))
        .child(box()
                   .width(Dim(168.0f))
                   .height(Dim(9.0f))
                   .fill(Paint::solid(worldhud::kTrack))
                   .foreground(
                       stroke(1.0f, Fill::color({0.05f, 0.04f, 0.03f, 0.9f})))
                   .child(box()
                              .left(1)
                              .top(1)
                              .width(Dim(166.0f))
                              .height(Dim(7.0f))
                              .transformOrigin(0.0f, 0.5f)
                              .scaleX(&enemyHp)
                              .fill(Paint::solid(wh::kEnemyHp))));
  }

  /** The HUD itself: everything Veloren draws over the world. */
  Element hud() {
    namespace wh = worldhud;
    using namespace std::chrono_literals;

    // The root paints NOTHING. What is behind the HUD is the frame's own
    // valley, and a scrim here would be this study answering its own
    // question.
    auto root = stack();

    root.child(box()
                   .column()
                   .left(28)
                   .top(70)
                   .zIndex(6)
                   .child(text(toU8("WELDRIN VALE"),
                               wh::type(20, wh::kInk, 2.6f, 640)))
                   .child(text(toU8("LEVEL 34  \xc2\xb7  CLEAR, LIGHT WIND"),
                               wh::type(11, wh::kInkDim, 0.9f))
                              .margin(0, 5, 0, 0)));

    root.child(buffRow());
    root.child(minimap());
    root.child(targetPlate());
    root.child(lootFeed());
    root.child(barStack());
    root.child(hotbar());
    return root;
  }
};

}  // namespace

SIGIL_SKETCH_AS(
    WorldHud, "world hud", "Catalog \xc2\xb7 Game UI",
    "Voxygen's own dimensions \xe2\x80\x94 bars, hotbar, minimap and "
    "nameplate \xe2\x80\x94 baked into one texture over a lit voxel "
    "valley, which is the thing a HUD has to stay legible on")
