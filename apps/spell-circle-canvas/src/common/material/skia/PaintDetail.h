#pragma once

/** @file
 * WHAT A PAINT'S OWN PARTS AGREE ON: the unit-square ramp its factories
 * compile to, the one Paint→SkShader conversion every slot
 * performs, the two questions asked of a runtime effect before a name is
 * stored against it, and the pass specialization the text runtime draws a
 * `textFx::pass` track through.
 *
 * Declared apart from the paint class because these are shared
 * mechanisms used by the paint and effect implementations. Consumers
 * submit pass inputs through the paint without managing specializations.
 */

#include <include/core/SkMatrix.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkShader.h>
#include <include/core/SkSize.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/advanced/FrameData.h>
#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/color/Color.h>

#include <cstddef>
#include <cstdint>
#include <glm/mat3x3.hpp>
#include <memory>
#include <string_view>
#include <vector>

namespace sigil::material {
class Material;
class Paint;
}  // namespace sigil::material

namespace sigil::material::skia {

/** WHAT ONE DRAW SUPPLIES a paint, in Skia's own terms: the frame data
 *  crossed once at the executor's door, so every resolve path reads the
 *  box, the root's size and the node→root matrix as Skia values. */
struct PaintFrame {
  /** The painted box in px; `uResolution` for a node-local paint. */
  SkSize size = SkSize::MakeEmpty();
  /** The root's laid-out size in canvas px — `uResolution` for a
   *  world-space paint. Empty falls back to `size`. */
  SkSize rootSize = SkSize::MakeEmpty();
  /** The box's local space to the root. Identity outside a composite. */
  SkMatrix toRoot = SkMatrix::I();
  /** Logical-node offsets expressed in the paint's sampling coordinates. */
  SkMatrix localToSample = SkMatrix::I();
  /** Seconds on the consumer's clock; the `uTime` uniform. */
  double seconds = 0.0;
  /** Device pixels per logical pixel; the `uContentScale` uniform. */
  float contentScale = 1.0f;
  /** Set true where `contentScale` is handed to a program; may be null. */
  bool* contentScaleRead = nullptr;
  /** The recorder the paint is drawn through; a texture standing on its
   *  device is bound there. Null reads such a texture back. */
  skgpu::graphite::Recorder* recorder = nullptr;
};

/** @p frame in Skia's terms. */
PaintFrame paintFrameOf(const FrameData& frame);
FrameData frameDataOf(const PaintFrame& frame);

/** A root-anchored body and its children sample in root coordinates. */
FrameData rootSamplingFrame(FrameData frame);
PaintFrame rootSamplingFrame(PaintFrame frame);

/** World-space anchoring, the ONE construction every resolve path
 *  shares: a root-coordinate shader is sampled through the inverse of the
 *  node's node-to-root matrix W, so a local drawing coordinate p evaluates
 *  the field at W·p — the root-frame point the node actually occupies.
 *  That is what lets two separate nodes sample one continuous field.
 *
 *  An identity W — outside a composer, or a root-level node with no
 *  transform — wraps nothing, because node-local and root-local are then
 *  the same frame. */
inline sk_sp<SkShader> anchorToRoot(sk_sp<SkShader> shader,
                                    const PaintFrame& frame) {
  if (!shader || frame.toRoot.isIdentity()) return shader;
  SkMatrix inverse;
  if (!frame.toRoot.invert(&inverse)) return shader;
  return shader->makeWithLocalMatrix(inverse);
}

using UniformType = SkRuntimeEffect::Uniform::Type;

// Skia copies uniform bytes without converting their type. Typed values
// require the matching non-array declaration; flat float packets may fill
// any floating declaration, but must supply its whole reflected size.
inline bool validUniform(const sk_sp<SkRuntimeEffect>& effect,
                         std::string_view name, UniformType type) {
  const auto* uniform = effect ? effect->findUniform(name) : nullptr;
  return uniform && uniform->type == type && !uniform->isArray();
}

inline bool validFloatPacket(const sk_sp<SkRuntimeEffect>& effect,
                             std::string_view name, size_t count) {
  const auto* uniform = effect ? effect->findUniform(name) : nullptr;
  if (!uniform) return false;
  switch (uniform->type) {
    case UniformType::kFloat:
    case UniformType::kFloat2:
    case UniformType::kFloat3:
    case UniformType::kFloat4:
    case UniformType::kFloat2x2:
    case UniformType::kFloat3x3:
    case UniformType::kFloat4x4:
      return uniform->sizeInBytes() / sizeof(float) == count;
    case UniformType::kInt:
    case UniformType::kInt2:
    case UniformType::kInt3:
    case UniformType::kInt4:
      return false;
  }
  return false;
}

namespace detail {

void ensureCompiler();

/** Root-coordinate reads in a material's paint tree, without resolving sources.
 *  Surface and effect dependencies belong to their executors. */
bool paintUsesWorldSpace(const Material& material);

/** The unit-square ramp a box-unit linear or radial gradient compiles to: one
 *  SkSL pass that divides by uResolution, so the gradient's coordinates are
 *  fractions of the node's laid-out box rather than pixels. Any number of
 *  stops — the count is baked into the generated source as a chain of
 *  mixes, each taking effect past its own start, and one effect is cached
 *  per stop count. */
Paint unitRamp(SkPoint a, SkPoint b, std::vector<material::ColorStop> stops,
               bool radial);

/** THE CHILD-SLOT CONVERSION, in one place because its callers must agree:
 *  sksl()'s children, blend()'s layers and Effect's children all need this
 *  Paint as the SkShader a builder slot takes. @p frame non-null is the
 *  per-draw `shaderFor()` form and null the frameless `asShader()`
 *  snapshot — the same split every child site makes — and a solid
 *  collapses to a colour shader either way. */
sk_sp<SkShader> childShader(const Paint& source, const PaintFrame* frame);

/** Does @p effect declare @p name as a `uniform shader`? Assigning a slot
 *  an effect does not declare aborts in a debug build, so both slot()
 *  doors — Paint's and Effect's — validate at STORE time and then warn
 *  and ignore: one typo in a live-reloaded sketch must not take the host
 *  process down. */
bool declaresShaderChild(const sk_sp<SkRuntimeEffect>& effect,
                         std::string_view name);

/** THE PASS SPECIALIZATION of @p authored at @p units: a recipe with the
 *  same parameters ABI whose SkSL body is the runtime's declarations —
 *  `uContent`, `uUnitRect[N]`, `uUnitPhase[N]`, `kUnitCount` — followed by
 *  the author's, held once per distinct (recipe, N) for the process. The
 *  unit count must be baked into the definition because a runtime effect's
 *  array size is fixed at compile and SkSL has no uniform-bounded loop;
 *  holding one specialization per count is what keeps that from meaning a
 *  compile per frame. Null when @p authored carries no SkSL body. */
std::shared_ptr<const sigil::material::Recipe> passRecipeFor(
    const std::shared_ptr<const sigil::material::Recipe>& authored,
    uint32_t units);

/** Whether @p recipe's SkSL body is a PASS body — one written against the
 *  declarations `passRecipeFor` prepends rather than against its own.
 *
 *  It is asked because such a body IS NOT A SHADER ON ITS OWN: compiled
 *  standalone it names `uContent`, `uUnitRect`, `uUnitPhase` and
 *  `kUnitCount`, none of which exist yet, and the compiler reports one
 *  error per mention. That failure is not information — the recipe was
 *  never meant to compile until the runtime knew the unit count — so it
 *  must not be provoked. The four names are the signal, because an author
 *  is told not to declare them and they mean nothing anywhere else. */
bool isPassBody(const sigil::material::Recipe& recipe);

}  // namespace detail
}  // namespace sigil::material::skia
