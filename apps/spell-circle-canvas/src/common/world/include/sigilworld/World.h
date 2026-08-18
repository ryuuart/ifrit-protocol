#pragma once

/** @file
 * SigilWorld — props in real 3D on a real GPU, through Diligent
 * Engine (Vulkan, via MoltenVK on macOS). It owns the device and renders
 * meshes with a depth buffer, multisampling and a physically-based
 * shading pass.
 *
 * Three bridge contracts, and nothing else crosses:
 *  - geometry in is a sigil::shape::Mesh, uploaded as it stands;
 *  - panel content in is any SkImage, uploaded as the prop's
 *    baseColor texture (mark the material unlit for a self-lit screen,
 *    leave it lit for a decal that should take the scene's light);
 *  - the camera is a sigil::shape::space::Camera, so a Skia-composited
 *    scene and a World render agree about where things sit.
 *
 * Headless by design: create() needs no window, render() draws into an
 * offscreen target, and readback() returns the frame as a raster SkImage
 * (savePng wraps it). There is no swapchain and no window handling.
 *
 * COLOUR SPACES ARE ASYMMETRIC HERE, deliberately. Material and light
 * colours are LINEAR — the shader shades in linear and encodes on the way
 * out. WorldConfig::clearColor is ENCODED sRGB. See its own comment
 * before authoring a background.
 */

#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>
#include <sigilshape/Mesh.h>
#include <sigilshape/Points.h>
#include <sigilshape/Pop.h>
#include <sigilshape/Space.h>

#include <cstdint>
#include <entt/entity/fwd.hpp>
#include <filesystem>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace sigil::world {

struct LightComponent;  // Components.h

struct WorldConfig {
  int width = 1280;
  int height = 720;
  enum class Backend : uint8_t { Auto, Vulkan } backend = Backend::Auto;
  int sampleCount = 4;  ///< MSAA; falls back to 1 when unsupported
  bool validation = false;

  /** Background colour, in **encoded sRGB** — the deliberate exception
   *  to the rest of this API.
   *
   *  Every other colour here (Material::baseColor and emissive, the
   *  Lighting sun/sky/ground, LightComponent colours) is LINEAR: the
   *  shader shades in linear and encodes on the way to the plain
   *  RGBA8_UNORM target. The clear goes through no shader at all — it
   *  writes this value straight into that target — so these components
   *  ARE the bytes the background pixel gets, times 255.
   *
   *  Read the wrong way round, this is a visibly wrong background rather
   *  than a subtle one: a mid-tone reads roughly a factor of two off in
   *  every channel. Authoring a background means picking the pixel you
   *  want to see, which is what a display-space value is. To match a
   *  linear Material colour instead, encode it yourself with the standard
   *  sRGB curve first. */
  glm::vec4 clearColor = {0.028f, 0.03f, 0.045f, 1};
};

/** WHERE on a prop something applies: a scalar over its surface, read
 *  per pixel from one of a few sources and then shaped — the same idea a
 *  pop mask is over points, with the sources a shaded surface has.
 *
 *  Sources: a constant; a channel of an image sampled at the prop's uv
 *  (through the mask's own uv window); a channel of the mesh's vertex
 *  colour lane (painted in Houdini or Blender, or a pop Color lane);
 *  SLOPE, the surface normal against an axis (moss on upward faces);
 *  HEIGHT, the world position against an axis (a tide line, dust on the
 *  top shelf). Shaping: `fit(low, high)` remaps the raw value onto 0..1
 *  and clamps (a slope or height mask is meaningless without it, so the
 *  factories take the range), `invert()` flips. The shader applies them
 *  in that order: source, fit, invert. */
struct Mask {
  enum class Source : uint8_t { Constant, Map, VertexColor, Slope, Height };
  Source source = Source::Constant;
  float value = 1;     ///< Source::Constant
  sk_sp<SkImage> map;  ///< Source::Map: sampled at uv * uvScale + uvOffset
  int channel = 0;     ///< Map and VertexColor: 0 red .. 3 alpha
  SkV2 uvScale = {1, 1};
  SkV2 uvOffset = {0, 0};
  bool tile = true;
  glm::vec3 axis = {
      0, 1,
      0};  ///< Slope: compared with the normal; Height: dotted with position
  float low = 0, high = 1;  ///< the raw range that maps onto 0..1
  bool inverted = false;

