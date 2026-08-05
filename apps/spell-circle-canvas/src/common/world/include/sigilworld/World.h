#pragma once

/** @file
 * SigilWorld — surfaces in real 3D on a real GPU, through Diligent
 * Engine (Vulkan, via MoltenVK on macOS). It owns the device and renders
 * meshes with a depth buffer, multisampling and a physically-based
 * shading pass.
 *
 * Three bridge contracts, and nothing else crosses:
 *  - geometry in is a sigil::shape::Mesh, uploaded as it stands;
 *  - panel content in is any SkImage, uploaded as the surface's
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

/** Surface shading. Textured surfaces multiply texture by baseColor;
 *  `unlit` skips lighting entirely, for self-lit screens. Alpha below 1
 *  routes the surface into the blended pass, sorted back to front by
 *  view depth.
 *
 *  Colours here are LINEAR (unlike WorldConfig::clearColor).
 *
 *  Every field is live on mutation of the entity's MaterialComponent
 *  EXCEPT @ref texture: the texture is uploaded when the surface is
 *  added, so swapping the pointer changes nothing. Remove the surface
 *  and add it again to change the image. */
struct Material {
  glm::vec4 baseColor = {0.8f, 0.8f, 0.8f, 1};
  float metallic = 0;
  float roughness = 0.5f;
  glm::vec4 emissive = {0, 0, 0, 1};
  float emissiveStrength = 0;
  sk_sp<SkImage> texture;
  bool unlit = false;

  /** UV window into the texture, applied at sample time:
   *  uv' = uv * uvScale + uvOffset. Live like the colours, so animating
   *  uvOffset on the MaterialComponent scrolls content across a surface
   *  with no texture uploads at all.
   *
   *  The sampler CLAMPS on both axes. There is no repeat or tile mode:
   *  a window that runs off the texture smears its edge texels rather
   *  than wrapping around. */
  SkV2 uvScale = {1, 1};
  SkV2 uvOffset = {0, 0};

  /** Textures compare by POINTER, so two identical images decoded
   *  separately are different materials. The scene reconciler tests reuse
   *  with this operator; share one sk_sp to keep a surface. */
  bool operator==(const Material&) const = default;
};

/** Per-instance lanes an instanced surface reads from its Cloud. The
 *  stamp mesh uploads ONCE and the points ride a per-instance vertex
 *  stream, rather than being merged into one large mesh. Every lane is
 *  optional: a bare Cloud stamps unscaled, untinted, in the stamp's own
 *  orientation. */
struct InstanceLanes {
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

  /** Add a surface: mesh + placement + material. The returned id IS an
   *  entt entity in registry() (see Components.h); 0 means failure. */
  uint32_t addSurface(const shape::Mesh& mesh, const glm::mat4& model,
                      const Material& material);
  void setTransform(uint32_t id, const glm::mat4& model);
  /** Replace a surface's geometry in place. Matching vertex and index
   *  counts update the GPU buffers directly, so geometry that keeps its
   *  topology and only moves its vertices costs no reallocation; a
   *  different shape recreates the buffers. The material, texture and
   *  entity survive either path. No-op on unknown ids; on an instanced
   *  surface this swaps the stamp. */
  void setSurfaceMesh(uint32_t id, const shape::Mesh& mesh);

  /** A GPU-computed ribbon sweep: the loop's control points live in a
   *  device buffer and a compute pass rewrites the surface's vertices IN
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
  /** Add a compute-swept ribbon surface (an ordinary surface entity;
   *  transform/material behave as usual). 0 on failure. */
  uint32_t addSweep(const SweepDesc& desc, const Material& material);
  /** Slide a sweep's window: two floats into a constant buffer, with the
   *  re-sweep dispatched at the next render(). No-op on other ids. */
  void setSweepWindow(uint32_t id, float head, float span);

  /** A GPU point flock: the points never exist on the CPU. A compute
   *  pass scatters `count` instances along a window of the loop — stable
   *  per-point radial offsets, optional drift noise, a tint ramp from
   *  tail to head — and packs the instanced draw stream directly. The
   *  window flows like the sweep's: slide the head and the whole flock
   *  streams along the loop. */
  struct FlockDesc {
    std::vector<glm::vec3> loop;  ///< closed Catmull-Rom control points
    int count = 10000;            ///< instances (fixed at creation)
    float head = 1;               ///< window end, in loop parameter
    float span = 1;               ///< window length back from head
    float radius = 30;            ///< scatter radius around the spline
    float scale = 1;              ///< base stamp scale (0.5-1.5x per point)
    float noiseAmplitude = 0;
    float noiseFrequency = 0.01f;
    float seed = 7;
    glm::vec4 tintTail = {1, 1, 1, 1};  ///< at the window's start
    glm::vec4 tintHead = {1, 1, 1, 1};  ///< at the window's end
  };
  /** Add a compute-generated flock drawing @p stamp at every point.
   *  An ordinary instanced surface entity otherwise. 0 on failure. */
  uint32_t addFlock(const shape::Mesh& stamp, const FlockDesc& desc,
                    const Material& material);
  /** Slide a flock's window; the GPU regenerates at next render(). */
  void setFlockWindow(uint32_t id, float head, float span);

