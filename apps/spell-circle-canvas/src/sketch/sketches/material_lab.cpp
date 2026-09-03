/** @file
 * material_lab — what a surface is made of, and what each tier can say
 * about it.
 *
 * Five curved cards stand in a row over a floor dressed with a texture
 * set, lit by the kit's three-point rig and seen from a parked
 * turntable. Each card is a different KIND of surface, and each is
 * chosen because a device SHADES it: one plain, one a STACK of two
 * through a mask, one wearing a normal map, one wearing a packed
 * roughness-and-metallic map, and one that emits.
 *
 * WHICH CARD READS ON WHICH TIER. The device tier runs each material's
 * own body, so every card there is what its params and its maps say. The
 * CPU tier has no compiler and reads a surface's base colour and its
 * base-colour map alone, so on it the row is five flat colours and the
 * floor's weave: the plain card is its colour, the stack is the colour
 * of the surface at the bottom of it, and the normal, packed and
 * emissive cards are their base colours with nothing of the map in them.
 * Both plates are honest pictures of their tier, and the row is built to
 * make the difference between them the thing the study shows.
 *
 * THE CARDS ARE CURVED, not flat. A Blinn highlight on a flat card is
 * one value over the whole face — the same value — so a card meant to
 * show a highlight narrowing has to present a range of normals to the
 * key light. Each card's face turns through most of a right angle, which
 * is enough that the highlight lands somewhere on every one of them.
 *
 * NO GLASS, AND NO SKY. This set is lit by three lamps and nothing else,
 * so `transmission`, `ior` and `thickness` have nothing to refract here:
 * a glass card would be a tinted rectangle labelled glass. What glass
 * needs is a panorama to bend, and the study of that is `reflection_lab`
 * — which is also where the environment terms are read, because a card
 * is the wrong shape to show a reflection and a sphere is the right
 * one.
 *
 * THE TEXTURE SET IS GENERATED HERE, not read off the disk. A plate is a
 * function of the declaration, and what a machine happens to have under
 * `build/assets` is not; the floor's two maps are baked off the pattern
 * shelf's checker and handed through the same `textures::` door a
 * scanned folder arrives by, so what is exercised is the vocabulary
 * rather than the filesystem. The maps the shelf has no tile for — a
 * tangent normal, a packed occlusion-roughness-metallic, a mask and an
 * emissive lattice — are written texel by texel below, each a function
 * of where the texel is and of nothing a machine decides.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkImageInfo.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/core/Combine.h>
#include <sigilmaterial/kit/Mask.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/pattern/Tile.h>
#include <sigilmaterial/texture/TextureSet.h>
#include <sigilsketch/set/Set.h>
#include <sigilworld/kit/Kit.h>

#include <algorithm>
#include <cmath>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace sketch = sigil::sketch;
namespace world = sigil::world;
namespace material = sigil::material;
namespace gm = sigil::geometry::mesh;

namespace {

constexpr int kMapSide = 128;
constexpr float kCardWidth = 96.0f;
constexpr float kCardHeight = 132.0f;
constexpr float kCardGap = 118.0f;
/** THE RADIUS THE CARDS ARE CURVED ON, and why they are curved at all:
 *  a Blinn highlight on a flat card is one value over the whole face, so
 *  a card meant to show a highlight narrowing has to present a range of
 *  normals to the key. At this radius a card's face turns through most
 *  of a right angle, which is wide enough that the highlight lands
 *  somewhere on every one of them whatever the key's exact bearing. */
constexpr float kCardCurve = 80.0f;
/** HOW FAR THE ROW LEANS BACK, and why it leans at all. A card curved
 *  about its own upright axis presents normals that all lie flat, and a
 *  highlight is where the surface faces halfway between the eye and the
 *  light — which for an eye above the row and a light that has to stand
 *  above the floor is never flat. Leaning the cards back until their
 *  normals point at that halfway direction is what puts the highlight on
 *  them at all; the curve then sweeps it across each face. */
