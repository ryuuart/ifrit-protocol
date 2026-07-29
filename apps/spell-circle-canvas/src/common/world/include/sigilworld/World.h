#pragma once

/** @file
 * SigilWorld — diegetic surfaces in real 3D, rendered by Diligent
 * Engine. Where SigilShape's Space.h fakes depth inside one SkCanvas,
 * SigilWorld owns a GPU device (Vulkan via MoltenVK on macOS; the
 * engine layer is Diligent's, so D3D/GL land with the Windows/Linux
 * ports) and renders meshes with a depth buffer, MSAA, and a PBR-lite
 * shading model — panels IN a scene, not sprites OVER one.
 *
 * The bridge contracts:
 *  - geometry is sigil::shape::Mesh — extrude/revolve/grid/cylinderPanel
 *    output uploads directly;
 *  - panel content is any SkImage — a compose scene, a web view, an
 *    SVG — uploaded as the surface's baseColor texture (unlit for
 *    emissive screens, lit for print-like decals);
 *  - the camera is sigil::shape::space::Camera, so a Skia-composited
 *    scene and a World render agree about where things sit.
 *
 * Headless by design: create() needs no window, render() draws into an
 * offscreen target, readback() returns the frame as a raster SkImage
 * (savePng wraps it). A swapchain path can join later without touching
 * the scene API.
 */

#include <sigilshape/Mesh.h>
#include <sigilshape/Pop.h>
#include <sigilshape/Points.h>
#include <sigilshape/Space.h>

#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>

#include <entt/entity/fwd.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace sigil::world {

struct LightComponent; // Components.h

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
   *  shader shades in linear and runs its own LinearToSrgb() on the way
   *  to the plain RGBA8_UNORM target. The clear does not go through a
   *  shader — it writes the value straight into that target — so these
   *  components ARE the bytes the background pixel gets: the default
   *  reads back as (7, 8, 11), a near-black navy, not the (47, 48, 60)
   *  slate an encode would produce.
   *
   *  This is intentional and pinned by
   *  World.ClearColorIsEncodedSrgbNotLinear; see the 2026-07-28 note in
   *  world/README.md. Authoring a background is picking the pixel you
   *  want to see, which is what a display-space value means; matching a
   *  linear Material colour instead means encoding it yourself
   *  (shape::blend's linearToSrgb curve). */
  glm::vec4 clearColor = {0.028f, 0.03f, 0.045f, 1};
};

/** Surface shading. Textured surfaces multiply texture by baseColor;
 *  `unlit` skips lighting entirely (self-lit UI screens). Alpha below 1
 *  renders in the blended pass, depth-sorted back to front. */
struct Material {
  glm::vec4 baseColor = {0.8f, 0.8f, 0.8f, 1};
  float metallic = 0;
  float roughness = 0.5f;
  glm::vec4 emissive = {0, 0, 0, 1};
  float emissiveStrength = 0;
  sk_sp<SkImage> texture;
  bool unlit = false;

  /** UV window into the texture, applied at sample time:
   *  uv' = uv * uvScale + uvOffset. LIVE like the colors — animate
   *  uvOffset on the MaterialComponent to scroll content across a
   *  surface (the marquee riding a ribbon) with zero texture
   *  uploads. Sampling clamps at the texture edge. */
  SkV2 uvScale = {1, 1};
  SkV2 uvOffset = {0, 0};

  /** Textures compare by pointer — the scene reconciler's reuse test. */
  bool operator==(const Material &) const = default;
};

/** Per-instance lanes an instanced surface reads from its Cloud —
 *  mirrors points::InstanceOptions, but the stamp uploads ONCE and the
 *  points ride a per-instance vertex stream instead of a merged mesh.
 *  Lanes are optional; a bare Cloud stamps unscaled, untinted, in the
 *  stamp's own orientation. */
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

/** One sun + hemisphere ambient — enough light vocabulary for panels
 *  and props; HDRI environments join with the loader integration. */
struct Lighting {
  glm::vec3 sunDirection = {-0.45f, -0.75f, -0.5f}; ///< toward the scene
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
  static std::unique_ptr<World> create(const WorldConfig &config,
                                       std::string *error = nullptr);
  ~World();

