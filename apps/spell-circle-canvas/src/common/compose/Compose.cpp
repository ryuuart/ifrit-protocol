// Element value builders — copy-on-write mutations of description payloads.
// Nothing here talks to Yoga, Skia surfaces, or Choreograph; that is the
// Composer's job.

#include <include/core/SkContourMeasure.h>
#include <include/core/SkImageFilter.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathUtils.h>
#include <include/core/SkShader.h>
#include <include/core/SkTypes.h>  // SkDebugf — the slot-rename diagnostic
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <include/pathops/SkPathOps.h>

#include <algorithm>
#include <cmath>   // std::isfinite — the profileOffset non-finite guard
#include <cstdio>  // std::snprintf — variationDrive's effect key
#include <set>

#include "ComposeInternal.h"
#include "sigilcompose/Lines.h"  // the ONE corner scanner (spans::corners)

namespace sigil::compose {

using detail::ElementNode;
using detail::Kind;

Fill Fill::shader(sk_sp<SkShader> s) {
  Fill f;
  f.kind = Fill::Kind::Shader;
  f.shaderValue = std::move(s);
  return f;
}

Element::Element() : m_node(std::make_shared<ElementNode>()) {}

ElementNode* Element::NodeHandle::operator->() {
  if (!value)
    value = std::make_shared<ElementNode>();
  else if (value.use_count() != 1)
    value = std::make_shared<ElementNode>(*value);
  return value.get();
}

const ElementNode* Element::NodeHandle::operator->() const {
  return value.get();
}

// ---- layout ---------------------------------------------------------------

Element& Element::row() {
  m_node->layout.row = true;
  return *this;
}
Element& Element::column() {
  m_node->layout.row = false;
  return *this;
}
Element& Element::wrapLines(bool on) {
  m_node->layout.wrap = on;
  return *this;
}
Element& Element::gap(float px) {
  m_node->layout.gap = px;
  return *this;
}
Element& Element::padding(float all) {
  m_node->layout.padding = {all, all, all, all};
  return *this;
}
Element& Element::padding(float h, float v) {
  m_node->layout.padding = {h, v, h, v};
  return *this;
}
Element& Element::padding(float l, float t, float r, float b) {
  m_node->layout.padding = {l, t, r, b};
  return *this;
}
Element& Element::margin(float all) {
  m_node->layout.margin = {all, all, all, all};
  return *this;
}
Element& Element::margin(float h, float v) {
  m_node->layout.margin = {h, v, h, v};
  return *this;
}
Element& Element::margin(float l, float t, float r, float b) {
  m_node->layout.margin = {l, t, r, b};
  return *this;
}
Element& Element::width(Dim d) {
  m_node->layout.width = d;
  return *this;
}
Element& Element::height(Dim d) {
  m_node->layout.height = d;
  return *this;
}
Element& Element::minWidth(Dim d) {
  m_node->layout.minWidth = d;
  return *this;
}
Element& Element::maxWidth(Dim d) {
  m_node->layout.maxWidth = d;
  return *this;
}
Element& Element::minHeight(Dim d) {
  m_node->layout.minHeight = d;
  return *this;
}
Element& Element::maxHeight(Dim d) {
  m_node->layout.maxHeight = d;
  return *this;
}
Element& Element::aspect(float r) {
  m_node->layout.aspect = r;
  return *this;
}
Element& Element::grow(float f) {
  m_node->layout.grow = f;
  return *this;
}
Element& Element::shrink(float f) {
  m_node->layout.shrink = f;
  return *this;
}
Element& Element::basis(Dim d) {
  m_node->layout.basis = d;
  return *this;
}
Element& Element::alignItems(Align a) {
  m_node->layout.alignItems = a;
  return *this;
}
Element& Element::alignSelf(Align a) {
  m_node->layout.alignSelf = a;
  return *this;
}
Element& Element::justify(Justify j) {
  m_node->layout.justify = j;
  return *this;
}
Element& Element::absolute() {
  m_node->layout.absolute = true;
  return *this;
}
Element& Element::inset(float all) { return inset(all, all, all, all); }
Element& Element::inset(float l, float t, float r, float b) {
  return inset(Dim(l), Dim(t), Dim(r), Dim(b));
}
Element& Element::inset(Dim l, Dim t, Dim r, Dim b) {
  m_node->layout.absolute = true;
  m_node->layout.hasInsets = true;
  m_node->layout.insets = {l, t, r, b};
  return *this;
}
Element& Element::left(Dim d) {
  m_node->layout.absolute = true;
  m_node->layout.hasInsets = true;
  m_node->layout.insets.left = d;
  return *this;
}
Element& Element::top(Dim d) {
  m_node->layout.absolute = true;
  m_node->layout.hasInsets = true;
  m_node->layout.insets.top = d;
  return *this;
}
Element& Element::right(Dim d) {
  m_node->layout.absolute = true;
  m_node->layout.hasInsets = true;
  m_node->layout.insets.right = d;
  return *this;
}
Element& Element::bottom(Dim d) {
  m_node->layout.absolute = true;
  m_node->layout.hasInsets = true;
  m_node->layout.insets.bottom = d;
  return *this;
}
Element& Element::centerAt(SkPoint p) {
  m_node->layout.absolute = true;
  m_node->layout.centerAt = p;
  return *this;
}
// rect()/at() go through the edge setters rather than writing LayoutProps
// themselves. That is the whole safety argument: they cannot describe a node
// the longhand could not, they touch no field the longhand does not, and
// they cannot drift from it when a setter changes. Keep them that way — the
// setters do more than assign (left/top also raise `absolute` and
// `hasInsets`), so a shortcut that wrote the fields directly would produce a
// node the longhand can never produce.
Element& Element::rect(const SkRect& r) {
  left(Dim(r.fLeft));
  top(Dim(r.fTop));
  width(Dim(r.width()));
  height(Dim(r.height()));
  return *this;
}
Element& Element::at(SkPoint topLeft) {
  left(Dim(topLeft.fX));
  top(Dim(topLeft.fY));
  return *this;
}

// ---- shape ----------------------------------------------------------------

Element& Element::corners(Corners c) {
  m_node->corners = c;
  return *this;
}
Element& Element::shape(Shape path) {
  m_node->shapeFn = std::move(path);
  return *this;
}
Element& Element::centered() {
  m_node->deriveData.ensure().bandFormation = Formation::Centered;
  return *this;
}
Element& Element::outward() {
  m_node->deriveData.ensure().bandFormation = Formation::Outward;
  return *this;
}
Element& Element::inward() {
  m_node->deriveData.ensure().bandFormation = Formation::Inward;
  return *this;
}
Element& Element::clip(bool on) {
  m_node->clipContent = on;
  return *this;
}

// ---- the masking family ---------------------------------------------------

Region Region::own() { return Region{}; }
Region Region::rect(const SkRect& r) {
  Region out;
  out.m_kind = Kind::Rect;
  out.m_rect = r;
  return out;
}
Region Region::oval(const SkRect& bounds) {
  Region out;
  out.m_kind = Kind::Oval;
  out.m_rect = bounds;
  return out;
}
Region Region::path(SkPath p) {
  Region out;
  out.m_kind = Kind::Path;
  out.m_path = std::move(p);
  return out;
}
bool Region::operator==(const Region& other) const {
  if (m_kind != other.m_kind) return false;
  switch (m_kind) {
    case Kind::Own:
      return true;
    case Kind::Rect:
    case Kind::Oval:
      return m_rect == other.m_rect;
    case Kind::Path:
      return m_path == other.m_path;
  }
  return false;
}
SkPath Region::resolve(const SkPath& ownShape) const {
  switch (m_kind) {
    case Kind::Own:
      return ownShape;
    case Kind::Rect: {
      SkPathBuilder b;
      b.addRect(m_rect);
      return b.detach();
    }
    case Kind::Oval: {
      SkPathBuilder b;
      b.addOval(m_rect);
      return b.detach();
    }
    case Kind::Path:
      return m_path;
  }
  return ownShape;
}

namespace parts {
Parts all() { return Parts{Parts::kAll, {}}; }
Parts marks() { return Parts{Parts::kMarks, {}}; }
Parts surface() { return Parts{Parts::kSurface, {}}; }
Parts content() { return Parts{Parts::kContent, {}}; }
Parts children() { return Parts{Parts::kChildren, {}}; }
Parts named(std::string_view name) {
  Parts p;
  p.names.emplace_back(name);
  return p;
}
}  // namespace parts

namespace by {
Gate spans(Spans where) {
  Gate g;
  g.kind = Gate::Kind::Spans;
  g.where = std::move(where);
  return g;
}
Gate edge(float angleDeg, Animatable<float> fraction) {
  Gate g;
  g.kind = Gate::Kind::Edge;
  g.angleDeg = angleDeg;
  g.fraction = std::move(fraction);
  return g;
}
Gate shape(Region r) {
  Gate g;
  g.kind = Gate::Kind::Shape;
  g.region = std::move(r);
  return g;
}
Gate outside(Region r) {
  Gate g = shape(std::move(r));
  g.outside = true;
  return g;
}
Gate alpha(Material coverage) {
  Gate g;
  g.kind = Gate::Kind::Coverage;
  g.coverage = std::make_shared<const Material>(std::move(coverage));
  return g;
}
Gate alphaOut(Material coverage) {
  Gate g = alpha(std::move(coverage));
  g.outside = true;
  return g;
}
Gate luma(Material coverage) {
  Gate g = alpha(std::move(coverage));
  g.channel = Gate::Channel::Luma;
  return g;
}
Gate lumaOut(Material coverage) {
  Gate g = luma(std::move(coverage));
  g.outside = true;
  return g;
}
}  // namespace by

size_t Gate::valueCount() const {
  switch (kind) {
    case Kind::Spans:
      return where.valueCount();
    case Kind::Edge:
      return 1;
    case Kind::Shape:
    case Kind::Coverage:
      return 0;
  }
  return 0;
}

Element& Element::mask(Gate with) {
  return mask(parts::all(), std::move(with));
}
Element& Element::mask(Parts what, Gate with) {
  // A fit() term inside a span GATE borrows another element's resolved box
  // exactly as one inside a span PASS does, so the keys ride into
  // DeriveData where the ONE derive-registration walk finds them.
  if (with.kind == Gate::Kind::Spans)
    for (const Spans::Term& t : with.where.terms)
      if (t.rule == Spans::Rule::Fit && !t.key.empty())
        m_node->deriveData.ensure().spanFitKeys.push_back(t.key);
  m_node->fxData.ensure().masks.push_back(
      Mask{std::move(what), std::move(with)});
  return *this;
}

// ---- paint ----------------------------------------------------------------

Element& Element::fill(Animatable<Fill> f) {
  m_node->paint.fill = std::move(f);
  // Symmetric with fill(Material): the fill setters are last-wins — a plain
  // fill after a live-material fill must actually take effect (and release
  // the node from the live-volatile path). staticMaterial must drop too, or
  // a stale equal-comparing recipe would over-prune this new fill.
  // Dropping the WHOLE block (not just its members) keeps propsEqual's
  // block-presence check aligned with a node that never had a material.
  m_node->materialData = {};
  return *this;
}
Effect Effect::filter(sk_sp<SkImageFilter> f) {
  Effect e;
  e.m_filter = std::move(f);
  return e;
}

Effect Effect::shader(sk_sp<SkRuntimeEffect> effect,
                      std::vector<std::pair<std::string, float>> uniforms) {
  Effect e;
  if (!effect) return e;
  SkRuntimeShaderBuilder builder(effect);
  for (const auto& [name, value] : uniforms)
    builder.uniform(name.c_str()) = value;
  e.m_filter = SkImageFilters::RuntimeShader(builder, "content", nullptr);
  e.m_effect = std::move(effect);
  e.m_uniforms = std::move(uniforms);
  return e;
}

namespace {
/** directionalBlur's filter, built from stock Skia filters rather than a
 *  hand-written kernel: the separable Gaussian stays Skia's, which is both
 *  faster than an SkSL rewrite and pixel-identical to a caller who was
 *  already spelling this out by hand.
 *
 *  An axis-aligned angle IS SkImageFilters::Blur with the sigmas swapped.
 *  Any other angle is a rotate → Blur → unrotate sandwich: three filter
 *  nodes, with bounds handled by the filter graph rather than by us. */
sk_sp<SkImageFilter> makeDirectionalBlur(float sigma, float angleDeg,
                                         float across) {
  float axis = std::fmod(angleDeg, 180.0f);  // a blur axis has no sign
  if (axis < 0) axis += 180.0f;
  if (axis == 0.0f) return SkImageFilters::Blur(sigma, across, nullptr);
  if (axis == 90.0f) return SkImageFilters::Blur(across, sigma, nullptr);
  const SkSamplingOptions sampling(SkFilterMode::kLinear);
  sk_sp<SkImageFilter> aligned = SkImageFilters::MatrixTransform(
      SkMatrix::RotateDeg(-angleDeg), sampling, nullptr);
  sk_sp<SkImageFilter> blurred =
      SkImageFilters::Blur(sigma, across, std::move(aligned));
  return SkImageFilters::MatrixTransform(SkMatrix::RotateDeg(angleDeg),
                                         sampling, std::move(blurred));
}
}  // namespace

Effect Effect::directionalBlur(float sigma, float angleDeg, float across) {
  Effect e;
  e.m_dirBlur = DirectionalBlur{sigma, angleDeg, across};
  e.m_filter = makeDirectionalBlur(sigma, angleDeg, across);
  return e;
}

namespace {
/** THE PYRAMID, and the whole of blur()'s recipe: a
 *  small fixed number of CONSTANT-sigma blurs of the layer, blended by the
 *  parameter. Skia's Gaussian is separable and its cost is a function of
 *  sigma; a spatially-varying sigma is NOT separable, so an author-written
 *  kernel pays the worst radius at every pixel. Three fixed levels
 *  (0, maxSigma/2, maxSigma) plus one mix pass is a constant number of
 *  full-resolution passes no matter how wide the range gets.
 *
 *  Branch-free because both nested mixes are exact at the level sigmas:
 *  t=0 → level0, t=1 → level1, t=2 → level2, and linear in sigma between.
 *  The level COUNT is deliberately not in the API — see Effect::blur. */
sk_sp<SkRuntimeEffect> paramBlurMix() {
  static const sk_sp<SkRuntimeEffect> fx = [] {
    auto [effect, error] = SkRuntimeEffect::MakeForShader(
        SkString("uniform shader level0;"
                 "uniform shader level1;"
                 "uniform shader level2;"
                 "uniform shader param;"
                 "half4 main(float2 p) {"
                 "  float t = clamp(param.eval(p).r, 0.0, 1.0) * 2.0;"
                 "  half4 lo = mix(level0.eval(p), level1.eval(p),"
                 "                 half(clamp(t, 0.0, 1.0)));"
                 "  return mix(lo, level2.eval(p),"
                 "             half(clamp(t - 1.0, 0.0, 1.0)));"
                 "}"));
    if (!effect)
      SkDebugf("[compose] Effect::blur: mix shader failed: %s\n",
               error.c_str());
    return effect;
  }();
  return fx;
}

/** blur()'s filter DAG. The intermediates are per-draw image-filter
 *  surfaces inside the effect's ONE saveLayer — the same place every
 *  effect intermediate already lives — so there is nothing new to
 *  invalidate. A null input means "the source", which is level 0. */
sk_sp<SkImageFilter> makeParamBlur(float maxSigma, sk_sp<SkShader> sigmaMap) {
  const sk_sp<SkRuntimeEffect> fx = paramBlurMix();
  if (!sigmaMap || !fx || !(maxSigma > 0)) {
    // No map or no SkSL: the honest fallback is the constant blur the
    // parameter would have modulated, which is also what maxSigma == 0
    // means (no blur anywhere).
    return maxSigma > 0 ? SkImageFilters::Blur(maxSigma, maxSigma, nullptr)
                        : nullptr;
  }
  SkRuntimeShaderBuilder b(fx);
  b.child("param") = std::move(sigmaMap);
  std::string_view names[3] = {"level0", "level1", "level2"};
  const sk_sp<SkImageFilter> inputs[3] = {
      nullptr,  // level 0 IS the layer, unblurred
      SkImageFilters::Blur(maxSigma * 0.5f, maxSigma * 0.5f, nullptr),
      SkImageFilters::Blur(maxSigma, maxSigma, nullptr)};
  return SkImageFilters::RuntimeShader(b, names, inputs, 3);
}
}  // namespace

Effect Effect::blur(Material sigmaMap, float maxSigma) {
  Effect e;
  e.m_paramBlur = ParamBlur{maxSigma};
  e.m_children.emplace_back(
      "sigma", std::make_shared<const Material>(std::move(sigmaMap)));
  // The static snapshot, built context-free exactly as Material::child
  // refreshes m_shader at store time; a context-needing map rebuilds this
  // per paint in resolvedImageFilter().
  e.m_filter = e.buildFilter(nullptr);
  return e;
}

Effect& Effect::child(std::string name, Material source) {
  // Which names this effect kind can fill — Material::child's structure,
  // one branch per kind, warn-and-ignore everywhere else.
  if (m_paramBlur) {
    if (name != "sigma") {
      SkDebugf(
          "[compose] Effect::child(\"%s\") on a blur() — its one child "
          "is \"sigma\", the map; ignored\n",
          name.c_str());
      return *this;
    }
  } else if (m_effect) {
    if (name == "content") {
      SkDebugf(
          "[compose] Effect::child(\"content\"): ignored — \"content\" "
          "is the node's own rendered layer, filled by the library\n");
      return *this;
    }
    if (!detail::declaresShaderChild(m_effect, name)) {
      SkDebugf(
          "[compose] Effect::child: \"%s\" is not declared by the effect "
          "as `uniform shader` — ignored\n",
          name.c_str());
      return *this;
    }
  } else {
    SkDebugf(
        "[compose] Effect::child(\"%s\"): ignored — this effect has no "
        "shader children to fill (only shader() and blur() do)\n",
        name.c_str());
    return *this;
  }
  auto held = std::make_shared<const Material>(std::move(source));
  // Last write wins on a name, like Material::child: re-filling a slot
  // replaces rather than stacking two entries the builder would both
  // assign.
  bool replaced = false;
  for (auto& slot : m_children)
    if (slot.first == name) {
      slot.second = std::move(held);
      replaced = true;
      break;
    }
  if (!replaced) m_children.emplace_back(std::move(name), std::move(held));
  // Refresh the static snapshot (Material::child does the same). Note this
  // must be an UNCONDITIONAL rebuild: resolvedImageFilter(nullptr) would
  // hand back the snapshot it is meant to replace, and a STATIC child on a
  // shader() effect — one the paint path has no reason to re-resolve —
  // would then never reach the filter at all.
  m_filter = buildFilter(nullptr);
  return *this;
}

bool Effect::anyChildNeedsContext() const {
  for (const auto& [name, child] : m_children)
    if (child && (child->isAnimated() || child->geometryDependent()))
      return true;
  return false;
}

sk_sp<SkShader> Effect::childShaderFor(std::string_view name,
                                       const PaintContext* ctx) const {
  for (const auto& [slot, child] : m_children)
    if (slot == name) return child ? detail::childShader(*child, ctx) : nullptr;
  return nullptr;
}

Effect& Effect::uniform(std::string name,
                        const choreograph::Output<float>* value) {
  // Every dropped binding says so — Material's guardrail: warn and ignore,
  // never a debug abort (one sketch typo must not kill the hot-reload
  // host). A silent drop here loses an animation with no diagnostic.
  if (!value) {
    SkDebugf(
        "[compose] Effect::uniform(\"%s\"): null Output — there is "
        "nothing to read at paint time; ignored\n",
        name.c_str());
    return *this;
  }
  if (m_dirBlur) {
    // The recipe's named parameters — anything else warns and is ignored.
    if (name != "sigma" && name != "angle" && name != "across") {
      SkDebugf(
          "[compose] Effect::uniform(\"%s\") on a directionalBlur() — "
          "not one of \"sigma\"/\"angle\"/\"across\"; ignored\n",
          name.c_str());
      return *this;
    }
    m_bound.emplace_back(std::move(name), value);
    return *this;
  }
  if (m_paramBlur) {
    if (name != "maxSigma") {
      SkDebugf(
          "[compose] Effect::uniform(\"%s\") on a blur() — its one "
          "parameter is \"maxSigma\" (the MAP is child(\"sigma\", "
          "Material)); ignored\n",
          name.c_str());
      return *this;
    }
    m_bound.emplace_back(std::move(name), value);
    return *this;
  }
  if (m_effect) {
    m_bound.emplace_back(std::move(name), value);
    return *this;
  }
  SkDebugf(
      "[compose] Effect::uniform(\"%s\"): ignored — this effect has no "
      "uniform to receive it (only shader(), directionalBlur() and "
      "blur() do; a filter() wraps an already-built SkImageFilter)\n",
      name.c_str());
  return *this;
}

Effect Effect::then(const Effect& next) const {
  Effect e;
  const bool thisReal = m_filter || isAnimated();
  const bool nextReal = next.m_filter || next.isAnimated();
  if (!thisReal) return next;
  if (!nextReal) return *this;
  if (isAnimated() || next.isAnimated()) {
    // A live side cannot precompose: hold both and re-compose per paint.
    e.m_chainA = std::make_shared<const Effect>(*this);
    e.m_chainB = std::make_shared<const Effect>(next);
    return e;
  }
  e.m_filter = SkImageFilters::Compose(next.m_filter, m_filter);
  return e;
}

sk_sp<SkImageFilter> Effect::resolvedImageFilter(
    const PaintContext* ctx) const {
  if (m_chainA)
    return SkImageFilters::Compose(m_chainB->resolvedImageFilter(ctx),
                                   m_chainA->resolvedImageFilter(ctx));
  // A context-needing child (live or geometry tier) has to be re-resolved
  // per paint; a static one is already in the snapshot. Same question
  // Material::build's memo asks of its children, same answer.
  if (m_bound.empty() && !(ctx && anyChildNeedsContext())) return m_filter;
  return buildFilter(ctx);
}

/** THE FILTER, built from the recipe — unconditionally, which is what
 *  separates it from resolvedImageFilter(): the store-time snapshot and the
 *  per-paint resolve are the SAME construction differing only in whether
 *  there is a context, exactly as Material::build(live, ctx) is. */
sk_sp<SkImageFilter> Effect::buildFilter(const PaintContext* ctx) const {
  if (m_paramBlur) {  // rebuild the pyramid from the parameter and the map
    float maxSigma = m_paramBlur->maxSigma;
    for (const auto& [name, out] : m_bound)
      if (name == "maxSigma") maxSigma = out->value();
    return makeParamBlur(maxSigma, childShaderFor("sigma", ctx));
  }
  if (m_dirBlur) {  // rebuild the sandwich from the bound parameters
    DirectionalBlur d = *m_dirBlur;
    for (const auto& [name, out] : m_bound) {
      if (name == "sigma")
        d.sigma = out->value();
      else if (name == "angle")
        d.angleDeg = out->value();
      else if (name == "across")
        d.across = out->value();
    }
    return makeDirectionalBlur(d.sigma, d.angleDeg, d.across);
  }
  if (!m_effect) return m_filter;
  SkRuntimeShaderBuilder builder(m_effect);
  for (const auto& [name, value] : m_uniforms)
    builder.uniform(name.c_str()) = value;
  for (const auto& [name, out] : m_bound)
    builder.uniform(name.c_str()) = out->value();
  // The child slots, against the painting node's box (Material::child's
  // contract: a child sees the SAME PaintContext, because there is one
  // node). "content" is the library's and is filled by the factory below.
  for (const auto& [name, child] : m_children)
    if (child) builder.child(name.c_str()) = detail::childShader(*child, ctx);
  return SkImageFilters::RuntimeShader(builder, "content", nullptr);
}

bool Effect::isAnimated() const {
  if (!m_bound.empty()) return true;
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
    const std::vector<std::pair<std::string, std::shared_ptr<const Material>>>&
        a,
    const std::vector<std::pair<std::string, std::shared_ptr<const Material>>>&
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
    return false;                    // live never prunes — the material rule
  if (m_paramBlur || o.m_paramBlur)  // blur(): by RECIPE + the sigma MAP
    return m_paramBlur == o.m_paramBlur &&
           childrenEqual(m_children, o.m_children);
  if (m_dirBlur || o.m_dirBlur)       // directionalBlur(): by RECIPE, so a
    return m_dirBlur == o.m_dirBlur;  // re-described equal one prunes
  if (m_effect || o.m_effect)
    return m_effect == o.m_effect && m_uniforms == o.m_uniforms &&
           childrenEqual(m_children, o.m_children);
  return m_filter == o.m_filter;  // filter(): pointer identity, as ever
}

Element& Element::hitTestable(bool enabled) {
  m_node->hitTestable = enabled;
  return *this;
}

/** Register whatever a decoration says it borrows (BorrowingDecoration) so
 *  the derive pass resolves it. EVERY slot that accepts a Decoration must
 *  route through here: a borrow honoured on some slots and not others fails
 *  silently — the decoration draws with nothing borrowed and says why on no
 *  channel — and the difference between the slots is invisible to the
 *  author. */
void Element::claimBorrows(const Decoration& d) {
  if (d.borrows().empty()) return;
  detail::DeriveData& derive = m_node->deriveData.ensure();
  for (const std::string& key : d.borrows())
    derive.borrowedPathKeys.push_back(key);
}

/** Bind a LOCAL label to the mark at (slot, index), so `parts::named()`
 *  can address it. `slot` is a detail::MarkSlot as an int, for the same
 *  reason addSpanPass takes its half that way. An empty name costs
 *  nothing — the vector stays absent on the overwhelming majority of
 *  nodes, which is why the labels are a side list and not a field beside
 *  every Decoration. */
void Element::labelMark(int slot, size_t index, std::string name) {
  if (name.empty()) return;
  m_node->fxData.ensure().markNames.push_back(detail::MarkLabel{
      (detail::MarkSlot)slot, (uint32_t)index, std::move(name)});
}

Element& Element::overlay(Decoration d, std::string name) {
  claimBorrows(d);
  const size_t index = m_node->fxData.ensure().overlays.size();
  m_node->fxData->overlays.push_back(std::move(d));
  labelMark((int)detail::MarkSlot::Overlay, index, std::move(name));
  return *this;
}
Element& Element::background(Decoration d, std::string name) {
  claimBorrows(d);
  const size_t index = m_node->backgrounds.size();
  m_node->backgrounds.push_back(std::move(d));
  labelMark((int)detail::MarkSlot::Background, index, std::move(name));
  return *this;
}
Element& Element::foreground(Decoration d, std::string name) {
  claimBorrows(d);
  const size_t index = m_node->foregrounds.size();
  m_node->foregrounds.push_back(std::move(d));
  labelMark((int)detail::MarkSlot::Foreground, index, std::move(name));
  return *this;
}
Element& Element::stroke(Decoration brush, std::string name) {
  return foreground(std::move(brush), std::move(name));
}
Element& Element::stroke(Spans where, Decoration what, std::string name) {
  return addSpanPass(std::move(where), std::move(what), std::move(name),
                     (int)detail::StrokePass::Half::Foreground);
}
Element& Element::background(Spans where, Decoration what, std::string name) {
  return addSpanPass(std::move(where), std::move(what), std::move(name),
                     (int)detail::StrokePass::Half::Background);
}
/** The one body both span-qualified slots share — see StrokePass: the two
 *  halves differ only in where the mark lands, so everything upstream of
 *  the paint (the fit() borrows, the claim ledger, the pass list) is one
 *  thing and must stay one thing. */
Element& Element::addSpanPass(Spans where, Decoration what, std::string name,
                              int half) {
  // A fit() term borrows another element's resolved box, so the keys ride
  // into DeriveData where the ONE derive-registration walk finds them —
  // the flowAround pattern, not a second phase.
  for (const Spans::Term& t : where.terms)
    if (t.rule == Spans::Rule::Fit && !t.key.empty())
      m_node->deriveData.ensure().spanFitKeys.push_back(t.key);
  claimBorrows(what);
  m_node->strokeData.ensure().passes.push_back(
      detail::StrokePass{std::move(where), std::move(what), std::move(name),
                         (detail::StrokePass::Half)half});
  return *this;
}
Element& Element::echo(SkVector offset, SkColor4f color) {
  m_node->fxData.ensure().echoes.push_back(Echo{offset, color});
  return *this;
}
Element& Element::style(LayerStyle s) {
  for (Decoration& d : s.under) {
    claimBorrows(d);
    m_node->backgrounds.push_back(std::move(d));
  }
  for (Decoration& d : s.over) {
    claimBorrows(d);
    m_node->foregrounds.push_back(std::move(d));
  }
  return *this;
}
Element& Element::effect(Effect e) {
  m_node->fxData.ensure().layerEffect = std::move(e);
  return *this;
}
Element& Element::backdrop(Effect e) {
  m_node->fxData.ensure().backdropEffect = std::move(e);
  return *this;
}
Element& Element::opacity(Animatable<float> o) {
  m_node->paint.opacity = std::move(o);
  return *this;
}
Element& Element::blend(SkBlendMode mode) {
  m_node->paint.blendMode = mode;
  return *this;
}
Element& Element::translateX(Animatable<float> v) {
  m_node->paint.translateX = std::move(v);
  return *this;
}
Element& Element::translateY(Animatable<float> v) {
  m_node->paint.translateY = std::move(v);
  return *this;
}
Element& Element::travel(MotionPath along) {
  m_node->motionData.ensure() = std::move(along);
  return *this;
}
Element& Element::rotate(Animatable<float> v) {
  m_node->paint.rotate = std::move(v);
  return *this;
}
Element& Element::scale(Animatable<float> v) {
  m_node->paint.scale = std::move(v);
  return *this;
}
Element& Element::textStroke(float width, Fill fill) {
  auto& t = m_node->textData.ensure();
  t.hasTextStroke = width > 0.0f;
  t.textStrokeWidth = width;
  t.textStrokeFill = std::move(fill);
  return *this;
}

Element& Element::onPath(TextPath spec) {
  m_node->textData.ensure().onPath = std::move(spec);
  return *this;
}
Element& Element::scaleX(Animatable<float> v) {
  m_node->paint.scaleX = std::move(v);
  return *this;
}
Element& Element::scaleY(Animatable<float> v) {
  m_node->paint.scaleY = std::move(v);
  return *this;
}
Element& Element::skewX(Animatable<float> v) {
  m_node->paint.skewX = std::move(v);
  return *this;
}
Element& Element::skewY(Animatable<float> v) {
  m_node->paint.skewY = std::move(v);
  return *this;
}
Element& Element::transformOrigin(float fx, float fy) {
  m_node->paint.originX = fx;
  m_node->paint.originY = fy;
  m_node->paint.originPx = false;
  return *this;
}
Element& Element::transformOriginPx(SkPoint p) {
  m_node->paint.originX = p.x();
  m_node->paint.originY = p.y();
  m_node->paint.originPx = true;
  return *this;
}
Element& Element::zIndex(int z) {
  m_node->paint.zIndex = z;
  return *this;
}

// ---- identity, caching, transitions --------------------------------------

Element& Element::key(std::string_view k) {
  // A slot's NAME is its key — one field, two spellings — so this call
  // RENAMES the mount, and renderSlot() on the original name then finds
  // nothing and leaves the slot laying out at zero on its content axis.
  // renderSlot warns about the same trap from the other side; this warning
  // fires where the caller still has BOTH names in hand, which is what
  // makes it actionable.
  if (m_node->kind == Kind::Slot && !m_node->key.empty() && m_node->key != k) {
    static std::set<std::string> warned;  // once per rename, not per frame
    if (warned.insert(m_node->key + "->" + std::string(k)).second)
      SkDebugf(
          "[compose] .key(\"%.*s\") on slot(\"%s\") RENAMES the slot: "
          "renderSlot(\"%s\") will no longer find it and the mount will "
          "lay out at zero on its content axis. A slot is named once, "
          "by slot().\n",
          (int)k.size(), k.data(), m_node->key.c_str(), m_node->key.c_str());
  }
  m_node->key = std::string(k);
  return *this;
}
Element& Element::cache(Cache c) {
  m_node->cacheMode = c;
  return *this;
}
Element& Element::bakeScale(float factor) {
  m_node->bakeScale = std::clamp(factor, 0.1f, 1.0f);
  return *this;
}
Element& Element::transition(Transition t) {
  m_node->nodeTransition = std::move(t);
  return *this;
}
Element& Element::staggerChildren(std::chrono::milliseconds each,
                                  Stagger::From from) {
  detail::FxData& fx = m_node->fxData.ensure();
  fx.staggerChildrenMs = (float)each.count();
  fx.staggerFrom = from;
  return *this;
}

Element& Element::child(Element e) {
  m_node->children.push_back(std::move(e));
  return *this;
}

// ---- factories ------------------------------------------------------------

Element box() { return {}; }

Element stack() {
  Element e;
  e.node()->kind = Kind::Stack;
  return e;
}

Element positioned() {
  Element e;
  e.node()->layout.positioned = true;
  return e;
}

Element text(std::u8string utf8, sigil::weave::TextStyle style) {
  Element e;
  e.node()->kind = Kind::Text;
  detail::TextData& text = e.node()->textData.ensure();
  text.utf8 = std::move(utf8);
  text.style = std::move(style);
  // The box fits the type: measured text must not stretch on the cross
  // axis. That demotion is NOT applied here — it happens when layout props
  // are written to Yoga, where the alignment this leaf actually resolved to
  // (its own, or its parent's) is known, so a parent's alignItems(Center)
  // or alignItems(End) still reaches text leaves untouched.
  return e;
}

Element text(RichText spans) {
  Element e;
  e.node()->kind = Kind::Text;
  detail::TextData& text = e.node()->textData.ensure();
  // The base rides along as `style` because everything downstream that asks
  // a text leaf what it is set in — the strut a line height comes from, the
  // metric band textFill() maps into — reads one style, and a mixed
  // paragraph's answer to that question is the style its unstyled runs use.
  text.style = spans.base();
  text.rich = std::move(spans);
  return e;
}

Element text(std::shared_ptr<sigil::weave::Paragraph> paragraph,
             sigil::weave::ParagraphLayoutOptions options) {
  Element e;
  e.node()->kind = Kind::Text;
  detail::TextData& text = e.node()->textData.ensure();
  text.paragraphOverride = std::move(paragraph);
  text.layoutOptions = std::move(options);
  return e;
}

// ---------------------------------------------------------------------------
// rich() — mixed text as a value

RichText rich(sigil::weave::TextStyle base) {
  return RichText(std::move(base));
}

RichText& RichText::add(std::u8string_view utf8) {
  m_runs.push_back(Run{std::u8string(utf8), m_base, {}});
  return *this;
}

RichText& RichText::add(std::u8string_view utf8,
                        sigil::weave::TextStyle style) {
  m_runs.push_back(Run{std::u8string(utf8), std::move(style), {}});
  return *this;
}

RichText& RichText::add(std::u8string_view utf8, std::string_view styleName) {
  // The inherited set is captured on the FIRST named run rather than at
  // rich(), so an unnamed value costs nothing and the capture still happens
  // inside the author's describe scope. styles() overrides it whichever way
  // round the two are written.
  if (!m_hasStyles && !m_stylesExplicit) {
    if (const sigil::weave::StyleSet* ambient =
            env::inherited<sigil::weave::StyleSet>()) {
      m_styles = *ambient;
      m_hasStyles = true;
    }
  }
  Run run{std::u8string(utf8), m_base, std::string(styleName)};
  if (m_hasStyles) {
    // find(), not operator[]: an unregistered name resolves to the base
    // handed to rich(), which is this text's one default.
    if (const sigil::weave::TextStyle* named = m_styles.find(run.styleName))
      run.style = *named;
  }
  m_runs.push_back(std::move(run));
  return *this;
}

RichText& RichText::slot(std::string key, SkSize size, float baselineDrop) {
  Run run;
  // U+FFFC OBJECT REPLACEMENT CHARACTER. The slot is CONTENT: it occupies
  // one code point, so it counts as a cluster, falls inside the ranges a
  // selector names, and takes its beat in a cascade exactly as a letter
  // does. The engine matches its reserved box to this occurrence by order.
  run.utf8 = u8"￼";
  run.style = m_base;
  run.slotKey = std::move(key);
  run.slotSize = size;
  run.slotBaselineDrop = baselineDrop;
  m_runs.push_back(std::move(run));
  return *this;
}

RichText& RichText::styles(sigil::weave::StyleSet set) {
  m_styles = std::move(set);
  m_hasStyles = true;
  m_stylesExplicit = true;
  for (Run& run : m_runs) {
    if (run.styleName.empty()) continue;
    const sigil::weave::TextStyle* named = m_styles.find(run.styleName);
    run.style = named ? *named : m_base;
  }
  return *this;
}

Element image(std::shared_ptr<const sigil::image::ImageAsset> asset) {
  Element e;
  e.node()->kind = Kind::Image;
  e.node()->imageData.ensure().asset = std::move(asset);
  return e;
}

Element& Element::sampling(SkSamplingOptions options) {
  m_node->imageData.ensure().sampling = options;
  return *this;
}

Element& Element::region(SkRect sourceRect) {
  m_node->imageData.ensure().region = sourceRect;
  return *this;
}

Element custom(PaintProgram program) {
  Element e;
  e.node()->kind = Kind::Custom;
  e.node()->customData.ensure().program = std::move(program);
  return e;
}

Element custom(std::string_view key, PaintProgram program) {
  Element e = custom(std::move(program));
  e.node()->customData->key = std::string(key);
  return e;
}

Element& Element::fx(Track track) {
  m_node->textData.ensure().tracks.push_back(std::move(track));
  return *this;
}

Element& Element::variationDrive(const char (&tag)[5],
                                 const choreograph::Output<float>* value) {
  // SUGAR over fx(): an axis coordinate is a per-glyph deviation like a
  // shove or a fade, so the drive is a whole-text track and composes with
  // whatever other tracks the element carries. A second, parallel text path
  // is what it used to be, and a track drawn over it hid it completely.
  //
  // The effect reads the Output DIRECTLY rather than through the track's
  // progress, because an axis coordinate is a design-space number (GRAD
  // runs to ±100 on the faces that have it) and a progress is a 0→1 ramp
  // the cascade clamps. The progress is bound to the same Output for the
  // one thing it is good for here: declaring the paint volatility, so the
  // node repaints while the drive moves and settles when it stops.
  const sigil::weave::FontVariation coordinate(tag, 0.0f);
  // The effect's key IS its identity, and a drive is identified by its axis
  // and by WHICH Output feeds it — the binding identity every bound value
  // in the tree is compared by. Two drives of one axis from two Outputs
  // must not prune onto each other.
  char key[64];
  std::snprintf(key, sizeof(key), "variationDrive:%.4s@%p", tag,
                (const void*)value);
  Track track;
  track.effect = TextEffect(
      key, {},
      [coordinate, value](const GlyphInfo&, float, Rng&) {
        GlyphMod mod;
        if (!value) return mod;
        sigil::weave::FontVariation driven = coordinate;
        driven.value = value->value();
        mod.axis = driven;
        return mod;
      },
      /*reach=*/0.0f);
  track.progress = value;
  m_node->textData.ensure().tracks.push_back(std::move(track));
  return *this;
}

Element& Element::textAlign(sigil::weave::TextAlignment a) {
  detail::TextOptions& options = m_node->textData.ensure().options;
  options.alignment = a;
  options.set |= detail::TextOptions::kAlignment;
  return *this;
}

Element& Element::writingMode(sigil::weave::WritingMode mode) {
  detail::TextOptions& options = m_node->textData.ensure().options;
  options.writingMode = mode;
  options.set |= detail::TextOptions::kWritingMode;
  return *this;
}

Element& Element::lineBreak(sigil::weave::LineBreakStrategy strategy) {
  detail::TextOptions& options = m_node->textData.ensure().options;
  options.lineBreak = strategy;
  options.set |= detail::TextOptions::kLineBreak;
  return *this;
}

Element& Element::hyphenation(sigil::weave::HyphenationOptions spec) {
  detail::TextOptions& options = m_node->textData.ensure().options;
  options.hyphenation = spec;
  options.set |= detail::TextOptions::kHyphenation;
  return *this;
}

Element& Element::ellipsis(std::u8string_view marker) {
  detail::TextOptions& options = m_node->textData.ensure().options;
  options.ellipsis = detail::toUtf16(marker);
  options.set |= detail::TextOptions::kEllipsis;
  return *this;
}

Element& Element::maxLines(int lines) {
  detail::TextOptions& options = m_node->textData.ensure().options;
  options.maxLines = lines;
  options.set |= detail::TextOptions::kMaxLines;
  return *this;
}

Element& Element::lastLine(sigil::weave::TextAlignment alignment,
                           bool justify) {
  detail::TextOptions& options = m_node->textData.ensure().options;
  options.lastLineAlignment = alignment;
  options.justifyLastLine = justify;
  options.set |= detail::TextOptions::kLastLine;
  return *this;
}

Element& Element::spanPaint(Selector where, sigil::weave::PaintStyle paint) {
  detail::SpanRestyle restyle;
  restyle.where = std::move(where);
  restyle.style.paint = std::move(paint);
  restyle.paintOnly = true;
  m_node->textData.ensure().spanRestyles.push_back(std::move(restyle));
  return *this;
}

Element& Element::spanStyle(Selector where, sigil::weave::TextStyle style) {
  detail::SpanRestyle restyle;
  restyle.where = std::move(where);
  restyle.style = std::move(style);
  m_node->textData.ensure().spanRestyles.push_back(std::move(restyle));
  return *this;
}

Element& Element::flowAround(std::string_view key, float margin) {
  detail::DeriveData& derive = m_node->deriveData.ensure();
  derive.flowAroundKeys.push_back(std::string(key));
  derive.flowAroundMargin = margin;
  return *this;
}

Element connector(std::string_view fromKey, std::string_view toKey,
                  Router router, float gap) {
  Element e;
  e.node()->kind = Kind::Custom;  // painted via derive-resolved outline
  detail::DeriveData& derive = e.node()->deriveData.ensure();
  derive.connectFrom = std::string(fromKey);
  derive.connectTo = std::string(toKey);
  derive.router = std::move(router);
  derive.connectorGap = gap;
  return e;
}

Element rail(std::vector<Anchor> anchors, RailRouter router) {
  Element e;
  e.node()->kind = Kind::Custom;  // painted via the derive-routed outline
  detail::DeriveData& derive = e.node()->deriveData.ensure();
  derive.railAnchors = std::move(anchors);
  derive.railRouter = std::move(router);
  return e;
}

Element slot(std::string_view name) {
  Element e;
  e.node()->kind = Kind::Slot;
  e.node()->key = std::string(name);
  return e;
}

namespace detail {
Element makeLayout(
    std::function<std::vector<SkRect>(const LayoutInput&)> place) {
  Element e;
  e.node()->deriveData.ensure().placeFn = std::move(place);
  return e;
}

Element makeMemo(std::any props,
                 std::function<bool(const std::any&, const std::any&)> equal,
                 std::function<Element(const std::any&)> invoke) {
  Element e;
  detail::MemoData& memo = e.node()->memoData.ensure();
  memo.props = std::move(props);
  memo.equal = std::move(equal);
  memo.invoke = std::move(invoke);
  // Captured HERE, in the author's scope — the whole point. By the time
  // the reconciler decides whether to call `invoke`, this stack is gone.
  memo.env = envStack();
  return e;
}

// ---- env: the describe-time ambient stack (see Compose.h "env") ----------

EnvSnapshot& envStack() {
  static thread_local EnvSnapshot stack;
  return stack;
}

bool envEqual(const EnvSnapshot& a, const EnvSnapshot& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (a[i].type != b[i].type) return false;
    if (a[i].value == b[i].value)
      continue;  // the same binding object: equal without asking
    if (!a[i].equal || !a[i].value || !b[i].value) return false;
    if (!a[i].equal(a[i].value.get(), b[i].value.get())) return false;
  }
  return true;
}

EnvRestore::EnvRestore(const EnvSnapshot& snapshot) {
  EnvSnapshot next = snapshot;  // copied first: `snapshot` may alias the stack
  m_saved = std::move(envStack());
  envStack() = std::move(next);
}

EnvRestore::~EnvRestore() { envStack() = std::move(m_saved); }

}  // namespace detail

