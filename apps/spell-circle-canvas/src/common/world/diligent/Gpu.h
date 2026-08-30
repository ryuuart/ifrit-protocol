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
#include <Graphics/GraphicsEngine/interface/Sampler.h>
#include <Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <Graphics/GraphicsEngine/interface/Texture.h>
#include <include/core/SkImage.h>
#include <include/core/SkSize.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilworld/diligent/Device.h>
#include <sigilworld/frame/Pass.h>
#include <sigilworld/frame/Targets.h>
#include <sigilworld/frame/View.h>

#include <Common/interface/RefCntAutoPtr.hpp>
#include <cstddef>
#include <cstdint>
#include <glm/mat4x4.hpp>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "Compile.h"

namespace sigil::world::diligent {

namespace dg = Diligent;

/** THE COLOUR FORMAT every target here holds. One format for every
 *  resource is what lets two resources whose lives do not overlap be
 *  handed one texture, exactly as the ordering hands two names one
 *  surface. */
inline constexpr dg::TEXTURE_FORMAT kColorFormat = dg::TEX_FORMAT_RGBA8_UNORM;
inline constexpr dg::TEXTURE_FORMAT kDepthFormat = dg::TEX_FORMAT_D32_FLOAT;

/** ONE NAMED IMAGE on the device: what this frame wrote, and what the
 *  frame before it wrote. `previous()` is answered from the second, and
 *  the two are exchanged when the frame closes. */
struct DeviceImage {
  dg::RefCntAutoPtr<dg::ITexture> current;
  dg::RefCntAutoPtr<dg::ITexture> previous;
  /** Something has painted `current` since the frame opened. */
  bool written = false;
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
  const Compiled* program = nullptr;
  /** kSrcOver for a body, kPlus for a composite that adds, and kSrc for
   *  a draw that replaces what stands. */
  SkBlendMode blend = SkBlendMode::kSrcOver;
  bool depth = false;
  bool depthWrite = false;
  /** No vertex layout and no index buffer: a triangle covering the
   *  target, which is what every post stage draws. */
  bool fullscreen = false;
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
  explicit Gpu(Device& d) : device(&d) {}
  ~Gpu();

  Device* device = nullptr;
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
  /** Every draw's uniforms, discarded and rewritten per draw. */
  dg::RefCntAutoPtr<dg::IBuffer> uniforms;
  size_t uniformCapacity = 0;
  dg::RefCntAutoPtr<dg::ISampler> sampler;
  /** What an unfilled sampled slot reads: one white texel, so a body
   *  multiplied by a map it was not given is the body. */
  dg::RefCntAutoPtr<dg::ITexture> white;

  std::map<uint64_t, MeshBuffers> meshes;
  std::map<PipelineKey, Pipeline> pipelines;

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
  const MeshBuffers* upload(uint64_t artefact, const Mesh& mesh);
  /** The pipeline for @p key, built on the first ask. Null when the
   *  program is empty or the device refused it. */
  const Pipeline* pipeline(const PipelineKey& key);
  /** The uniform buffer, grown to hold at least @p bytes. */
  dg::IBuffer* uniformBuffer(size_t bytes);
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

  /** A texture of this frame's size and format. */
  dg::RefCntAutoPtr<dg::ITexture> makeColor(const char* label);
};

/** ONE DRAW'S UNIFORMS, written at the offsets the program reported. */
class Uniforms {
 public:
  explicit Uniforms(const Compiled& program)
      : m_program(&program), m_bytes(program.uniformBytes, std::byte{0}) {}

  /** @p count floats into @p name, spread over the member's rows or
   *  elements where the layout put them apart. A name the program does
   *  not carry is skipped: an optimiser that dropped an unused uniform
   *  is not a mistake to report. */
  void set(std::string_view name, const float* values, size_t count);
  void set(std::string_view name, const glm::mat4& m);
  void set(std::string_view name, float x, float y, float z, float w);
  /** Element @p index of an array member. */
  void setElement(std::string_view name, size_t index, const float* values,
                  size_t count);

  [[nodiscard]] const std::vector<std::byte>& bytes() const { return m_bytes; }

 private:
  const Compiled* m_program;
  std::vector<std::byte> m_bytes;
};

/** Binds @p pipeline's uniform buffer to @p values and its sampled slots
 *  to @p textures, in the program's declared order, then commits. A slot
 *  with no texture reads the one white texel. */
void bindAndCommit(Gpu& gpu, const Pipeline& pipeline, const Compiled& program,
                   const Uniforms& values,
                   const std::vector<dg::ITexture*>& textures);

/** The two stages of a frame, one file each. */
void paintGeometry(Gpu& gpu, const PassWork& work, const View& view,
                   Targets& targets);
void applyPost(Gpu& gpu, const PassWork& work);

}  // namespace sigil::world::diligent
