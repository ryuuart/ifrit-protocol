/** @file
 * material_lab — what a surface is made of, and what a tier with no
 * compiler can say about it.
 *
 * Five cards stand in a row over a floor dressed with a texture set, lit
 * by the kit's three-point rig and seen from its turntable. Each card is
 * a different KIND of surface: one plain, one a STACK of two through a
 * mask, one glass, one emissive, and one wearing a map of its own.
 *
 * WHAT REACHES THE PIXELS HERE is the surface's base colour and the map
 * in its base-colour slot, because neither tier compiles the
 * metallic-roughness body: a stack reaches them as the surface at the
 * bottom of it, since the mask that decides where the top shows is a
 * program. So the stack card reads as its base and the glass card as its
 * tint — which is the honest picture of what this library can shade
 * today, and the plate moves the day that changes.
 *
 * THE TEXTURE SET IS GENERATED HERE, not read off the disk. A plate is a
 * function of the declaration, and what a machine happens to have under
 * `build/assets` is not; the set is built through the same
 * `textures::` door a scanned folder arrives by, so what is exercised is
 * the vocabulary rather than the filesystem.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkImageInfo.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/core/Combine.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilmaterial/texture/TextureSet.h>
#include <sigilsketch/set/Set.h>
#include <sigilworld/kit/Kit.h>

#include <cmath>
#include <glm/vec4.hpp>
#include <map>
#include <string>
#include <string_view>

namespace sketch = sigil::sketch;
namespace world = sigil::world;
namespace material = sigil::material;

using namespace sigil::world;

namespace {

namespace gm = ::sigil::geometry::mesh;

constexpr int kMapSide = 128;
constexpr float kCardWidth = 96.0f;
constexpr float kCardHeight = 132.0f;
constexpr float kCardGap = 118.0f;

/** One generated map. @p warm and @p cool are what its two bands are,
 *  so one function makes a colour map, a roughness map and a normal map
 *  by being asked for different pairs. */
sk_sp<SkImage> band(SkColor4f warm, SkColor4f cool, int period) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(kMapSide, kMapSide));
  for (int y = 0; y < kMapSide; ++y) {
    for (int x = 0; x < kMapSide; ++x) {
      // A woven check: two frequencies crossing, so the map has an
      // orientation and a scale a floor can be read by.
      const bool warmCell = ((x / period) + (y / period)) % 2 == 0;
      const float edge =
          (float)((x % period) + (y % period)) / (float)(2 * period);
      const SkColor4f base = warmCell ? warm : cool;
      const SkColor4f shade{base.fR * (0.86f + 0.14f * edge),
                            base.fG * (0.86f + 0.14f * edge),
                            base.fB * (0.86f + 0.14f * edge), 1.0f};
      *bitmap.getAddr32(x, y) = shade.toSkColor();
    }
  }
  bitmap.setImmutable();
  return bitmap.asImage();
}

/** The generated set, keyed by the usage words a material tool tags its
 *  outputs with — the same door a discovered folder's files arrive
 *  through once they are decoded. */
material::textures::TextureMaps floorMaps() {
  std::map<std::string, sk_sp<SkImage>> byUsage;
  byUsage["baseColor"] =
      band({0.30f, 0.33f, 0.38f, 1}, {0.17f, 0.19f, 0.23f, 1}, 16);
  byUsage["roughness"] =
      band({0.7f, 0.7f, 0.7f, 1}, {0.35f, 0.35f, 0.35f, 1}, 32);
  byUsage["normal"] = band({0.5f, 0.5f, 1.0f, 1}, {0.5f, 0.5f, 1.0f, 1}, 64);
  return material::textures::fromUsageMap(byUsage, /*normalDirectX=*/false);
}

/** A card standing upright at @p x, wearing @p surface. */
Element card(std::string_view key, float x, material::Material surface) {
  return Element()
      .key(key)
      .at({x, 0.0f, 0.0f})
      .mesh(gm::quad(kCardWidth, kCardHeight))
      .fill(std::move(surface))
      .tag("card");
}

