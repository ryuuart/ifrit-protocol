#pragma once

/** @file
 * WHAT A DRAW ON THIS DEVICE IS MADE OF, once the program is compiled:
 * the pipeline built from it, the binding its uniforms and sampled slots
 * are written into, and the target it draws onto.
 *
 * None of it decides a picture. A pipeline is a program plus how it
 * blends, whether it depth-tests and whether it reads the primitive
 * lane; two draws that agree on all of those share one. It stands here
 * because the vertex layout it declares is the residency's, the uniform
 * buffer it writes through is the device's shared one, and two executors
 * on a device should not each build the same pipeline out of the same
 * program.
 */

#include <Graphics/GraphicsEngine/interface/PipelineState.h>
#include <Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <Graphics/GraphicsEngine/interface/Texture.h>
#include <include/core/SkBlendMode.h>
#include <include/core/SkSamplingOptions.h>
#include <sigilgeometry/device/Device.h>
#include <sigilmaterial/slang/SlangCompiler.h>

#include <Common/interface/RefCntAutoPtr.hpp>
#include <boost/container/map.hpp>
#include <span>
#include <string_view>

#include "Resources.h"

namespace sigil::geometry::device {

/** HOW A PIPELINE DIFFERS from another built out of the same program:
 *  how it blends, and whether it writes depth. Two draws that agree on
 *  both share one pipeline. */
struct PipelineKey {
  const material::slang::Compiled* program = nullptr;
  /** kSrcOver for a body laid over what stands, kPlus for one that adds,
   *  kMultiply, kScreen and kModulate for the modes a blend state can
   *  express over premultiplied colour, and kSrc for a draw that replaces
   *  what stands. Any other mode would need to read the target it draws
   *  into and is laid over instead. */
  SkBlendMode blend = SkBlendMode::kSrcOver;
  bool depth = false;
  bool depthWrite = false;
  /** No vertex layout and no index buffer: a triangle covering the
   *  target, which is what every post stage draws. */
  bool fullscreen = false;
  /** Does the vertex layout declare the PRIMITIVE lane? Every vertex
   *  carries one either way — it is the same buffer — but a program that
   *  does not read it is not given an attribute it never declared. */
  bool primitive = false;
  /** Are back faces dropped? A draw the caller asked to keep them for
   *  is a different pipeline and not a different program. */
  bool cull = true;
  /** The format of the target it draws into; `TEX_FORMAT_UNKNOWN` for a
   *  draw that writes depth and no colour at all. */
  Diligent::TEXTURE_FORMAT target = kColorFormat;
  /** Does the vertex layout declare the INSTANCE elements, read from a
   *  second buffer once per instance? A program whose vertex stage reads
   *  an instance is drawn with one bound, a single neutral one for a
   *  mesh that is not instanced. */
  bool instanced = false;
  /** Does the vertex layout declare the TANGENT, for a program whose
   *  vertex stage reads it? Every vertex carries one either way. */
  bool tangent = false;
  auto operator<=>(const PipelineKey&) const = default;
};

/** A PIPELINE AND ITS BINDING, made once per key. */
struct Pipeline {
  Diligent::RefCntAutoPtr<Diligent::IPipelineState> state;
  Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> binding;
};

/** The pipelines standing on one device, built on the first ask and kept
 *  for the device's life. A program compiles once and is drawn with many
 *  times; what varies between two draws of it is the key.
 *
 *  A BUILD RUNS ON THE DEVICE'S WORKERS where the device offers them:
 *  `prepare` starts one and returns at once, `ready` answers a pipeline
 *  only once it is built and never waits, and `pipeline` waits for one
 *  still building. A device with no workers builds on the calling thread,
 *  and every one of the three is then the same build. The cache is asked
 *  from one thread. */
class PipelineCache {
 public:
  explicit PipelineCache(Device& device);

  /** The pipeline for @p key, waiting for it while it builds. Null when
   *  the program is empty or the device refused it — the refusal is
   *  remembered, so a program that cannot be built is not attempted once
   *  per draw. */
  const Pipeline* pipeline(const PipelineKey& key);

  /** The pipeline for @p key if it is built, starting the build when it
   *  was never asked for; null while it builds and where it was refused. */
  const Pipeline* ready(const PipelineKey& key);

  /** Starts building the pipeline for @p key ahead of the frame that will
   *  draw with it. Nothing when it was asked for before. */
  void prepare(const PipelineKey& key);

  /** How many pipelines are building and not yet asked for since. */
  size_t building() const;

 private:
  /** A pipeline, and whether its build was last seen under way. */
  struct Entry {
    Pipeline made;
    bool building = false;
  };
  Entry& start(const PipelineKey& key);
  /** @p entry once built — waiting for it when @p wait — or null. */
  const Pipeline* finish(Entry& entry, bool wait);

  Device* m_device = nullptr;
  boost::container::map<PipelineKey, Entry> m_pipelines;
};

/** Binds @p pipeline's uniform buffer to @p values and its sampled slots
 *  to @p textures, in the program's declared order, read through
 *  @p filter — or through the sampler @p samplers names at the same
 *  position, for a slot whose texture states how it is read — then
 *  commits. A view @p views names at a slot's position is bound in place
 *  of its texture's own: a depth texture is read through a view of its
 *  own format, which some devices will not reinterpret. A slot with no texture
 * reads @p shared's one white texel.
 *
 *  @p panoramaSlot names the slots that are read as an equirect map
 *  instead: one wrap on each axis, and linearly across the prefiltered
 *  levels. Every other slot in a draw shares one filter and one wrap,
 *  which is what a base-colour map's sampling decides for all of them. */
void bindDraw(Resources& shared, const Pipeline& pipeline,
              const material::slang::Compiled& program,
              const material::slang::Uniforms& values,
              std::span<Diligent::ITexture* const> textures,
              SkFilterMode filter = SkFilterMode::kLinear, bool tile = false,
              bool (*panoramaSlot)(std::string_view) = nullptr,
              std::span<Diligent::ISampler* const> samplers = {},
              std::span<Diligent::ITextureView* const> views = {});

/** Binds @p colour as a stage's target, with @p depth when there is one
 *  to write, and clears both. */
void openTarget(Device& device, Diligent::ITexture* colour,
                Diligent::ITexture* depth, const float* clear);

}  // namespace sigil::geometry::device
