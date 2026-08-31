/** @file
 * Layout phase: Yoga's calculate pass, the SigilWeave-measured text leaves
 * (measure/baseline callbacks + on-demand text layout), the bounded
 * convergence loop that settles custom layout() containers and centre pins
 * against the geometry Yoga produced, and resolved-rect reads. The derive
 * pass the loop drives lives in Derive.cpp.
 */

#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/layout/Flow.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <optional>
#include <ranges>
#include <set>
#include <span>

#include "ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

// ---------------------------------------------------------------------------
// Text measurement

YGSize detail::measureTextNode(YGNodeConstRef node, float width,
                               YGMeasureMode widthMode, float height,
                               YGMeasureMode heightMode) {
  auto* inst =
      static_cast<Instance*>(YGNodeGetContext(const_cast<YGNodeRef>(node)));
  const float constraint = widthMode == YGMeasureModeUndefined ? 1.0e6f : width;
  // The HEIGHT is a measure too once the text runs down the page: it is what
  // a column may fill before the next one starts, the job the width does for
  // a horizontal line. A horizontal leaf ignores it, exactly as it always
  // has.
  const float down = heightMode == YGMeasureModeUndefined ? 1.0e6f : height;
  inst->owner->layoutText(*inst, constraint, down);
  return inst->measuredSize;
}

namespace {

/** WHERE THE FIRST CHARACTER'S BASELINE SITS, down from the top of what the
 *  layout placed — the one number `Align::Baseline` reads off a text leaf.
 *
 *  A horizontal leaf answers with its first line's ascent. A VERTICAL leaf
 *  HAS NO BASELINE: its reading axis is y, and a column's glyphs centre
 *  themselves ACROSS the axis instead of standing on one. It answers with
 *  its first character's own baseline all the same, because that is the
 *  same question asked of the same glyph — which lines a column's opening
 *  character up with a horizontal neighbour's first line, and is the only
 *  answer either writing mode can give the other. */
float textBaseline(const Instance& inst, const SkRect& bounds) {
  if (!inst.lines.empty()) {
    const sigil::weave::LineMetrics& first = inst.lines.front();
    return first.baseline - first.rect().top();
  }
  if (inst.columns.empty()) return 0.0f;
  // Runs arrive in logical order, so the first one carrying glyphs holds the
  // first character. A rotated run is skipped: its placement is baked per
  // glyph and it has no upright baseline to report.
  for (const sigil::weave::PositionedRun& run : inst.textLayout.runs) {
    if (!run.shaped || run.transformed || run.shaped->positions.empty())
      continue;
    return run.origin.y() + run.shaped->positions.front().y() - bounds.top();
  }
  return 0.0f;
}

}  // namespace

float detail::baselineOfTextNode(YGNodeConstRef node, float, float) {
  auto* inst =
      static_cast<Instance*>(YGNodeGetContext(const_cast<YGNodeRef>(node)));
  return inst->measuredBaseline;
}

