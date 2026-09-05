#pragma once

/** @file
 * What the GPU executor holds between one pass and the next: the frame's
 * resources as device textures, the pipelines built from compiled
 * programs, and the device's own shared resources and residencies — the
 * uniform buffer every draw is written into, and the meshes and maps a
 * view names, which the device feature puts there and this one only asks
 * for.
 *
 * Diligent's types appear here and in this feature's sources alone; the
 * public header names none of them.
 */

#include <Graphics/GraphicsEngine/interface/Buffer.h>
#include <Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <Graphics/GraphicsEngine/interface/PipelineState.h>
#include <Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <Graphics/GraphicsEngine/interface/Texture.h>
#include <include/core/SkImage.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkSize.h>
#include <sigilgeometry/device/Device.h>
#include <sigilgeometry/device/Meshes.h>
#include <sigilgeometry/device/Resources.h>
#include <sigilgeometry/device/Textures.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilmaterial/texture/EnvironmentMap.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilworld/frame/Pass.h>
#include <sigilworld/frame/Targets.h>
#include <sigilworld/frame/View.h>

#include <Common/interface/RefCntAutoPtr.hpp>
#include <boost/container/map.hpp>
#include <cstddef>
#include <cstdint>
#include <glm/mat4x4.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "Programs.h"

namespace sigil::world::diligent {

/** How many emitters one draw carries: the array every program here
 *  declares, the same number for the scaffold and the mesh painter, so a
 *  mesh drawn through the painter and the same mesh drawn as a body are
 *  lit by the same emitters and the host tier honours the same count. A
 *  description naming more is drawn with the first eight. */
constexpr size_t kLights = 8;

// The device every executor here stands on is SigilGeometry's — it is
// the one point in the tree that can create a Diligent device — and this
// is the name it is spelled by in this feature's own sources.
using ::sigil::geometry::device::Device;

namespace dg = Diligent;

/** The formats every target here holds are the device's own — one format
 *  for every resource is what lets two resources whose lives do not
 *  overlap be handed one texture, exactly as the ordering hands two
 *  names one surface. */
using ::sigil::geometry::device::kColorFormat;
using ::sigil::geometry::device::kDepthFormat;

/** ONE NAMED IMAGE on the device: what this frame wrote, and what the
 *  frame before it wrote. `previous()` is answered from the second, and
 *  the two are exchanged when the frame closes. */
struct DeviceImage {
  dg::RefCntAutoPtr<dg::ITexture> current;
  dg::RefCntAutoPtr<dg::ITexture> previous;
  /** Something has painted `current` since the frame opened. */
  bool written = false;
};

/** What a mesh is once it stands on the device. The residency that puts
 *  it there is the device feature's, and this is the name a draw here
 *  reads it by. */
using ::sigil::geometry::device::MeshBuffers;

/** HOW A PIPELINE DIFFERS from another built out of the same program:
 *  how it blends, and whether it writes depth. Two draws that agree on
 *  both share one pipeline. */
struct PipelineKey {
  const material::slang::Compiled* program = nullptr;
  /** kSrcOver for a body, kPlus for a composite that adds, and kSrc for
   *  a draw that replaces what stands. */
  SkBlendMode blend = SkBlendMode::kSrcOver;
  bool depth = false;
  bool depthWrite = false;
  /** No vertex layout and no index buffer: a triangle covering the
   *  target, which is what every post stage draws. */
  bool fullscreen = false;
  /** Does the vertex layout declare the PRIMITIVE lane? Every vertex
   *  carries one either way — it is the same buffer — but a program that
   *  does not read it is not given an attribute it never declared. */
  bool prim = false;
  /** Are back faces dropped? A draw the caller asked to keep them for
   *  is a different pipeline and not a different program. */
  bool cull = true;
  auto operator<=>(const PipelineKey&) const = default;
};

/** A PIPELINE AND ITS BINDING, made once per key. */
struct Pipeline {
  dg::RefCntAutoPtr<dg::IPipelineState> state;
  dg::RefCntAutoPtr<dg::IShaderResourceBinding> binding;
};

/** THE EXECUTOR'S STATE, shared by every copy of the runtime value one
 *  call made. */
struct Gpu {
  explicit Gpu(Device& d) : device(&d), shared(d), meshes(d), maps(d) {}
  ~Gpu();

