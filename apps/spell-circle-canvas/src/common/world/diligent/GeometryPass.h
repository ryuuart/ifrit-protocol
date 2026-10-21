#pragma once

/** @file
 * The pieces of the geometry pass more than one of its files reads: what
 * a body is drawn as and with, how it composites, the box it stands in,
 * the fold a kept map or tint is checked against, and the steps the pass
 * is made of — choosing a body's program, writing its uniforms, drawing
 * it, drawing the sky, drawing the frame's bodies phase by phase, and
 * fitting and drawing the shadow maps and the tint beside them.
 */

#include <include/core/SkBlendMode.h>
#include <sigilgeometry/mesh/Instances.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilworld/advanced/Light.h>
#include <sigilworld/advanced/View.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <glm/common.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

#include "Gpu.h"

namespace sigil::world::diligent {

// ---- the sky's slots --------------------------------------------------

/** The four panorama slots the scaffold declares, and the sampler they
 *  are read with. Named here because the binding has to tell them from
 *  a material's own slots: an equirect map wants a wrap of its own on
 *  each axis and reads linearly across its levels, and the one sampler
 *  every other slot in a draw shares can be neither. */
constexpr std::string_view kEnvironmentSlots[4] = {
    "uEnvironment", "uEnvironmentNext", "uIrradiance", "uIrradianceNext"};

/** The split sum's table, read with the same sampler: linear, and read
 *  only between its texel centres, so how it wraps never shows. */
constexpr std::string_view kBrdfSlot = "uEnvironmentBrdf";

/** Whether @p slot is one of the four panorama slots or the split sum's
 *  table, which are read through the sampler of their own. */
bool isEnvironmentSlot(std::string_view slot);

/** The split sum's table as a texture, made once for the process. */
const material::Texture& brdfTable();

// ---- what a body is drawn with and as ---------------------------------

/** What one body is drawn with: its program, the bytes its material
 *  resolved to, and whether the emitters reach it. */
struct Surface {
  const material::slang::Compiled* program = nullptr;
  material::Material::Resolved upload;
  const material::Recipe* recipe = nullptr;
  bool lit = true;
  /** Drawn as a stand-in while its own program compiles. */
  bool standIn = false;
};

/** HOW ONE BODY COMPOSITES: its blend, whether it writes depth, and
 *  whether it fades into the scene behind it. */
struct Blending {
  SkBlendMode blend = SkBlendMode::kSrcOver;
  bool depthWrite = true;
  bool fades = false;
};

/** What one body is drawn as: its mesh, the artefact number its buffers
 *  are held under, and the instances it is repeated at — null for one
 *  neutral instance — with whether each turns to the camera. */
struct Shape {
  uint64_t artefact = 0;
  const geometry::mesh::Mesh* mesh = nullptr;
  const geometry::mesh::Instances* instances = nullptr;
  Facing facing = Facing::None;
  /** THE ONE MATERIAL SLOT drawn, of a mesh whose faces wear several: only
   *  the triangles its "Material" lane names with it. Below zero draws
   *  every triangle. */
  int slot = -1;
  /** The primitive lane whose colour multiplies each triangle, or empty:
   *  part of how the buffers are packed, so every draw of one body names
   *  the same one. */
  std::string_view primitiveColor;
};

/** A BODY DRAWN INTO A SHADOW MAP: the light's projection, and whether
 *  the body's surface decides where it covers. */
struct Caster {
  glm::mat4 viewProjection{1.0f};
  bool coverage = false;
  /** Drawn into the tint beside the map rather than into its depth: the
   *  body's colour, the share of a light it lets through, laid over what
   *  is there by multiplying, behind nothing the map holds. */
  bool tint = false;
};

/** WHAT OF A LIGHT A TRANSMISSIVE SURFACE LETS THROUGH, as its tint reads
 *  it: its colour in linear light, the share it transmits, how deep it is,
 *  and its volume's colour and distance. The tint multiplies the first two
 *  numbers by their maps, dims the colour by the volume across the depth,
 *  and lets all of the share it does not transmit through, which its depth
 *  in the map already answers for. A number the material does not state
 *  reads as a clear pane's. */
struct TintTerms {
  glm::vec3 colour{1.0f};
  float weight = 1.0f;
  float thickness = 0.0f;
  float distance = 0.0f;
  glm::vec3 attenuation{1.0f};
};

/** An axis-aligned box in the world, empty until a point is added. */
struct Box {
  glm::vec3 low{std::numeric_limits<float>::max()};
  glm::vec3 high{std::numeric_limits<float>::lowest()};
  void add(const glm::vec3& p) {
    low = glm::min(low, p);
    high = glm::max(high, p);
  }
  void add(const Box& other) {
    if (other.empty()) return;
    add(other.low);
    add(other.high);
  }
  [[nodiscard]] bool empty() const { return low.x > high.x; }
  [[nodiscard]] glm::vec3 corner(int i) const {
    return {(i & 1) ? high.x : low.x, (i & 2) ? high.y : low.y,
            (i & 4) ? high.z : low.z};
  }
};

/** A FOLD OF BYTES INTO A NUMBER, one value at a time. */
struct Fold {
  uint64_t value = 1469598103934665603ull;
  void bytes(const void* data, size_t size) {
    const auto* at = static_cast<const unsigned char*>(data);
    for (size_t i = 0; i < size; ++i) {
      value ^= at[i];
      value *= 1099511628211ull;
    }
  }
  template <class T>
  void add(const T& thing) {
    bytes(&thing, sizeof thing);
  }
  template <class T>
  void all(const std::vector<T>& things) {
    add(things.size());
    if (!things.empty()) bytes(things.data(), things.size() * sizeof(T));
  }
};

// ---- choosing a body's program ---------------------------------------

/** Whether @p material's program for @p variant can be had without
 *  waiting, starting it on a worker when it cannot. */
bool programReady(const material::Material& material, uint32_t variant);

/** The program and the bytes a body is drawn with, or its stand-in while
 *  its own program compiles. */
Surface surfaceOf(const material::Material* material, bool lit, double seconds,
                  bool shadow = false, bool standIns = false,
                  uint32_t lights = 0u);

/** The kinds of light a lit program is specialised on, of the lights that
 *  reach a body. */
uint32_t lightVariant(const Gpu& gpu,
                      std::span<const PlacedLight* const> lights,
                      bool receives);

// ---- writing a body's uniforms ----------------------------------------

/** The material's resolved bytes, written where its program reads them. */
void writeMaterial(material::slang::Uniforms& uniforms, const Surface& surface);

/** What a draw's instances need of the scaffold. */
void writeInstancing(material::slang::Uniforms& uniforms, const glm::mat4& view,
                     Facing facing, const device::InstanceBuffers& instances);

/** Everything a draw's scaffold needs: placement, camera, lights, sky and
 *  shadows. */
void writeScaffold(material::slang::Uniforms& uniforms,
                   const material::slang::Compiled& program, const Gpu& gpu,
                   const glm::mat4& viewProj, const glm::mat4& view,
                   const glm::mat4& model, glm::vec4 baseColor,
                   std::span<const PlacedLight* const> lights,
                   const Environment& sky, int levels, bool lit,
                   bool depthReadable, bool receives);

// ---- how a body composites --------------------------------------------

/** How @p body composites, off its material or a flat colour. */
Blending blendingOf(const Draw& body, glm::vec4 colour, bool flat);

/** Whether @p body draws in the blended phase. */
bool blended(const Draw& body, const Blending& blending, bool flat);

/** Whether @p body's surface transmits or distorts. */
bool seesThrough(const Draw& body, bool flat);

/** How a body's pipeline differs from another built out of @p program. */
PipelineKey bodyKey(const material::slang::Compiled& program,
                    const Blending& blending, bool cull, const Caster* caster);

// ---- drawing ------------------------------------------------------------

/** One body, drawn into the picture or, with @p caster, into a shadow map
 *  or its tint. */
void drawBody(Gpu& gpu, const glm::mat4& viewProj, const glm::mat4& view,
              const Shape& shape, const glm::mat4& model, glm::vec4 baseColor,
              const material::Material* worn, const material::Texture* map,
              std::span<const PlacedLight* const> lights,
              const Environment& sky, bool lit, const Blending& blending,
              bool cull, dg::ITexture* sceneDepth,
              dg::ITexture* sceneColor = nullptr,
              const Caster* caster = nullptr, bool receives = true);

/** The sky, drawn over the whole target before every body in it. */
void drawBackdrop(Gpu& gpu, const View& view, const glm::mat4& projection,
                  const glm::mat4& viewMatrix);

/** The frame's bodies, or the ones @p only leaves, phase by phase. */
void drawBodies(Gpu& gpu, const View& view, const glm::mat4& viewProj,
                const glm::mat4& viewMatrix, const Collection* only, bool lit,
                const glm::vec4* flat, dg::ITexture* target,
                bool timed = false);

// ---- the shadow stage ---------------------------------------------------

/** Where @p body stands in the world, every instance included. */
Box boxOf(const Draw& body);

/** A directional light's maps, one per cascade its shadow states. */
std::vector<ShadowMap> fitCascades(const PlacedLight& light, const View& view,
                                   const Box& scene, int size);

/** A spot light's map. */
ShadowMap fitSpot(const PlacedLight& light, const Box& scene, int size);

/** A point light's six maps, one along each of @p axes each way. */
std::array<ShadowMap, 6> fitPoint(const PlacedLight& light, const Box& scene,
                                  int size, const glm::mat3& axes);

/** WHAT @p body IS WEARING, as a number that stays the same while its
 *  surface and its map are the same values as @p was holds and no texture
 *  pass painted a page it wears this frame, and changes otherwise; @p was
 *  is brought up to what it wears. A kept drawing folds this number
 *  rather than the surface's address, which a later frame may hand a
 *  surface saying something else. */
uint64_t wornBy(Gpu& gpu, Gpu::Worn& was, const Draw& body);

/** Every casting light's map drawn into the atlas; answers how many. */
int64_t drawShadows(Gpu& gpu, const View& view, const glm::mat4& cameraView);

/** What of a light a transmissive surface lets through. */
TintTerms tintTermsOf(const material::Material& worn);

/** Whether @p body tints a light passing through it. */
bool tints(const Draw& body);

/** The tint beside the shadow maps; answers how many maps' tints it drew. */
int64_t drawTints(Gpu& gpu, const View& view, const glm::mat4& cameraView,
                  int64_t mapsDrawn);

}  // namespace sigil::world::diligent
