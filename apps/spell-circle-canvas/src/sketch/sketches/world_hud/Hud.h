#pragma once

#include <include/core/SkPathBuilder.h>
#include <sigilcompose/brush/Adaptors.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/brush/PixelStyles.h>
#include <sigilcompose/core/Instances.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/kit/Pbr.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/kit/Cells.h>
#include <sigilsketch/set/Set.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/style/Type.h>
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

namespace arrange = sigil::geometry::arrange;
namespace sketch = sigil::sketch;
namespace skit = sigil::sketch::kit;
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
using sigil::compose::toU8;
using sigil::material::skia::Paint;
using namespace std::chrono_literals;

namespace {
/** The canvas this piece was drawn against, which is also the default a
 *  sketch gets when it declares none. */
constexpr SkSize kSceneSize = {900, 640};

namespace worldhud {

constexpr float kW = kSceneSize.fWidth, kH = kSceneSize.fHeight;

// voxygen/src/hud/mod.rs, verbatim.
constexpr SkColor4f kHp = hexColor(0x54A100);
constexpr SkColor4f kLowHp = hexColor(0xED9608);
constexpr SkColor4f kCritHp = hexColor(0xC9302B);
constexpr SkColor4f kStamina = hexColor(0x4A9EBF);
constexpr SkColor4f kXp = hexColor(0x9669AB);
constexpr SkColor4f kPoise = hexColor(0xB30099);
constexpr SkColor4f kPoiseTick = hexColor(0xB3E600);
constexpr SkColor4f kEnemyHp = hexColor(0xED1A4A);
constexpr SkColor4f kBuff = hexColor(0x10B01F);
constexpr SkColor4f kDebuff = hexColor(0xC9302B);
constexpr SkColor4f kQualityLow = hexColor(0x999999);
constexpr SkColor4f kQualityCommon = hexColor(0xC9FFFF);
constexpr SkColor4f kQualityModerate = hexColor(0x10B01F);
constexpr SkColor4f kQualityHigh = hexColor(0x2E52E6);
constexpr SkColor4f kQualityEpic = hexColor(0x944AED);
constexpr SkColor4f kQualityLegendary = hexColor(0xEBC200);
constexpr SkColor4f kQualityArtifact = hexColor(0xBD3D1C);

// The frame material: Veloren's UI is carved bone over dark wood.
constexpr SkColor4f kBoneHi = hexColor(0xD8CBA8);
constexpr SkColor4f kBone = hexColor(0xA2947A);
constexpr SkColor4f kBoneLo = hexColor(0x584E3D);
constexpr SkColor4f kWood = hexColor(0x2A2118);
constexpr SkColor4f kWoodLo = hexColor(0x160F0A);
constexpr SkColor4f kTrack = hexColor(0x0B0906);
constexpr SkColor4f kInk = hexColor(0xEDE6D4);
constexpr SkColor4f kInkDim = hexColor(0x8C8271);

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
  return skit::well(
      {.width = Dim(w),
       .height = Dim(h),
       .ground = Fill::color(kTrack),
       .padding = 0,
       .clip = false,
       .recess = skit::Well::Recess{.shade = Fill::color({0, 0, 0, 0.85f}),
                                    .offset = {0, 2},
                                    .blur = 3}});
}

/** The carved bone frame Veloren hangs on everything: a plate standing
 *  proud of what holds it, with a keyline round the OUTSIDE of its box —
 *  the piece is cut to a size and its outline is drawn around it, not
 *  inside it, which is the one thing a well's own hairline may not do. */
inline Element boneFrame(float w, float h, float radius = 3) {
  return skit::well({.width = Dim(w),
                     .height = Dim(h),
                     .ground = Paint::linear(
                         {0, 0}, {0, h},
                         {{0.0f, kBoneHi}, {0.45f, kBone}, {1.0f, kBoneLo}}),
                     .padding = 0,
                     .clip = false,
                     .corners = radius,
                     .relief = skit::Well::Relief{.depth = 1.6f,
                                                  .blur = 2.4f,
                                                  .angleDeg = 120,
                                                  .light = {1, 1, 1, 0.35f},
                                                  .shade = {0, 0, 0, 0.65f}}})
      // The grain: Veloren's frames are carved, and a ramp with no noise
      // in it is a plastic one. It rides UNDER the bevel, so the carve
      // reads through the highlight rather than over it.
      .child(box()
                 .inset(0)
                 .corners({radius})
                 .fill(Paint::recipe(field::noise(0.36f, 3, 1.0f)))
                 .opacity(0.38f)
                 .blend(SkBlendMode::kMultiply))
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
          const SkPoint arm =
              arrange::onRing((size_t)i, 6, {0, 0}, {w * 0.36f, h * 0.36f},
                              0.0f, 6.2831853f, arrange::Turn::Closed);
          const float dx = arm.fX, dy = arm.fY;
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

/** One axis-aligned column: FIVE faces, not six, each carrying its own
 *  fraction of the colour.
 *
 *  It is not a box primitive and does not want to be one. The camera
 *  stands above the valley, so the underside is a tenth of the triangles
 *  for nothing, and the top face at full colour against the sides at
 *  0.72 is what makes a voxel field read as blocks rather than as a
 *  surface. A generic six-faced box would draw a different picture at a
 *  higher cost. */
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

}  // namespace
