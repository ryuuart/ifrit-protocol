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
#include <span>

namespace sigil::material::skia {

/** A recipe compiled for Skia. */
class SkiaProgram : public Program {
 public:
  SkiaProgram(std::shared_ptr<const Recipe> recipe, Variant variant,
              sk_sp<SkRuntimeEffect> effect)
      : Program(std::move(recipe), Target::SkSL, variant),
        m_effect(std::move(effect)) {}

  const sk_sp<SkRuntimeEffect>& effect() const { return m_effect; }

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

/** The shader for @p material at @p frame: resolves it, builds from its
 *  program, resolves and binds each child slot recursively, and makes the
 *  shader. Null when the material's recipe has no Skia program, which the
 *  cache has already reported. */
sk_sp<SkShader> shader(const Material& material, const FrameData& frame,
                       Variant variant = {});

}  // namespace sigil::material::skia