// ---------------------------------------------------------------------------
// Spans: the boundary's arc length, in claimed runs
//
// Every answer is in ONE normal form (clamped, sorted, merged, non-empty)
// so overlap tests and complements are plain interval arithmetic. Any rule
// added here must normalize too: the alternative is per-rule special cases
// in every consumer, which is exactly how one rule ends up behaving subtly
// differently from its neighbours.

namespace {

/** Where each contour starts and ends in the path's GLOBAL arc length.
 *  Global, not per-contour, because that is what SkTrimPathEffect uses
 *  and therefore what trim() has always meant: a reveal and a trim of the
 *  same numbers must describe the same run. */
struct ContourRun {
  float start = 0, length = 0;
  bool closed = false;
};

std::vector<ContourRun> measureContours(const SkPath& path, float* total) {
  std::vector<ContourRun> runs;
  float at = 0;
  SkContourMeasureIter iter(path, false);
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    const float len = contour->length();
    runs.push_back({at, len, contour->isClosed()});
    at += len;
  }
  if (total) *total = at;
  return runs;
}

void pushWindow(std::vector<Span>& out, float lo, float hi, float total) {
  if (total <= 0 || hi <= lo) return;
  out.push_back({lo / total, hi / total});
}

/** A window around `d` on one contour, wrapping on a closed contour and
 *  clamping on an open one — an open contour has no seam to wrap over. */
