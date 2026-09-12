/** @file
 * THE POST-PROCESSING VALUE: the already-built filter an effect wraps,
 * the SkSL program it is described as, the lowering of a channelwise
 * recipe to a table, the chaining of two effects, the filter every kind
 * is built into per paint, and what an effect declares about its own
 * motion and its need for the root frame.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <include/core/SkTypes.h>  // SkDebugf — the slot diagnostics
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/texture/Texture.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "EffectInternal.h"

namespace sigil::material::skia {

Effect Effect::filter(sk_sp<SkImageFilter> f) {
  Effect e;
  e.m_filter = std::move(f);
  return e;
}

Effect Effect::filter(sk_sp<SkColorFilter> f) {
  Effect e;
  e.m_colorFilter = std::move(f);
  return e;
}

Effect Effect::recipe(const Material& material) {
  static constexpr std::string_view kContent[] = {"content"};
  std::unique_ptr<SkRuntimeShaderBuilder> built =
      skia::builder(material, {}, {}, kContent);
  if (!built) return {};
  Effect effect =
      filter(SkImageFilters::RuntimeShader(*built, "content", nullptr));
  if (!effect.m_filter) return {};
  std::optional<Material> comparable;
  if (!material.isAnimated()) comparable = material;
  effect.m_recipeSnapshot = std::make_shared<const RecipeSnapshot>(
      RecipeSnapshot{std::move(comparable), sk_ref_sp(built->effect())});
  return effect;
}

namespace {

/** Can a 256-entry table per channel carry everything @p type does? Only
 *  where the surface itself is eight unsigned bits per colour channel:
 *  above that a table would quantize what the surface can hold, and
 *  kUnknown_SkColorType — what a canvas backed by neither raster nor GPU
 *  answers — says the surface is not known at all. */
bool tableCarriesEveryCode(SkColorType type) {
  switch (type) {
    case kRGBA_8888_SkColorType:
    case kBGRA_8888_SkColorType:
    case kRGB_888x_SkColorType:
      return true;
    default:
      return false;
  }
}

/** The channelwise recipe's response row read back as three 256-entry
 *  tables, or nothing when the material is not one, or its row is not
 *  256 samples of a readable image.
 *
 *  The row is read UNPREMULTIPLIED into eight-bit codes, which is the
 *  space Skia's table filter maps in and the space the recipe's own body
 *  unpremultiplies into, so the two agree code for code. */
struct ResponseTables {
  uint8_t r[256], g[256], b[256];
};
std::optional<ResponseTables> responseTables(const Material& material) {
  const std::string& slot = material.recipe().channelwiseSlot();
  if (slot.empty()) return std::nullopt;
  const auto* texture = dynamic_cast<const Texture*>(material.leaf(slot));
  if (!texture) return std::nullopt;
  const sk_sp<SkImage> row = texture->image();
  if (!row || row->width() != 256 || row->height() != 1) return std::nullopt;
  SkBitmap codes;
  if (!codes.tryAllocPixels(SkImageInfo::Make(256, 1, kRGBA_8888_SkColorType,
                                              kUnpremul_SkAlphaType)))
    return std::nullopt;
  if (!row->readPixels(nullptr, codes.pixmap(), 0, 0)) return std::nullopt;
  ResponseTables tables;
  const uint8_t* px = (const uint8_t*)codes.getPixels();
  for (int i = 0; i < 256; ++i) {
    tables.r[i] = px[i * 4];
    tables.g[i] = px[i * 4 + 1];
    tables.b[i] = px[i * 4 + 2];
  }
  return tables;
}

}  // namespace

Effect Effect::recipe(const Material& material, SkColorType surface) {
  if (tableCarriesEveryCode(surface))
    if (std::optional<ResponseTables> tables = responseTables(material))
      // Alpha null is the identity: a view transform maps colour and
      // leaves coverage alone, exactly as the body's repremultiply does.
      return filter(
          SkColorFilters::TableARGB(nullptr, tables->r, tables->g, tables->b));
  return recipe(material);
}

Effect Effect::glow(SkColor4f color, float sigma) {
  return filter(SkImageFilters::DropShadow(0, 0, sigma, sigma,
                                           color.toSkColor(), nullptr));
}

Effect Effect::shader(sk_sp<SkRuntimeEffect> effect,
                      std::vector<std::pair<std::string, float>> uniforms) {
  Effect e;
  if (!effect) return e;
  // Material's guardrail, and for the same reason: SkRuntimeShaderBuilder
  // answers a name the effect does not declare — or one whose declared size
  // is not four bytes, which is every float2, float4 and array — with a
  // debug abort and no write, and this Skia is built without SK_DEBUG, so
  // the value is dropped and the effect paints with a zeroed uniform. Drop
  // the entry here instead, loudly, and keep the recipe free of anything
  // buildFilter would have to re-check.
  std::erase_if(uniforms, [&](const std::pair<std::string, float>& entry) {
    if (detail::declaresUniform(effect, entry.first, sizeof(float)))
      return false;
    warnUndeclaredEffectUniform("shader", entry.first);
    return true;
  });
  SkRuntimeShaderBuilder builder(effect);
  for (const auto& [name, value] : uniforms) builder.uniform(name) = value;
  e.m_filter = SkImageFilters::RuntimeShader(builder, "content", nullptr);
  e.m_effect = std::move(effect);
  e.m_uniforms = std::move(uniforms);
  return e;
}