void Composer::Impl::layoutText(Instance& inst, float constraint,
                                float downConstraint) {
  // onPath: the PATH is the measure, not the box. Laying the run out to
  // the node's width would wrap it, and every line after the first would
  // then be placed along the path from the start again — the glyphs pile
  // up on each other. The box still sizes the path; it does not bound the
  // run.
  if (inst.desc && inst.desc->textData && inst.desc->textData->onPath)
    constraint = 1.0e6f;
  if (constraint == inst.measuredForWidth &&
      downConstraint == inst.measuredForHeight &&
      inst.measuredRev == inst.contentRev)
    return;  // layout is already valid for this content and measure
  const sigil::weave::ParagraphLayoutOptions options = textLayoutOptions(inst);
  // Vertical-RL: the geometry is columns, not bands, and they hang off the
  // RIGHT edge of the measure — so the constraint is not just a wrap width
  // here, it is where the first column stands. That is why a vertical leaf
  // must be laid out again at its RESOLVED width before it paints, the way
  // aligned horizontal text must (StackingPainter.cpp).
  const bool vertical =
      inst.paragraph &&
      inst.paragraph->writingMode() == sigil::weave::WritingMode::kVerticalRL;
  const auto layOut = [&] {
    if (vertical) {
      // Exclusions subtract HORIZONTAL extents from horizontal line bands;
      // there is no column-flow spelling of them. Say so and run the
      // columns clean rather than silently laying out over the target.
      if (!inst.exclusionsLocal.empty()) detail::warnFlowAroundVertical();
      sigil::weave::VerticalBlockFlow flow(
          SkRect::MakeWH(constraint, downConstraint));
      inst.textLayout =
          sigil::weave::layoutParagraph(fonts, *inst.paragraph, flow, options);
    } else if (!inst.exclusionsLocal.empty()) {
      sigil::weave::ExclusionFlow flow(SkRect::MakeWH(constraint, 1.0e6f));
      const float flowMargin = inst.desc->deriveData
                                   ? inst.desc->deriveData->flowAroundMargin
                                   : 0.0f;
      // One weave shape per resolved target, in the form the derive pass
      // resolved it to: an outline for a target that declared a silhouette,
      // an analytic circle for a round one, its box for a target that
      // declared none. The margin is the same standoff in all three.
      for (const detail::Exclusion& exclusion : inst.exclusionsLocal) {
        if (exclusion.circle)
          flow.shapes().push_back(
              sigil::weave::ExclusionFlow::Shape::fromCircle(exclusion.bounds,
                                                             flowMargin));
        else if (!exclusion.path.isEmpty())
          flow.shapes().push_back(sigil::weave::ExclusionFlow::Shape::fromPath(
              exclusion.path, flowMargin));
        else
          flow.shapes().push_back(
              sigil::weave::ExclusionFlow::Shape::fromRectangle(
                  exclusion.bounds, flowMargin));
      }
      inst.textLayout =
          sigil::weave::layoutParagraph(fonts, *inst.paragraph, flow, options);
    } else {
      sigil::weave::BlockFlow flow(SkRect::MakeWH(constraint, 1.0e6f));
      inst.textLayout =
          sigil::weave::layoutParagraph(fonts, *inst.paragraph, flow, options);
    }
  };
  // ONE of the two is populated, and which one is the writing mode: a
  // column has no baseline to report and lineMetrics() answers with
  // nothing there, while columnMetrics() answers with nothing in a
  // horizontal passage.
  const auto readGeometry = [&] {
    inst.lines.clear();
    inst.columns.clear();
    if (vertical)
      inst.columns = inst.textLayout.columnMetrics(*inst.paragraph);
    else
      inst.lines = inst.textLayout.lineMetrics(*inst.paragraph);
  };
  layOut();
  readGeometry();
  // A sel::line span restyle needs line geometry to name a line at all, and
  // the materialization that ran at describe time had none. Re-materialize
  // against the lines just produced — plain values, so the paragraph they
  // came from is free to go — and lay out once more. The WHOLE restyle list
  // runs again in declaration order, so the "later wins" rule holds across
  // the line-scoped ones and the rest alike.
  //
  // It resolves against THE TEXT BEFORE THE RESTYLE and stops there: a
  // spanStyle that moves the line breaks does not chase its own result,
  // which is what keeps this two passes rather than a fixed-point search
  // that may not have a fixed point.
  if (inst.desc->textData &&
      std::ranges::any_of(inst.desc->textData->spanRestyles,
                          [](const detail::SpanRestyle& restyle) {
                            return detail::selectorNeedsLayout(restyle.where);
                          })) {
    materializeText(inst, inst.lines, inst.columns);
    layOut();
    readGeometry();
  }
  // rich().slot(): where the finished layout put each reserved box. Resolved
  // once per layout rather than per read, and by NAME rather than by index,
  // because a slot the geometry could not place is simply absent from the
  // report and every later slot would otherwise shift up onto its rect.
  if (inst.paragraph && !inst.textSlotKeys.empty()) {
    inst.textSlotRects.clear();
    for (const sigil::weave::ParagraphLayout::PlacedPlaceholder& placed :
         inst.textLayout.placeholderRects(*inst.paragraph)) {
      const size_t index = (size_t)placed.index;
      if (index < inst.textSlotKeys.size())
        inst.textSlotRects.emplace_back(inst.textSlotKeys[index], placed.rect);
    }
  }
  // mark(): where each anchored child's selector landed, resolved here for
  // the same reason the slot rects are — the layout has just finished and
  // is the only thing that knows, and resolving once per layout rather than
  // once per read keeps a mark's box as cheap as a slot's. A PATH-laid
  // run's marks are the one exception: their curve resolves against the
  // node's final box, which this measure does not know, so they resolve in
  // ensureLayout's post-layout pass instead.
  if (!inst.desc->textData || !inst.desc->textData->onPath)
    resolveTextMarks(inst);
  inst.measuredForWidth = constraint;
  inst.measuredForHeight = downConstraint;
  SkRect bounds = SkRect::MakeEmpty();
  for (const sigil::weave::LineMetrics& line : inst.lines)
    bounds.join(line.rect());
  for (const sigil::weave::ColumnMetrics& column : inst.columns)
    bounds.join(column.rect());
  // THE AXES SWAP. A horizontal passage grows along x and stacks on y; a
  // vertical one grows along y and stacks on x, so the same union answers
  // both — one column of type measures tall and one pitch wide.
  inst.measuredSize = {std::ceil(bounds.width()), std::ceil(bounds.height())};
  inst.measuredBaseline = textBaseline(inst, bounds);
  inst.measuredRev = inst.contentRev;
}