  World(const World &) = delete;
  World &operator=(const World &) = delete;

  /** Add a surface: mesh + placement + material. Returns an id. */
  /** Add a surface: mesh + placement + material. The returned id IS an
   *  entt entity in registry() (see Components.h); 0 means failure. */
  uint32_t addSurface(const shape::Mesh &mesh, const glm::mat4 &model,
                      const Material &material);
  void setTransform(uint32_t id, const glm::mat4 &model);
  /** Replace a surface's geometry in place. Matching vertex/index
   *  counts update the GPU buffers directly (the towed-flag path: a
   *  ribbon window sliding along a spline keeps its topology and only
   *  moves its vertices); a different shape recreates them. The
   *  material, texture, and entity stay untouched. No-op on unknown
   *  ids; on an instanced flock this swaps the stamp. */
  void setSurfaceMesh(uint32_t id, const shape::Mesh &mesh);

  /** A GPU-computed ribbon sweep — the POP-style generator path: the
   *  loop's control points live in a GPU buffer and a compute pass
   *  re-sweeps the surface's vertices IN PLACE at render time,
   *  matching shape::Spline3's closed Catmull-Rom parameter-exactly
   *  (CPU queries like a dart's position(t) stay on the band) and the
   *  gravity rig (width hangs world-vertical off the tangent).
   *  Topology is fixed at creation; the window is two floats. */
  struct SweepDesc {
    std::vector<glm::vec3> loop; ///< closed Catmull-Rom control points
    float width = 100;
    int sections = 200; ///< ribbon cross-sections (fixed topology)
    float head = 1;     ///< window end, in loop parameter
    float span = 1;     ///< window length back from head
  };
  /** Add a compute-swept ribbon surface (an ordinary surface entity;
   *  transform/material behave as usual). 0 on failure. */
  uint32_t addSweep(const SweepDesc &desc, const Material &material);
  /** Slide a sweep's window: two floats in a constant buffer — the
   *  GPU re-sweep runs at the next render(). No-op on other ids. */
  void setSweepWindow(uint32_t id, float head, float span);

  /** A GPU point flock — POP phase 2: the points never exist on the
   *  CPU. A compute pass scatters `count` instances along a window of
   *  the loop (stable per-point radial offsets, optional drift noise,
   *  a tint ramp from tail to head) and packs the instanced draw
   *  stream directly. The window flows like the sweep's: slide head
   *  and the whole flock streams along the loop — a comet. */
  struct FlockDesc {
    std::vector<glm::vec3> loop; ///< closed Catmull-Rom control points
    int count = 10000;      ///< instances (fixed at creation)
    float head = 1;         ///< window end, in loop parameter
    float span = 1;         ///< window length back from head
    float radius = 30;      ///< scatter radius around the spline
    float scale = 1;        ///< base stamp scale (0.5-1.5x per point)
    float noiseAmplitude = 0;
    float noiseFrequency = 0.01f;
    float seed = 7;
    glm::vec4 tintTail = {1, 1, 1, 1}; ///< at the window's start
    glm::vec4 tintHead = {1, 1, 1, 1}; ///< at the window's end
  };
  /** Add a compute-generated flock drawing @p stamp at every point.
   *  An ordinary instanced surface entity otherwise. 0 on failure. */
  uint32_t addFlock(const shape::Mesh &stamp, const FlockDesc &desc,
                    const Material &material);
  /** Slide a flock's window; the GPU regenerates at next render(). */
  void setFlockWindow(uint32_t id, float head, float span);

  /** POP-style point combinators. The LANGUAGE (operator values,
   *  Chain, Lane) lives in SigilShape — <sigilshape/Pop.h> — because
   *  it is backend-neutral computational geometry with a CPU
   *  reference cook (shape::popops::cook -> Cloud, for the Skia
   *  painter path). SigilWorld is ONE executor of that language:
   *  addPoints() cooks the same Chain as compute dispatches over GPU
   *  attribute lanes with an implicit Copy-POP instancing sink, and
   *  the two implementations share formulas bit for bit. */
  using pop = shape::pop;