  Device* device = nullptr;
  /** What every executor on this device stands on — the uniform buffer,
   *  the samplers, the white texel and the readback — which this one
   *  holds rather than owns a second copy of. */
  ::sigil::geometry::device::Resources shared;
  /** …and, on the same terms, the meshes and the maps standing on that
   *  device. A frame names what it wants drawn; putting it there is the
   *  device's own business and is not spelled again here. */
  ::sigil::geometry::device::MeshResidency meshes;
  ::sigil::geometry::device::TextureResidency maps;
  SkISize extent{0, 0};

  boost::container::map<std::string, DeviceImage, std::less<>> images;
  dg::RefCntAutoPtr<dg::ITexture> depth;
  /** TARGETS NO RESOURCE NAMES, made on the first ask and kept for the
   *  extent's life. A device cannot sample an image it is drawing into,
   *  so every stage that reads and writes at once — one direction of a
   *  blur, a masked op, a pass that writes what it reads — takes one of
   *  these, and they are addressed by index so that two such stages in
   *  one pass cannot be handed the same one. */
  std::vector<dg::RefCntAutoPtr<dg::ITexture>> scratch;
  boost::container::map<PipelineKey, Pipeline> pipelines;

  // ---- what the whole of it is made of (Gpu.cpp) ----
  /** Sizes the frame's targets to @p size, dropping everything made at
   *  another one. */
  void resize(SkISize size);
  /** The texture @p name is written into this frame, made on the first
   *  ask. Null when there is no extent to make one at. */
  dg::ITexture* target(std::string_view name);
  /** Working target @p index, made on the first ask. */
  dg::ITexture* working(size_t index);
  /** @p name as it stands, or as it stood at the end of the frame
   *  before; null when nothing has written it. */
  dg::ITexture* current(std::string_view name);
  dg::ITexture* previous(std::string_view name);
  /** The pipeline for @p key, built on the first ask. Null when the
   *  program is empty or the device refused it. */
  const Pipeline* pipeline(const PipelineKey& key);
  /** Opens a frame: what the frame before wrote becomes what this one's
   *  `previous()` names, and the texture that held the frame before THAT
   *  is what this one writes into — so a resource costs two textures for
   *  its whole life, no copy, and no allocation per frame. */
  void beginFrame();
  /** Closes it: the device's frame is finished and the residencies let
   *  go of what no view has named lately. */
  void endFrame();
  /** @p name's pixels, read back through a staging texture. Null when
   *  nothing has written it. */
  sk_sp<SkImage> read(std::string_view name);

  /** A texture of this frame's size and format. */
  dg::RefCntAutoPtr<dg::ITexture> makeColor(const char* label);
};

/** Binds @p pipeline's uniform buffer to @p values and its sampled slots
 *  to @p textures, in the program's declared order, read through
 *  @p filter, then commits. A slot with no texture reads the one white
 *  texel.
 *
 *  @p panoramaSlot names the slots that are read as an equirect map
 *  instead: one wrap on each axis, and linearly across the prefiltered
 *  levels. Every other slot in a draw shares one filter and one wrap,
 *  which is what a base-colour map's sampling decides for all of them. */
void bindAndCommit(Gpu& gpu, const Pipeline& pipeline,
                   const material::slang::Compiled& program,
                   const material::slang::Uniforms& values,
                   const std::vector<dg::ITexture*>& textures,
                   SkFilterMode filter = SkFilterMode::kLinear,
                   bool tile = false,
                   bool (*panoramaSlot)(std::string_view) = nullptr);

/** THE FRAME STATE every runtime here stands on, over the device's own
 *  resources. Every runtime makes one of these and shares it among the
 *  copies of the value it hands back. */
std::shared_ptr<Gpu> makeGpu(Device& device);

/** Binds @p colour as a stage's target, with the depth buffer when one
 *  is wanted, and clears both. */
void openTarget(Gpu& gpu, dg::ITexture* colour, const float* clear,
                bool withDepth);

/** A texture's placement as a shader reads it: the same matrix a host
 *  tier puts on its style, in the four-by-four the uniform is. */
glm::mat4 mapMatrix(const SkMatrix& uv);

/** The two stages of a frame, one file each. */
void paintGeometry(Gpu& gpu, const PassWork& work, const View& view,
                   Targets& targets);
void applyPost(Gpu& gpu, const PassWork& work);

}  // namespace sigil::world::diligent
