// Layout phase: Yoga's calculate pass, the SigilWeave-measured text leaves
// (measure/baseline callbacks + on-demand text layout), the bounded second
// pass for custom layout() containers, and resolved-rect reads. The derive
// pass it triggers lives in Derive.cpp.

#include "ComposeRuntime.h"

#include <sigilweave/Flow.h>
#include <sigilweave/FontContext.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace sigil::compose {

using namespace detail;

// ---------------------------------------------------------------------------
// Text measurement (the spike, productized)

YGSize detail::measureTextNode(YGNodeConstRef node, float width,
                               YGMeasureMode widthMode, float, YGMeasureMode) {
  auto *inst =
      static_cast<Instance *>(YGNodeGetContext(const_cast<YGNodeRef>(node)));
  const float constraint = widthMode == YGMeasureModeUndefined ? 1.0e6f : width;
  inst->owner->layoutText(*inst, constraint);
  return inst->measuredSize;
}

float detail::baselineOfTextNode(YGNodeConstRef node, float, float) {
  auto *inst =
      static_cast<Instance *>(YGNodeGetContext(const_cast<YGNodeRef>(node)));
  if (inst->lines.empty())
    return 0.0f;
  const sigil::weave::LineMetrics &first = inst->lines.front();
  return first.baseline - first.rect().top();
}

void Composer::Impl::layoutText(Instance &inst, float constraint) {
  // onPath: the PATH is the measure, not the box. Laying the run out to
  // the node's width would wrap it, and every line after the first would
  // then be placed along the path from the start again — the glyphs pile
  // up on each other. The box still sizes the path; it does not bound the
  // run.
  if (inst.desc && inst.desc->textData && inst.desc->textData->onPath)
    constraint = 1.0e6f;
  if (constraint == inst.measuredForWidth &&
      inst.measuredRev == inst.contentRev)
    return; // layout is already valid for this content and width
  static const sigil::weave::ParagraphLayoutOptions kDefaultOptions;
  const sigil::weave::ParagraphLayoutOptions &options =
      inst.desc->textData ? inst.desc->textData->layoutOptions
                          : kDefaultOptions;
  if (!inst.exclusionsLocal.empty()) {
    sigil::weave::ExclusionFlow flow(SkRect::MakeWH(constraint, 1.0e6f));
    const float flowMargin =
        inst.desc->deriveData ? inst.desc->deriveData->flowAroundMargin : 0.0f;
    for (const SkRect &exclusion : inst.exclusionsLocal)
      flow.shapes().push_back(sigil::weave::ExclusionFlow::Shape::fromRectangle(
          exclusion, flowMargin));
    inst.textLayout =
        sigil::weave::layoutParagraph(fonts, *inst.paragraph, flow, options);
  } else {
    sigil::weave::BlockFlow flow(SkRect::MakeWH(constraint, 1.0e6f));
    inst.textLayout =
        sigil::weave::layoutParagraph(fonts, *inst.paragraph, flow, options);
  }
  inst.lines = inst.textLayout.lineMetrics(*inst.paragraph);
  inst.measuredForWidth = constraint;
  SkRect bounds = SkRect::MakeEmpty();
  for (const sigil::weave::LineMetrics &line : inst.lines)
    bounds.join(line.rect());
  inst.measuredSize = {std::ceil(bounds.width()), std::ceil(bounds.height())};
  inst.measuredRev = inst.contentRev;
}

// ---------------------------------------------------------------------------
// Layout passes

void Composer::Impl::ensureLayout() {
  if (!root || (!needsLayout && !YGNodeIsDirty(root->yoga)))
    return;
  // The root fills the viewport (the CSS-root rule) — except under an empty
  // setSize(), which means "intrinsic": the root sizes to its content (the
  // snapshot()/stamp path).
  if (!size.isEmpty()) {
    YGNodeStyleSetWidth(root->yoga, size.width());
    YGNodeStyleSetHeight(root->yoga, size.height());
  }
  YGNodeCalculateLayout(root->yoga, YGUndefined, YGUndefined, YGDirectionLTR);
  // Bounded convergence rounds for the post-measure machinery: custom
  // layout() placement, auto-sizing, centerAt pins, and the derive phase
  // all react to RESOLVED geometry, and each can move what another read
  // (an auto-sized container changes the box a pin centered in; a pinned
  // node moves an anchor a rail routed through). Three rounds settle every
  // legal composition; applyCustomLayouts/applyCenterPins report `changed`
  // only on actual deltas, so stable layouts exit after one extra pass.
  for (int round = 0; round < 3; ++round) {
    bool changed = false;
    if (hasCustomLayout)
      changed |= applyCustomLayouts(*root);
    if (hasCenterPins)
      changed |= applyCenterPins(*root);
    if (hasDerived)
      changed |= resolveDerived();
    if (!changed)
      break;
    YGNodeCalculateLayout(root->yoga, YGUndefined, YGUndefined,
                          YGDirectionLTR);
    if (hasDerived)
      resolveDerived(); // refresh routes against the moved geometry
  }
  // Post-layout invalidation: recordings bake geometry (child offsets, text
  // lines, geometry-material uResolution), so any rect that moved or resized
  // must stale the recordings that captured it — even when NO prop changed
  // (setSize, sibling growth, measured-text reflow). Runs only when layout
  // ran; a stable layout is a no-op walk.
  syncLayoutRects(*root);
  needsLayout = false;
}

