#pragma once

/** @file
 * @ingroup material-surface
 *
 * THE SURFACE PROGRAM a lit renderer shades with: the metallic-roughness
 * model the authoring tools export for. One parameter struct is its ABI
 * and one slot per map, so a discovered texture set drops straight in,
 * under two recipes: one takes light, the other is its own light. The
 * bodies are composed from the library's shading TERMS. Its renderer
 * supplies geometry normals, the view and lighting.
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
#include <optional>
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
inline constexpr std::string_view kClearcoatSlot =
    "clearcoatMap";  ///< the channel `clearcoatChannel` names
inline constexpr std::string_view kClearcoatRoughnessSlot =
    "clearcoatRoughnessMap";  ///< the channel `clearcoatRoughnessChannel` names
inline constexpr std::string_view kClearcoatNormalSlot =
    "clearcoatNormalMap";  ///< tangent space, the coat's own; read like
                           ///< `normalMap`
inline constexpr std::string_view kSheenColorSlot =
    "sheenColorMap";  ///< a colour, multiplied by `sheenColor`
inline constexpr std::string_view kSheenRoughnessSlot =
    "sheenRoughnessMap";  ///< the channel `sheenRoughnessChannel` names
inline constexpr std::string_view kAnisotropySlot =
    "anisotropyMap";  ///< red and green the direction, blue the strength
inline constexpr std::string_view kIridescenceSlot =
    "iridescenceMap";  ///< the channel `iridescenceChannel` names
inline constexpr std::string_view kIridescenceThicknessSlot =
    "iridescenceThicknessMap";  ///< `iridescenceThicknessChannel`, from the
                                ///< minimum at black to the thickness at white
inline constexpr std::string_view kSpecularSlot =
    "specularMap";  ///< the channel `specularChannel` names
inline constexpr std::string_view kSpecularColorSlot =
    "specularColorMap";  ///< a colour, multiplied by `specularColor`
inline constexpr std::string_view kTransmissionSlot =
    "transmissionMap";  ///< red, multiplied by `transmission`
inline constexpr std::string_view kThicknessSlot =
    "thicknessMap";  ///< red, multiplied by `thickness`
inline constexpr std::string_view kDistortionSlot =
    "distortionMap";  ///< red across and green up, half-grey still

/** The metallic-roughness ABI. Its colours are FACTORS ON THE MAPS in
 *  their slots — `baseColor` on `baseColorMap`, `emissive` on
 *  `emissiveMap` — and both are COLOURS as an author types them and an
 *  image stores them: encoded for display, and decoded into linear light
 *  before any light touches them, so a surface whose map is the white one
 *  `program()` dresses it with and that is lit to show its own colour
 *  shows the colour it was given. Every other map is DATA, read as the
 *  numbers the image stores.
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
  /** How far the occlusion map darkens the light arriving from
   *  everywhere — the environment and ambient light, never a light from
   *  one direction; 0 ignores it. */
  float occlusionStrength = 1;
  float opacityChannel = 0;
  /** Above 0 the opacity is a CUTOUT: below the threshold the surface is
   *  absent rather than translucent. */
  float alphaCutoff = 0;
  /** TRANSMISSION: how much of what lies behind the surface shows
   *  through it — 0 an ordinary surface, 1 clear glass — bent by `ior`
   *  across a volume `thickness` scene units deep, zero a thin wall; each
   *  multiplied by the red of the map in its slot. */
  float transmission = 0;
  float ior = 1.5f;
  float thickness = 0;
  /** VOLUME: light that has crossed `attenuationDistance` scene units of
   *  the medium has become `attenuationColor`, a colour as an author types
   *  it; zero distance takes nothing out. */
  Color attenuationColor = {1, 1, 1, 1};
  float attenuationDistance = 0;
  /** DISPERSION: how far the index spreads across the three colours,
   *  0 none. */
  float dispersion = 0;
  /** DISTORTION: how many scene units at the surface the map in its slot
   *  displaces what stands behind; 0 none. */
  float distortionStrength = 0;
  /** How much environment an ADDITIVE reflection puts on the surface.
   *  The split-sum composition ignores it: there the weight IS the
   *  surface's reflectance and its Fresnel. */
  float reflectionWeight = 1;
  /** A CLEAR COAT: a colourless second reflection of this weight over the
   *  surface, at its own roughness and, when `clearcoatNormalMap` is
   *  dressed, its own normal; without one the coat follows the geometry.
   *  Each is multiplied by the map in its slot. */
  float clearcoat = 0;
  float clearcoatRoughness = 0;
  /** Which channel of the coat's maps is read: glTF packs the weight in
   *  red and the roughness in green, so one image fills both slots. */
  float clearcoatChannel = 0;
  float clearcoatRoughnessChannel = 1;
  /** SHEEN: its colour — a colour as an author types it, black none —
   *  and its roughness, each multiplied by the map in its slot. glTF packs
   *  the roughness in the colour image's alpha. */
  Color sheenColor = {0, 0, 0, 1};
  float sheenRoughness = 0;
  float sheenRoughnessChannel = 3;
  /** ANISOTROPY: its strength, multiplied by the blue of the map in its
   *  slot, and the turn of its direction in degrees counter-clockwise from
   *  the surface's u; the map's red and green turn it further. */
  float anisotropy = 0;
  float anisotropyRotation = 0;
  /** IRIDESCENCE: the film's weight, its index of refraction, and its
   *  thickness in nanometres — the map in the thickness slot runs it from
   *  `iridescenceThicknessMinimum` to `iridescenceThickness`. glTF packs
   *  the weight in red and the thickness in green. */
  float iridescence = 0;
  float iridescenceIor = 1.3f;
  float iridescenceThickness = 400;
  float iridescenceThicknessMinimum = 100;
  float iridescenceChannel = 0;
  float iridescenceThicknessChannel = 1;
  /** SPECULAR: a dielectric's reflection strength and colour, each
   *  multiplied by the map in its slot. glTF packs the strength in the
   *  colour image's alpha. The colour carries glTF's own name for it,
   *  because `specularColor` is the term that answers a surface's head-on
   *  reflectance, and a parameter compiled beside a term of its name would
   *  be an ambiguous reference. */
  float specular = 1;
  Color specularColorFactor = {1, 1, 1, 1};
  float specularChannel = 3;
  /** HOW THE SURFACE COMPOSITES, as `SurfaceOptions` states it: the blend
   *  mode by its enumerator, whether it writes depth — 1 yes, 0 no, below
   *  zero unstated — and the distance it fades over where it meets what
   *  stands behind it. A renderer reads the first two as its blend state
   *  (`compositing()`); the bodies hand the third to the renderer, which
   *  is the one holding the scene's depth. */
  float blend = 0;
  float depthWrite = -1;
  float depthFade = 0;
};

