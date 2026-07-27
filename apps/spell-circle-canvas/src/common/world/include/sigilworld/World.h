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
#include <sigilshape/Points.h>
#include <sigilshape/Space.h>

#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>

#include <entt/entity/fwd.hpp>

#include <filesystem>
#include <memory>
#include <string>

namespace sigil::world {

struct LightComponent; // Components.h

struct WorldConfig {
  int width = 1280;
  int height = 720;
  enum class Backend : uint8_t { Auto, Vulkan } backend = Backend::Auto;
  int sampleCount = 4;  ///< MSAA; falls back to 1 when unsupported
  bool validation = false;
  SkColor4f clearColor = {0.028f, 0.03f, 0.045f, 1};
};

/** Surface shading. Textured surfaces multiply texture by baseColor;
 *  `unlit` skips lighting entirely (self-lit UI screens). Alpha below 1
 *  renders in the blended pass, depth-sorted back to front. */
struct Material {
  SkColor4f baseColor = {0.8f, 0.8f, 0.8f, 1};
  float metallic = 0;
  float roughness = 0.5f;
  SkColor4f emissive = {0, 0, 0, 1};
  float emissiveStrength = 0;
  sk_sp<SkImage> texture;
  bool unlit = false;

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
  SkV3 up = {0, 1, 0};
};

/** One sun + hemisphere ambient — enough light vocabulary for panels
 *  and props; HDRI environments join with the loader integration. */
struct Lighting {
  SkV3 sunDirection = {-0.45f, -0.75f, -0.5f}; ///< toward the scene
  SkColor4f sunColor = {1.0f, 0.96f, 0.9f, 1};
  float sunIntensity = 2.6f;
  SkColor4f skyColor = {0.35f, 0.45f, 0.65f, 1};
  SkColor4f groundColor = {0.10f, 0.09f, 0.11f, 1};
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
  uint32_t addSurface(const shape::Mesh &mesh, const SkM44 &model,
                      const Material &material);
  void setTransform(uint32_t id, const SkM44 &model);
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
