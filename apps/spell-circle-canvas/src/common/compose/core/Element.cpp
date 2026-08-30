/** @file
 * Element value builders — copy-on-write mutations of description
 * payloads, and the factories that start one. Nothing here talks to Yoga,
 * Skia surfaces or Choreograph; that is the Composer's job.
 */

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
#include "sigilgeometry/path/Contour.h"

namespace sigil::compose {

using detail::ElementNode;
using detail::Kind;

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

Element& Element::mark(Selector where, Element what) {
  detail::TextData& text = m_node->textData.ensure();
  // A KEY IS THE ANCHOR'S HANDLE, so a mark that carries none is given one
  // from its declaration order: the layout looks its rect up by key, and
  // the reconciler matches children by key. The generated name is namespaced
  // with a character no author writes, so it cannot collide with a key
  // somebody chose.
  if (what.node()->key.empty())
    what.key("mark#" + std::to_string(text.marks.size()));
  text.marks.push_back({std::move(where), what.node()->key});
  return child(std::move(what));
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
      // Only an ADVANCE-INVARIANT axis is honoured, which is precisely the
      // condition that the glyphs keep the pen positions shaping gave them:
      // a drive re-cuts outlines where they already stand, so a run under a
      // sweeping grade is type at rest and keeps its whole-pixel origins.
      /*reach=*/0.0f, /*curves=*/{}, /*displaces=*/false);
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

}  // namespace detail

}  // namespace sigil::compose