/** HOW A SURFACE COMPOSITES, read back off the program as a renderer acts
 *  on it: the blend mode, whether it writes depth where it said, and its
 *  depth fade. */
struct Compositing {
  BlendMode blend = BlendMode::Normal;
  std::optional<bool> depthWrite;
  float depthFade = 0;
  bool operator==(const Compositing&) const = default;
};

/** What @p material states about compositing: a surface program's own
 *  three fields, and the stock answer — normal, unstated, no fade — for
 *  any material that carries none of them. */
Compositing compositing(const Material& material);

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

/** The lobes @p parameters state by their numbers: a sheen colour other
 *  than black, an anisotropy or an iridescence other than zero, a specular
 *  strength other than one or a colour other than white, a transmission
 *  or a clear coat other than zero. A distortion needs its map, so it is
 *  stated by carrying it. */
Lobes lobes(const SurfaceParameters& parameters);

/** The lobes @p material carries: none for anything that is not a lit
 *  surface program. */
Lobes lobes(const Material& material);

/** The lit recipe for @p reflection carrying @p carried, defined once per
 *  model and set of lobes; it declares every map slot above, and its
 *  bodies sample only the slots of the lobes it carries. */
const std::shared_ptr<const Recipe>& surfaceRecipe(
    Reflection reflection = Reflection::SplitSum, Lobes carried = {});

/** A lit metallic-roughness surface: its base colour, emission, normal,
 *  roughness, metalness, occlusion and whichever optional lobes — a clear
 *  coat among them — its numbers state, handed to whatever renderer
 *  shades it. */
Material program(const SurfaceParameters& parameters = {},
                 Reflection reflection = Reflection::SplitSum);
/** …carrying @p carried as well as the lobes the numbers state: what a
 *  surface whose lobe lives in a map, with its factor left at the stock
 *  value, is built with. */
Material program(const SurfaceParameters& parameters, Reflection reflection,
                 Lobes carried);
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
 *  @p material are dropped and said once. */
Material lower(const Material& material);

}  // namespace sigil::material::surface
