#pragma once

/** @file
 * The metallic-roughness surface — the shading model the authoring tools
 * export for and glTF, USD's preview surface and every scanned material
 * set are written against. One params struct is its ABI: base colour,
 * metallic, roughness, emission, the normal convention, the channel each
 * packed map is read from, the cutout threshold and the glass terms. One
 * child slot per map, named for the role it fills, so a discovered
 * texture set drops straight in.
 *
 * Two recipes over that ABI, and the choice between them is what a
 * surface IS rather than a flag on it: `surface()` takes light,
 * `unlit()` is its own light — a screen, a decal that must not be shaded.
 *
 * The SkSL bodies are what a device-space shader can answer honestly:
 * there is no surface normal, no view vector and no light in a 2D paint,
 * so metallic, roughness, the normal map and the glass terms have no
 * effect there. `surface()` shades the albedo attenuated by occlusion
 * plus its emission — the ambient-only evaluation of the model — and
 * `unlit()` shades the albedo alone. A renderer that HAS the surface
 * attributes reads the same params and slots and shades the full model.
 */

#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Recipe.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmaterial/texture/TextureSet.h>

#include <memory>
#include <string_view>

namespace sigil::material::kit {

/** The child slots the surface recipes declare, one per map a texture
 *  set carries. Each takes a `Texture` (or any leaf a renderer binds);
 *  an empty slot reads as the neutral value for that role. */
inline constexpr std::string_view kBaseColorSlot = "baseColorMap";
inline constexpr std::string_view kNormalSlot =
    "normalMap";  ///< tangent space; `normalScale` and `normalDirectX` read it
inline constexpr std::string_view kRoughnessSlot =
    "roughnessMap";  ///< the channel `roughnessChannel` names
inline constexpr std::string_view kMetallicSlot =
    "metallicMap";  ///< the channel `metallicChannel` names
inline constexpr std::string_view kOcclusionSlot =
    "occlusionMap";  ///< `occlusionChannel`, weighted by `occlusionStrength`
inline constexpr std::string_view kEmissiveSlot =
    "emissiveMap";  ///< multiplied by `emissive` and `emissiveStrength`
inline constexpr std::string_view kOpacitySlot =
    "opacityMap";  ///< `opacityChannel`; `alphaCutoff` turns it into a cutout

/** The metallic-roughness ABI. Colours are LINEAR. Each scalar is
 *  multiplied by the map in the matching slot, so a set that ships a
 *  metallic map wants `metallic = 1` for the map's values to come
 *  through — which is what `surface(TextureMaps)` arranges. */
struct SurfaceParams {
  Color baseColor = {0.8f, 0.8f, 0.8f, 1};
  float metallic = 0;
  float roughness = 0.5f;
  Color emissive = {0, 0, 0, 1};
  float emissiveStrength = 0;
  /** Scales the normal map's perturbation: 0 flat, 1 as authored. */
  float normalScale = 1;
  /** 1 when the normal map's green points DOWN the image (DirectX);
   *  0 for the OpenGL convention. */
  float normalDirectX = 0;
  /** Which channel of the map in each slot is read, 0 red .. 3 alpha, so
   *  one packed occlusion-roughness-metallic image can fill all three
   *  slots at channels 0, 1 and 2. */
  float roughnessChannel = 0;
  float metallicChannel = 0;
  float occlusionChannel = 0;
  /** How far the occlusion map darkens the ambient term; 0 ignores it. */
  float occlusionStrength = 1;
  float opacityChannel = 0;
  /** Above 0 the opacity is a CUTOUT: below the threshold the surface is
   *  absent rather than translucent. */
  float alphaCutoff = 0;
  /** GLASS: how much of what lies behind the surface shows through it —
   *  0 an ordinary surface, 1 clear glass — refracted by `ior` and read
   *  `thickness` units into the surface. */
  float transmission = 0;
  float ior = 1.5f;
  float thickness = 40;
};

/** The recipes, defined once. Both declare every map slot above. */
const std::shared_ptr<const Recipe>& surfaceRecipe();
/** The unlit half of that pair — the same params and the same slots, with
 *  no shading terms read. */
const std::shared_ptr<const Recipe>& unlitRecipe();

/** A lit metallic-roughness surface. */
Material surface(const SurfaceParams& params = {});
/** A surface that is its own light: no shading, no shadow terms. */
Material unlit(const SurfaceParams& params = {});

/** Whether @p m is an instance of either surface recipe. */
bool isSurface(const Material& m);
/** Whether @p m is the unlit one specifically. `isSurface` answers true
 *  for these too. */
bool isUnlit(const Material& m);

/** The MAP in @p slot: the texture a caller placed there, or null when
 *  the slot still holds the neutral fill every surface is built with.
 *  A reader asking what a surface is dressed with wants this rather than
 *  `leaf()`, which never answers null on a built surface. */
const Texture* map(const Material& m, std::string_view slot);

/** @p base dressed with a decoded texture set: every role that decoded
 *  placed in its slot; a packed occlusion-roughness-metallic image wired
 *  to whichever of the three channel slots no separate map fills, at
 *  channels 0, 1 and 2; the set's normal convention flagged; and the
 *  scalar a present map multiplies started at one — left at its stock
 *  value a metallic map would multiply zero and never be seen — unless
 *  @p base already moved it. */
Material surface(const textures::TextureMaps& maps, SurfaceParams base = {});

}  // namespace sigil::material::kit
