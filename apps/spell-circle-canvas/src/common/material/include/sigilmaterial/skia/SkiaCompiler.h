#pragma once

/** @file
 * The SkSL backend: turns a recipe's SkSL body into an SkRuntimeEffect,
 * and the program handle that builds an
 * SkRuntimeShaderBuilder from resolved bytes.
 */

#include <include/core/SkRefCnt.h>
#include <include/core/SkShader.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/core/FrameData.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Program.h>

#include <cstddef>
#include <memory>
#include <span>
#include <string_view>

namespace sigil::material::skia {

/** A recipe compiled for Skia. */
class SkiaProgram : public Program {
 public:
  SkiaProgram(std::shared_ptr<const Recipe> recipe, Variant variant,
              sk_sp<SkRuntimeEffect> effect)
      : Program(std::move(recipe), Target::SkSL, variant),
        m_effect(std::move(effect)) {}

  const sk_sp<SkRuntimeEffect>& effect() const { return m_effect; }

  /** Whether the COMPILED PROGRAM still has the uniform @p name.
   *
   *  Skia's reflection keeps every uniform the source declared, read or
   *  not, so this is yes for every field of the recipe — and it is the
   *  truthful answer to the question asked: nothing was discarded, and
   *  the bytes are uploaded. Whether the BODY reads a field is a
   *  different question, and it is asked of the recipe
   *  (`Recipe::readsField`) at the moment a caller writes one. */
  bool keeps(std::string_view name) const override {
    return m_effect && m_effect->findUniform(name) != nullptr;
  }

  /** Sets every uniform of @p builder — one over `effect()` — from
   *  @p bytes, which are in the recipe's `layout()`: what
   *  `Material::resolve` returns. Children are left for the caller. */
  void upload(SkRuntimeShaderBuilder& builder,
              std::span<const std::byte> bytes) const;

 private:
  sk_sp<SkRuntimeEffect> m_effect;
};

/** Compiles the distinct recipes instantiated by @p materials for Skia
 *  before their first draw. The built-in compiler is available on first
 *  use; an explicitly registered SkSL compiler takes precedence. */
WarmupResult warmup(std::span<const Material> materials, Variant variant = {});

/** THE MOST IMAGE SAMPLERS ONE LOWERED MATERIAL MAY ASK A DEVICE FOR.
 *
 *  A GPU backend does not compile a runtime effect as a program of its
 *  own: it inlines the whole tree of effects a shader is made of into
 *  ONE fragment program, so every image anywhere under a material is a
 *  sampler of that one program. Metal binds fragment textures at indices
 *  zero to fifteen and its driver rejects a program declaring more —
 *  after Skia has accepted it, so the only sign is a pipeline that never
 *  builds and a pass that paints nothing. */
inline constexpr int kSamplerLimit = 16;

/** The image samplers the SkSL lowering of @p material asks a device
 *  for: one per leaf bound into a slot the body samples, summed down the
 *  whole tree, since a child material's effect is inlined into the
 *  parent's program rather than compiled apart. A leaf yielding a shader
 *  that itself samples several images counts as the one slot it fills,
 *  which is all a material can know about it. */
int samplerCount(const Material& material);

/** The builder for @p material at @p frame: its program's effect with
 *  every uniform set from the resolved bytes and every child slot bound —
 *  a material child resolved and bound recursively, a ShaderLeaf as the
 *  shader it yields — except any slot named in @p leave, which the caller
 *  fills itself (an image filter's input, say). Null when the material has
 *  no Skia program. The built-in compiler is available on first use;
 *  an explicitly registered SkSL compiler takes precedence. */
std::unique_ptr<SkRuntimeShaderBuilder> builder(
    const Material& material, const FrameData& frame, Variant variant = {},
    std::span<const std::string_view> leave = {});

/** The shader for @p material at @p frame: resolves it, builds from its
 *  program, binds each child slot — a material child resolved and bound
 *  recursively, a ShaderLeaf (a Texture, say) as the shader it yields —
 *  and makes the
 *  shader. Null when the material's recipe has no Skia program, which the
 *  cache has already reported. */
sk_sp<SkShader> shader(const Material& material, const FrameData& frame,
                       Variant variant = {});

}  // namespace sigil::material::skia