  static Mask constant(float v) {
    Mask m;
    m.value = v;
    return m;
  }
  static Mask fromMap(sk_sp<SkImage> image, int channel = 0) {
    Mask m;
    m.source = Source::Map;
    m.map = std::move(image);
    m.channel = channel;
    return m;
  }
  static Mask vertexColor(int channel = 0) {
    Mask m;
    m.source = Source::VertexColor;
    m.channel = channel;
    return m;
  }
  /** 1 where the normal points along @p up (dot = 1), falling to 0 as
   *  it turns away: raw = dot(N, up), fitted onto [low, high]. */
  static Mask slope(glm::vec3 up, float low = 0.5f, float high = 0.9f) {
    Mask m;
    m.source = Source::Slope;
    m.axis = up;
    m.low = low;
    m.high = high;
    return m;
  }
  /** raw = dot(worldPosition, axis), fitted onto [low, high]. */
  static Mask height(float low, float high, glm::vec3 axis = {0, 1, 0}) {
    Mask m;
    m.source = Source::Height;
    m.axis = axis;
    m.low = low;
    m.high = high;
    return m;
  }
  Mask fit(float lo, float hi) const {
    Mask m = *this;
    m.low = lo;
    m.high = hi;
    return m;
  }
  Mask invert() const {
    Mask m = *this;
    m.inverted = !m.inverted;
    return m;
  }
  Mask window(SkV2 scale, SkV2 offset = {0, 0}) const {
    Mask m = *this;
    m.uvScale = scale;
    m.uvOffset = offset;
    return m;
  }
  bool operator==(const Mask&) const = default;
};

/** How a layered material's parameters combine where its mask says. */
enum class Blend : uint8_t {
  Mix,  ///< every parameter lerps to the layer's by the mask
  Add,  ///< base colour and emission add (scaled by the mask); the rest lerp
  Multiply,  ///< base colour multiplies (toward the layer's, by the mask); the
             ///< rest lerp
};

/** Surface shading: a metallic-roughness material with the full
 *  texture set the authoring tools export — base colour, normal,
 *  roughness, metallic, occlusion and emissive maps, each optional and
 *  each multiplying (or perturbing) the scalar next to it. `unlit`
 *  skips lighting entirely, for self-lit screens. Alpha below 1 routes
 *  the surface into the blended pass, sorted back to front by view
 *  depth.
 *
 *  Colours here are LINEAR (unlike WorldConfig::clearColor). The base
 *  colour and emissive maps are read as sRGB-encoded images and
 *  linearized on sample; the normal, roughness, metallic and occlusion
 *  maps are read as plain data.
 *
 *  Every field is live on mutation of the entity's MaterialComponent.
 *  Swapping an image pointer rebinds that prop's textures at the next
 *  render — an upload, so not free per frame, but no re-add. */
struct Material {
  glm::vec4 baseColor = {0.8f, 0.8f, 0.8f, 1};
  float metallic = 0;
  float roughness = 0.5f;
  glm::vec4 emissive = {0, 0, 0, 1};
  float emissiveStrength = 0;
  /** Base colour: multiplied by baseColor (and a mesh's colour lane). */
  sk_sp<SkImage> texture;
  bool unlit = false;