sk_sp<SkImageFilter> Effect::liftedFilter() const {
  if (m_filter || !m_colorFilter) return m_filter;
  return SkImageFilters::ColorFilter(m_colorFilter, nullptr);
}

Effect Effect::then(const Effect& next) const {
  Effect e;
  const sk_sp<SkImageFilter> mine = liftedFilter();
  const sk_sp<SkImageFilter> theirs = next.liftedFilter();
  // A GEOMETRY-dependent child counts as content exactly as a live one
  // does. Precomposing freezes the whole chain at the null-context
  // snapshot, so a sigma map that reads uResolution, or a blur whose crop
  // is the box, would paint the box it was first described in for ever —
  // and the composed effect would answer usesWorldSpace() with false
  // because the children it was built from are gone.
  const bool mineNeedsCtx = isAnimated() || anyChildNeedsContext();
  const bool nextNeedsCtx = next.isAnimated() || next.anyChildNeedsContext();
  const bool thisReal = mine || mineNeedsCtx;
  const bool nextReal = theirs || nextNeedsCtx;
  if (!thisReal) return next;
  if (!nextReal) return *this;
  if (mineNeedsCtx || nextNeedsCtx) {
    // Neither side can precompose: hold both and re-compose per paint.
    e.m_chainA = std::make_shared<const Effect>(*this);
    e.m_chainB = std::make_shared<const Effect>(next);
    return e;
  }
  e.m_filter = SkImageFilters::Compose(theirs, mine);
  return e;
}

sk_sp<SkImageFilter> Effect::resolvedImageFilter(
    const PaintFrame* paintFrame) const {
  if (m_chainA)
    return SkImageFilters::Compose(m_chainB->resolvedImageFilter(paintFrame),
                                   m_chainA->resolvedImageFilter(paintFrame));
  // A context-needing child (live or geometry tier) has to be re-resolved
  // per paint; a static one is already in the snapshot. Same question
  // Material::build's memo asks of its children, same answer.
  if (m_bound.empty() && m_blocks.empty() &&
      !(paintFrame && anyChildNeedsContext()))
    return liftedFilter();
  return buildFilter(paintFrame);
}

/** THE FILTER, built from the recipe — unconditionally, which is what
 *  separates it from resolvedImageFilter(): the store-time snapshot and the
 *  per-paint resolve are the SAME construction differing only in whether
 *  there is a context, exactly as Material::build(live, paintFrame) is. */
sk_sp<SkImageFilter> Effect::buildFilter(const PaintFrame* paintFrame) const {
  if (m_parametricBlur) {  // re-wrap the held pyramid with the parameter's
                           // scale
    float sigma = m_parametricBlur->maxSigma;
    for (const auto& [name, out] : m_bound)
      if (name == "maxSigma") sigma = motion::resolveFloatAt(nullptr, out);
    // A declared 0 holds no pyramid; a bound value then builds one at
    // the value, at every paint — the cost declaring the range avoids.
    const std::shared_ptr<const BlurLevels> levels =
        m_blurLevels ? m_blurLevels : makeBlurLevels(sigma);
    return makeParametricBlur(
        levels.get(), sigma, childShaderFor("sigma", paintFrame),
        paintFrame ? paintFrame->size : SkSize::MakeEmpty());
  }
  if (m_directionalBlur) {  // rebuild the sandwich from the bound parameters
    DirectionalBlur d = *m_directionalBlur;
    for (const auto& [name, out] : m_bound) {
      const float v = motion::resolveFloatAt(nullptr, out);
      if (name == "sigma")
        d.sigma = v;
      else if (name == "angle")
        d.angleDeg = v;
      else if (name == "across")
        d.across = v;
    }
    return makeDirectionalBlur(d.sigma, d.angleDeg, d.across);
  }
  if (!m_effect) return m_filter;
  SkRuntimeShaderBuilder builder(m_effect);
  for (const auto& [name, value] : m_uniforms) builder.uniform(name) = value;
  for (const auto& [name, value] : m_uniforms2) builder.uniform(name) = value;
  for (const auto& [name, value] : m_uniforms4) builder.uniform(name) = value;
  for (const auto& [name, values] : m_uniformArrays)
    builder.uniform(name).set(values.data(), (int)values.size());
  for (const auto& [name, out] : m_bound)
    builder.uniform(name) = motion::resolveFloatAt(nullptr, out);
  for (const auto& [name, block] : m_blocks)
    builder.uniform(name).set(block->values().data(), (int)block->size());
  // The child slots, against the painting node's box (Paint::child's
  // contract: a child sees the SAME frame, because there is one node).
  // "content" is the library's and is filled by the factory below.
  for (const auto& [name, child] : m_children)
    if (child) builder.child(name) = detail::childShader(*child, paintFrame);
  if (m_gatheredHalo) {
    static const sk_sp<SkRuntimeEffect> composite =
        bloomProgram("phosphorBloom", "PhosphorComposite.sksl");
    // The reach the reduction is chosen from is the recipe's, or the
    // bound value where one drives it — a breathing radius picks its own
    // divisor rather than riding a stale one.
    float radius = 0;
    for (const auto& [name, value] : m_uniforms)
      if (name == "uRadius") radius = value;
    for (const auto& [name, out] : m_bound)
      if (name == "uRadius") radius = motion::resolveFloatAt(nullptr, out);
    return makePhosphorBloom(builder, composite, radius);
  }
  return SkImageFilters::RuntimeShader(builder, "content", nullptr);
}