  /** Point-operator combinators. The LANGUAGE — operator values, Chain,
   *  Lane — lives in SigilShape, because it is backend-neutral
   *  computational geometry with a CPU reference implementation there.
   *  SigilWorld is one EXECUTOR of that language: addPoints() cooks the
   *  same Chain as compute dispatches over GPU attribute lanes, and the
   *  two implementations share their formulas bit for bit.
   *
   *  A few operator kinds have no GPU counterpart. Every door here
   *  DECLINES a chain containing one — returning 0, or doing nothing —
   *  rather than dropping the operator and cooking something subtly
   *  wrong. Cook such a chain on the CPU instead. */
  using pop = shape::pop;

  /** Cook @p chain (its first op must be a generator) and draw @p stamp
   *  at every cooked point. An ordinary instanced surface otherwise.
   *  0 on failure, including a chain the GPU executor declines. */
  uint32_t addPoints(const shape::Mesh& stamp, const pop::Chain& chain,
                     const Material& material);
  /** Replace the chain and re-cook at the next render(). A changed point
   *  count, operator list, custom attribute set or lookup table rebuilds
   *  the lanes and bindings; anything else is a parameter edit. No-op on
   *  other ids, and on a chain the GPU executor declines — the surface
   *  keeps cooking the chain it already had. */
  void setPoints(uint32_t id, const pop::Chain& chain);
  /** Slide the leading scatter's window without re-describing the chain
   *  — the point-chain sibling of setSweepWindow and setFlockWindow, so
   *  animating costs two floats per frame and the call site need not
   *  hold the chain value. Parameters only: buffers and bindings stay
   *  put, and any surface built on this one with addPointsOn re-cooks in
   *  the same frame. No-op unless @p id is a point surface led by a
   *  SplineScatter. */
  void setPointsWindow(uint32_t id, float head, float span);
  /** COMPOSE ON DEVICE: cook @p chain with its generator's loop taken
   *  from @p upstream's cooked position lane, so one chain feeds another
   *  with no CPU round trip. The leading SplineScatter's own loop points
   *  are ignored; its count, window, spread and seed still apply. When
   *  the upstream re-cooks, this surface re-cooks in the same frame, in
   *  dependency order. Semantically identical to composing the two
   *  chains on the CPU. 0 on failure — it needs a valid upstream point
   *  surface with at least 3 points. */
  uint32_t addPointsOn(uint32_t upstream, const shape::Mesh& stamp,
                       const pop::Chain& chain, const Material& material);

  /** THE QUERY DOOR: read a point surface's cooked attribute lanes back
   *  from the GPU as a Cloud — positions plus the same conventional
   *  lanes ("t", "dir", "size", "tint") the CPU cook writes, plus any
   *  custom lanes the chain named. Cook on the GPU, consume anywhere.
   *
   *  Synchronous: it copies and waits for the device. Fine as a query,
   *  wrong as a per-frame path. Valid only after a render() has cooked
   *  the chain; empty on other ids. */
  shape::Cloud readPoints(uint32_t id);
  void removeSurface(uint32_t id);
  size_t surfaceCount() const;

  /** One draw call stamping @p stamp at every point of @p cloud, for the
   *  thousands range where merging the stamps into one mesh would waste
   *  vertices.
   *
   *  The whole flock is ONE surface: one id, one TransformComponent
   *  moving all of it together, one MaterialComponent whose alpha routes
   *  all of it opaque or blended. Instances are therefore not depth
   *  sorted against each other. 0 on failure; an empty cloud is a valid
   *  but invisible flock awaiting setInstances(). */
  uint32_t addInstanced(const shape::Mesh& stamp, const shape::Cloud& cloud,
                        const Material& material,
                        const InstanceLanes& lanes = {});
  /** Re-upload an instanced surface's points (UpdateBuffer when the
   *  count is unchanged, recreate otherwise). No-op on plain
   *  surfaces. */
  void setInstances(uint32_t id, const shape::Cloud& cloud,
                    const InstanceLanes& lanes = {});

  /** The world's entity registry — surfaces live here as entities with
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