constexpr float kCardLean = 25.0f;

/** One generated map, written texel by texel by @p texel — every map in
 *  this study is a function of where the texel is and of nothing a
 *  machine decides. */
template <class F>
sk_sp<SkImage> generated(const F& texel) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(kMapSide, kMapSide));
  for (int y = 0; y < kMapSide; ++y)
    for (int x = 0; x < kMapSide; ++x) {
      const SkColor4f colour = texel(x, y);
      *bitmap.getAddr32(x, y) = colour.toSkColor();
    }
  bitmap.setImmutable();
  return bitmap.asImage();
}

/** A CHECK the floor is read by, baked off the pattern shelf's own tile
 *  rather than written texel by texel: a scale and an orientation are
 *  all a floor needs from a map, and the tile is the shortest true
 *  statement of both. */
sk_sp<SkImage> check(material::Color a, material::Color b, float cell) {
  return material::pattern::checker(cell, a, b).image();
}

/** A TANGENT NORMAL MAP: a grid of domes, each rising out of the flat
 *  surface and falling back to it at the cell's edge. Encoded the way a
 *  tangent normal is — each axis centred on a half, so the flat surface
 *  is (0.5, 0.5, 1) and a map that says nothing is the one value white
 *  cannot be mistaken for. */
sk_sp<SkImage> domes(int cells, float bulge) {
  const float step = (float)kMapSide / (float)cells;
  return generated([&](int x, int y) {
    const float u = (std::fmod((float)x, step) / step) * 2.0f - 1.0f;
    const float v = (std::fmod((float)y, step) / step) * 2.0f - 1.0f;
    const float r2 = u * u + v * v;
    float nx = 0.0f, ny = 0.0f, nz = 1.0f;
    if (r2 < 1.0f) {
      nx = u * bulge;
      ny = v * bulge;
      nz = std::sqrt(1.0f - r2);
      const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
      nx /= len;
      ny /= len;
      nz /= len;
    }
    return SkColor4f{nx * 0.5f + 0.5f, ny * 0.5f + 0.5f, nz * 0.5f + 0.5f,
                     1.0f};
  });
}

/** ONE PACKED IMAGE holding occlusion, roughness and metallic at glTF's
 *  channel order, sweeping down the card: rough dielectric at the top,
 *  polished metal at the bottom. Neither swept channel reaches one,
 *  because a map that is white everywhere is what "no map here" means
 *  and a texel of it would drop back to the shading the vertices
 *  carried. */
sk_sp<SkImage> occlusionRoughnessMetallic() {
  return generated([](int, int y) {
    const float down = (float)y / (float)(kMapSide - 1);
    return SkColor4f{1.0f, 0.94f - 0.88f * down, 0.02f + 0.80f * down, 1.0f};
  });
}

/** THE MASK a stack is read through: soft irregular patches, so the top
 *  material shows in some places, not in others, and passes through
 *  every value between at their edges. */
sk_sp<SkImage> patches() {
  return generated([](int x, int y) {
    const float u = (float)x;
    const float v = (float)y;
    const float field = 0.5f +
                        0.30f * std::sin(u * 0.13f) * std::sin(v * 0.11f) +
                        0.20f * std::sin(u * 0.052f + v * 0.081f) +
                        0.10f * std::sin(u * 0.31f + v * 0.27f);
    const float t = std::clamp((field - 0.34f) * 2.6f, 0.0f, 1.0f);
    const float smooth = t * t * (3.0f - 2.0f * t);
    return SkColor4f{smooth, smooth, smooth, 1.0f};
  });
}

/** A LATTICE OF FILAMENTS on black — what an emissive slot multiplies,
 *  so the card emits in a pattern rather than all over. */
sk_sp<SkImage> filaments(int period) {
  const auto rule = [&](int at) {
    const float centred =
        std::abs(std::fmod((float)at, (float)period) - (float)period * 0.5f);
    return std::clamp(1.0f - centred / 2.5f, 0.0f, 1.0f);
  };
  return generated([&](int x, int y) {
    const float lit = std::clamp(rule(x) + rule(y), 0.0f, 1.0f);
    return SkColor4f{lit, lit, lit, 1.0f};
  });
}

