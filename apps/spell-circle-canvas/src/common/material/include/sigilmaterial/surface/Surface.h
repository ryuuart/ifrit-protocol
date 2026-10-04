#pragma once

/** @file
 * @ingroup material-surface
 *
 * THE SURFACE PROGRAM a lit renderer shades with: the metallic-roughness
 * model the authoring tools export for. One parameter struct is its ABI
 * and one slot per map, so a discovered texture set drops straight in,
 * under two recipes: one takes light, the other is its own light. The
 * bodies are composed from the library's shading TERMS. Its renderer
 * supplies geometry normals, the view and lighting. Planar lighting is
 * a separate Skia pass over SurfaceOptions and does not run this program.
 *
 * A material states its response with `Material::surface(SurfaceOptions)`;
 * `lower()` is the step that turns that statement into this program, and
 * the rest of this header is the program itself, for a renderer or a
 * writer that reads its slots.
 */

#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmaterial/texture/TextureSet.h>

#include <cstdint>
#include <memory>
#include <string_view>

namespace sigil::material::surface {

/** The green-axis convention of each encoded normal map. */
struct NormalBlendOptions {
  bool baseDirectX = false;
  bool detailDirectX = false;
  bool outputDirectX = false;
  bool operator==(const NormalBlendOptions&) const = default;
};

/** Reorient @p detail into @p base's tangent frame. Both are opaque RGB
 *  maps encoding unit normals as (normal + 1) / 2. The result uses the
 *  requested output convention; a flat map leaves the other unchanged.
 *  Base normals must face outward (z >= 0). */
Material blendNormals(Material base, Material detail,
                      NormalBlendOptions options = {});

struct HeightNormalOptions {
  /** Height of white relative to black, in logical pixels. Negative dents. */
  float depth = 1;
  /** Central-difference sample distance in logical pixels. Must be positive. */
  float step = 1;
  /** Encode green down the image rather than the OpenGL green-up convention. */
  bool directX = false;
  bool operator==(const HeightNormalOptions&) const = default;
};

/** A Skia program differentiating grayscale height into opaque encoded normals.
 *  Premultiplied RGB luminance supplies height, clamped to [0, 1], so
 *  transparent input is zero. Smooth hard coverage before supplying it
 *  when a rounded shoulder is wanted. Sampling follows the input's placement
 *  and repeat settings. Zero/nonfinite depth or an invalid step yields flat
 *  normals. The input's colour stack is sampled; its surface/effects are not.
 */
Material normalFromHeight(Material height, HeightNormalOptions options = {});

/** The slots the surface recipes declare, one per map a texture
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

/** The metallic-roughness ABI. Its colours are FACTORS ON THE MAPS in
 *  their slots — `baseColor` on `baseColorMap`, `emissive` on
 *  `emissiveMap` — and neither body transforms either side of that
 *  multiply or the product it hands on: a map is sampled as the numbers
 *  the image stores, and what comes out reaches the target as it is. So
 *  a colour is held here exactly as it is typed, in the encoding the
 *  images beside it are in, and a surface whose map is the white one
 *  `program()` dresses it with shows the colour it was given.
 *
 *  Each scalar is multiplied by the map in the matching slot, so a set
 *  that ships a metallic map wants `metallic = 1` for the map's values
 *  to come through — which is what `program(TextureMaps)` arranges. */
struct SurfaceParameters {
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
  /** GLASS: what the medium takes out of the light per unit of
   *  thickness, per channel — the Beer-Lambert coefficient rather than
   *  a colour anyone looks at, so nothing holds it inside the unit
   *  range. Zero is water-clear; a little in red and blue is what makes
   *  thick glass green at its edge and clear across its face. */
  Color absorption = {0, 0, 0, 1};
  /** How much environment an ADDITIVE reflection puts on the surface.
   *  The split-sum composition ignores it: there the weight IS the
   *  surface's reflectance and its Fresnel. */
  float reflectionWeight = 1;
};

/** HOW THE ENVIRONMENT REACHES A SURFACE, which is a choice about the
 *  model and not a dial on it, so it is one recipe each and no body
 *  carries a branch. */
enum class Reflection : uint8_t {
  /** The split sum: prefiltered radiance times the surface's own
   *  reflectance and its Fresnel, so a metal keeps its colour, a
   *  dielectric picks up the sky at its rim, and a rough surface takes
   *  less than a smooth one. What the metallic-roughness vocabulary was
   *  written for. */
  SplitSum,
  /** The radiance at `reflectionWeight`, with no Fresnel and no energy
   *  accounting. Wrong at a grazing angle and right wherever an author
   *  wants to say how much sky is on a surface rather than be told. */
  Additive,
};

/** The lit recipe for @p reflection, defined once per model; it declares
 *  every map slot above. */
const std::shared_ptr<const Recipe>& surfaceRecipe(
    Reflection reflection = Reflection::SplitSum);

/** A lit metallic-roughness surface, composed from the shading terms:
 *  occlusion over the albedo, emission added, and the surface's PBR
 *  standing handed to whatever renderer shades it. */
Material program(const SurfaceParameters& parameters = {},
                 Reflection reflection = Reflection::SplitSum);
/** A surface that is its own light: no shading, no shadow terms. */
Material unlit(const SurfaceParameters& parameters = {});

/** Whether @p material is an instance of either surface recipe. */
bool isSurface(const Material& material);
/** Whether @p material is the unlit one specifically. `isSurface` answers true
 *  for these too. */
bool isUnlit(const Material& material);

/** The MAP in @p slot: the texture a caller placed there, or null when
 *  the slot still holds the neutral fill every surface is built with.
 *  A reader asking what a surface is dressed with wants this rather than
 *  `leaf()`, which never answers null on a built surface. */
const Texture* map(const Material& material, std::string_view slot);

/** @p base dressed with a decoded texture set: every role that decoded
 *  placed in its slot; a packed occlusion-roughness-metallic image wired
 *  to whichever of the three channel slots no separate map fills, at
 *  channels 0, 1 and 2; the set's normal convention flagged; and the
 *  scalar a present map multiplies started at one — left at its stock
 *  value a metallic map would multiply zero and never be seen — unless
 *  @p base already moved it. */
Material program(const texture::TextureMaps& maps, SurfaceParameters base = {});

/** THE SURFACE PROGRAM @p material states. A bare program — a surface
 *  program, or any recipe instance with nothing stated around it — comes
 *  back as it is, since a renderer draws a program as the program it is.
 *  Otherwise the base is the base colour: a colour is the `baseColor`
 *  factor, an image base's texture fills `baseColorMap`, and any other
 *  base — a gradient, a noise, a program, or the base with its layers —
 *  fills that slot as a material. Each `SurfaceOptions` channel is a
 *  number or a material, placed the same way in the channel's map slot
 *  with its factor at one. `.unlit` answers the unlit program, and a
 *  material that states no response at all is its base, unlit.
 *
 *  A surface has no coverage for an effects stage to read, so effects on
 *  @p material are dropped and said once; so is a clearcoat, which the
 *  program has no term for. */
Material lower(const Material& material);

}  // namespace sigil::material::surface