  /** Tangent-space normal map. Green is +Y "up the image" in the
   *  OpenGL convention (the default); set `normalMapDirectX` for a map
   *  authored with green pointing down. No vertex tangents are needed:
   *  the tangent frame is derived per pixel from the surface's position
   *  and uv derivatives. `normalScale` scales the perturbation (0 flat,
   *  1 as authored). */
  sk_sp<SkImage> normalMap;
  float normalScale = 1;
  bool normalMapDirectX = false;
  /** Roughness, metallic and occlusion maps: ONE channel each, chosen
   *  by the matching `*Channel` (0 red .. 3 alpha), so a packed
   *  occlusion-roughness-metallic image can be assigned to all three
   *  slots with channels 0, 1, 2. The value multiplies `roughness` /
   *  `metallic`; occlusion darkens the ambient term by
   *  `occlusionStrength` (0 ignores the map). A missing map reads as
   *  1 everywhere. */
  sk_sp<SkImage> roughnessMap;
  int roughnessChannel = 0;
  sk_sp<SkImage> metallicMap;
  int metallicChannel = 0;
  sk_sp<SkImage> occlusionMap;
  int occlusionChannel = 0;
  float occlusionStrength = 1;
  /** Emissive map, multiplied by emissive * emissiveStrength. */
  sk_sp<SkImage> emissiveMap;
  /** Opacity map: ONE channel (`opacityChannel`, 0 red .. 3 alpha)
   *  multiplied into the surface's alpha with `baseColor.a` and the
   *  base texture's alpha. Any material carrying one routes into the
   *  blended pass. `alphaCutoff` above 0 turns opacity into a cutout:
   *  fragments below the cutoff are discarded rather than blended
   *  (glTF's MASK mode; leaves, grilles, decals). */
  sk_sp<SkImage> opacityMap;
  int opacityChannel = 0;
  float alphaCutoff = 0;
  /** GLASS. `transmission` is how much of what lies behind the surface
   *  shows through it — 0 an ordinary surface, 1 clear glass — sampled
   *  from the frame's opaque pass through a refracted look (screen-space:
   *  the view ray bent by `ior` and pushed `thickness` world units into
   *  the surface decides where behind it is read), tinted by baseColor
   *  and blurred by roughness. Specular light stays on top, so a rough
   *  glass frosts and a smooth one shines. Any transmission above 0
   *  routes into the blended pass and writes an opaque pixel (it has
   *  composed its own background). Only OPAQUE surfaces are seen through
   *  glass: glass behind glass shows the opaque scene, not the nearer
   *  pane. */
  float transmission = 0;
  float ior = 1.5f;
  float thickness = 40;

  /** UV window into the textures, applied at sample time:
   *  uv' = uv * uvScale + uvOffset. Live like the colours, so animating
   *  uvOffset on the MaterialComponent scrolls content across a surface
   *  with no texture uploads at all.
   *
   *  By default the sampler CLAMPS on both axes: a window that runs off
   *  the texture smears its edge texels. `tile` switches every map on
   *  the material to REPEAT, which is what a scanned material set
   *  wants — uvScale {4, 4} then lays the set down four times across
   *  the surface. */
  SkV2 uvScale = {1, 1};
  SkV2 uvOffset = {0, 0};
  bool tile = false;

  /** LAYERS. A material can carry others on top of it, each applied
   *  where its Mask says with a Blend: the material a pixel is shaded
   *  with is the base's parameters, then each layer's blended in, then
   *  ONE shading pass. `over()` appends a layer and returns the result,
   *  so composition reads top-down: `steel.over(rust, Mask::fromMap(ao)
   *  .invert()).over(moss, Mask::slope({0, 1, 0}))`. Up to kMaxLayers
   *  are evaluated live on the GPU; further layers are ignored (a
   *  warning once). A layer's own layers are flattened onto it — a
   *  layer is one material, not a tree. */
  struct Layer;
  std::vector<Layer> layers;
  static constexpr int kMaxLayers = 3;
  Material over(Material top, Mask mask, Blend blend = Blend::Mix) const;
  /** This material's own parameters, without its layers. */
  Material flat() const;

  /** Textures compare by POINTER, so two identical images decoded
   *  separately are different materials. The scene reconciler tests reuse
   *  with this operator; share one sk_sp to keep a prop. */
  bool operator==(const Material&) const;

  /** Whether the prop draws in the blended pass: alpha below 1, an
   *  opacity map, or any transmission — on the base or on any layer. */
  bool blended() const;
};

struct Material::Layer {
  Material material;
  Mask mask;
  Blend blend = Blend::Mix;
  bool operator==(const Layer&) const = default;
};

