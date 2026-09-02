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
      if (t.rule == Spans::Rule::Fit && !t.key.empty()) {
        detail::DeriveData& derive = m_node->deriveData.ensure();
        derive.spanFitKeys.push_back(t.key);
        // A gap sized from where a node LANDED is a read of its box.
        derive.reads.push_back({t.key, sigil::core::Facet::Bounds});
      }
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
  for (const std::string& key : d.borrows()) {
    derive.borrowedPathKeys.push_back(key);
    // A borrowed strand is the target's own outline, not its box.
    derive.reads.push_back({key, sigil::core::Facet::Outline});
  }
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
                                  motion::Spread::From from) {
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

Element frame(Story story) {
  Element e = text(story.content());
  const std::span<const sigil::weave::ParagraphStyle> blocks = story.blocks();
  if (!blocks.empty())
    e.paragraphs(std::vector<sigil::weave::ParagraphStyle>(blocks.begin(),
                                                           blocks.end()));
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

Element& Element::paragraphs(std::vector<sigil::weave::ParagraphStyle> blocks) {
  detail::TextOptions& options = m_node->textData.ensure().options;
  options.blocks = std::move(blocks);
  options.set |= detail::TextOptions::kBlocks;
  return *this;
}

Element& Element::paragraphs(std::span<const std::string_view> names) {
  // The set is read HERE, inside the author's describe scope, exactly as a
  // named character run reads its own: the finished description then holds
  // real styles and depends on no scope that has since ended.
  const sigil::weave::ParagraphStyleSet* set =
      env::inherited<sigil::weave::ParagraphStyleSet>();
  std::vector<sigil::weave::ParagraphStyle> resolved;
  resolved.reserve(names.size());
  for (const std::string_view name : names)
    resolved.push_back(set ? (*set)[name] : sigil::weave::ParagraphStyle{});
  return paragraphs(std::move(resolved));
}

Element& Element::paragraph(sigil::weave::ParagraphStyle style) {
  return paragraphs(
      std::vector<sigil::weave::ParagraphStyle>{std::move(style)});
}

Element& Element::firstBaseline(sigil::weave::FrameOptions::FirstBaseline rule,
                                float offset) {
  detail::TextOptions& options = m_node->textData.ensure().options;
  options.frame.firstBaseline = rule;
  options.frame.firstBaselineOffset = offset;
  options.set |= detail::TextOptions::kFrame;
  return *this;
}

Element& Element::distribute(sigil::weave::FrameOptions::Distribute rule,
                             float maximumInterlineSpacing) {
  detail::TextOptions& options = m_node->textData.ensure().options;
  options.frame.distribute = rule;
  options.frame.maximumInterlineSpacing = maximumInterlineSpacing;
  options.set |= detail::TextOptions::kFrame;
  return *this;
}

Element& Element::justification(sigil::weave::JustificationOptions spec) {
  detail::TextOptions& options = m_node->textData.ensure().options;
  options.justification = std::move(spec);
  options.set |= detail::TextOptions::kJustification;
  return *this;
}

Element& Element::tabStops(sigil::weave::TabStopOptions stops) {
  detail::TextOptions& options = m_node->textData.ensure().options;
  options.tabStops = std::move(stops);
  options.set |= detail::TextOptions::kTabStops;
  return *this;
}

Element& Element::boundary(Boundary source) {
  m_node->boundary = source;
  return *this;
}

Element& Element::thread(std::string_view key) {
  m_node->textData.ensure().threadTo = std::string(key);
  // A frame is cut where the frame before it stopped, so it reads the
  // FINEST answer that frame produces — its units, not its box. The link
  // itself is LAST-WINS, so the read is replaced rather than added to: a
  // frame threads into exactly one frame, and a chain that named another
  // one first no longer waits for it.
  detail::DeriveData& derive = m_node->deriveData.ensure();
  std::erase_if(derive.reads, [](const sigil::core::Read& read) {
    return read.facet == sigil::core::Facet::Units;
  });
  derive.reads.push_back({std::string(key), sigil::core::Facet::Units});
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

Element& Element::flowAround(std::string_view key, float margin) {
  detail::DeriveData& derive = m_node->deriveData.ensure();
  derive.flowAroundKeys.emplace_back(key);
  derive.flowAroundMargin = margin;
  // An exclusion subtracts the target's SILHOUETTE where it declares one,
  // which is a read of its outline and not merely of its box.
  derive.reads.push_back({std::string(key), sigil::core::Facet::Outline});
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
  // A wire is routed between two settled BOXES, one at each end.
  derive.reads.push_back({derive.connectFrom, sigil::core::Facet::Bounds});
  derive.reads.push_back({derive.connectTo, sigil::core::Facet::Bounds});
  return e;
}

Element rail(std::vector<Anchor> anchors, RailRouter router) {
  Element e;
  e.node()->kind = Kind::Custom;  // painted via the derive-routed outline
  detail::DeriveData& derive = e.node()->deriveData.ensure();
  derive.railAnchors = std::move(anchors);
  derive.railRouter = std::move(router);
  // …and a rail through as many boxes as it has waypoints.
  for (const Anchor& anchor : derive.railAnchors)
    derive.reads.push_back({anchor.nodeKey, sigil::core::Facet::Bounds});
  return e;
}

Element slot(std::string_view name) {
  Element e;
  e.node()->kind = Kind::Slot;
  e.node()->key = std::string(name);
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

// ---------------------------------------------------------------------------
// TextEffect — the comparable effect value (its comparator sits with the
// others in Reconcile.cpp)

TextEffect::TextEffect(std::string name, std::vector<float> params,
                       GlyphModFn fn, float reach,
                       std::vector<choreograph::EaseFn> curves,
                       bool displaces) {
  auto state = std::make_shared<State>();
  state->name = std::move(name);
  state->params = std::move(params);
  state->curves = std::move(curves);
  state->fn = std::move(fn);
  state->reach = reach;
  state->displaces = displaces;
  m_state = std::move(state);
}

TextEffect TextEffect::composite(std::string name, std::vector<float> params,
                                 std::vector<TextEffect> operands,
                                 GlyphModFn fn, float reach, bool displaces) {
  auto state = std::make_shared<State>();
  state->name = std::move(name);
  state->params = std::move(params);
  state->operands = std::move(operands);
  state->fn = std::move(fn);
  state->reach = reach;
  state->displaces = displaces;
  TextEffect out;
  out.m_state = std::move(state);
  return out;
}

const std::string& TextEffect::name() const {
  static const std::string kEmpty;
  return m_state ? m_state->name : kEmpty;
}

std::span<const float> TextEffect::params() const {
  return m_state ? std::span<const float>(m_state->params)
                 : std::span<const float>();
}

std::span<const TextEffect> TextEffect::operands() const {
  return m_state ? std::span<const TextEffect>(m_state->operands)
                 : std::span<const TextEffect>();
}

TextEffect TextEffect::pass(Material material) {
  const sigil::material::Material* backing = material.recipeMaterial();
  if (!backing || !backing->recipe().has(sigil::material::Target::SkSL)) {
    // Once per process: the door takes only the recipe-backed form,
    // because the runtime specializes the recipe per unit count and needs
    // the SkSL body to do it.
    static bool warned = false;
    if (!warned) {
      warned = true;
      SkDebugf(
          "[compose] fx::pass: the material carries no SkSL recipe — a "
          "pass is compiled per unit count, which needs "
          "Material::recipe(...) over a recipe with an SkSL body. The "
          "effect is empty and the track draws its glyphs at rest.\n");
    }
    return {};
  }
  auto state = std::make_shared<State>();
  state->name = "pass";
  // The identity body keeps the value truthy (a track with an empty effect
  // is skipped) and keeps a pass harmless as a seq/mix/hold operand, where
  // only the deviation is consulted.
  state->fn = [](const GlyphInfo&, float, Rng&) { return GlyphMod{}; };
  // A pass paints where its material says it does; the material's declared
  // reserve is the effect's reach, and Track::reach overrides as ever.
  state->reach = material.bleed();
  // A PASS IS NOT A PLACEMENT. Its shader reads a layer whose glyphs were
  // rasterized at their RESTING origins — the pass moves pixels, not pen
  // positions — so putting those origins on the subpixel grid refines masks
  // the shader's own output does not follow, and pays the multiplied atlas
  // population for letters that are provably standing still. Whatever the
  // pass does with them, the layer is re-rendered every frame it runs.
  state->displaces = false;
  state->pass = std::make_shared<const Material>(std::move(material));
  TextEffect out;
  out.m_state = std::move(state);
  return out;
}

const Material* TextEffect::passMaterial() const {
  return m_state ? m_state->pass.get() : nullptr;
}

TextEffect TextEffect::withRests(std::initializer_list<float> phases) const {
  if (!m_state || !m_state->pass) {
    // Once per process: the declaration is about a pass's SkSL, and a
    // per-glyph effect has no shader to promise anything about.
    static bool warned = false;
    if (!warned) {
      warned = true;
      SkDebugf(
          "[compose] restsAt() declares where a PASS's shader is an exact "
          "pass-through; this effect carries no pass material, so the "
          "declaration says nothing and is dropped.\n");
    }
    return *this;
  }
  auto state = std::make_shared<State>(*m_state);
  state->params.insert(state->params.end(), phases.begin(), phases.end());
  TextEffect out;
  out.m_state = std::move(state);
  return out;
}

TextEffect TextEffect::restsAt(float phase) const { return withRests({phase}); }
TextEffect TextEffect::restsAt(float a, float b) const {
  return withRests({a, b});
}

std::span<const float> TextEffect::restPhases() const {
  return m_state && m_state->pass ? std::span<const float>(m_state->params)
                                  : std::span<const float>();
}

bool TextEffect::displaces() const { return m_state && m_state->displaces; }

TextEffect TextEffect::displacing(bool moves) const {
  if (!m_state) return *this;
  if (m_state->pass) {
    // Once per process: a pass runs over already-rasterized pixels, so it
    // has no pen position to move and the declaration says nothing. Its
    // params slot is the rest declaration, which this must not write into.
    static bool warned = false;
    if (!warned) {
      warned = true;
      SkDebugf(
          "[compose] displacing() declares whether a body moves glyphs off "
          "their pen positions; a pass runs over pixels already rasterized "
          "at the resting origins, so the declaration is dropped.\n");
    }
    return *this;
  }
  auto state = std::make_shared<State>(*m_state);
  state->displaces = moves;
  // The declaration JOINS THE PARAMS, which is where an effect's identity
  // already lives — no second equality lane, and two bodies under one key
  // that disagree about placement compare unequal and re-patch. Every
  // library-built effect answers at construction and never comes through
  // here, so nothing appends twice.
  state->params.push_back(moves ? 1.0f : 0.0f);
  TextEffect out;
  out.m_state = std::move(state);
  return out;
}

Phase TextEffect::until(float t) const { return Phase(*this, t); }

// ---------------------------------------------------------------------------
// Selector

Selector Selector::of(State s) {
  Selector out;
  out.m_state = std::make_shared<const State>(std::move(s));
  return out;
}

Selector Selector::take(int n) const {
  State s = m_state ? *m_state : State{};
  s.take = n;
  return of(std::move(s));
}

Selector Selector::drop(int n) const {
  State s = m_state ? *m_state : State{};
  s.drop = n;
  return of(std::move(s));
}

namespace {
Selector combine(Selector::Kind kind, const Selector& a, const Selector& b) {
  Selector::State s;
  s.kind = kind;
  s.operands = {a, b};
  return Selector::of(std::move(s));
}
}  // namespace

Selector Selector::operator|(const Selector& other) const {
  return combine(Kind::Union, *this, other);
}
Selector Selector::operator&(const Selector& other) const {
  return combine(Kind::Intersect, *this, other);
}
Selector Selector::operator!() const {
  State s;
  s.kind = Kind::Complement;
  s.operands = {*this};
  return of(std::move(s));
}

namespace sel {
namespace {
Selector indexed(Selector::Kind kind, uint32_t lo, uint32_t hi) {
  Selector::State s;
  s.kind = kind;
  s.lo = lo;
  s.hi = hi;
  return Selector::of(std::move(s));
}
Selector needle(Selector::Kind kind, std::u8string_view pattern) {
  Selector::State s;
  s.kind = kind;
  s.pattern = std::u8string(pattern);
  return Selector::of(std::move(s));
}
}  // namespace

Selector word(uint32_t index) {
  return indexed(Selector::Kind::Word, index, index + 1);
}
Selector words(uint32_t lo, uint32_t hi) {
  return indexed(Selector::Kind::Words, lo, hi);
}
Selector line(uint32_t index) {
  return indexed(Selector::Kind::Line, index, index + 1);
}
Selector sentence(uint32_t index) {
  return indexed(Selector::Kind::Sentence, index, index + 1);
}
Selector range(sigil::weave::CharRange chars) {
  return indexed(Selector::Kind::Range, chars.start, chars.end);
}
Selector regex(std::u8string_view utf8Pattern) {
  return needle(Selector::Kind::Regex, utf8Pattern);
}
Selector text(std::u8string_view utf8Substring) {
  return needle(Selector::Kind::Text, utf8Substring);
}
Selector style(std::string_view name) {
  // A style name is ASCII-or-whatever the author typed, and the needle slot
  // holds UTF-8 bytes; both resolvers compare it against the same bytes a
  // run's name was written with, so no transcoding is involved either way.
  return needle(Selector::Kind::Style,
                std::u8string_view((const char8_t*)name.data(), name.size()));
}
Selector each(Unit granularity) {
  Selector::State s;
  s.kind = Selector::Kind::Each;
  s.each = granularity;
  return Selector::of(std::move(s));
}
}  // namespace sel

// ---------------------------------------------------------------------------
// The cascade over text

motion::Spread cues(std::vector<float> startMs, motion::Spread spec) {
  spec.cueMs = std::move(startMs);
  return spec;
}

namespace {
const TextPainterOps*& textEngineSlot() {
  static const TextPainterOps* engine = nullptr;
  return engine;
}
}  // namespace

void detail::registerTextEngine(const TextPainterOps* engine) {
  textEngineSlot() = engine;
}

const TextPainterOps* detail::registeredTextEngine() {
  return textEngineSlot();
}

void detail::TextOptions::applyTo(
    sigil::weave::ParagraphLayoutOptions& options) const {
  if (set & kAlignment) options.alignment = alignment;
  if (set & kLineBreak) options.lineBreakStrategy = lineBreak;
  if (set & kHyphenation) options.hyphenation = hyphenation;
  if (set & kEllipsis) options.overflow.ellipsis = ellipsis;
  if (set & kMaxLines) options.overflow.maxLines = maxLines;
  if (set & kJustification) options.justification = justification;
  if (set & kLastLine) {
    options.justification.lastLineAlignment = lastLineAlignment;
    options.justification.justifyLastLine = justifyLastLine;
  }
  if (set & kTabStops) options.tabStops = tabStops;
  if (set & kBlocks) options.blocks = blocks;
  if (set & kFrame) options.frame = frame;
}

std::u16string detail::toUtf16(std::u8string_view utf8) {
  // Hand-rolled rather than borrowed from the weave layer: compose speaks
  // UTF-8 at its surface and UTF-16 at exactly this boundary, and a full
  // Unicode library for that is a dependency the kernel does not otherwise
  // need. Ill-formed input yields U+FFFD, never a silent truncation.
  std::u16string out;
  out.reserve(utf8.size());
  for (size_t i = 0; i < utf8.size();) {
    const auto byte = (unsigned char)utf8[i];
    char32_t code = 0xFFFD;
    size_t length = 1;
    if (byte < 0x80) {
      code = byte;
    } else if ((byte & 0xE0u) == 0xC0) {
      length = 2;
      code = byte & 0x1Fu;
    } else if ((byte & 0xF0u) == 0xE0) {
      length = 3;
      code = byte & 0x0Fu;
    } else if ((byte & 0xF8u) == 0xF0) {
      length = 4;
      code = byte & 0x07u;
    }
    if (length > 1) {
      if (i + length > utf8.size()) {
        code = 0xFFFD;
        length = utf8.size() - i;
      } else {
        for (size_t k = 1; k < length; ++k) {
          const auto continuation = (unsigned char)utf8[i + k];
          if ((continuation & 0xC0u) != 0x80) {
            code = 0xFFFD;
            length = k;
            break;
          }
          code = (code << 6u) | (continuation & 0x3Fu);
        }
      }
    }
    if (code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) code = 0xFFFD;
    if (code >= 0x10000) {
      const char32_t rest = code - 0x10000;
      out.push_back((char16_t)(0xD800 + (rest >> 10u)));
      out.push_back((char16_t)(0xDC00 + (rest & 0x3FFu)));
    } else {
      out.push_back((char16_t)code);
    }
    i += length;
  }
  return out;
}

TextEffect TextEffect::variableAxis(const char (&tag)[5], float value) {
  const sigil::weave::FontVariation coordinate(tag, value);
  return TextEffect(
      "variableAxis",
      {(float)(unsigned char)tag[0], (float)(unsigned char)tag[1],
       (float)(unsigned char)tag[2], (float)(unsigned char)tag[3], value},
      [coordinate](const GlyphInfo&, float, Rng&) {
        GlyphMod m;
        m.axis = coordinate;
        return m;
      },
      // An advance-invariant axis leaves every pen position where the
      // layout put it, so the effect does not displace.
      0.0f, {}, /*displaces=*/false);
}

// ---------------------------------------------------------------------------
// Region — the masking family's comparable region value (its resolution
// against a node's outline is the brush tier's)

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