void pushCornerWindow(std::vector<Span>& out, const ContourRun& run, float d,
                      float arm, float total) {
  const float lo = d - arm, hi = d + arm;
  if (!run.closed) {
    pushWindow(out, run.start + std::max(lo, 0.0f),
               run.start + std::min(hi, run.length), total);
    return;
  }
  if (lo < 0) {
    pushWindow(out, run.start + run.length + lo, run.start + run.length, total);
    pushWindow(out, run.start, run.start + std::min(hi, run.length), total);
  } else if (hi > run.length) {
    pushWindow(out, run.start + lo, run.start + run.length, total);
    pushWindow(out, run.start, run.start + (hi - run.length), total);
  } else {
    pushWindow(out, run.start + lo, run.start + hi, total);
  }
}

std::vector<Span> cornerSpans(const SkPath& outline, float arm,
                              float angleDeg) {
  std::vector<Span> out;
  float total = 0;
  const std::vector<ContourRun> runs = measureContours(outline, &total);
  if (total <= 0) return out;
  size_t i = 0;
  SkContourMeasureIter iter(outline, false);
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    if (i >= runs.size()) break;
    for (const lines::detail::CornerHit& hit :
         lines::detail::findCorners(*contour, angleDeg))
      pushCornerWindow(out, runs[i], hit.d, arm, total);
    ++i;
  }
  return detail::normalizeSpans(std::move(out));
}

