// Element value builders — copy-on-write mutations of description payloads.
// Nothing here talks to Yoga, Skia surfaces, or Choreograph; that is the
// Composer's job.

#include "ComposeInternal.h"

#include "sigilcompose/Lines.h" // the ONE corner scanner (spans::corners)

#include <include/core/SkContourMeasure.h>
#include <include/core/SkImageFilter.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkShader.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>

#include <algorithm>

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

ElementNode *Element::NodeHandle::operator->() {
  if (!value)
    value = std::make_shared<ElementNode>();
  else if (value.use_count() != 1)
    value = std::make_shared<ElementNode>(*value);
  return value.get();
}

const ElementNode *Element::NodeHandle::operator->() const {
  return value.get();
}

// ---- layout ---------------------------------------------------------------

Element &Element::row() { m_node->layout.row = true; return *this; }
Element &Element::column() { m_node->layout.row = false; return *this; }
Element &Element::wrapLines(bool on) {
  m_node->layout.wrap = on;
  return *this;
}
Element &Element::gap(float px) { m_node->layout.gap = px; return *this; }
Element &Element::padding(float all) {
  m_node->layout.padding = {all, all, all, all};
  return *this;
}
Element &Element::padding(float h, float v) {
  m_node->layout.padding = {h, v, h, v};
  return *this;
}
Element &Element::padding(float l, float t, float r, float b) {
  m_node->layout.padding = {l, t, r, b};
  return *this;
}
Element &Element::margin(float all) {
  m_node->layout.margin = {all, all, all, all};
  return *this;
}
Element &Element::margin(float h, float v) {
  m_node->layout.margin = {h, v, h, v};
  return *this;
}
Element &Element::margin(float l, float t, float r, float b) {
  m_node->layout.margin = {l, t, r, b};
  return *this;
}
Element &Element::width(Dim d) { m_node->layout.width = d; return *this; }
Element &Element::height(Dim d) { m_node->layout.height = d; return *this; }
Element &Element::minWidth(Dim d) { m_node->layout.minWidth = d; return *this; }
Element &Element::maxWidth(Dim d) { m_node->layout.maxWidth = d; return *this; }
Element &Element::minHeight(Dim d) { m_node->layout.minHeight = d; return *this; }
Element &Element::maxHeight(Dim d) { m_node->layout.maxHeight = d; return *this; }
Element &Element::aspect(float r) { m_node->layout.aspect = r; return *this; }
Element &Element::grow(float f) { m_node->layout.grow = f; return *this; }
Element &Element::shrink(float f) { m_node->layout.shrink = f; return *this; }
Element &Element::basis(Dim d) { m_node->layout.basis = d; return *this; }
Element &Element::alignItems(Align a) {
  m_node->layout.alignItems = a;
  return *this;
}
Element &Element::alignSelf(Align a) {
  m_node->layout.alignSelf = a;
  return *this;
}
Element &Element::justify(Justify j) { m_node->layout.justify = j; return *this; }
Element &Element::absolute() { m_node->layout.absolute = true; return *this; }
Element &Element::inset(float all) { return inset(all, all, all, all); }
Element &Element::inset(float l, float t, float r, float b) {
  return inset(Dim(l), Dim(t), Dim(r), Dim(b));
}
Element &Element::inset(Dim l, Dim t, Dim r, Dim b) {
  m_node->layout.absolute = true;
  m_node->layout.hasInsets = true;
  m_node->layout.insets = {l, t, r, b};
  return *this;
}
Element &Element::left(Dim d) {
  m_node->layout.absolute = true;
  m_node->layout.hasInsets = true;
  m_node->layout.insets.left = d;
  return *this;
}
Element &Element::top(Dim d) {
  m_node->layout.absolute = true;
  m_node->layout.hasInsets = true;
  m_node->layout.insets.top = d;
  return *this;
}
Element &Element::right(Dim d) {
  m_node->layout.absolute = true;
  m_node->layout.hasInsets = true;
  m_node->layout.insets.right = d;
  return *this;
}
Element &Element::bottom(Dim d) {
  m_node->layout.absolute = true;
  m_node->layout.hasInsets = true;
  m_node->layout.insets.bottom = d;
  return *this;
}
Element &Element::centerAt(SkPoint p) {
  m_node->layout.absolute = true;
  m_node->layout.centerAt = p;
  return *this;
}
// rect()/at() go through the edge setters rather than writing LayoutProps
// themselves. That is the whole safety argument: they cannot describe a node
// the longhand could not, they touch no field the longhand does not, and
// they cannot drift from it when a setter changes. Nine studies wrote this
// by hand under four names and two of those bodies are the same line twice
// (black_watch.cpp:544, chevreul_circle.cpp:406).
Element &Element::rect(const SkRect &r) {
  left(Dim(r.fLeft));
  top(Dim(r.fTop));
  width(Dim(r.width()));
  height(Dim(r.height()));
  return *this;
}
Element &Element::at(SkPoint topLeft) {
  left(Dim(topLeft.fX));
  top(Dim(topLeft.fY));
  return *this;
}