  /** Cook @p chain (first op must be a generator) and draw @p stamp
   *  at every point. An ordinary instanced surface otherwise. */
  uint32_t addPoints(const shape::Mesh &stamp, const pop::Chain &chain,
                     const Material &material);
  /** Replace the chain and re-cook at the next render(). A changed
   *  point count recreates the lanes. No-op on other ids. */
  void setPoints(uint32_t id, const pop::Chain &chain);
  /** The ANIMATION verb: slide the leading scatter's window without
   *  re-describing the chain — the pop sibling of setSweepWindow and
   *  setFlockWindow, so a marching comet is two floats per frame and
   *  no chain value kept at the call site. Param-only (buffers and
   *  bindings stay); downstream addPointsOn surfaces re-cook the same
   *  frame. No-op unless @p id is a pop surface led by a
   *  SplineScatter. */
  void setPointsWindow(uint32_t id, float head, float span);
  /** COMPOSE ON DEVICE: cook @p chain with its generator's loop taken
   *  from @p upstream's cooked P lane — pops feed pops with no CPU
   *  round trip; the arena reads the arena. The chain's leading
   *  SplineScatter's own loop points are ignored (its count/window/
   *  spread/seed still apply); the upstream re-cooking re-cooks this
   *  surface too, same frame, in dependency order. Semantically
   *  identical to the CPU composing entry pop::on(chain). 0 on
   *  failure (needs a valid pop upstream with >= 3 points). */
  uint32_t addPointsOn(uint32_t upstream, const shape::Mesh &stamp,
                       const pop::Chain &chain,
                       const Material &material);

  /** THE QUERY DOOR: read a pop surface's cooked attribute lanes back
   *  from the GPU as a Cloud (positions + "t"/"dir"/"size"/"tint" —
   *  the same conventional lanes the CPU cook writes). Cook on the
   *  GPU, consume anywhere: the Skia painter, exports, analysis —
   *  no render required beyond the cook itself. Synchronous (copy +
   *  wait): fine for queries, not a per-frame hot path. Valid after
   *  a render() has cooked the chain; empty on other ids. */
  shape::Cloud readPoints(uint32_t id);
  void removeSurface(uint32_t id);
  size_t surfaceCount() const;

  /** One draw call stamping @p stamp at every point of @p cloud — the
   *  GPU-instanced sibling of points::instance() for the thousands
   *  range where a merged mesh wastes vertices. The flock is ONE
   *  surface: one id, one TransformComponent (the whole-flock
   *  transform), one MaterialComponent whose alpha routes the whole
   *  flock opaque or blended. 0 on failure; an empty cloud is a valid
   *  (invisible) flock awaiting setInstances(). */
  uint32_t addInstanced(const shape::Mesh &stamp, const shape::Cloud &cloud,
                        const Material &material,
                        const InstanceLanes &lanes = {});
  /** Re-upload an instanced surface's points (UpdateBuffer when the
   *  count is unchanged, recreate otherwise). No-op on plain
   *  surfaces. */
  void setInstances(uint32_t id, const shape::Cloud &cloud,
                    const InstanceLanes &lanes = {});

  /** The world's entity registry — surfaces live here as entities with
   *  TransformComponent + MaterialComponent (Components.h). Attach your
   *  own components and run your own systems over the same entities. */
  entt::registry &registry();
  const entt::registry &registry() const;

  /** Convenience: a fresh entity carrying @p light. The registry is
   *  the real API — mutate the LightComponent live, attach one to any
   *  entity yourself, destroy via registry().destroy(entity(id)).
   *  render() honors at most kLightBudget lights per frame. */
  uint32_t addLight(const LightComponent &light);

  /** The fallback camera. An entity with an ACTIVE CameraComponent
   *  (Components.h) takes precedence while it exists. */
  void setCamera(const shape::space::Camera &camera);
  void setLighting(const Lighting &lighting);

  /** Render the scene into the offscreen target. */
  bool render();
  /** The last rendered frame as a raster SkImage (RGBA, opaque). */
  sk_sp<SkImage> readback();
  /** render() must have run; encodes the readback as PNG. */
  bool savePng(const std::filesystem::path &path);

  const char *backendName() const;

private:
  World();
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

} // namespace sigil::world