bool detail::selectorNeedsLayout(const Selector& selector) {
  const Selector::State* s = selector.state();
  if (!s) return false;
  if (s->kind == Selector::Kind::Line) return true;
  if (s->kind == Selector::Kind::Each && s->each == Unit::Line) return true;
  for (const Selector& operand : s->operands)
    if (selectorNeedsLayout(operand)) return true;
  return false;
}

// ---------------------------------------------------------------------------
// Layout passes

bool Composer::Impl::phaseYoga() {
  YGNodeCalculateLayout(root->yoga, YGUndefined, YGUndefined, YGDirectionLTR);
  return false;
}

bool Composer::Impl::phaseCustomLayouts() {
  return hasCustomLayout && applyCustomLayouts(*root);
}

bool Composer::Impl::phaseCenterPins() {
  return hasCenterPins && applyCenterPins(*root);
}

bool Composer::Impl::phaseDerive() { return hasDerived && resolveDerived(); }

bool Composer::Impl::phasePathMarks() {
  // mark() on a path-laid run resolves HERE, not in measure with the flow
  // runs: the curve resolves against the node's final box, and only the
  // finished layout knows that box. The path layout underneath is memoized
  // against the box and the content; the mark walk itself is one pass over
  // the run's glyphs per layout. Runs before syncLayoutRects so a
  // mark-anchored child whose rect moved is seen by the same invalidation
  // walk as everything else.
  for (detail::Instance* marked : pathMarkInstances) resolveTextMarks(*marked);
  return false;
}

bool Composer::Impl::phaseSyncRects() {
  // Post-layout invalidation: recordings bake geometry (child offsets, text
  // lines, geometry-material uResolution), so any rect that moved or resized
  // must stale the recordings that captured it — even when NO prop changed
  // (setSize, sibling growth, measured-text reflow). Runs only when layout
  // ran; a stable layout is a no-op walk.
  syncLayoutRects(*root);
  return false;
}