/** The runs of the outline that lie inside a rect — spans::fit(). Walked
 *  rather than solved: the boundary is any shape, including one the rect
 *  enters and leaves several times. */
std::vector<Span> fitSpans(const SkPath& outline, const SkRect& box,
                           float margin) {
  std::vector<Span> out;
  SkRect grown = box;
  grown.outset(margin, margin);
  float total = 0;
  const std::vector<ContourRun> runs = measureContours(outline, &total);
  if (total <= 0) return out;
  size_t i = 0;
  SkContourMeasureIter iter(outline, false);
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    if (i >= runs.size()) break;
    const ContourRun& run = runs[i++];
    const float step = std::max(1.0f, run.length / 512.0f);
    bool inside = false;
    float enter = 0;
    for (float d = 0; d <= run.length + step * 0.5f; d += step) {
      SkPoint pos;
      const float at = std::min(d, run.length);
      if (!contour->getPosTan(at, &pos, nullptr)) continue;
      const bool now = grown.contains(pos.fX, pos.fY);
      if (now && !inside) {
        enter = at;
        inside = true;
      } else if (!now && inside) {
        pushWindow(out, run.start + enter, run.start + at, total);
        inside = false;
      }
    }
    if (inside)
      pushWindow(out, run.start + enter, run.start + run.length, total);
  }
  return detail::normalizeSpans(std::move(out));
}

}  // namespace