inline bool Material::operator==(const Material& o) const {
  return baseColor == o.baseColor && metallic == o.metallic &&
         roughness == o.roughness && emissive == o.emissive &&
         emissiveStrength == o.emissiveStrength && texture == o.texture &&
         unlit == o.unlit && normalMap == o.normalMap &&
         normalScale == o.normalScale &&
         normalMapDirectX == o.normalMapDirectX &&
         roughnessMap == o.roughnessMap &&
         roughnessChannel == o.roughnessChannel &&
         metallicMap == o.metallicMap && metallicChannel == o.metallicChannel &&
         occlusionMap == o.occlusionMap &&
         occlusionChannel == o.occlusionChannel &&
         occlusionStrength == o.occlusionStrength &&
         emissiveMap == o.emissiveMap && opacityMap == o.opacityMap &&
         opacityChannel == o.opacityChannel && alphaCutoff == o.alphaCutoff &&
         transmission == o.transmission && ior == o.ior &&
         thickness == o.thickness && uvScale == o.uvScale &&
         uvOffset == o.uvOffset && tile == o.tile && layers == o.layers;
}

inline Material Material::flat() const {
  Material m = *this;
  m.layers.clear();
  return m;
}

inline Material Material::over(Material top, Mask mask, Blend blend) const {
  Material m = *this;
  Layer layer;
  layer.material = top.flat();  // a layer is one material, not a tree
  layer.mask = std::move(mask);
  layer.blend = blend;
  m.layers.push_back(std::move(layer));
  return m;
}

inline bool Material::blended() const {
  const auto one = [](const Material& m) {
    return m.baseColor.w < 1.0f || m.opacityMap != nullptr ||
           m.transmission > 0.0f;
  };
  if (one(*this)) return true;
  for (const Layer& layer : layers)
    if (one(layer.material)) return true;
  return false;
}

/** Per-stamp lanes a stamps prop reads from its Cloud. The stamp mesh
 *  uploads ONCE and the points ride a per-instance vertex stream, rather
 *  than being merged into one large mesh. Every lane is optional: a bare
 *  Cloud stamps unscaled, untinted, in the stamp's own orientation. */
struct StampLanes {
  float scale = 1;
  /** Scalar lane multiplied into scale per point (e.g. "size"). */
  std::string scaleLane;
  /** Color lane multiplied into baseColor per point (e.g. "tint"). */
  std::string tintLane;
  /** Vector lane orienting the stamp's +z (e.g. "normal"); empty =
   *  keep the stamp's own orientation. */
  std::string orientLane;
  glm::vec3 up = {0, 1, 0};
};

/** The scene-wide light: one sun plus a hemisphere ambient. Colours are
 *  LINEAR. Per-entity LightComponents add to this. */
struct Lighting {
  glm::vec3 sunDirection = {-0.45f, -0.75f, -0.5f};  ///< toward the scene
  glm::vec4 sunColor = {1.0f, 0.96f, 0.9f, 1};
  float sunIntensity = 2.6f;
  glm::vec4 skyColor = {0.35f, 0.45f, 0.65f, 1};
  glm::vec4 groundColor = {0.10f, 0.09f, 0.11f, 1};
  float ambient = 0.55f;

  /** Image-based light: an equirectangular panorama lighting every lit
   *  surface — a diffuse term read along the normal from the panorama's
   *  blurriest levels, a specular term read along the reflection at a
   *  roughness-chosen blur, weighted by the split-sum environment BRDF.
   *  When set it REPLACES the sky/ground hemisphere; `ambient` still
   *  scales it. Float images keep their range (uploaded as half float
   *  with a full mip chain); 8-bit images work and simply have none.
   *  Compared by pointer, like material images: setting the same
   *  sk_sp again uploads nothing. u = 0.5 of the image faces -Z; the
   *  rotation turns the panorama about +Y. */
  sk_sp<SkImage> environment;
  float environmentIntensity = 1;
  float environmentRotationDeg = 0;
};

class World {
 public:
  /** Bring up the device and offscreen targets. Returns null (and
   *  fills @p error) when no backend can initialize — no Vulkan
   *  runtime, for instance. */
  static std::unique_ptr<World> create(const WorldConfig& config,
                                       std::string* error = nullptr);
  ~World();