/** The generated floor set, keyed by the usage words a material tool
 *  tags its outputs with — the same door a discovered folder's files
 *  arrive through once they are decoded. */
material::textures::TextureMaps floorMaps() {
  std::map<std::string, sk_sp<SkImage>> byUsage;
  byUsage["baseColor"] =
      check({0.30f, 0.33f, 0.38f, 1}, {0.17f, 0.19f, 0.23f, 1}, 16.0f);
  byUsage["roughness"] =
      check({0.7f, 0.7f, 0.7f, 1}, {0.35f, 0.35f, 0.35f, 1}, 32.0f);
  return material::textures::fromUsageMap(byUsage, /*normalDirectX=*/false);
}

/** A card standing upright at @p x, wearing @p surface. */
world::Element card(std::string_view key, float x,
                    material::Material surface) {
  return world::Element()
      .key(key)
      .at({x, 0.0f, 0.0f})
      .rotateX(-kCardLean)
      .mesh(gm::cylinderPanel(kCardWidth, kCardHeight, kCardCurve, 24, 6))
      .fill(std::move(surface))
      .tag("card");
}

/** PLAIN: a colour and a roughness, and no map anywhere. It is the
 *  control — the one card that reads the same on both tiers. */
material::Material plain() {
  return material::kit::surface(
      {.baseColor = {0.62f, 0.28f, 0.24f, 1.0f}, .roughness = 0.55f});
}

/** A STACK: a dark patinated base, a pale bumpy crust over it, and a
 *  mask that says where the crust shows. Every operand is an ordinary
 *  material, which is the whole point of the combinator — and on the
 *  device the stack shades AS a stack, the two operands' colours and
 *  their per-pixel terms mixed by the same coverage. */
material::Material stacked() {
  const material::Material base = material::kit::surface(
      {.baseColor = {0.13f, 0.20f, 0.17f, 1.0f}, .roughness = 0.8f});
  material::Material crust = material::kit::surface(
      {.baseColor = {0.80f, 0.72f, 0.56f, 1.0f}, .roughness = 0.35f});
  crust.child(material::kit::kNormalSlot,
              material::Texture::produce("material_lab.crust.normal",
                                         [] { return domes(6, 0.9f); }));
  const material::Material mask =
      material::kit::maskMap(material::Texture::produce(
          "material_lab.patches", [] { return patches(); }));
  return material::over(base, std::move(crust), mask);
}

/** MAPPED NORMAL: one flat colour, and a normal map that makes the key
 *  light land on a field of domes. Nothing but the map varies over this
 *  card, so what the device shows past its colour is the map. */
material::Material bumped() {
  material::Material m = material::kit::surface(
      {.baseColor = {0.42f, 0.47f, 0.58f, 1.0f}, .roughness = 0.35f});
  m.child(material::kit::kNormalSlot,
          material::Texture::produce("material_lab.bumps",
                                     [] { return domes(5, 1.0f); }));
  return m;
}

/** MAPPED ROUGHNESS AND METALLIC: one packed image in two slots, read at
 *  two channels — the way a set that ships one occlusion-roughness-
 *  metallic image is wired. The scalars start at one so the map's own
 *  values are what reach the shading. */
material::Material sweep() {
  material::Material m = material::kit::surface({
      .baseColor = {0.78f, 0.76f, 0.70f, 1.0f},
      .metallic = 1.0f,
      .roughness = 1.0f,
      .roughnessChannel = 1.0f,
      .metallicChannel = 2.0f,
  });
  const material::Texture packed = material::Texture::produce(
      "material_lab.orm", [] { return occlusionRoughnessMetallic(); });
  m.child(material::kit::kRoughnessSlot, packed);
  m.child(material::kit::kMetallicSlot, packed);
  return m;
}