void Composer::Impl::ensureLayout() {
  if (!root || (!needsLayout && !YGNodeIsDirty(root->yoga))) return;
  // The root fills the viewport (the CSS-root rule) — except under an empty
  // setSize(), which means "intrinsic": the root sizes to its content (the
  // snapshot()/stamp path).
  if (!size.isEmpty()) {
    YGNodeStyleSetWidth(root->yoga, size.width());
    YGNodeStyleSetHeight(root->yoga, size.height());
  }
  // The runner walks `phases` in order: a non-converging phase runs once,
  // and the converging phases form one contiguous group that repeats until
  // a round changes nothing or the round cap is reached. Custom layout()
  // placement, auto-sizing, centerAt pins and the derive phase all read
  // RESOLVED geometry and then write back into Yoga out of band, so each
  // can move what another already read: an auto-sized container changes the
  // box a pin centres in, and a pinned node moves an anchor a rail routed
  // through. Every writer here is idempotent and reports `changed` only on
  // an actual delta, so a stable layout costs one extra pass and exits, and
  // a settling one converges within the round cap.
  core::runPhases(*this, std::span<const core::Phase<Impl>>(phases),
                  kConvergeRounds, [this] {
                    phaseYoga();
                    // The settle step: refresh routes against the moved
                    // geometry. A route read before the relayout is a route
                    // on stale anchors, so the derive pass runs once more
                    // here rather than waiting for a round that may never
                    // come.
                    phaseDerive();
                  });
  needsLayout = false;
}

void Composer::Impl::syncLayoutRects(Instance& inst, bool movedAbove) {
  const SkRect r = instanceRect(inst);
  const bool rectChanged = r != inst.lastLayoutRect;
  if (rectChanged) {
    const bool sizeChanged = r.width() != inst.lastLayoutRect.width() ||
                             r.height() != inst.lastLayoutRect.height();
    inst.lastLayoutRect = r;
    if (sizeChanged)
      inst.markPaintDirtyUp();  // own content baked the old bounds
    else if (inst.parent)
      // The parent's RECORDING baked this child's old offset; the parent's
      // OWN paint did not — it never contained the child at all. The split
      // bake (own paint baked, volatile children drawn live over the blit)
      // is the cache tier that can tell those apart, and this is the case
      // it exists for: passing ownPaint=true here would remake the parent's
      // bake on every frame a child moves and the tier would silently never
      // pay off.
      inst.parent->markPaintDirtyUp(/*ownPaint=*/false);
    contentDirty = true;
  }
  // A world-space material samples its field in ROOT coordinates, so the
  // node's recording bakes the node-to-root matrix W. Any rect change at or
  // above the node changes W and must stale that recording:
  //   - its own move: the position-only branch above stales the PARENT, not
  //     this node, which is right for ordinary content and wrong here;
  //   - an ancestor's move: instanceRect is parent-relative, so this node's
  //     own rect compares equal while W has changed underneath it.
  // `movedAbove` carries any rect change, position or size, because a
  // resized ancestor with a centred transform origin moves its descendants'
  // W as surely as a repositioned one does.
  if (inst.hasWorldSpaceMaterial && (movedAbove || rectChanged)) {
    inst.markPaintDirtyUp();
    contentDirty = true;
  }
  for (auto& child : inst.children)
    syncLayoutRects(*child, movedAbove || rectChanged);
}

/** centerAt(): set the pinned node's left/top so its MEASURED box centres
 *  on the point. Runs after measurement, since the box is what it centres.
 *  Must stay idempotent — it reports a change only when the target position
 *  actually moved — or the bounded loop in ensureLayout would never see a
 *  quiet round and would burn its full round count every frame. */