namespace detail {

std::vector<Span> normalizeSpans(std::vector<Span> spans) {
  std::vector<Span> out;
  out.reserve(spans.size());
  for (Span s : spans) {
    if (s.end < s.begin) std::swap(s.begin, s.end);
    s.begin = std::clamp(s.begin, 0.0f, 1.0f);
    s.end = std::clamp(s.end, 0.0f, 1.0f);
    if (s.end - s.begin > 1e-6f) out.push_back(s);
  }
  std::sort(out.begin(), out.end(),
            [](const Span& a, const Span& b) { return a.begin < b.begin; });
  std::vector<Span> merged;
  for (const Span& s : out) {
    if (!merged.empty() && s.begin <= merged.back().end + 1e-6f)
      merged.back().end = std::max(merged.back().end, s.end);
    else
      merged.push_back(s);
  }
  return merged;
}

std::vector<Span> complementSpans(const std::vector<Span>& spans) {
  std::vector<Span> out;
  float at = 0;
  for (const Span& s : spans) {
    if (s.begin - at > 1e-6f) out.push_back({at, s.begin});
    at = std::max(at, s.end);
  }
  if (1.0f - at > 1e-6f) out.push_back({at, 1.0f});
  return out;
}

std::vector<Span> intersectSpans(const std::vector<Span>& a,
                                 const std::vector<Span>& b) {
  // Both inputs are normalized (sorted, disjoint, non-degenerate), so one
  // sweep suffices. Touching endpoints are not an intersection, by the
  // same 1e-6 rule normalizeSpans drops empties with — two runs meeting
  // at a corner share no arc length.
  std::vector<Span> out;
  size_t i = 0, j = 0;
  while (i < a.size() && j < b.size()) {
    const float lo = std::max(a[i].begin, b[j].begin);
    const float hi = std::min(a[i].end, b[j].end);
    if (hi - lo > 1e-6f) out.push_back({lo, hi});
    if (a[i].end < b[j].end)
      ++i;
    else
      ++j;
  }
  return out;
}

std::optional<Span> spansOverlap(const std::vector<Span>& a,
                                 const std::vector<Span>& b) {
  for (const Span& x : a)
    for (const Span& y : b) {
      const float lo = std::max(x.begin, y.begin);
      const float hi = std::min(x.end, y.end);
      // A shared END POINT is two runs meeting, not two runs overlapping —
      // exactly what corners() next to edges() produces, and it must not
      // be an error.
      if (hi - lo > 1e-4f) return Span{lo, hi};
    }
  return std::nullopt;
}

SkPath spanPath(const SkPath& src, const std::vector<Span>& spans) {
  SkPathBuilder out;
  float total = 0;
  const std::vector<ContourRun> runs = measureContours(src, &total);
  if (total <= 0 || spans.empty()) return out.detach();
  size_t i = 0;
  SkContourMeasureIter iter(src, false);
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    if (i >= runs.size()) break;
    const ContourRun& run = runs[i++];
    // Emit one span against this contour. `stitch` appends WITHOUT a
    // moveTo, continuing the run already in flight.
    const auto emit = [&](const Span& s, bool stitch) {
      const float lo = std::max(s.begin * total, run.start) - run.start;
      const float hi =
          std::min(s.end * total, run.start + run.length) - run.start;
      if (hi - lo <= 1e-4f) return false;
      // A whole contour claimed whole stays whole — closed stays closed,
      // so joins and additive brushes behave as they do untrimmed.
      //
      // The close() is load-bearing. getSegment hands back an OPEN run
      // whose ends merely coincide, so without it the vertex at the seam
      // gets two butt caps instead of a miter join: a notch at one corner,
      // small under a hairline and obvious under any wide or additive
      // brush.
      if (lo <= 1e-4f && hi >= run.length - 1e-4f) {
        (void)contour->getSegment(0, run.length, &out, !stitch);
        if (run.closed && !stitch) out.close();
      } else
        (void)contour->getSegment(lo, hi, &out, !stitch);
      return true;
    };

    // THE SEAM. Spans arrive sorted, so a claim that straddles fraction 0
    // — a corner sitting on the seam, a wrapped window — arrives as its
    // two halves at opposite ends of the list. On a CLOSED contour those
    // halves are geometrically adjacent, and emitting them as two
    // subpaths makes round caps and additive halo brushes double-hit
    // there (the same defect the Wrap-mode trim path stitches away). So
    // emit the tail first and append the head to it. An OPEN contour has
    // no seam: joining its ends would invent a straight chord.
    const bool seamStraddled =
        run.closed && spans.size() >= 2 &&
        spans.front().begin * total <= run.start + 1e-4f &&
        spans.back().end * total >= run.start + run.length - 1e-4f;
    if (seamStraddled) {
      const bool inFlight = emit(spans.back(), false);
      (void)emit(spans.front(), inFlight);
      for (size_t k = 1; k + 1 < spans.size(); ++k) (void)emit(spans[k], false);
      continue;
    }
    for (const Span& s : spans) (void)emit(s, false);
  }
  return out.detach();
}