/** EMISSIVE: a dark surface that is also a light source in a pattern —
 *  the emissive slot multiplied by the emission's own colour and
 *  strength, added after the lighting. */
material::Material emitting() {
  material::Material m = material::kit::surface({
      .baseColor = {0.30f, 0.13f, 0.09f, 1.0f},
      .roughness = 0.6f,
      .emissive = {1.0f, 0.62f, 0.24f, 1.0f},
      .emissiveStrength = 2.4f,
  });
  m.child(material::kit::kEmissiveSlot,
          material::Texture::produce("material_lab.filaments",
                                     [] { return filaments(22); }));
  return m;
}

/** The five surfaces, left to right. */
world::Element cards() {
  const float left = -2.0f * kCardGap;
  return world::Element()
      .key("cards")
      .at({0.0f, 40.0f, 0.0f})
      // The row runs across the turntable's parked station rather than
      // along it, so the cards are seen face on.
      .rotateY(90.0f)
      .child(card("plain", left, plain()))
      .child(card("stacked", left + kCardGap, stacked()))
      .child(card("bumped", left + 2.0f * kCardGap, bumped()))
      .child(card("sweep", left + 3.0f * kCardGap, sweep()))
      .child(card("emissive", left + 4.0f * kCardGap, emitting()));
}

}  // namespace

namespace {

struct MaterialLab final : sketch::Set {
  /** The row and the floor it stands on, made once. Neither is a
   *  function of the time — the five recipes are what the sketch is
   *  about and the floor's two maps are generated pixel by pixel — so
   *  building them per frame would generate the same images sixty times
   *  a second to describe a picture that never changed. */
  world::Element row;
  std::optional<material::Material> floorSurface;

  void setup(sketch::SetContext& ctx) override {
    ctx.canvas(880, 520);
    ctx.background({0.035f, 0.038f, 0.05f, 1.0f});
    ctx.captureAt(1.1);
    row = cards();
    // The floor wears the set: a texture that repeats is what says how
    // large the room is.
    material::Material floor =
        material::kit::surface(floorMaps(), {.baseColor = {1, 1, 1, 1}});
    if (const material::Texture* map =
            material::kit::map(floor, material::kit::kBaseColorSlot)) {
      material::Texture tiled = *map;
      tiled.tile(SkTileMode::kRepeat)
          // The baked tile is two cells across, so the repeat is set
          // against ITS size rather than against a map's: this many
          // tiles cover the floor.
          .uv(SkMatrix::Scale(1.0f / 20.0f, 1.0f / 20.0f));
      floor.child(material::kit::kBaseColorSlot, std::move(tiled));
    }
    floorSurface = std::move(floor);
  }

  world::Frame describe(float seconds) override {
    world::kit::Set set;
    set.rig.extent = 140.0f;
    // The key stands just off the eye's own bearing and above it, so
    // every card is lit face on and the highlight lands where a reader
    // is already looking; the fill opens the shadowed side and the back
    // light separates the row from the floor behind it.
    set.rig.bearing = 78.0f;
    set.rig.elevation = 30.0f;
    set.rig.fill = 0.4f;
    set.rig.back = 0.5f;
    set.ground = 5.0f;
    set.drop = 0.5f;
    set.surface = floorSurface;

    set.table.radius = 760.0f;
    set.table.height = 420.0f;
    // PARKED. A lab is read, not watched: the row faces the eye and
    // stays there, so the live picture and the plate are the same
    // picture and a reader comparing two surfaces is not comparing them
    // at two bearings.
    set.table.period = 0.0f;
    set.table.fovYDeg = 46.0f;
    return world::Frame(world::kit::litSet(row, set, seconds));
  }
};

}  // namespace

SIGIL_SKETCH(
    MaterialLab, "Set",
    "What a surface is made of \xe2\x80\x94 five cards over a textured "
    "floor: one plain, one a stack through a mask, one normal-mapped, "
    "one sweeping rough to metal, and one emitting")