bool Composer::Impl::applyCenterPins(Instance& inst) {
  bool applied = false;
  if (inst.desc->layout.centerAt && inst.yoga) {
    const SkPoint p = *inst.desc->layout.centerAt;
    // Correct by the observed layout delta rather than writing the target
    // into the style directly — converges whatever reference box Yoga
    // resolves absolute positions against (padding, borders).
    const float dl = (p.x() - YGNodeLayoutGetWidth(inst.yoga) / 2) -
                     YGNodeLayoutGetLeft(inst.yoga);
    const float dt = (p.y() - YGNodeLayoutGetHeight(inst.yoga) / 2) -
                     YGNodeLayoutGetTop(inst.yoga);
    if (std::abs(dl) > 0.25f || std::abs(dt) > 0.25f) {
      auto styleBase = [&](YGEdge edge) {
        const YGValue v = YGNodeStyleGetPosition(inst.yoga, edge);
        return v.unit == YGUnitPoint ? v.value : 0.0f;
      };
      YGNodeStyleSetPositionType(inst.yoga, YGPositionTypeAbsolute);
      YGNodeStyleSetPosition(inst.yoga, YGEdgeLeft, styleBase(YGEdgeLeft) + dl);
      YGNodeStyleSetPosition(inst.yoga, YGEdgeTop, styleBase(YGEdgeTop) + dt);
      applied = true;
    }
  }
  for (const auto& child : inst.children) applied |= applyCenterPins(*child);
  return applied;
}

bool Composer::Impl::applyCustomLayouts(Instance& inst) {
  bool applied = false;
  // layout() schemes are a flex-world feature; inside a positioned
  // subtree (no Yoga nodes) — or ON a positioned() container, whose
  // children have none — the placeFn is documented-unsupported.
  if (inst.yoga && !inst.desc->layout.positioned && inst.desc->deriveData &&
      inst.desc->deriveData->placeFn && !inst.children.empty()) {
    LayoutInput input;
    input.container = {YGNodeLayoutGetWidth(inst.yoga),
                       YGNodeLayoutGetHeight(inst.yoga)};
    for (const auto& child : inst.children) {
      input.childSizes.push_back({YGNodeLayoutGetWidth(child->yoga),
                                  YGNodeLayoutGetHeight(child->yoga)});
      // First-baseline offset from the child's top — measured text only
      // (pass one measured it); everything else has no baseline.
      float baseline = std::numeric_limits<float>::quiet_NaN();
      if (!child->lines.empty()) {
        const sigil::weave::LineMetrics& first = child->lines.front();
        baseline = first.baseline - first.rect().top();
      }
      input.childBaselines.push_back(baseline);
    }
    std::vector<SkRect> rects = inst.desc->deriveData->placeFn(input);
    const size_t count = std::min(rects.size(), inst.children.size());
    for (size_t i = 0; i < count; ++i) {
      // A centerAt() child opts OUT of the scheme's placement — the pin
      // wins (otherwise place() and the pin fight in a period-2
      // oscillation that never settles).
      if (inst.children[i]->desc->layout.centerAt) continue;
      YGNodeRef child = inst.children[i]->yoga;
      // Count a change only on an actual delta: the convergence loop in
      // ensureLayout keys off this (idempotent writes are free).
      const SkRect cur = instanceRect(*inst.children[i]);
      if (std::abs(cur.left() - rects[i].left()) > 0.25f ||
          std::abs(cur.top() - rects[i].top()) > 0.25f ||
          std::abs(cur.width() - rects[i].width()) > 0.25f ||
          std::abs(cur.height() - rects[i].height()) > 0.25f)
        applied = true;
      YGNodeStyleSetPositionType(child, YGPositionTypeAbsolute);
      YGNodeStyleSetPosition(child, YGEdgeLeft, rects[i].left());
      YGNodeStyleSetPosition(child, YGEdgeTop, rects[i].top());
      YGNodeStyleSetWidth(child, rects[i].width());
      YGNodeStyleSetHeight(child, rects[i].height());
    }
    // Auto-size an ABSOLUTE container from the placed extent, per axis,
    // when the author left that axis open (no explicit dim, no
    // opposing-inset pair) — an absolutely-positioned container has no flex
    // parent to size it, so without this it would collapse and the scheme
    // would place its children outside a zero box. Flex-embedded layout()
    // containers are left alone: their flex/stretch sizing already holds.
    const LayoutProps& l = inst.desc->layout;
    if (l.absolute) {
      SkRect extent = SkRect::MakeEmpty();
      for (size_t i = 0; i < count; ++i) extent.join(rects[i]);
      const bool widthPinned = l.hasInsets &&
                               l.insets.left.unit != Dim::Unit::Auto &&
                               l.insets.right.unit != Dim::Unit::Auto;
      const bool heightPinned = l.hasInsets &&
                                l.insets.top.unit != Dim::Unit::Auto &&
                                l.insets.bottom.unit != Dim::Unit::Auto;
      if (l.width.unit == Dim::Unit::Auto && !widthPinned &&
          extent.right() > 0 &&
          std::abs(YGNodeLayoutGetWidth(inst.yoga) - extent.right()) > 0.25f) {
        YGNodeStyleSetWidth(inst.yoga, extent.right());
        applied = true;
      }
      if (l.height.unit == Dim::Unit::Auto && !heightPinned &&
          extent.bottom() > 0 &&
          std::abs(YGNodeLayoutGetHeight(inst.yoga) - extent.bottom()) >
              0.25f) {
        YGNodeStyleSetHeight(inst.yoga, extent.bottom());
        applied = true;
      }
    }
  }
  for (const auto& child : inst.children) applied |= applyCustomLayouts(*child);
  return applied;
}