// ---- shape ----------------------------------------------------------------

Element &Element::corners(Corners c) { m_node->corners = c; return *this; }
Element &Element::shape(std::function<SkPath(SkSize)> path) {
  m_node->shapeFn = std::move(path);
  return *this;
}
Element &Element::centered() {
  m_node->deriveData.ensure().bandFormation = Formation::Centered;
  return *this;
}
Element &Element::outward() {
  m_node->deriveData.ensure().bandFormation = Formation::Outward;
  return *this;
}
Element &Element::inward() {
  m_node->deriveData.ensure().bandFormation = Formation::Inward;
  return *this;
}
Element &Element::clip(bool on) { m_node->clipContent = on; return *this; }
Element &Element::trim(PropValue<float> start, PropValue<float> end,
                       PropValue<float> offset, TrimMode mode) {
  detail::FxData &fx = m_node->fxData.ensure();
  fx.hasTrim = true;
  fx.trimStart = std::move(start);
  fx.trimEnd = std::move(end);
  fx.trimOffset = std::move(offset);
  fx.trimMode = mode;
  return *this;
}

// ---- paint ----------------------------------------------------------------

Element &Element::fill(PropValue<Fill> f) {
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
  if (!effect)
    return e;
  SkRuntimeShaderBuilder builder(std::move(effect));
  for (const auto &[name, value] : uniforms)
    builder.uniform(name.c_str()) = value;
  e.m_filter = SkImageFilters::RuntimeShader(builder, "content", nullptr);
  return e;
}

Effect Effect::then(const Effect &next) const {
  Effect e;
  if (!m_filter)
    return next;
  if (!next.m_filter)
    return *this;
  e.m_filter = SkImageFilters::Compose(next.m_filter, m_filter);
  return e;
}

Element &Element::wipe(float angleDeg, PropValue<float> fraction) {
  auto &fx = m_node->fxData.ensure();
  fx.hasWipe = true;
  fx.wipeAngleDeg = angleDeg;
  fx.wipeFraction = std::move(fraction);
  return *this;
}

Element &Element::hitTestable(bool enabled) {
  m_node->hitTestable = enabled;
  return *this;
}

Element &Element::overlay(Decoration d) {
  m_node->fxData.ensure().overlays.push_back(std::move(d));
  return *this;
}
Element &Element::background(Decoration d) {
  m_node->backgrounds.push_back(std::move(d));
  return *this;
}
Element &Element::foreground(Decoration d) {
  m_node->foregrounds.push_back(std::move(d));
  return *this;
}
Element &Element::stroke(Decoration brush) {
  return foreground(std::move(brush));
}
Element &Element::stroke(Spans where, Decoration what, std::string name) {
  // A fit() term borrows another element's resolved box, so the keys ride
  // into DeriveData where the ONE derive-registration walk finds them —
  // the flowAround pattern, not a second phase.
  for (const Spans::Term &t : where.terms)
    if (t.rule == Spans::Rule::Fit && !t.key.empty())
      m_node->deriveData.ensure().spanFitKeys.push_back(t.key);
  m_node->strokeData.ensure().passes.push_back(
      detail::StrokePass{std::move(where), std::move(what), std::move(name)});
  return *this;
}
Element &Element::echo(SkVector offset, SkColor4f color) {
  m_node->fxData.ensure().echoes.push_back(Echo{offset, color});
  return *this;
}
Element &Element::style(LayerStyle s) {
  for (Decoration &d : s.under)
    m_node->backgrounds.push_back(std::move(d));
  for (Decoration &d : s.over)
    m_node->foregrounds.push_back(std::move(d));
  return *this;
}
Element &Element::effect(Effect e) {
  m_node->fxData.ensure().layerEffect = std::move(e);
  return *this;
}
Element &Element::backdrop(Effect e) {
  m_node->fxData.ensure().backdropEffect = std::move(e);
  return *this;
}
Element &Element::opacity(PropValue<float> o) {
  m_node->paint.opacity = std::move(o);
  return *this;
}
Element &Element::blend(SkBlendMode mode) {
  m_node->paint.blendMode = mode;
  return *this;
}
Element &Element::translateX(PropValue<float> v) {
  m_node->paint.translateX = std::move(v);
  return *this;
}
Element &Element::translateY(PropValue<float> v) {
  m_node->paint.translateY = std::move(v);
  return *this;
}
Element &Element::rotate(PropValue<float> v) {
  m_node->paint.rotate = std::move(v);
  return *this;
}
Element &Element::scale(PropValue<float> v) {
  m_node->paint.scale = std::move(v);
  return *this;
}
Element &Element::textStroke(float width, Fill fill) {
  auto &t = m_node->textData.ensure();
  t.hasTextStroke = width > 0.0f;
  t.textStrokeWidth = width;
  t.textStrokeFill = std::move(fill);
  return *this;
}

