#pragma once

/** @file
 * What the GPU executor holds between one pass and the next: the frame's
 * resources as device textures, the meshes uploaded from the extracted
 * view, the pipelines built from compiled programs, and the one buffer
 * every draw's uniforms are written into.
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
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilmaterial/texture/EnvironmentMap.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilgeometry/device/Device.h>
#include <sigilgeometry/device/Resources.h>
#include <sigilworld/frame/Pass.h>
#include <sigilworld/frame/Targets.h>
#include <sigilworld/frame/View.h>

#include <Common/interface/RefCntAutoPtr.hpp>
#include <Graphics/GraphicsTools/interface/DynamicBuffer.hpp>
#include <cstddef>
#include <cstdint>
#include <glm/mat4x4.hpp>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "Programs.h"

namespace sigil::world::diligent {

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

/** A MAP ON THE DEVICE: either an image uploaded from host memory, or a
 *  texture someone else painted on THIS device, wrapped without a copy.
 *  `used` is the frame it was last drawn with, so a map no view names
 *  any more is let go. */
struct SampledImage {
  dg::RefCntAutoPtr<dg::ITexture> texture;
  uint64_t used = 0;
};

/** A MESH UPLOADED, held under the number the frame gave the artefact it
 *  came from. NOT under its address: an artefact that is dropped frees
 *  its memory and the next one cooked can land on it, so an address
 *  cannot say whether two frames are looking at the same triangles. */
struct MeshBuffers {
  dg::RefCntAutoPtr<dg::IBuffer> vertices;
  dg::RefCntAutoPtr<dg::IBuffer> indices;
  size_t vertexCount = 0;
  uint32_t indexCount = 0;
  /** The frame this was last drawn in, so a mesh no view names any more
   *  is let go. */
  uint64_t used = 0;
};

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
  explicit Gpu(Device& d) : device(&d), shared(d) {}
  ~Gpu();

  Device* device = nullptr;
  /** What every executor on this device stands on — the uniform buffer,
   *  the samplers, the white texel and the readback — which this one
   *  holds rather than owns a second copy of. */
  ::sigil::geometry::device::Resources shared;
  SkISize extent{0, 0};
  uint64_t frame = 0;

  std::map<std::string, DeviceImage, std::less<>> images;
  dg::RefCntAutoPtr<dg::ITexture> depth;
  /** TARGETS NO RESOURCE NAMES, made on the first ask and kept for the
   *  extent's life. A device cannot sample an image it is drawing into,
   *  so every stage that reads and writes at once — one direction of a
   *  blur, a masked op, a pass that writes what it reads — takes one of
   *  these, and they are addressed by index so that two such stages in
   *  one pass cannot be handed the same one. */
  std::vector<dg::RefCntAutoPtr<dg::ITexture>> scratch;
  std::map<uint64_t, MeshBuffers> meshes;
  /** THE ONE PAIR OF BUFFERS a mesh nobody can name is written into,
   *  grown to fit and overwritten by the next draw. A draw whose seam
   *  carries no artefact number is told nothing that says two of them
   *  are the same triangles, so there is nothing to key a cache on and
   *  nothing kept between them.
   *
   *  The two below own the storage and the growth; `streamed` carries
   *  the counts of THIS draw, which are not the buffers'. Each is made
   *  on the first stream, because a `DynamicBuffer` cannot be moved and
   *  an executor that never streams should hold no buffer at all. */
  std::unique_ptr<dg::DynamicBuffer> streamVertices;
  std::unique_ptr<dg::DynamicBuffer> streamIndices;
  MeshBuffers streamed;
  std::map<PipelineKey, Pipeline> pipelines;
  /** Maps whose pixels already stand on this device, under the name the
   *  API gave them. Nothing is copied for one of these. */
  std::map<uint64_t, SampledImage> wrapped;
  /** …and maps that had to be brought over, under the id of the image
   *  they were brought from. */
  std::map<uint32_t, SampledImage> uploaded;
  /** PREFILTERED PANORAMAS, under the id of the panorama they were
   *  built from. One texture with the whole chain in it, in a float
   *  format, because a sky holds values above one and an eight-bit
   *  upload would put the sun and the sky beside it at the same
   *  brightness. */
  std::map<uint32_t, SampledImage> environments;
  /** …and the cosine convolutions, under the same key. */
  std::map<uint32_t, SampledImage> irradiances;

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
  /** @p mesh's buffers, uploaded the first time @p artefact is asked
   *  for. A frame cooking a mesh of its own — the stamps of a point set
   *  — has no artefact to name, and passes an id of its own that no
   *  frame after it repeats. */
  const MeshBuffers* upload(uint64_t artefact, const Mesh& mesh,
                            std::string_view primColorLane = {});
  /** THE PANORAMA on the device: one texture whose levels are the map's
   *  prefiltered chain, uploaded once per map and kept. Null when the
   *  map carries nothing or the device refused it. */
  dg::ITexture* environment(const material::EnvironmentMap& map);
  /** …and the cosine convolution beside it, one small texture a normal
   *  reads directly. */
  dg::ITexture* irradiance(const material::EnvironmentMap& map);
  /** @p mesh in the streaming buffers, overwriting whatever draw wrote
   *  them last. For a caller whose seam carries no artefact number. */
  const MeshBuffers* stream(const Mesh& mesh,
                            std::string_view primColorLane = {});
  /** The pipeline for @p key, built on the first ask. Null when the
   *  program is empty or the device refused it. */
  const Pipeline* pipeline(const PipelineKey& key);
  /** Opens a frame: what the frame before wrote becomes what this one's
   *  `previous()` names, and the texture that held the frame before THAT
   *  is what this one writes into — so a resource costs two textures for
   *  its whole life, no copy, and no allocation per frame. */
  void beginFrame();
  /** Closes it: lets go of the meshes no view has named lately. */
  void endFrame();
  /** @p name's pixels, read back through a staging texture. Null when
   *  nothing has written it. */
  sk_sp<SkImage> read(std::string_view name);

  /** THE MAP @p map IS, on this device.
   *
   *  A texture whose source says its pixels already stand on THIS device
   *  is wrapped where it is — nothing is copied, and a scene painted by
   *  another library into a texture on the shared device is sampled as
   *  it was painted. Anything else is brought over from host memory once
   *  and held under the image it came from. Null when the texture yields
   *  no image. */
  dg::ITexture* sample(const material::Texture& map);

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
void bindAndCommit(Gpu& gpu, const Pipeline& pipeline, const material::slang::Compiled& program,
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

/** THE CAMERA AS THE DEVICE WANTS IT.
 *
 *  A camera's `viewProjection` lands in PIXELS, because that is what a
 *  canvas concat needs; a device wants clip space, so the projection and
 *  the view are composed without the viewport step. What is left to
 *  correct is depth: the projection runs z from one at the near plane to
 *  minus one at the far one, and the device wants zero to one the other
 *  way about. The x and y of that clip space already agree — both count
 *  y upward — so nothing turns them over. */
glm::mat4 clipFor(const ::sigil::geometry::mesh::camera::Camera& camera,
                  SkISize extent);

/** A texture's placement as a shader reads it: the same matrix a host
 *  tier puts on its style, in the four-by-four the uniform is. */
glm::mat4 mapMatrix(const SkMatrix& uv);

/** The two stages of a frame, one file each. */
void paintGeometry(Gpu& gpu, const PassWork& work, const View& view,
                   Targets& targets);
void applyPost(Gpu& gpu, const PassWork& work);

}  // namespace sigil::world::diligent