// ---------------------------------------------------------------------------
// Resolved-rect reads

SkRect Composer::Impl::instanceRect(const Instance& inst) const {
  if (inst.yoga)
    return SkRect::MakeXYWH(
        YGNodeLayoutGetLeft(inst.yoga), YGNodeLayoutGetTop(inst.yoga),
        YGNodeLayoutGetWidth(inst.yoga), YGNodeLayoutGetHeight(inst.yoga));
  return positionedRect(inst);
}

namespace {

/** A child keyed for a slot the text content DOES NOT DECLARE. Loud once,
 *  because the symptom — a pill that simply is not there — points at the
 *  child rather than at the misspelling in the caption that was supposed to
 *  reserve room for it.
 *
 *  A key the content does declare but the geometry could not place is a
 *  different answer and stays silent: it is the same "did not fit" a line
 *  clamp, an ellipsis or an exclusion gives every other word. */
void warnUnknownTextSlot(const Instance& text, const std::string& key) {
  for (const std::string& declared : text.textSlotKeys)
    if (declared == key)
      return;  // declared; the layout just could not place it
  static std::set<std::string> warned;  // once per name, not once per frame
  if (!warned.insert(key).second) return;
  std::string have;
  for (const std::string& declared : text.textSlotKeys)
    have += (have.empty() ? "" : ", ") + declared;
  SkDebugf(
      "[compose] a child keyed \"%s\" sits on text that reserves no slot by "
      "that name, so it lays out at zero and draws nothing. Slots this "
      "text DOES reserve: [%s]. Room for one is reserved in the CONTENT, "
      "with rich(...).slot(name, size).\n",
      key.c_str(), have.empty() ? "none" : have.c_str());
}

}  // namespace

/** A positioned child's rect, straight from its description: px/pct
 *  left/top insets, px/pct dims, an open dim with an opposing
 *  right/bottom inset pinning the far edge, and text measuring against
 *  its resolved (or the parent's) width. O(depth) for pct/derived
 *  terms — arithmetic, no engine. */