Element &Element::onPath(TextPath spec) {
  m_node->textData.ensure().onPath = std::move(spec);
  return *this;
}
Element &Element::scaleX(PropValue<float> v) {
  m_node->paint.scaleX = std::move(v);
  return *this;
}
Element &Element::scaleY(PropValue<float> v) {
  m_node->paint.scaleY = std::move(v);
  return *this;
}
Element &Element::skewX(PropValue<float> v) {
  m_node->paint.skewX = std::move(v);
  return *this;
}
Element &Element::skewY(PropValue<float> v) {
  m_node->paint.skewY = std::move(v);
  return *this;
}
Element &Element::transformOrigin(float fx, float fy) {
  m_node->paint.originX = fx;
  m_node->paint.originY = fy;
  m_node->paint.originPx = false;
  return *this;
}
Element &Element::transformOriginPx(SkPoint p) {
  m_node->paint.originX = p.x();
  m_node->paint.originY = p.y();
  m_node->paint.originPx = true;
  return *this;
}
Element &Element::zIndex(int z) { m_node->paint.zIndex = z; return *this; }

// ---- identity, caching, transitions --------------------------------------

Element &Element::key(std::string_view k) {
  m_node->key = std::string(k);
  return *this;
}
Element &Element::cache(Cache c) { m_node->cacheMode = c; return *this; }
Element &Element::bakeScale(float factor) {
  m_node->bakeScale = std::clamp(factor, 0.1f, 1.0f);
  return *this;
}
Element &Element::transition(Transition t) {
  m_node->nodeTransition = std::move(t);
  return *this;
}
Element &Element::staggerChildren(std::chrono::milliseconds each,
                                  Stagger::From from) {
  detail::FxData &fx = m_node->fxData.ensure();
  fx.staggerChildrenMs = (float)each.count();
  fx.staggerFrom = from;
  return *this;
}

