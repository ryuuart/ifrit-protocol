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
  Diligent::RefCntAutoPtr<Diligent::IPipelineState> state;
  Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> binding;
};

/** The pipelines standing on one device, built on the first ask and kept
 *  for the device's life. A program compiles once and is drawn with many
 *  times; what varies between two draws of it is the key. */
class PipelineCache {
 public:
  explicit PipelineCache(Device& device);

  /** The pipeline for @p key. Null when the program is empty or the
   *  device refused it — the refusal is remembered, so a program that
   *  cannot be built is not attempted once per draw. */
  const Pipeline* pipeline(const PipelineKey& key);

 private:
  Device* m_device = nullptr;
  boost::container::map<PipelineKey, Pipeline> m_pipelines;
};

/** Binds @p pipeline's uniform buffer to @p values and its sampled slots
 *  to @p textures, in the program's declared order, read through
 *  @p filter, then commits. A slot with no texture reads @p shared's one
 *  white texel.
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
              bool (*panoramaSlot)(std::string_view) = nullptr);

/** Binds @p colour as a stage's target, with @p depth when there is one
 *  to write, and clears both. */
void openTarget(Device& device, Diligent::ITexture* colour,
                Diligent::ITexture* depth, const float* clear);

}  // namespace sigil::geometry::device