SkRect Composer::Impl::positionedRect(const Instance& inst) const {
  // A MARK child: the rect its selector resolved is its PARENT BOX, not its
  // box. Everything the child says about its own placement is then read
  // against that rect exactly as a positioned child reads it against its
  // parent's — px or pct, in the rect's space, and free to land outside it
  // — and a child that says nothing at all simply IS the rect. That is the
  // whole difference from a slot below, whose box the CONTENT reserved and
  // whose child cannot argue with it. Looked up first for the same reason:
  // a text node may carry both, and a mark is not an unknown slot.
  const SkRect* anchor = nullptr;
  if (inst.parent && inst.parent->desc->kind == Kind::Text &&
      inst.parent->desc->textData && !inst.desc->key.empty() &&
      std::ranges::any_of(inst.parent->desc->textData->marks,
                          [&](const detail::MarkAnchor& mark) {
                            return mark.key == inst.desc->key;
                          })) {
    for (const auto& [key, rect] : inst.parent->textMarkRects)
      if (key == inst.desc->key) {
        anchor = &rect;
        break;
      }
    // A mark whose selector resolved NOTHING has no rect, and the honest
    // answer is an empty box rather than the child's own dims at the text
    // node's origin — which would be a mark drawn confidently in the wrong
    // place. resolveTextMarks() already said so once.
    if (!anchor) return SkRect::MakeEmpty();
  }
  // A TEXT SLOT child: its box is the placeholder rect the paragraph
  // reserved for its key, and nothing in the child's own description
  // decides it. Read here rather than written back into the tree because a
  // slot child has no Yoga node to write to — the paragraph IS its layout,
  // so a reflow that moves the placeholder moves the child with no second
  // pass and no convergence round.
  if (!anchor && inst.parent && inst.parent->desc->kind == Kind::Text &&
      !inst.parent->textSlotKeys.empty() && !inst.desc->key.empty()) {
    for (const auto& [key, rect] : inst.parent->textSlotRects)
      if (key == inst.desc->key) return rect;
    warnUnknownTextSlot(*inst.parent, inst.desc->key);
    return SkRect::MakeEmpty();
  }
  const LayoutProps& l = inst.desc->layout;
  float parentW = 0, parentH = 0;
  if (anchor) {
    parentW = anchor->width();
    parentH = anchor->height();
  } else if (inst.parent) {
    const SkRect parentRect = instanceRect(*inst.parent);
    parentW = parentRect.width();
    parentH = parentRect.height();
  }
  auto resolve = [](const Dim& d, float parentExtent) -> std::optional<float> {
    switch (d.unit) {
      case Dim::Unit::Px:
        return d.value;
      case Dim::Unit::Pct:
        return parentExtent * d.value / 100.0f;
      case Dim::Unit::Auto:
      default:
        return std::nullopt;
    }
  };
  const float left =
      l.hasInsets ? resolve(l.insets.left, parentW).value_or(0.0f) : 0.0f;
  const float top =
      l.hasInsets ? resolve(l.insets.top, parentH).value_or(0.0f) : 0.0f;
  std::optional<float> width = resolve(l.width, parentW);
  std::optional<float> height = resolve(l.height, parentH);
  if (!width && l.hasInsets)
    if (std::optional<float> right = resolve(l.insets.right, parentW))
      width = std::max(parentW - left - *right, 0.0f);
  if (!height && l.hasInsets)
    if (std::optional<float> bottom = resolve(l.insets.bottom, parentH))
      height = std::max(parentH - top - *bottom, 0.0f);
  // Text with an open extent: measure now, against the width we have.
  // The measure caches are logically mutable (measuredForWidth guards),
  // hence the casts.
  if (inst.desc->kind == Kind::Text && inst.paragraph && (!width || !height)) {
    const_cast<Composer::Impl*>(this)->layoutText(const_cast<Instance&>(inst),
                                                  width ? *width : parentW,
                                                  height ? *height : 1.0e6f);
    if (!width) width = inst.measuredSize.width;
    if (!height) height = inst.measuredSize.height;
  }
  if (anchor) {
    // An unstated extent on a mark is the ANCHOR's, which is what makes a
    // mark with no dims at all the unit's own rect. Text keeps the extent
    // it just measured for itself above.
    if (!width) width = parentW;
    if (!height) height = parentH;
    return SkRect::MakeXYWH(anchor->left() + left, anchor->top() + top, *width,
                            *height);
  }
  return SkRect::MakeXYWH(left, top, width.value_or(0.0f),
                          height.value_or(0.0f));
}

SkRect Composer::Impl::absoluteRect(const Instance& inst) const {
  SkRect rect = instanceRect(inst);
  for (Instance* p = inst.parent; p; p = p->parent) {
    const SkRect parentRect = instanceRect(*p);
    rect.offset(parentRect.left(), parentRect.top());
  }
  return rect;
}

}  // namespace sigil::compose