Element &Element::child(Element e) {
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

Element text(std::u8string utf8, sigil::weave::TextStyle style) {
  Element e;
  e.node()->kind = Kind::Text;
  detail::TextData &text = e.node()->textData.ensure();
  text.utf8 = std::move(utf8);
  text.style = std::move(style);
  // "The box fits the type": measured text must not stretch on the
  // cross axis (the spike's API lesson) — but that demotion happens at
  // layout-apply time, where the resolved alignment is known, so a
  // parent's alignItems(Center/End) still reaches text leaves.
  return e;
}

Element text(std::shared_ptr<sigil::weave::Paragraph> paragraph,
             sigil::weave::ParagraphLayoutOptions options) {
  Element e;
  e.node()->kind = Kind::Text;
  detail::TextData &text = e.node()->textData.ensure();
  text.paragraphOverride = std::move(paragraph);
  text.layoutOptions = std::move(options);
  return e;
}

Element image(std::shared_ptr<const sigil::image::ImageAsset> asset) {
  Element e;
  e.node()->kind = Kind::Image;
  e.node()->imageData.ensure().asset = std::move(asset);
  return e;
}

Element &Element::sampling(SkSamplingOptions options) {
  m_node->imageData.ensure().sampling = options;
  return *this;
}

Element &Element::region(SkRect sourceRect) {
  m_node->imageData.ensure().region = sourceRect;
  return *this;
}

Element custom(PaintProgram program) {
  Element e;
  e.node()->kind = Kind::Custom;
  e.node()->customData.ensure().program = std::move(program);
  return e;
}

Element &Element::glyphFx(GlyphFx fx) {
  m_node->textData.ensure().glyphFx = std::move(fx);
  return *this;
}

Element &Element::variationDrive(const char (&tag)[5],
                                 const choreograph::Output<float> *value) {
  detail::TextData &text = m_node->textData.ensure();
  text.driveTag[0] = tag[0];
  text.driveTag[1] = tag[1];
  text.driveTag[2] = tag[2];
  text.driveTag[3] = tag[3];
  text.driveValue = value;
  return *this;
}

Element &Element::textAlign(sigil::weave::TextAlignment a) {
  m_node->textData.ensure().layoutOptions.alignment = a;
  return *this;
}

Element &Element::flowAround(std::string_view key, float margin) {
  detail::DeriveData &derive = m_node->deriveData.ensure();
  derive.flowAroundKeys.push_back(std::string(key));
  derive.flowAroundMargin = margin;
  return *this;
}

Element connector(std::string_view fromKey, std::string_view toKey,
                  Router router) {
  Element e;
  e.node()->kind = Kind::Custom; // painted via derive-resolved outline
  detail::DeriveData &derive = e.node()->deriveData.ensure();
  derive.connectFrom = std::string(fromKey);
  derive.connectTo = std::string(toKey);
  derive.router = std::move(router);
  return e;
}

Element rail(std::vector<Anchor> anchors, RailRouter router) {
  Element e;
  e.node()->kind = Kind::Custom; // painted via the derive-routed outline
  detail::DeriveData &derive = e.node()->deriveData.ensure();
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
    std::function<std::vector<SkRect>(const LayoutInput &)> place) {
  Element e;
  e.node()->deriveData.ensure().placeFn = std::move(place);
  return e;
}

Element makeMemo(std::any props,
                 std::function<bool(const std::any &, const std::any &)> equal,
                 std::function<Element(const std::any &)> invoke) {
  Element e;
  detail::MemoData &memo = e.node()->memoData.ensure();
  memo.props = std::move(props);
  memo.equal = std::move(equal);
  memo.invoke = std::move(invoke);
  return e;
}
} // namespace detail

// ---------------------------------------------------------------------------
// Spans: the boundary's arc length, in claimed runs
//
// Every answer is in ONE normal form (clamped, sorted, merged, non-empty)
// so overlap tests and complements are interval arithmetic rather than a
// pile of per-rule special cases — the seam where a sibling-path failure
// would otherwise live.

namespace {

/** Where each contour starts and ends in the path's GLOBAL arc length.
 *  Global, not per-contour, because that is what SkTrimPathEffect uses
 *  and therefore what trim() has always meant: a reveal and a trim of the
 *  same numbers must describe the same run. */
struct ContourRun {
  float start = 0, length = 0;
  bool closed = false;
};

std::vector<ContourRun> measureContours(const SkPath &path, float *total) {
  std::vector<ContourRun> runs;
  float at = 0;
  SkContourMeasureIter iter(path, false);
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    const float len = contour->length();
    runs.push_back({at, len, contour->isClosed()});
    at += len;
  }
  if (total)
    *total = at;
  return runs;
}

void pushWindow(std::vector<Span> &out, float lo, float hi, float total) {
  if (total <= 0 || hi <= lo)
    return;
  out.push_back({lo / total, hi / total});
}

/** A window around `d` on one contour, wrapping on a closed contour and
 *  clamping on an open one — an open contour has no seam to wrap over. */
void pushCornerWindow(std::vector<Span> &out, const ContourRun &run, float d,
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

std::vector<Span> cornerSpans(const SkPath &outline, float arm,
                              float angleDeg) {
  std::vector<Span> out;
  float total = 0;
  const std::vector<ContourRun> runs = measureContours(outline, &total);
  if (total <= 0)
    return out;
  size_t i = 0;
  SkContourMeasureIter iter(outline, false);
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    if (i >= runs.size())
      break;
    for (const lines::detail::CornerHit &hit :
         lines::detail::findCorners(*contour, angleDeg))
      pushCornerWindow(out, runs[i], hit.d, arm, total);
    ++i;
  }
  return detail::normalizeSpans(std::move(out));
}

/** The runs of the outline that lie inside a rect — spans::fit(). Walked
 *  rather than solved: the boundary is any shape, including one the rect
 *  enters and leaves several times. */