void Composer::Impl::syncLayoutRects(Instance &inst) {
  const SkRect r = instanceRect(inst);
  if (r != inst.lastLayoutRect) {
    const bool sizeChanged = r.width() != inst.lastLayoutRect.width() ||
                             r.height() != inst.lastLayoutRect.height();
    inst.lastLayoutRect = r;
    if (sizeChanged)
      inst.markPaintDirtyUp(); // own content baked the old bounds
    else if (inst.parent)
      // The parent's RECORDING baked this child's old offset; the parent's
      // OWN paint did not — it never contained the child at all. §15's
      // split bake is the one thing that can tell those apart, and it must,
      // because a moving child is precisely the case it exists for: if this
      // marked the parent's own paint dirty the bake would be remade every
      // frame and the feature would silently do nothing.
      inst.parent->markPaintDirtyUp(/*ownPaint=*/false);
    contentDirty = true;
  }
  for (auto &child : inst.children)
    syncLayoutRects(*child);
}

/** centerAt(): set the pinned node's left/top so its MEASURED box centers
 *  on the point. Runs post-measure; idempotent (only dirties when the
 *  target position actually changed), so the bounded repass converges. */
bool Composer::Impl::applyCenterPins(Instance &inst) {
  bool applied = false;
  if (inst.desc->layout.centerAt) {
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
      YGNodeStyleSetPosition(inst.yoga, YGEdgeLeft,
                             styleBase(YGEdgeLeft) + dl);
      YGNodeStyleSetPosition(inst.yoga, YGEdgeTop, styleBase(YGEdgeTop) + dt);
      applied = true;
    }
  }
  for (const auto &child : inst.children)
    applied |= applyCenterPins(*child);
  return applied;
}

bool Composer::Impl::applyCustomLayouts(Instance &inst) {
  bool applied = false;
  if (inst.desc->deriveData && inst.desc->deriveData->placeFn &&
      !inst.children.empty()) {
    LayoutInput input;
    input.container = {YGNodeLayoutGetWidth(inst.yoga),
                       YGNodeLayoutGetHeight(inst.yoga)};
    for (const auto &child : inst.children) {
      input.childSizes.push_back(
          {YGNodeLayoutGetWidth(child->yoga), YGNodeLayoutGetHeight(child->yoga)});
      // First-baseline offset from the child's top — measured text only
      // (pass one measured it); everything else has no baseline.
      float baseline = std::numeric_limits<float>::quiet_NaN();
      if (!child->lines.empty()) {
        const sigil::weave::LineMetrics &first = child->lines.front();
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
      if (inst.children[i]->desc->layout.centerAt)
        continue;
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
    // opposing-inset pair). The hand-guessed `.width(340).height(360)` on
    // a pinned Diagonal battery becomes unnecessary; flex-embedded
    // layout() containers keep sizing by flex/stretch rules untouched.
    const LayoutProps &l = inst.desc->layout;
    if (l.absolute) {
      SkRect extent = SkRect::MakeEmpty();
      for (size_t i = 0; i < count; ++i)
        extent.join(rects[i]);
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
  for (const auto &child : inst.children)
    applied |= applyCustomLayouts(*child);
  return applied;
}

// ---------------------------------------------------------------------------
// Resolved-rect reads

SkRect Composer::Impl::instanceRect(const Instance &inst) const {
  return SkRect::MakeXYWH(YGNodeLayoutGetLeft(inst.yoga),
                          YGNodeLayoutGetTop(inst.yoga),
                          YGNodeLayoutGetWidth(inst.yoga),
                          YGNodeLayoutGetHeight(inst.yoga));
}

SkRect Composer::Impl::absoluteRect(const Instance &inst) const {
  SkRect rect = instanceRect(inst);
  for (Instance *p = inst.parent; p; p = p->parent)
    rect.offset(YGNodeLayoutGetLeft(p->yoga), YGNodeLayoutGetTop(p->yoga));
  return rect;
}

} // namespace sigil::compose
