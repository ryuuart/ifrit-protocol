/** @file
 * THE MECHANISMS A PAINT'S PARTS AGREE ON: the two questions asked of a
 * runtime effect before a name is stored against it, the Paint→SkShader
 * conversion every child slot performs, and the pass specialization a
 * text runtime draws a track through — one definition per unit count,
 * held for the process.
 */

#include "PaintDetail.h"

#include <include/core/SkShader.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/skia/Paint.h>

#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::material::skia {

namespace detail {

// A named child is usable iff the effect declares it as a SHADER child —
// assigning a missing child SkDEBUGFAILs exactly like a missing uniform.
// Shared with Effect::child: one validation behind both ways of filling a
// child slot, so the two cannot disagree about what is legal.
bool declaresShaderChild(const sk_sp<SkRuntimeEffect>& effect,
                         std::string_view name) {
  if (!effect) return false;
  const SkRuntimeEffect::Child* c = effect->findChild(name);
  return c && c->type == SkRuntimeEffect::ChildType::kShader;
}

// The uniform half of the same guardrail, shared with Effect::shader and
// Effect::uniform so the two doors that take an author's uniform name cannot
// disagree about what the effect will accept.
bool declaresUniform(const sk_sp<SkRuntimeEffect>& effect,
                     std::string_view name, size_t bytes) {
  if (!effect) return false;
  const SkRuntimeEffect::Uniform* u = effect->findUniform(name);
  return u && u->sizeInBytes() == bytes;
}

bool isPassBody(const sigil::material::Recipe& recipe) {
  if (!recipe.has(sigil::material::Target::SkSL)) return false;
  for (std::string_view name :
       {"uContent", "uUnitRect", "uUnitPhase", "kUnitCount"})
    if (recipe.readsField(name)) return true;
  return false;
}

// THE ONE SPECIALIZATION behind every pass: the author's recipe with the
// runtime's declarations prepended to its SkSL body at the requested unit
// count, held per (recipe identity, count) for the process. The
// specialization carries the author's params layout, so the instance's
// values, bindings and children ride it unchanged; it is a second
// definition, so its program compiles and caches apart, which is what
// stops a count from meaning a compile per frame.
std::shared_ptr<const sigil::material::Recipe> passRecipeFor(
    const std::shared_ptr<const sigil::material::Recipe>& authored,
    uint32_t units) {
  if (!authored || !authored->has(sigil::material::Target::SkSL))
    return nullptr;
  // The authored recipe is held, not pointed at: a definition freed and a
  // second allocated at its address would otherwise inherit the first's
  // specializations. A recipe is defined once and held anyway.
  struct Cached {
    std::shared_ptr<const sigil::material::Recipe> authored;
    uint32_t units;
    std::shared_ptr<const sigil::material::Recipe> recipe;
  };
  static std::vector<Cached> cache;
  // Held under a lock because a host may paint two composers on two
  // threads, and both would reach here for the same definition.
  static std::mutex mutex;
  const std::lock_guard lock(mutex);
  const uint32_t n = std::max(units, 1u);
  for (const Cached& c : cache)
    if (c.units == n && c.authored == authored) return c.recipe;
  const std::string count = std::to_string(n);
  // The arrays and the loop bound sit at the head of the BODY rather than
  // among the declarations, because a params field is a value the author
  // sets and these three are the runtime's alone: nothing may write them
  // through the instance, and the count is a compile-time constant no
  // upload could carry.
  sigil::material::Recipe spec = *authored;
  spec.child("uContent");
  spec.body(sigil::material::Target::SkSL,
            "uniform float4 uUnitRect[" + count +
                "];\n"
                "uniform float2 uUnitPhase[" +
                count +
                "];\n"
                "const int kUnitCount = " +
                count + ";\n" + *authored->body(sigil::material::Target::SkSL));
  auto held = std::make_shared<const sigil::material::Recipe>(std::move(spec));
  cache.push_back({authored, n, held});
  return held;
}

// The Paint→SkShader conversion every child slot performs: the
// per-draw resolve when there is a frame, the
// frameless snapshot when there is not, a solid as a colour shader.
sk_sp<SkShader> childShader(const Paint& source, const PaintFrame* ctx) {
  if (!ctx)
    return source.asShader();  // already turns a solid into SkShaders::Color
  if (source.isNone()) return nullptr;
  if (source.isSolid()) return SkShaders::Color(source.solidColor(), nullptr);
  return source.shaderFor(*ctx);
}

}  // namespace detail

}  // namespace sigil::material::skia
