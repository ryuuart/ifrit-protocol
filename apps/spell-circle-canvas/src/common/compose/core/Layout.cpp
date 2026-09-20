/** @file
 * Layout phase: Yoga's calculate pass and the bounded convergence loop
 * that settles custom layout() containers and centre pins against the
 * geometry Yoga produced. The text leaves it measures through are in
 * LayoutText.cpp, the derive pass the loop drives in Derive.cpp, and the
 * rects it all resolves to in Rects.cpp.
 */

#include <algorithm>
#include <boost/unordered/unordered_flat_set.hpp>
#include <cmath>
#include <iterator>
#include <optional>
#include <span>

#include "ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

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
  // The cascade before layout, wherever layout is asked for — a frame, a
  // bake, an intrinsic size: an inheriting leaf must be set in its font
  // before it is measured, and a length in ems before Yoga reads it.
  if (cascadeDirty) runCascade();
  if (!root || (!needsLayout && !YGNodeIsDirty(root->yoga))) return;
  // A length measured against the CANVAS is resolved into pixels before
  // Yoga sees it, so a canvas of a new size means those styles are stale and
  // nothing else would notice: a percent Yoga owns re-resolves itself, and
  // one of these does not.
  if (anyCanvasLengths && canvasLengthsAt != size) {
    canvasLengthsAt = size;
    reapplyCanvasLengths(*root);
  }
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