/** The five surfaces, left to right. */
Element cards() {
  const material::Material plain = material::kit::surface(
      {.baseColor = {0.62f, 0.28f, 0.24f, 1.0f}, .roughness = 0.55f});

  // A STACK: a copper base, a pale crust over it, and a third surface
  // read as the mask that says where the crust shows. Every operand is
  // an ordinary material, which is the whole point of the combinator.
  const material::Material crust =
      material::kit::surface({.baseColor = {0.78f, 0.76f, 0.70f, 1.0f}});
  const material::Material mask =
      material::kit::surface({.baseColor = {0.5f, 0.5f, 0.5f, 1.0f}});
  const material::Material stacked = material::over(
      material::kit::surface({.baseColor = {0.24f, 0.42f, 0.34f, 1.0f}}), crust,
      mask, material::Blend::Mix);

  const material::Material glass =
      material::kit::surface({.baseColor = {0.70f, 0.82f, 0.86f, 0.55f},
                              .roughness = 0.05f,
                              .transmission = 0.92f,
                              .ior = 1.52f,
                              .thickness = 18.0f});

  const material::Material emissive =
      material::kit::unlit({.baseColor = {0.98f, 0.62f, 0.22f, 1.0f},
                            .emissive = {0.98f, 0.62f, 0.22f, 1.0f},
                            .emissiveStrength = 3.0f});

  // …and one card wearing a map, so that the row shows a surface whose
  // colour is a picture rather than a number.
  material::Material mapped =
      material::kit::surface(floorMaps(), {.baseColor = {1, 1, 1, 1}});

  const float left = -2.0f * kCardGap;
  return Element()
      .key("cards")
      .at({0.0f, 40.0f, 0.0f})
      // The row runs across the turntable's first station rather than
      // along it, so the cards are seen face on where the sweep stops.
      .rotateY(90.0f)
      .child(card("plain", left, plain))
      .child(card("stacked", left + kCardGap, stacked))
      .child(card("glass", left + 2.0f * kCardGap, glass))
      .child(card("emissive", left + 3.0f * kCardGap, emissive))
      .child(card("mapped", left + 4.0f * kCardGap, std::move(mapped)));
}

}  // namespace

namespace {

struct MaterialLab final : sketch::Set {
  void setup(sketch::SetContext& ctx) override {
    ctx.canvas(880, 520);
    ctx.background({0.035f, 0.038f, 0.05f, 1.0f});
    ctx.captureAt(1.1);
  }

  world::Frame describe(float seconds) override {
    kit::Set set;
    set.rig.extent = 140.0f;
    set.rig.bearing = -28.0f;
    set.rig.elevation = 34.0f;
    set.ground = 5.0f;
    set.drop = 0.5f;
    // The floor wears the set: a texture that repeats is what says how
    // large the room is.
    material::Material floor =
        material::kit::surface(floorMaps(), {.baseColor = {1, 1, 1, 1}});
    if (const material::Texture* map =
            material::kit::map(floor, material::kit::kBaseColorSlot)) {
      material::Texture tiled = *map;
      tiled.tile(SkTileMode::kRepeat)
          .uv(SkMatrix::Scale(1.0f / 5.0f, 1.0f / 5.0f));
      floor.child(material::kit::kBaseColorSlot, std::move(tiled));
    }
    set.surface = std::move(floor);

    set.table.radius = 760.0f;
    set.table.height = 420.0f;
    set.table.period = 14.0f;
    set.table.fovYDeg = 46.0f;
    return Frame(kit::litSet(cards(), set, seconds));
  }
};

}  // namespace

SIGIL_SKETCH(
    MaterialLab, "Set",
    "What a surface is made of \xe2\x80\x94 five cards over a textured "
    "floor: one plain, one a stack through a mask, one glass, one "
    "emissive, and one wearing a map")