  World(const World&) = delete;
  World& operator=(const World&) = delete;

  /** Place a PROP: mesh + placement + material. The returned id IS an
   *  entt entity in registry() (see Components.h); 0 means failure. Every
   *  place* door below returns the same kind of id: a prop is a prop
   *  whether its geometry was uploaded, stamped or cooked. */
  uint32_t place(const shape::Mesh& mesh, const glm::mat4& model,
                 const Material& material);
  /** Place a prop with SEVERAL materials — Blender's material slots: the
   *  mesh's "Material" prim lane (its .x, per triangle) picks which of
   *  @p slots each triangle wears, clamped into range; a mesh without
   *  the lane wears slot 0 throughout. One entity, one transform, one
   *  draw per slot. Slot 0 lands on MaterialComponent::material, the
   *  rest on MaterialComponent::slots. 0 on failure or empty slots. */
  uint32_t place(const shape::Mesh& mesh, const glm::mat4& model,
                 const std::vector<Material>& slots);
  void setTransform(uint32_t id, const glm::mat4& model);
  /** Replace a prop's geometry in place. Matching vertex and index
   *  counts update the GPU buffers directly, so geometry that keeps its
   *  topology and only moves its vertices costs no reallocation; a
   *  different shape recreates the buffers. The material, texture and
   *  entity survive either path. No-op on unknown ids; on a stamps or
   *  chain prop this swaps the stamp. */
  void setMesh(uint32_t id, const shape::Mesh& mesh);

  /** A GPU-computed ribbon sweep: the loop's control points live in a
   *  device buffer and a compute pass rewrites the prop's vertices IN
   *  PLACE at render time, so no CPU mesh for the ribbon exists at all.
   *
   *  The kernel matches shape::Spline3's closed Catmull-Rom parameter for
   *  parameter, so a CPU query of position(t) on the same loop lands on
   *  the band. Width hangs world-vertical off the tangent.
   *
   *  Topology is fixed at creation; the animation is the two-float
   *  window. */
  struct SweepDesc {
    std::vector<glm::vec3> loop;  ///< closed Catmull-Rom control points
    float width = 100;
    int sections = 200;  ///< ribbon cross-sections (fixed topology)
    float head = 1;      ///< window end, in loop parameter
    float span = 1;      ///< window length back from head
  };
  /** Place a compute-swept ribbon (an ordinary prop entity;
   *  transform/material behave as usual). 0 on failure. */
  uint32_t placeSweep(const SweepDesc& desc, const Material& material);
  /** Slide a sweep's window: two floats into a constant buffer, with the
   *  re-sweep dispatched at the next render(). No-op on other ids. */
  void setSweepWindow(uint32_t id, float head, float span);

  /** Point-operator combinators. The LANGUAGE — operator values, Chain,
   *  Lane — lives in SigilShape, because it is backend-neutral
   *  computational geometry with a CPU reference implementation there.
   *  SigilWorld is one EXECUTOR of that language: placeChain() cooks the
   *  same Chain as compute dispatches over GPU attribute lanes, and the
   *  two implementations share their formulas bit for bit.
   *
   *  A few operator kinds have no GPU counterpart. Every door here
   *  DECLINES a chain containing one — returning 0, or doing nothing —
   *  rather than dropping the operator and cooking something subtly
   *  wrong. Cook such a chain on the CPU instead. */
  using pop = shape::pop;