/** One of a band's two rails, as a Profile — so the rail is built by
 *  profileOffset and gets the SAME corner repair a relative strand gets.
 *  Formation is folded in here rather than at the sample site, which is
 *  what lets the two rails be two ordinary profiles.
 *
 *  `slice` remaps this contour's own [0,1] onto its span of the WHOLE
 *  spine, so a multi-contour spine keeps one continuous parameterisation
 *  even though the rails are built one contour at a time (which is what
 *  keeps the region from bridging between contours).
 *
 *  `spineLen` is the WHOLE spine's measured length, which is what a
 *  px-keyed base profile is evaluated against — the rail itself is always
 *  fraction-keyed (it is asked in fractions of its own contour), so the
 *  conversion happens here, once, on the way in. */
struct BandRail {
  Profile base;
  Formation formation = Formation::Centered;
  bool outer = true;
  float sliceStart = 0.0f, sliceSpan = 1.0f;
  float spineLen = 0.0f;
  bool operator==(const BandRail&) const = default;
  float max() const { return base.max(); }
  float across(float along) const {
    const float w = base.acrossAt(sliceStart + along * sliceSpan, spineLen);
    switch (formation) {
      case Formation::Centered:
        return outer ? w * 0.5f : -w * 0.5f;
      case Formation::Outward:
        return outer ? w : 0.0f;
      case Formation::Inward:
        return outer ? 0.0f : -w;
    }
    return 0.0f;
  }
};

/** Uniform arc-length samples of a rail, in ONE forward walk.
 *
 *  Do not implement this by calling bandPointAt per sample: that function
 *  re-measures the whole path on every call, which makes sampling quadratic
 *  in the sample count — and the sample count scales with the spine's
 *  length, so the cost grows fastest on exactly the large rings this is
 *  wanted for. Here the contours are measured once and the cursor only ever
 *  moves forward. */
std::vector<SkPoint> sampleRail(const SkPath& rail, int steps) {
  std::vector<SkPoint> out;
  std::vector<sk_sp<SkContourMeasure>> contours;
  float total = 0;
  SkContourMeasureIter iter(rail, false);
  while (sk_sp<SkContourMeasure> m = iter.next()) {
    total += m->length();
    contours.push_back(std::move(m));
  }
  if (total <= 0 || contours.empty()) return out;
  out.reserve((size_t)steps + 1);
  size_t at = 0;
  float consumed = 0;
  for (int k = 0; k <= steps; ++k) {
    const float want = total * (float)k / (float)steps;
    while (at + 1 < contours.size() &&
           want > consumed + contours[at]->length()) {
      consumed += contours[at]->length();
      ++at;
    }
    SkPoint pos;
    const float d = std::clamp(want - consumed, 0.0f, contours[at]->length());
    if (contours[at]->getPosTan(d, &pos, nullptr)) out.push_back(pos);
  }
  return out;
}

/** The contours of a path, each as its own path — so a rail pair can be
 *  zipped and CLOSED per contour instead of chained into one run. */
std::vector<std::pair<SkPath, float>> splitContours(const SkPath& path) {
  std::vector<std::pair<SkPath, float>> out;
  SkContourMeasureIter iter(path, false);
  while (sk_sp<SkContourMeasure> m = iter.next()) {
    const float len = m->length();
    if (len <= 0) continue;
    SkPathBuilder b;
    (void)m->getSegment(0, len, &b, true);
    if (m->isClosed()) b.close();
    out.emplace_back(b.detach(), len);
  }
  return out;
}

SkPath bandRegion(const SkPath& spine, const Across& width,
                  Formation formation) {
  const float reach = width.profile.max();
  if (spine.isEmpty() || reach <= 0) return SkPath();
  float total = 0;
  measureContours(spine, &total);
  if (total <= 0) return SkPath();

  // PER CONTOUR, and that is load-bearing: a single moveTo/lineTo chain
  // across all contours closed ONCE bridges between them with a filled
  // chord, so two concentric ring spines came out as a filled disc.
  //
  // BOTH RAILS GO THROUGH profileOffset, which is the other half: a
  // constant width then rides lines::offsetAcross's corner repair (real
  // vertices, arc outside a turn, miter inside) instead of a naive
  // sample-and-displace that leaves a spur on the inside of every
  // rectangle.
  //
  // Sign and frame, the one convention for the whole band family: positive
  // `across` is LEFT of travel, which with y pointing down is OUTSIDE a
  // clockwise path — and clockwise is SkPath's own direction for rects and
  // circles, so `.outward()` exits the shape. bandPointAt and
  // lines::offsetAcross mean the same side; a helper that flipped it would
  // turn every band inside out on one code path only.
  SkPathBuilder out;
  float consumed = 0;
  for (const auto& [contour, len] : splitContours(spine)) {
    const float sliceStart = total > 0 ? consumed / total : 0.0f;
    const float sliceSpan = total > 0 ? len / total : 1.0f;
    consumed += len;

    const SkPath outerRail =
        profileOffset(contour, Profile(BandRail{width.profile, formation, true,
                                                sliceStart, sliceSpan, total}));
    const SkPath innerRail =
        profileOffset(contour, Profile(BandRail{width.profile, formation, false,
                                                sliceStart, sliceSpan, total}));
    if (outerRail.isEmpty() || innerRail.isEmpty()) continue;

    // Zip by arc length rather than by index: offsetAcross inserts join
    // geometry, so the two rails do not share a point count.
    const int steps = std::max(16, (int)std::ceil(len / 2.0f));
    const std::vector<SkPoint> outerPts = sampleRail(outerRail, steps);
    const std::vector<SkPoint> innerPts = sampleRail(innerRail, steps);
    if (outerPts.size() < 2 || innerPts.size() < 2) continue;

    out.moveTo(outerPts.front());
    for (size_t k = 1; k < outerPts.size(); ++k) out.lineTo(outerPts[k]);
    for (size_t k = innerPts.size(); k-- > 0;) out.lineTo(innerPts[k]);
    out.close();
  }
  return out.detach();
}

}  // namespace detail

SkPath bandRegion(const SkPath& spine, const Across& width,
                  Formation formation) {
  return detail::bandRegion(spine, width, formation);
}

Spans& Spans::offset(Animatable<float> by) {
  for (size_t i = 0; i < terms.size(); ++i)
    terms[i].offset = i + 1 < terms.size() ? by : std::move(by);
  return *this;
}