void Composer::Impl::reapplyCanvasLengths(Instance& inst) {
  if (inst.canvasLengths) applyLayoutProps(inst);
  if (originsFollowCanvas(*inst.description)) inst.markPaintDirtyUp();
  for (auto& child : inst.children) reapplyCanvasLengths(*child);
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
  if (inst.description->layout.centerAt && inst.yoga) {
    const SkPoint p = *inst.description->layout.centerAt;
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

/** THE SMALLEST @p child CAN BE without its content spilling — what a
 *  track-sizing rule floors a content-sized track at.
 *
 *  A text leaf is asked: laid out at a nil measure it wraps at every
 *  opportunity, so what it reports back is the widest run it cannot break,
 *  which is exactly the minimum. The measure it was standing at is then
 *  restored, because the layout the rest of the pass reads must be the one
 *  the node's own box produced and not this probe.
 *
 *  Everything else answers with what Yoga measured. A scheme does not
 *  re-describe a box at a proposed width, so there is no honest
 *  smaller number to give for a box: reporting zero would let a content
 *  track collapse under content that cannot in fact shrink. */
SkSize Composer::Impl::minimumSizeOf(Instance& child) {
  const SkSize box{YGNodeLayoutGetWidth(child.yoga),
                   YGNodeLayoutGetHeight(child.yoga)};
  SkSize least = box;
  if (!child.paragraph) return least;
  const float wasWidth = child.measuredForWidth;
  const float wasHeight = child.measuredForHeight;
  layoutText(child, 0.0f, kUnbounded);
  // The answer is a BOX, as every other minimum here is: the widest run the
  // paragraph cannot break, standing inside the leaf's own padding.
  least.fWidth = child.measuredSize.width + paddingOf(child).across();
  // THE PROBE IS PUT BACK WHATEVER IT ANSWERED. A child that carries no
  // previous measure — the first pass, or a layout that degraded — is
  // laid out at the box it resolved to, because the alternative is
  // leaving it wrapped at nil width for the frame the probe ran in.
  if (wasWidth >= 0)
    layoutText(child, wasWidth, wasHeight);
  else
    layoutTextInBox(child, box.width(), box.height());
  return least;
}

namespace {

/** The node whose flex line @p inst is an item of: its parent, or the
 *  nearest ancestor above parents that have no box of their own. */
const Instance* flexContainerOf(const Instance& inst) {
  const Instance* container = inst.parent;
  while (container && container->parent &&
         container->description->layout.display == Display::Contents)
    container = container->parent;
  return container;
}

bool constrainedAxis(const Instance& inst, bool horizontal) {
  const LayoutProps& layout = inst.description->layout;
  const Dimension& size = horizontal ? layout.width : layout.height;
  if (size.unit != Dimension::Unit::Auto) return true;
  if (layout.hasInsets &&
      (horizontal ? layout.insets.left : layout.insets.top).unit !=
          Dimension::Unit::Auto &&
      (horizontal ? layout.insets.right : layout.insets.bottom).unit !=
          Dimension::Unit::Auto)
    return true;
  if (!inst.parent) return true;
  const Instance& parent = *flexContainerOf(inst);
  if (parent.description->deriveData && parent.description->deriveData->placeFn)
    return true;
  const LayoutProps& parentLayout = parent.description->layout;
  if (horizontal == mainAxisHorizontal(parentLayout.direction))
    return layout.grow > 0 || layout.basis.unit != Dimension::Unit::Auto;
  const Align alignment = layout.alignSelf == Align::Auto
                              ? parentLayout.alignItems
                              : layout.alignSelf;
  return alignment == Align::Stretch && constrainedAxis(parent, horizontal);
}

// A stretched child still contributes its natural extent to an auto-sized
// flex line. Once a scheme's children are placed absolutely, Yoga cannot
// derive that contribution from them; the scheme supplies it instead.
bool stretchedFromContent(const Instance& inst, bool horizontal) {
  if (!inst.parent) return false;
  const Instance& container = *flexContainerOf(inst);
  const LayoutProps& layout = inst.description->layout;
  const LayoutProps& parent = container.description->layout;
  const Align alignment =
      layout.alignSelf == Align::Auto ? parent.alignItems : layout.alignSelf;
  return horizontal != mainAxisHorizontal(parent.direction) &&
         alignment == Align::Stretch && !constrainedAxis(container, horizontal);
}

float boundInPixels(const Instance& inst, bool horizontal, YGValue value) {
  if (value.unit == YGUnitPoint) return value.value;
  if (value.unit == YGUnitPercent && inst.parent) {
    const float parent = horizontal ? YGNodeLayoutGetWidth(inst.parent->yoga)
                                    : YGNodeLayoutGetHeight(inst.parent->yoga);
    return value.value * parent * 0.01f;
  }
  return YGUndefined;
}

float maximumMeasure(const Instance& inst, bool horizontal, float extent) {
  const float maximum =
      boundInPixels(inst, horizontal,
                    horizontal ? YGNodeStyleGetMaxWidth(inst.yoga)
                               : YGNodeStyleGetMaxHeight(inst.yoga));
  if (std::isfinite(maximum) && maximum >= 0)
    extent = std::min(extent, maximum);
  return extent;
}

float naturalContribution(const Instance& inst, bool horizontal, float extent) {
  extent = maximumMeasure(inst, horizontal, extent);
  const YGEdge start = horizontal ? YGEdgeLeft : YGEdgeTop;
  const YGEdge end = horizontal ? YGEdgeRight : YGEdgeBottom;
  float insets = 0;
  const Instance* child = &inst;
  for (const Instance* parent = inst.parent; parent; parent = parent->parent) {
    insets += YGNodeLayoutGetMargin(child->yoga, start) +
              YGNodeLayoutGetMargin(child->yoga, end) +
              YGNodeLayoutGetPadding(parent->yoga, start) +
              YGNodeLayoutGetPadding(parent->yoga, end) +
              YGNodeLayoutGetBorder(parent->yoga, start) +
              YGNodeLayoutGetBorder(parent->yoga, end);
    const float maximum = maximumMeasure(
        *parent, horizontal, std::numeric_limits<float>::infinity());
    if (std::isfinite(maximum))
      extent = std::min(extent, std::max(maximum - insets, 0.0f));
    // A maximum bounds an auto-sized flex line without fixing its size.
    // Follow only the stretch chain that passes that bound to this child.
    if (!stretchedFromContent(*parent, horizontal)) break;
    child = parent;
  }
  return extent;
}

float constrainedMeasure(const Instance& inst, bool horizontal, float extent) {
  extent = maximumMeasure(inst, horizontal, extent);
  const float minimum =
      boundInPixels(inst, horizontal,
                    horizontal ? YGNodeStyleGetMinWidth(inst.yoga)
                               : YGNodeStyleGetMinHeight(inst.yoga));
  if (std::isfinite(minimum) && minimum >= 0)
    extent = std::max(extent, minimum);
  return extent;
}

}  // namespace

bool Composer::Impl::applyCustomLayouts(Instance& inst) {
  bool applied = false;
  // THE CHILDREN A SCHEME PLACES: every child that has a box. One with
  // `Display::None` takes no cell, as it takes no room on a flex line.
  std::vector<Instance*> placed;
  if (inst.description->deriveData && inst.description->deriveData->placeFn)
    for (const auto& child : inst.children)
      if (child->description->layout.display != Display::None)
        placed.push_back(child.get());
  // layout() schemes are a flex-world feature; inside a positioned
  // subtree (no Yoga nodes) — or ON a positioned() container, whose
  // children have none — the placeFn is documented-unsupported.
  if (inst.yoga && !inst.description->layout.positioned &&
      inst.description->deriveData && inst.description->deriveData->placeFn &&
      !placed.empty()) {
    LayoutInput input;
    input.container = {YGNodeLayoutGetWidth(inst.yoga),
                       YGNodeLayoutGetHeight(inst.yoga)};
    for (Instance* child : placed) {
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
      input.childCells.push_back(child->description->layout.cells);
      // The region name is a rare field and lives in the child's derive
      // block; the scheme reads it beside the cell numbers.
      input.childAreas.push_back(child->description->deriveData
                                     ? child->description->deriveData->cellArea
                                     : std::string());
    }
    if (inst.description->deriveData->placeReadsMinSizes)
      for (Instance* child : placed)
        input.childMinSizes.push_back(minimumSizeOf(*child));
    std::vector<SkRect> rects = inst.description->deriveData->placeFn(input);
    size_t count = std::min(rects.size(), placed.size());
    // Track placement supplies a text leaf's final reading measure. Its
    // automatic cross extent must be measured at that width (or depth in
    // vertical writing) before the scheme sizes the other tracks. The
    // preferred width remains an intrinsic contribution; only the wrapped
    // extent changes. An authored extent or a threaded frame stays bounded.
    for (int pass = 0; pass < 2; ++pass) {
      bool reflowed = false;
      for (size_t i = 0; i < count; ++i) {
        Instance& child = *placed[i];
        if (!child.paragraph || child.description->layout.centerAt) continue;
        const TextData* text = child.description->textData
                                   ? &*child.description->textData
                                   : nullptr;
        if (text && text->onPath) continue;
        const bool vertical = child.paragraph->writingMode() ==
                              sigil::weave::WritingMode::kVerticalRL;
        const bool frame =
            child.threadedInto || (text && !text->threadTo.empty());
        const float width = constrainedMeasure(child, true, rects[i].width());
        const float height =
            vertical || frame
                ? constrainedMeasure(child, false, rects[i].height())
                : kUnbounded;
        layoutTextInBox(child, width, height);
        const LayoutProps& layout = child.description->layout;
        if (frame || (vertical ? layout.width : layout.height).unit !=
                         Dimension::Unit::Auto)
          continue;
        const detail::Insets padding = paddingOf(child);
        const float measured = constrainedMeasure(
            child, vertical,
            vertical ? child.measuredSize.width + padding.across()
                     : child.measuredSize.height + padding.down());
        float& extent =
            vertical ? input.childSizes[i].fWidth : input.childSizes[i].fHeight;
        if (std::abs(extent - measured) <= 0.25f) continue;
        extent = measured;
        if (i < input.childMinSizes.size()) {
          float& minimum = vertical ? input.childMinSizes[i].fWidth
                                    : input.childMinSizes[i].fHeight;
          minimum = measured;
        }
        reflowed = true;
      }
      if (!reflowed) break;
      rects = inst.description->deriveData->placeFn(input);
      count = std::min(rects.size(), placed.size());
    }
    for (size_t i = 0; i < count; ++i) {
      // A centerAt() child opts OUT of the scheme's placement — the pin
      // wins (otherwise place() and the pin fight in a period-2
      // oscillation that never settles).
      if (placed[i]->description->layout.centerAt) continue;
      YGNodeRef child = placed[i]->yoga;
      // Count a change only on an actual delta: the convergence loop in
      // ensureLayout keys off this (idempotent writes are free).
      const SkRect cur = instanceRect(*placed[i]);
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
    // would place its children outside a zero box.
    //
    // A flex parent may assign an extent, or derive it from its children.
    // Keep assigned sizes. Supply the placed extent when children leaving
    // flow collapse the container, or when a content-sized flex line would
    // otherwise derive its cross extent from only the remaining siblings.
    const LayoutProps& l = inst.description->layout;
    SkRect extent = SkRect::MakeEmpty();
    for (size_t i = 0; i < count; ++i) extent.join(rects[i]);
    const bool widthPinned = l.hasInsets &&
                             l.insets.left.unit != Dimension::Unit::Auto &&
                             l.insets.right.unit != Dimension::Unit::Auto;
    const bool heightPinned = l.hasInsets &&
                              l.insets.top.unit != Dimension::Unit::Auto &&
                              l.insets.bottom.unit != Dimension::Unit::Auto;
    const bool contentWidth = l.width.unit == Dimension::Unit::Auto &&
                              !widthPinned && stretchedFromContent(inst, true);
    const bool contentHeight = l.height.unit == Dimension::Unit::Auto &&
                               !heightPinned &&
                               stretchedFromContent(inst, false);
    // A minimum contributes to an auto-sized flex line without disabling
    // stretch beside a taller or wider sibling. Re-resolve the author's
    // own minimum each time so content can also become smaller, and restore
    // that minimum when the parent supplies a constrained extent instead.
    const auto minimum = [&](bool horizontal, bool content, float extent) {
      Dimension declared = horizontal ? l.minWidth : l.minHeight;
      if (declared.unit == Dimension::Unit::Var) {
        const VarValue* value =
            inst.vars ? inst.vars->find(declared.reference()) : nullptr;
        const Dimension* length =
            value ? std::get_if<Dimension>(value) : nullptr;
        declared = length && length->unit != Dimension::Unit::Var
                       ? *length
                       : Dimension(0.0f);
      }
      bool relative = false;
      float value = resolveLength(inst, declared, relative);
      YGUnit unit = declared.unit == Dimension::Unit::Auto  ? YGUnitUndefined
                    : declared.unit == Dimension::Unit::Pct ? YGUnitPercent
                                                            : YGUnitPoint;
      if (content) {
        if (unit == YGUnitPercent) {
          const float parentExtent =
              horizontal ? YGNodeLayoutGetWidth(inst.parent->yoga)
                         : YGNodeLayoutGetHeight(inst.parent->yoga);
          value *= parentExtent * 0.01f;
        }
        // The content contribution cannot raise an implicit minimum past
        // an authored maximum. An explicit minimum keeps its own meaning.
        value = std::max(naturalContribution(inst, horizontal, extent),
                         std::isfinite(value) ? value : 0.0f);
        unit = YGUnitPoint;
      }
      const YGValue before = horizontal ? YGNodeStyleGetMinWidth(inst.yoga)
                                        : YGNodeStyleGetMinHeight(inst.yoga);
      if (before.unit == unit &&
          (unit == YGUnitUndefined || std::abs(before.value - value) <= 0.25f))
        return false;
      if (horizontal) {
        if (unit == YGUnitPercent)
          YGNodeStyleSetMinWidthPercent(inst.yoga, value);
        else
          YGNodeStyleSetMinWidth(inst.yoga, value);
      } else {
        if (unit == YGUnitPercent)
          YGNodeStyleSetMinHeightPercent(inst.yoga, value);
        else
          YGNodeStyleSetMinHeight(inst.yoga, value);
      }
      return true;
    };
    applied |= minimum(true, contentWidth, extent.right());
    applied |= minimum(false, contentHeight, extent.bottom());
    if (contentWidth && inst.schemeSizedWidth) {
      YGNodeStyleSetWidth(inst.yoga, YGUndefined);
      inst.schemeSizedWidth = false;
      applied = true;
    }
    if (contentHeight && inst.schemeSizedHeight) {
      YGNodeStyleSetHeight(inst.yoga, YGUndefined);
      inst.schemeSizedHeight = false;
      applied = true;
    }
    // …and it keeps sizing an axis it once sized. WHICH IT REMEMBERS: the
    // point width in the style is not evidence, because the placement loop
    // above writes point widths on every child, so a scheme nested in a
    // scheme would read its parent's placement as its own and override it.
    const bool sizesWidth = l.absolute ||
                            YGNodeLayoutGetWidth(inst.yoga) <= 0.25f ||
                            inst.schemeSizedWidth;
    const bool sizesHeight = l.absolute ||
                             YGNodeLayoutGetHeight(inst.yoga) <= 0.25f ||
                             inst.schemeSizedHeight;
    if (!contentWidth && l.width.unit == Dimension::Unit::Auto &&
        !widthPinned && sizesWidth && extent.right() > 0 &&
        std::abs(YGNodeLayoutGetWidth(inst.yoga) - extent.right()) > 0.25f) {
      YGNodeStyleSetWidth(inst.yoga, extent.right());
      inst.schemeSizedWidth = true;
      applied = true;
    }
    if (!contentHeight && l.height.unit == Dimension::Unit::Auto &&
        !heightPinned && sizesHeight && extent.bottom() > 0 &&
        std::abs(YGNodeLayoutGetHeight(inst.yoga) - extent.bottom()) > 0.25f) {
      YGNodeStyleSetHeight(inst.yoga, extent.bottom());
      inst.schemeSizedHeight = true;
      applied = true;
    }
  }
  for (const auto& child : inst.children) applied |= applyCustomLayouts(*child);
  return applied;
}

}  // namespace sigil::compose