  /** Cook @p chain (its first op must be a generator) and draw @p stamp
   *  at every cooked point. An ordinary stamps prop otherwise.
   *  0 on failure, including a chain the GPU executor declines. */
  uint32_t placeChain(const shape::Mesh& stamp, const pop::Chain& chain,
                      const Material& material);
  /** Replace the chain and re-cook at the next render(). A changed point
   *  count, operator list, custom attribute set or lookup table rebuilds
   *  the lanes and bindings; anything else is a parameter edit. No-op on
   *  other ids, and on a chain the GPU executor declines — the prop
   *  keeps cooking the chain it already had. */
  void setChain(uint32_t id, const pop::Chain& chain);
  /** Slide the leading scatter's window without re-describing the chain
   *  — the point-chain sibling of setSweepWindow, so
   *  animating costs two floats per frame and the call site need not
   *  hold the chain value. Parameters only: buffers and bindings stay
   *  put, and any prop built on this one with placeChainOn re-cooks in
   *  the same frame. No-op unless @p id is a chain prop led by a
   *  SplineScatter. */
  void setChainWindow(uint32_t id, float head, float span);
  /** COMPOSE ON DEVICE: cook @p chain with its generator's loop taken
   *  from @p upstream's cooked position lane, so one chain feeds another
   *  with no CPU round trip. The leading SplineScatter's own loop points
   *  are ignored; its count, window, spread and seed still apply. When
   *  the upstream re-cooks, this prop re-cooks in the same frame, in
   *  dependency order. Semantically identical to composing the two
   *  chains on the CPU. 0 on failure — it needs a valid upstream point
   *  chain prop with at least 3 points. */
  uint32_t placeChainOn(uint32_t upstream, const shape::Mesh& stamp,
                        const pop::Chain& chain, const Material& material);

  /** THE QUERY DOOR: read a chain prop's cooked attribute lanes back
   *  from the GPU as a Cloud — positions plus the same conventional
   *  lanes ("t", "dir", "size", "tint") the CPU cook writes, plus any
   *  custom lanes the chain named. Cook on the GPU, consume anywhere.
   *
   *  Synchronous: it copies and waits for the device. Fine as a query,
   *  wrong as a per-frame path. Valid only after a render() has cooked
   *  the chain; empty on other ids. */
  shape::Cloud readChain(uint32_t id);
  void remove(uint32_t id);
  size_t propCount() const;

  /** One draw call stamping @p stamp at every point of @p cloud, for the
   *  thousands range where merging the stamps into one mesh would waste
   *  vertices.
   *
   *  All the stamps are ONE prop: one id, one TransformComponent
   *  moving all of it together, one MaterialComponent whose alpha routes
   *  all of it opaque or blended. Instances are therefore not depth
   *  sorted against each other. 0 on failure; an empty cloud is a valid
   *  but invisible prop awaiting setStamps(). */
  uint32_t placeStamps(const shape::Mesh& stamp, const shape::Cloud& cloud,
                       const Material& material, const StampLanes& lanes = {});
  /** Re-upload a stamps prop's points (UpdateBuffer when the
   *  count is unchanged, recreate otherwise). No-op on plain
   *  props. */
  void setStamps(uint32_t id, const shape::Cloud& cloud,
                 const StampLanes& lanes = {});

  /** The world's entity registry — props live here as entities with
   *  TransformComponent + MaterialComponent (Components.h). Attach your
   *  own components and run your own systems over the same entities. */
  entt::registry& registry();
  const entt::registry& registry() const;

  /** Convenience: a fresh entity carrying @p light. The registry is the
   *  real API — mutate the LightComponent live, attach one to any entity
   *  yourself, destroy via registry().destroy(entity(id)).
   *
   *  render() takes at most kLightBudget lights per frame, in registry
   *  iteration order, and silently ignores the rest. */
  uint32_t addLight(const LightComponent& light);

  /** The FALLBACK camera. An entity carrying an active CameraComponent
   *  (Components.h) outranks this one while it exists — including one
   *  activated before a later call to this setter. With several active,
   *  the first the registry iterates wins. */
  void setCamera(const shape::space::Camera& camera);
  void setLighting(const Lighting& lighting);

  /** Render the scene into the offscreen target.
   *
   *  Reads no clock: the frame is a pure function of what the registry
   *  and any bound animation Outputs hold when it is called, so the same
   *  state renders the same pixels. It does resolve the Animated*
   *  components first (Animation.h), so a caller stepping a ticker need
   *  not call the resolver itself. */
  bool render();
  /** The last rendered frame as a raster SkImage (RGBA, opaque). */
  sk_sp<SkImage> readback();
  /** render() must have run; encodes the readback as PNG. */
  bool savePng(const std::filesystem::path& path);

  const char* backendName() const;

 private:
  World();
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

}  // namespace sigil::world
