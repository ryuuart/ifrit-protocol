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
  if (constraint == inst.measuredForWidth &&
      inst.measuredRev == inst.contentRev)
    return; // layout is already valid for this content and width
  const sigil::weave::ParagraphLayoutOptions &options = inst.desc->layoutOptions;
  if (!inst.exclusionsLocal.empty()) {
    sigil::weave::ExclusionFlow flow(SkRect::MakeWH(constraint, 1.0e6f));
    for (const SkRect &exclusion : inst.exclusionsLocal)
      flow.shapes().push_back(sigil::weave::ExclusionFlow::Shape::fromRectangle(
          exclusion, inst.desc->flowAroundMargin));
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
  // Custom layout() containers: a bounded second pass — children were
  // measured by pass one; scheme.place() pins them, pass two resolves.
  bool repass = hasCustomLayout && applyCustomLayouts(*root);
  // Derive phase: geometry-consuming content (exclusions, connectors).
  if (hasDerived)
    repass |= resolveDerived(*root);
  if (repass) {
    YGNodeCalculateLayout(root->yoga, YGUndefined, YGUndefined, YGDirectionLTR);
    if (hasDerived)
      resolveDerived(*root); // refresh connectors post-move
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
      inst.parent->markPaintDirtyUp(); // parent baked the old offset
    contentDirty = true;
  }
  for (auto &child : inst.children)
    syncLayoutRects(*child);
}

bool Composer::Impl::applyCustomLayouts(Instance &inst) {
  bool applied = false;
  if (inst.desc->placeFn && !inst.children.empty()) {
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
    std::vector<SkRect> rects = inst.desc->placeFn(input);
    const size_t count = std::min(rects.size(), inst.children.size());
    for (size_t i = 0; i < count; ++i) {
      YGNodeRef child = inst.children[i]->yoga;
      YGNodeStyleSetPositionType(child, YGPositionTypeAbsolute);
      YGNodeStyleSetPosition(child, YGEdgeLeft, rects[i].left());
      YGNodeStyleSetPosition(child, YGEdgeTop, rects[i].top());
      YGNodeStyleSetWidth(child, rects[i].width());
      YGNodeStyleSetHeight(child, rects[i].height());
    }
    applied = true;
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