bool Effect::isAnimated() const {
  // A bound block is a bound Output whose value is a table: read at every
  // paint, so the node must stay volatile for as long as it is attached.
  if (!m_blocks.empty()) return true;
  // A scalar binding counts only while it is LIVE: one holding a plain
  // number is a uniform value, and a node does not repaint forever for a
  // constant.
  for (const auto& [name, out] : m_bound)
    if (motion::isLive(nullptr, out)) return true;
  // Tier inheritance: a live child makes the whole effect live, so the node
  // is declared volatile and no cache can sample the parameter once and
  // freeze it. Material answers this question for its own subtree, so the
  // recursion stops at the child.
  for (const auto& [name, child] : m_children)
    if (child && child->isAnimated()) return true;
  return m_chainA && (m_chainA->isAnimated() || m_chainB->isAnimated());
}

bool Effect::usesWorldSpace() const {
  // Same tier-inheritance shape as isAnimated(): Material's own recursion
  // answers for blend layers and nested children.
  for (const auto& [name, child] : m_children)
    if (child && child->usesWorldSpace()) return true;
  return m_chainA && (m_chainA->usesWorldSpace() || m_chainB->usesWorldSpace());
}

/** Children compare by VALUE (Material::operator==, recursive), by the same
 *  rule material children follow: anything read live that did not
 *  participate in reconciler equality would leave a pruned node sampling
 *  the parameter its recording was made with. */
static bool childrenEqual(
    const std::vector<std::pair<std::string, std::shared_ptr<const Paint>>>& a,
    const std::vector<std::pair<std::string, std::shared_ptr<const Paint>>>&
        b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (a[i].first != b[i].first) return false;
    if (!a[i].second || !b[i].second) {
      if (a[i].second != b[i].second) return false;
      continue;
    }
    if (!(*a[i].second == *b[i].second)) return false;
  }
  return true;
}

bool Effect::operator==(const Effect& o) const {
  if (isAnimated() || o.isAnimated())
    return false;  // live never prunes — the material rule
  if (m_recipeSnapshot || o.m_recipeSnapshot) {
    if (!m_recipeSnapshot || !o.m_recipeSnapshot) return false;
    if (m_recipeSnapshot->program != o.m_recipeSnapshot->program) return false;
    if (!m_recipeSnapshot->material || !o.m_recipeSnapshot->material)
      return m_filter == o.m_filter;
    return *m_recipeSnapshot->material == *o.m_recipeSnapshot->material;
  }
  if (m_parametricBlur ||
      o.m_parametricBlur)  // blur(): by RECIPE + the sigma MAP
    return m_parametricBlur == o.m_parametricBlur &&
           childrenEqual(m_children, o.m_children);
  if (m_directionalBlur ||
      o.m_directionalBlur)  // directionalBlur(): by RECIPE, so a
    return m_directionalBlur ==
           o.m_directionalBlur;  // re-described equal one prunes
  if (m_effect || o.m_effect)
    return m_effect == o.m_effect && m_gatheredHalo == o.m_gatheredHalo &&
           m_uniforms == o.m_uniforms && m_uniforms2 == o.m_uniforms2 &&
           m_uniforms4 == o.m_uniforms4 &&
           m_uniformArrays == o.m_uniformArrays &&
           childrenEqual(m_children, o.m_children);
  // filter(): pointer identity, as ever, on both lanes — an already-built
  // filter of either kind carries no recipe to compare.
  return m_filter == o.m_filter && m_colorFilter == o.m_colorFilter;
}

}  // namespace sigil::material::skia