std::vector<Span> Spans::resolve(const SpanInput& in) const {
  std::vector<Span> out;
  if (!in.outline) return out;
  // begin, end, offset per term — the order Instance::spanAnims and
  // spanEndpoints() both walk. The offset is ADDED to both ends before the
  // interval is read, which is exactly what trim() does with its third
  // argument (Paint.cpp's trim block: s0 = start + off, e0 = end + off).
  auto at = [&](size_t i, float fallback) {
    return in.values && in.values->size() > i ? (*in.values)[i] : fallback;
  };
  for (size_t t = 0; t < terms.size(); ++t) {
    const Term& term = terms[t];
    switch (term.rule) {
      case Rule::Range: {
        const float off = at(t * 3 + 2, 0.0f);
        const float a = at(t * 3, 0.0f) + off;
        const float b = at(t * 3 + 1, 1.0f) + off;
        out.push_back({a, b});
        break;
      }
      case Rule::Wrap: {
        const float off = at(t * 3 + 2, 0.0f);
        const float a = at(t * 3, 0.0f) + off;
        const float b = at(t * 3 + 1, 1.0f) + off;
        // The same three cases TrimMode::Wrap resolves, and deliberately in
        // the same order: the RAW difference decides emptiness and fullness
        // (a window driven to [1.1, 1.35] is still a quarter of the cycle),
        // and only then do the endpoints wrap into [0,1).
        const float length = b - a;
        if (length <= 0.0f) break;  // claims nothing
        if (length >= 1.0f) {
          out.push_back({0.0f, 1.0f});  // the whole cycle
          break;
        }
        const float s = a - std::floor(a);
        const float e = b - std::floor(b);
        if (s < e) {
          out.push_back({s, e});
        } else {
          // Straddles the seam: two runs, which normalizeSpans sorts to the
          // ends of the list and spanPath stitches back into one contour.
          out.push_back({s, 1.0f});
          out.push_back({0.0f, e});
        }
        break;
      }
      case Rule::Corners: {
        const std::vector<Span> hits =
            cornerSpans(*in.outline, term.arm, term.angleDeg);
        out.insert(out.end(), hits.begin(), hits.end());
        break;
      }
      case Rule::Edges: {
        const std::vector<Span> gaps = detail::complementSpans(
            cornerSpans(*in.outline, term.arm, term.angleDeg));
        out.insert(out.end(), gaps.begin(), gaps.end());
        break;
      }
      case Rule::Every: {
        const int n = std::max(1, term.count);
        const float duty = std::clamp(term.duty, 0.0f, 1.0f);
        for (int k = 0; k < n; ++k)
          out.push_back({(float)k / (float)n, ((float)k + duty) / (float)n});
        break;
      }
      case Rule::At: {
        const int n = std::max(1, term.count);
        const int k = std::clamp(term.index, 0, n - 1);
        out.push_back({(float)k / (float)n, (float)(k + 1) / (float)n});
        break;
      }
      case Rule::Fit: {
        if (!in.fitRects) break;
        for (const auto& [key, box] : *in.fitRects)
          if (key == term.key) {
            const std::vector<Span> hits =
                fitSpans(*in.outline, box, term.margin);
            out.insert(out.end(), hits.begin(), hits.end());
          }
        break;
      }
      case Rule::Rest:
        break;  // the complement needs the element's other passes
    }
  }
  return detail::normalizeSpans(std::move(out));
}

namespace spans {

Spans range(Animatable<float> begin, Animatable<float> end) {
  Spans s;
  Spans::Term t;
  t.rule = Spans::Rule::Range;
  t.begin = std::move(begin);
  t.end = std::move(end);
  s.terms.push_back(std::move(t));
  return s;
}
Spans wrap(Animatable<float> begin, Animatable<float> end) {
  Spans s;
  Spans::Term t;
  t.rule = Spans::Rule::Wrap;
  t.begin = std::move(begin);
  t.end = std::move(end);
  s.terms.push_back(std::move(t));
  return s;
}
Spans upTo(Animatable<float> end) { return range(0.0f, std::move(end)); }
Spans corners(float arm, float angleDeg) {
  Spans s;
  Spans::Term t;
  t.rule = Spans::Rule::Corners;
  t.arm = arm;
  t.angleDeg = angleDeg;
  s.terms.push_back(std::move(t));
  return s;
}
Spans edges(float arm, float angleDeg) {
  Spans s;
  Spans::Term t;
  t.rule = Spans::Rule::Edges;
  t.arm = arm;
  t.angleDeg = angleDeg;
  s.terms.push_back(std::move(t));
  return s;
}
Spans every(int count, float duty) {
  Spans s;
  Spans::Term t;
  t.rule = Spans::Rule::Every;
  t.count = count;
  t.duty = duty;
  s.terms.push_back(std::move(t));
  return s;
}
Spans at(int index, int count) {
  Spans s;
  Spans::Term t;
  t.rule = Spans::Rule::At;
  t.index = index;
  t.count = count;
  s.terms.push_back(std::move(t));
  return s;
}
Spans fit(std::string_view key, float margin) {
  Spans s;
  Spans::Term t;
  t.rule = Spans::Rule::Fit;
  t.key = std::string(key);
  t.margin = margin;
  s.terms.push_back(std::move(t));
  return s;
}
Spans rest() {
  Spans s;
  Spans::Term t;
  t.rule = Spans::Rule::Rest;
  s.terms.push_back(std::move(t));
  return s;
}
Spans rest(std::string_view passName) {
  Spans s;
  Spans::Term t;
  t.rule = Spans::Rule::Rest;
  t.key = std::string(passName);
  s.terms.push_back(std::move(t));
  return s;
}

}  // namespace spans

// ---------------------------------------------------------------------------
// band()

SkPoint bandPointAt(const SkPath& spine, float along, float acrossPx) {
  float total = 0;
  measureContours(spine, &total);
  if (total <= 0) return {0, 0};
  const float want = std::clamp(along, 0.0f, 1.0f) * total;
  float consumed = 0;
  SkContourMeasureIter iter(spine, false);
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    const float len = contour->length();
    if (want <= consumed + len || consumed + len >= total - 1e-4f) {
      SkPoint pos;
      SkVector tan;
      const float d = std::clamp(want - consumed, 0.0f, len);
      if (contour->getPosTan(d, &pos, &tan))
        return {pos.fX + tan.y() * acrossPx, pos.fY - tan.x() * acrossPx};
      return pos;
    }
    consumed += len;
  }
  return {0, 0};
}

SkPath profileOffset(const SkPath& spine, const Profile& profile) {
  if (spine.isEmpty()) return SkPath();
  float total = 0;
  measureContours(spine, &total);
  if (total <= 0) return SkPath();
  // A CONSTANT profile is a parallel, and lines::offsetAcross already does
  // parallels exactly — it finds the real vertices and joins them (arc
  // outside a turn, miter inside) instead of chording across. The naive
  // sample-and-displace walk below cannot: at a hard corner it offsets one
  // sampled point along ONE edge's normal, which leaves a spur on the
  // inside of every rectangle. Delegating rather than growing a second
  // corner repair here is deliberate — two repairs would drift apart.
  // No sign conversion is needed: offsetAcross is LEFT of travel, which is
  // this file's frame exactly (see bandPointAt).
  //
  // Constancy is detected by SAMPLING, and that is a real limitation, not
  // a rounding detail: a stepped profile whose period divides the sample
  // spacing reads as constant. Sampled at 97 points (prime, so no profile
  // whose period is a simple fraction aligns with it) offset by half a
  // step (so a value read exactly at 0, 1/2, 1 cannot be the whole basis).
  // A profile that defeats this still gets a correct-shaped answer — the
  // exact-corner parallel — just not the varying one it asked for. If that
  // ever bites, the honest fix is a `constant()` query on the Profile
  // seam, which is additive.
  {
    const float first = profile.acrossAt(0.5f / 97.0f, total);
    bool constant = true;
    for (int k = 1; k < 97 && constant; ++k)
      constant = profile.acrossAt(((float)k + 0.5f) / 97.0f, total) == first;
    if (constant)
      return first == 0.0f ? spine : lines::offsetAcross(spine, first);
  }
  SkPathBuilder out;
  SkContourMeasureIter iter(spine, false);
  float consumed = 0;
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    const float len = contour->length();
    const float base = consumed;
    consumed += len;
    if (len <= 0) continue;
    const int steps = std::max(8, (int)std::ceil(len / 2.0f));
    bool started = false;
    for (int k = 0; k <= steps; ++k) {
      const float d = len * (float)k / (float)steps;
      SkPoint pos;
      SkVector tan;
      if (!contour->getPosTan(d, &pos, &tan)) continue;
      // The band's frame: positive across is LEFT of travel, which with y
      // down is outside a clockwise path. One body for the band's rails
      // and a relative strand, so the two cannot drift apart.
      float w = profile.acrossAt(total > 0 ? (base + d) / total : 0.0f, total);
      // ONE non-finite sample would delete the WHOLE band: Skia draws none
      // of a path that contains a non-finite vertex, and says nothing. An
      // author profile only has to misbehave at a single parameter value to
      // hit this — sqrt(sin(pi*along)) rounds to a tiny negative at
      // along == 1 — so a non-finite width is clamped to a LOCAL pinch down
      // to the spine rather than allowed to erase everything.
      if (!std::isfinite(w)) w = 0.0f;
      const SkPoint at{pos.fX + tan.y() * w, pos.fY - tan.x() * w};
      if (!started) {
        out.moveTo(at);
        started = true;
      } else {
        out.lineTo(at);
      }
    }
    if (started && contour->isClosed()) out.close();
  }
  return out.detach();
}

// ---------------------------------------------------------------------------
// Crossing discovery
//
// Crossings are DISCOVERED, never authored: the strands are flattened and
// every pair of segments is tested for a PROPER crossing. "Proper" is
// load-bearing — coincident strands (which is what layers() is) and
// endpoint touches (a shared polygon vertex) are meetings, not crossings,
// and reporting them would put a knot at every corner of every rectangle.

namespace {

struct Flat {
  std::vector<SkPoint> points;
  std::vector<float> at;  // cumulative arc length at each point
  float length = 0;
  SkRect bounds = SkRect::MakeEmpty();  // of `points` — the pair rejection
};

Flat flatten(const SkPath& path) {
  Flat f;
  SkContourMeasureIter iter(path, false);
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    const float len = contour->length();
    if (len <= 0) continue;
    const int steps = std::max(2, (int)std::ceil(len / 2.0f));
    for (int k = 0; k <= steps; ++k) {
      const float d = len * (float)k / (float)steps;
      SkPoint pos;
      if (!contour->getPosTan(d, &pos, nullptr)) continue;
      f.points.push_back(pos);
      f.at.push_back(f.length + d);
    }
    f.length += len;
    // A break between contours: repeat the last point so the segment loop
    // below can skip the join (a chord between two contours is not a
    // strand and must not manufacture crossings). Guarded because a
    // contour whose every getPosTan failed appends nothing at all.
    if (!f.points.empty()) {
      f.points.push_back(f.points.back());
      f.at.push_back(f.length);
    }
  }
  if (!f.points.empty()) f.bounds.setBounds({f.points.data(), f.points.size()});
  return f;
}

/** The point on a flattened strand at arc length `s`. */
SkPoint pointAtArc(const Flat& f, float s) {
  if (f.points.empty()) return {0, 0};
  s = std::clamp(s, 0.0f, f.length);
  for (size_t k = 0; k + 1 < f.at.size(); ++k) {
    if (s > f.at[k + 1]) continue;
    const float span = f.at[k + 1] - f.at[k];
    const float w = span > 1e-6f ? (s - f.at[k]) / span : 0.0f;
    return {f.points[k].fX + (f.points[k + 1].fX - f.points[k].fX) * w,
            f.points[k].fY + (f.points[k + 1].fY - f.points[k].fY) * w};
  }
  return f.points.back();
}