std::vector<Span> fitSpans(const SkPath &outline, const SkRect &box,
                           float margin) {
  std::vector<Span> out;
  SkRect grown = box;
  grown.outset(margin, margin);
  float total = 0;
  const std::vector<ContourRun> runs = measureContours(outline, &total);
  if (total <= 0)
    return out;
  size_t i = 0;
  SkContourMeasureIter iter(outline, false);
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    if (i >= runs.size())
      break;
    const ContourRun &run = runs[i++];
    const float step = std::max(1.0f, run.length / 512.0f);
    bool inside = false;
    float enter = 0;
    for (float d = 0; d <= run.length + step * 0.5f; d += step) {
      SkPoint pos;
      const float at = std::min(d, run.length);
      if (!contour->getPosTan(at, &pos, nullptr))
        continue;
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

} // namespace

namespace detail {

std::vector<Span> normalizeSpans(std::vector<Span> spans) {
  std::vector<Span> out;
  out.reserve(spans.size());
  for (Span s : spans) {
    if (s.end < s.begin)
      std::swap(s.begin, s.end);
    s.begin = std::clamp(s.begin, 0.0f, 1.0f);
    s.end = std::clamp(s.end, 0.0f, 1.0f);
    if (s.end - s.begin > 1e-6f)
      out.push_back(s);
  }
  std::sort(out.begin(), out.end(),
            [](const Span &a, const Span &b) { return a.begin < b.begin; });
  std::vector<Span> merged;
  for (const Span &s : out) {
    if (!merged.empty() && s.begin <= merged.back().end + 1e-6f)
      merged.back().end = std::max(merged.back().end, s.end);
    else
      merged.push_back(s);
  }
  return merged;
}

std::vector<Span> complementSpans(const std::vector<Span> &spans) {
  std::vector<Span> out;
  float at = 0;
  for (const Span &s : spans) {
    if (s.begin - at > 1e-6f)
      out.push_back({at, s.begin});
    at = std::max(at, s.end);
  }
  if (1.0f - at > 1e-6f)
    out.push_back({at, 1.0f});
  return out;
}

std::optional<Span> spansOverlap(const std::vector<Span> &a,
                                 const std::vector<Span> &b) {
  for (const Span &x : a)
    for (const Span &y : b) {
      const float lo = std::max(x.begin, y.begin);
      const float hi = std::min(x.end, y.end);
      // A shared END POINT is two runs meeting, not two runs overlapping —
      // exactly what corners() next to edges() produces, and it must not
      // be an error.
      if (hi - lo > 1e-4f)
        return Span{lo, hi};
    }
  return std::nullopt;
}

SkPath spanPath(const SkPath &src, const std::vector<Span> &spans) {
  SkPathBuilder out;
  float total = 0;
  const std::vector<ContourRun> runs = measureContours(src, &total);
  if (total <= 0 || spans.empty())
    return out.detach();
  size_t i = 0;
  SkContourMeasureIter iter(src, false);
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    if (i >= runs.size())
      break;
    const ContourRun &run = runs[i++];
    // Emit one span against this contour. `stitch` appends WITHOUT a
    // moveTo, continuing the run already in flight.
    const auto emit = [&](const Span &s, bool stitch) {
      const float lo = std::max(s.begin * total, run.start) - run.start;
      const float hi = std::min(s.end * total, run.start + run.length) -
                       run.start;
      if (hi - lo <= 1e-4f)
        return false;
      // A whole contour claimed whole stays whole — closed stays closed,
      // so joins and additive brushes behave as they do untrimmed.
      if (lo <= 1e-4f && hi >= run.length - 1e-4f)
        (void)contour->getSegment(0, run.length, &out, !stitch);
      else
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
      for (size_t k = 1; k + 1 < spans.size(); ++k)
        (void)emit(spans[k], false);
      continue;
    }
    for (const Span &s : spans)
      (void)emit(s, false);
  }
  return out.detach();
}

SkPath bandRegion(const SkPath &spine, const Across &width,
                  Formation formation) {
  const float reach = width.profile.max();
  if (spine.isEmpty() || reach <= 0)
    return SkPath();
  SkPathBuilder out;
  float total = 0;
  measureContours(spine, &total);
  if (total <= 0)
    return SkPath();
  SkContourMeasureIter iter(spine, false);
  float consumed = 0;
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    const float len = contour->length();
    // `along` is a fraction of the WHOLE spine, so the cursor advances for
    // every contour including the ones the walk gives up on — a degenerate
    // contour that did not advance it would shift the profile of every
    // contour after it.
    const float base = consumed;
    consumed += len;
    if (len <= 0)
      continue;
    const int steps = std::max(8, (int)std::ceil(len / 2.0f));
    std::vector<SkPoint> left, right;
    left.reserve((size_t)steps + 1);
    right.reserve((size_t)steps + 1);
    for (int k = 0; k <= steps; ++k) {
      const float d = len * (float)k / (float)steps;
      SkPoint pos;
      SkVector tan;
      if (!contour->getPosTan(d, &pos, &tan))
        continue;
      const float along = total > 0 ? (base + d) / total : 0.0f;
      const float w = width.profile.across(along);
      // Positive `across` is to the LEFT of travel, which in screen space
      // (y down) puts it OUTSIDE a clockwise path — SkPath's own direction
      // for rects and circles, so `.outward()` exits the shape.
      //
      // This is the NEGATION of lines::offsetAlong, whose documented and
      // implemented convention is right-of-travel ((-tan.y, +tan.x)) — but
      // it MATCHES TextPath::offset, which has always been
      // left-of-travel-is-outward. The kernel says left, the Lines
      // extension says right, and that split predates the band; the band
      // follows the kernel. Stage two shares the Profile value between
      // bands and strands and has to reconcile the two signs.
      const SkVector n{tan.y(), -tan.x()};
      float inner = 0, outer = w;
      if (formation == Formation::Centered) {
        inner = -w * 0.5f;
        outer = w * 0.5f;
      } else if (formation == Formation::Inward) {
        inner = -w;
        outer = 0;
      }
      right.push_back({pos.fX + n.x() * outer, pos.fY + n.y() * outer});
      left.push_back({pos.fX + n.x() * inner, pos.fY + n.y() * inner});
    }
    if (right.size() < 2)
      continue;
    out.moveTo(right.front());
    for (size_t k = 1; k < right.size(); ++k)
      out.lineTo(right[k]);
    for (size_t k = left.size(); k-- > 0;)
      out.lineTo(left[k]);
    out.close();
  }
  return out.detach();
}

} // namespace detail

