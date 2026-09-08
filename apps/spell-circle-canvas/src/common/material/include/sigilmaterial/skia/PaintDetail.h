#pragma once

/** @file
 * WHAT A PAINT'S OWN PARTS AGREE ON: the unit-square ramp its factories
 * compile to, the one Paint→SkShader conversion every child slot
 * performs, the two questions asked of a runtime effect before a name is
 * stored against it, and the pass specialization the text runtime draws a
 * `fx::pass` track through.
 *
 * Declared apart from the paint class because these are shared
 * mechanisms rather than doors an author reaches for: `Effect` validates
 * its own slots here, and the text runtime fills a pass through
 * `PassInputs`.
 */

#include <include/core/SkPoint.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkShader.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/core/Recipe.h>

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace sigil::material::skia {

class Paint;
struct PaintFrame;
struct Stop;

namespace detail {

/** The unit-square ramp both linearUnit() and radialUnit() compile to: one
 *  SkSL pass that divides by uResolution, so the gradient's coordinates are
 *  fractions of the node's laid-out box rather than pixels. Any number of
 *  stops — the count is baked into the generated source as a chain of
 *  mixes, each taking effect past its own start, and one effect is cached
 *  per stop count. */
Paint unitRamp(SkPoint a, SkPoint b, std::vector<Stop> stops, bool radial);

/** THE CHILD-SLOT CONVERSION, in one place because its callers must agree:
 *  sksl()'s children, blend()'s layers and Effect's children all need this
 *  Paint as the SkShader a builder slot takes. @p frame non-null is the
 *  per-draw `shaderFor()` form and null the frameless `asShader()`
 *  snapshot — the same split every child site makes — and a solid
 *  collapses to a colour shader either way. */
sk_sp<SkShader> childShader(const Paint& source, const PaintFrame* frame);

/** Does @p effect declare @p name as a `uniform shader`? Assigning a child
 *  an effect does not declare aborts in a debug build, so both child()
 *  doors — Paint's and Effect's — validate at STORE time and then warn
 *  and ignore: one typo in a live-reloaded sketch must not take the host
 *  process down. */
bool declaresShaderChild(const sk_sp<SkRuntimeEffect>& effect,
                         std::string_view name);

/** Does @p effect declare @p name as a uniform of exactly @p bytes? The
 *  same guardrail one paragraph up, for the other kind of slot: assigning
 *  an undeclared uniform — or one whose declared size is not the caller's,
 *  which is every mismatched float2, float4 and array — aborts a debug
 *  build and drops
 *  the value silently in a release one. Every door that takes a uniform
 *  name from an author validates here at STORE time, so the builder is
 *  never handed an entry it would refuse. An ARRAY validates by its TOTAL
 *  byte size, which is all the builder checks: 12 floats fill
 *  `float4 uRect[3]` and `float uWeights[12]` alike. */
bool declaresUniform(const sk_sp<SkRuntimeEffect>& effect,
                     std::string_view name, size_t bytes);

/** THE PER-UNIT DATA A TEXT PASS IS HANDED — what the fx() runtime fills
 *  for a `fx::pass` track's material and `Paint::resolvePass` uploads.
 *  `content` is the addressed units' rendered layer; `rects` is 4 floats
 *  per unit (x, y, w, h, node-local px); `phases` is 2 per unit (that
 *  unit's cascade-local 0→1, then its stable seed). Non-owning views,
 *  valid for the call. */
struct PassInputs {
  sk_sp<SkShader> content;
  const float* rects = nullptr;
  const float* phases = nullptr;
  uint32_t units = 0;
};

/** THE PASS SPECIALIZATION of @p authored at @p units: a recipe with the
 *  same params ABI whose SkSL body is the runtime's declarations —
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