/** Does one strand change sides of the other's local direction at `hit`? */
bool changesSides(const Flat& other, float sOther, SkPoint hit, SkVector dir) {
  const float delta = 3.0f;
  const SkPoint before = pointAtArc(other, sOther - delta);
  const SkPoint after = pointAtArc(other, sOther + delta);
  const auto side = [&](SkPoint q) {
    return dir.x() * (q.fY - hit.fY) - dir.y() * (q.fX - hit.fX);
  };
  return side(before) * side(after) < 0.0f;
}

/** Do these two strands genuinely CROSS at `hit`, or only meet there?
 *
 *  BOTH directions are tested, and that is the point: asking only "does B
 *  change sides of A" is order-asymmetric, so an A endpoint landing on B's
 *  interior answered yes while the mirror case answered no — the same
 *  meeting classified two ways depending on which strand happened to be
 *  indexed first. A crossing is a symmetric property and is tested as one.
 *
 *  This is also what keeps a rectangle's corners from each becoming a knot:
 *  at a shared vertex the neighbours sit on one side (or collinear), so at
 *  least one of the two tests fails. */
bool crossesTransversally(const Flat& fa, float sA, const Flat& fb, float sB,
                          SkPoint hit, SkVector aDir, SkVector bDir) {
  return changesSides(fb, sB, hit, aDir) && changesSides(fa, sA, hit, bDir);
}

}  // namespace

std::vector<Crossing> discoverCrossings(const std::vector<SkPath>& strands) {
  std::vector<Crossing> found;
  if (strands.size() < 2) return found;
  std::vector<Flat> flats;
  flats.reserve(strands.size());
  for (const SkPath& p : strands) flats.push_back(flatten(p));

  for (size_t a = 0; a < strands.size(); ++a)
    for (size_t b = a + 1; b < strands.size(); ++b) {
      // COINCIDENT strands never cross. This is the layers() case, and
      // testing it by path identity is exact where it matters most.
      if (strands[a] == strands[b]) continue;
      const Flat& fa = flats[a];
      const Flat& fb = flats[b];
      // Bounds rejection before the segment-by-segment loop, which is
      // quadratic in the flattened point counts. A reported crossing's hit
      // point lies on a segment of EACH strand, up to the parametric eps
      // overshoot below — a fraction of the flattening step, so far under a
      // pixel — which means two strands whose bounds stay half a pixel
      // apart provably cannot cross. The 0.5 px outset is orders of
      // magnitude larger than that overshoot, so this skips only provably
      // empty work and cannot change an answer.
      SkRect nearA = fa.bounds, nearB = fb.bounds;
      nearA.outset(0.5f, 0.5f);
      nearB.outset(0.5f, 0.5f);
      if (!SkRect::Intersects(nearA, nearB)) continue;
      for (size_t i = 0; i + 1 < fa.points.size(); ++i) {
        const SkPoint p0 = fa.points[i], p1 = fa.points[i + 1];
        const SkVector r{p1.fX - p0.fX, p1.fY - p0.fY};
        if (r.length() <= 1e-6f) continue;  // the contour join
        for (size_t j = 0; j + 1 < fb.points.size(); ++j) {
          const SkPoint q0 = fb.points[j], q1 = fb.points[j + 1];
          const SkVector sv{q1.fX - q0.fX, q1.fY - q0.fY};
          if (sv.length() <= 1e-6f) continue;
          const float denom = r.x() * sv.y() - r.y() * sv.x();
          // Parallel or collinear: no transversal crossing. Two copies of
          // one path land here for every corresponding segment.
          if (std::abs(denom) < 1e-9f) continue;
          const SkVector d{q0.fX - p0.fX, q0.fY - p0.fY};
          const float t = (d.x() * sv.y() - d.y() * sv.x()) / denom;
          const float u = (d.x() * r.y() - d.y() * r.x()) / denom;
          // CLOSED intervals, then a transversality test.
          //
          // Strict interiors cannot be used here. Symmetric geometry — two
          // diagonals of a square, a horizontal met by verticals on a
          // regular sampling grid — puts a genuine crossing EXACTLY on a
          // sample boundary, and a strict test discards all of them. So the
          // endpoints are accepted, and the question that actually
          // separates the two cases is asked afterwards: does the other
          // strand pass THROUGH here, or does it merely touch?
          const float eps = 1e-3f;
          if (t < -eps || t > 1.0f + eps || u < -eps || u > 1.0f + eps)
            continue;
          const SkPoint hit{p0.fX + r.x() * t, p0.fY + r.y() * t};
          const float sA = fa.at[i] + (fa.at[i + 1] - fa.at[i]) * t;
          const float sB = fb.at[j] + (fb.at[j + 1] - fb.at[j]) * u;
          if (!crossesTransversally(fa, sA, fb, sB, hit, r, sv)) continue;
          Crossing x;
          x.a = a;
          x.b = b;
          x.at = hit;
          x.alongA = fa.length > 0 ? sA / fa.length : 0.0f;
          x.alongB = fb.length > 0 ? sB / fb.length : 0.0f;
          // Sampling can report one meeting from two adjacent segment
          // pairs; keep the first and drop its neighbours.
          bool duplicate = false;
          for (const Crossing& seen : found)
            if (seen.a == x.a && seen.b == x.b &&
                std::abs(seen.at.fX - x.at.fX) < 1.5f &&
                std::abs(seen.at.fY - x.at.fY) < 1.5f) {
              duplicate = true;
              break;
            }
          if (!duplicate) found.push_back(x);
        }
      }
    }

  // Numbered ALONG THE BOUNDARY: ascending by position on the lower-indexed
  // strand, then by strand pair, so the order is deterministic and a
  // positional pin means the same knot on every frame the geometry holds.
  std::sort(found.begin(), found.end(),
            [](const Crossing& l, const Crossing& r) {
              if (l.alongA != r.alongA) return l.alongA < r.alongA;
              if (l.a != r.a) return l.a < r.a;
              return l.b < r.b;
            });
  for (size_t i = 0; i < found.size(); ++i) found[i].index = i;
  return found;
}

SkPath crossingPatch(const SkPath& a, float reachA, const SkPath& b,
                     float reachB, SkPoint at, float maxRadius) {
  const auto tube = [](const SkPath& path, float reach) {
    SkPaint p;
    p.setStyle(SkPaint::kStroke_Style);
    // `reach` is the mark's FULL width, and the tube is twice it. That is
    // deliberately conservative: alignment can put the whole mark on ONE
    // side of the path (Align::Inner/Outer), so a tube of exactly the mark
    // width, centred on the path, would miss half of it. The cost is a
    // lens up to 2x larger than the true overlap — harmless with opaque
    // inks, and bounded by maxRadius either way.
    p.setStrokeWidth(std::max(reach, 0.5f) * 2.0f);
    p.setStrokeCap(SkPaint::kRound_Cap);
    p.setStrokeJoin(SkPaint::kRound_Join);
    return skpathutils::FillPathWithPaint(path, p);
  };
  // The knot's OWN territory. Without this the neighbouring lenses of an
  // ordinary braid touch, pathops merges them into one contour, and the
  // first crossing's patch claims the entire run.
  SkPathBuilder territoryBuilder;
  territoryBuilder.addCircle(at.fX, at.fY, std::max(maxRadius, 1.0f));
  const SkPath territory = territoryBuilder.detach();

  SkPath overlap, lens;
  if (Op(tube(a, reachA), tube(b, reachB), kIntersect_SkPathOp, &overlap) &&
      !overlap.isEmpty() &&
      Op(overlap, territory, kIntersect_SkPathOp, &lens) && !lens.isEmpty()) {
    // The intersection holds EVERY overlap of the two strands, which is one
    // component per crossing. Keep the component this crossing is in, so a
    // strand pair that meets several times repairs each meeting on its own
    // terms rather than repainting all of them at the first.
    SkPathBuilder mine;
    bool found = false;
    SkPath::Iter iter(lens, false);
    SkPathBuilder run;
    bool runOpen = false;
    const auto flushRun = [&] {
      if (!runOpen) return;
      SkPath contour = run.detach();
      SkRect bounds = contour.getBounds();
      bounds.outset(0.5f, 0.5f);
      if (bounds.contains(at.fX, at.fY)) {
        mine.addPath(contour);
        found = true;
      }
      runOpen = false;
    };
    SkPoint pts[4];
    for (SkPath::Verb verb = iter.next(pts); verb != SkPath::kDone_Verb;
         verb = iter.next(pts)) {
      switch (verb) {
        case SkPath::kMove_Verb:
          flushRun();
          run.moveTo(pts[0]);
          runOpen = true;
          break;
        case SkPath::kLine_Verb:
          run.lineTo(pts[1]);
          break;
        case SkPath::kQuad_Verb:
          run.quadTo(pts[1], pts[2]);
          break;
        case SkPath::kConic_Verb:
          run.conicTo(pts[1], pts[2], iter.conicWeight());
          break;
        case SkPath::kCubic_Verb:
          run.cubicTo(pts[1], pts[2], pts[3]);
          break;
        case SkPath::kClose_Verb:
          run.close();
          break;
        default:
          break;
      }
    }
    flushRun();
    if (found) return mine.detach();
    return lens;  // the point missed every component's box — repair it all
  }
  // Degenerate or non-overlapping: a disc sized for the perpendicular case
  // is the best available answer and is what the exact form replaced. Still
  // bounded by the knot's own territory.
  SkPathBuilder disc;
  disc.addCircle(at.fX, at.fY,
                 std::min(std::max({reachA, reachB, 3.0f}) + 1.0f,
                          std::max(maxRadius, 1.0f)));
  return disc.detach();
}

Element band(Shape spine, Across width) {
  Element e;
  detail::DeriveData& derive = e.node()->deriveData.ensure();
  derive.bandSpine = std::move(spine);
  derive.bandWidth = std::move(width);
  return e;
}

Element band(Around spine, Across width) {
  Element e;
  detail::DeriveData& derive = e.node()->deriveData.ensure();
  derive.bandAround = std::move(spine.key);
  derive.bandWidth = std::move(width);
  return e;
}

}  // namespace sigil::compose