std::vector<Span> Spans::resolve(const SpanInput &in) const {
  std::vector<Span> out;
  if (!in.outline)
    return out;
  for (size_t t = 0; t < terms.size(); ++t) {
    const Term &term = terms[t];
    switch (term.rule) {
    case Rule::Range: {
      const float a =
          in.values && in.values->size() > t * 2 ? (*in.values)[t * 2] : 0.0f;
      const float b = in.values && in.values->size() > t * 2 + 1
                          ? (*in.values)[t * 2 + 1]
                          : 1.0f;
      out.push_back({a, b});
      break;
    }
    case Rule::Corners: {
      const std::vector<Span> hits =
          cornerSpans(*in.outline, term.arm, term.angleDeg);
      out.insert(out.end(), hits.begin(), hits.end());
      break;
    }
    case Rule::Edges: {
      const std::vector<Span> gaps =
          detail::complementSpans(cornerSpans(*in.outline, term.arm,
                                              term.angleDeg));
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
      if (!in.fitRects)
        break;
      for (const auto &[key, box] : *in.fitRects)
        if (key == term.key) {
          const std::vector<Span> hits =
              fitSpans(*in.outline, box, term.margin);
          out.insert(out.end(), hits.begin(), hits.end());
        }
      break;
    }
    case Rule::Rest:
      break; // the complement needs the element's other passes
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

} // namespace spans

// ---------------------------------------------------------------------------
// band()

SkPoint bandPointAt(const SkPath &spine, float along, float acrossPx) {
  float total = 0;
  measureContours(spine, &total);
  if (total <= 0)
    return {0, 0};
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

Element band(std::function<SkPath(SkSize)> spine, Across width) {
  Element e;
  detail::DeriveData &derive = e.node()->deriveData.ensure();
  derive.bandSpine = std::move(spine);
  derive.bandWidth = std::move(width);
  return e;
}

Element band(Around spine, Across width) {
  Element e;
  detail::DeriveData &derive = e.node()->deriveData.ensure();
  derive.bandAround = std::move(spine.key);
  derive.bandWidth = std::move(width);
  return e;
}

} // namespace sigil::compose
