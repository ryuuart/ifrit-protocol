/** @file
 * SkSL compilation through SkRuntimeEffect::MakeForShader, uniform upload
 * from layout bytes, and the recursive child binding that makes one
 * shader from a material tree — a material child resolves and binds its
 * own shader, a shader leaf binds the shader it yields.
 */

#include "sigilmaterial/skia/SkiaCompiler.h"

#include <include/core/SkString.h>
#include <sigilmaterial/texture/ShaderLeaf.h>

#include <cstring>
#include <mutex>

namespace sigil::material::skia {

void SkiaProgram::upload(SkRuntimeShaderBuilder& builder,
                         std::span<const std::byte> bytes) const {
  for (const Field& f : recipe().layout().fields) {
    // The effect declares every field the layout holds, because its source
    // was generated from the same layout; a name Skia does not find means
    // the SkSL optimiser dropped an unused uniform, which is a silent skip
    // and not an error.
    SkRuntimeShaderBuilder::BuilderUniform u = builder.uniform(f.name);
    if (!u.fVar) continue;
    if (f.offset + f.floats * sizeof(float) > bytes.size()) continue;
    u.set(reinterpret_cast<const float*>(bytes.data() + f.offset),
          (int)f.floats);
  }
}

namespace {

std::shared_ptr<Program> compile(std::shared_ptr<const Recipe> recipe,
                                 Variant variant, std::string& error) {
  const std::string source = recipe->source(Target::SkSL);
  auto [effect, message] =
      SkRuntimeEffect::MakeForShader(SkString(source.c_str()));
  if (!effect) {
    error = message.c_str();
    return nullptr;
  }
  return std::make_shared<SkiaProgram>(std::move(recipe), variant,
                                       std::move(effect));
}

}  // namespace

void install() {
  static std::once_flag once;
  std::call_once(once, [] { registerCompiler(Target::SkSL, compile); });
}

std::unique_ptr<SkRuntimeShaderBuilder> builder(
    const Material& material, const FrameData& frame, Variant variant,
    std::span<const std::string_view> leave) {
  const Material::Resolved resolved =
      material.resolve(Target::SkSL, frame, variant);
  if (!resolved.program) return nullptr;
  const auto* program = resolved.program->as<SkiaProgram>();
  if (!program) return nullptr;
  auto b = std::make_unique<SkRuntimeShaderBuilder>(program->effect());
  program->upload(*b, resolved.bytes);
  for (const auto& [slot, child] : material.children()) {
    bool skip = false;
    for (std::string_view name : leave) skip |= name == slot;
    if (skip) continue;
    SkRuntimeShaderBuilder::BuilderChild c = b->child(slot);
    if (!c.fChild) continue;
    if (child.material) {
      c = shader(*child.material, frame, variant);
    } else if (const auto* leaf =
                   dynamic_cast<const ShaderLeaf*>(child.leaf.get())) {
      c = leaf->shader();
    }
  }
  return b;
}

sk_sp<SkShader> shader(const Material& material, const FrameData& frame,
                       Variant variant) {
  std::unique_ptr<SkRuntimeShaderBuilder> b = builder(material, frame, variant);
  return b ? b->makeShader() : nullptr;
}

}  // namespace sigil::material::skia
