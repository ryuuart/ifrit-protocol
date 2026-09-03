#pragma once

/** @file
 * The SkSL backend: registers the compiler that turns a recipe's SkSL
 * body into an SkRuntimeEffect, and the program handle that builds an
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

/** Registers the SkSL compiler with the shared program cache. Idempotent;
 *  call it once before the first resolve for Target::SkSL. */
void install();

/** The builder for @p material at @p frame: its program's effect with
 *  every uniform set from the resolved bytes and every child slot bound —
 *  a material child resolved and bound recursively, a ShaderLeaf as the
 *  shader it yields — except any slot named in @p leave, which the caller
 *  fills itself (an image filter's input, say). Null when the material has
 *  no Skia program. */
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
