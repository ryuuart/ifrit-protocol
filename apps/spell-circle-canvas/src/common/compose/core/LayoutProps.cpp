/** @file
 * A node's layout properties, as Yoga's setters: one dimension at a time, each
 * unit of Dimension to the setter that takes it.
 */

#include "ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

namespace {

YGAlign toYogaAlign(Align a) {
  switch (a) {
    case Align::Auto:
      return YGAlignAuto;
    case Align::Start:
      return YGAlignFlexStart;
    case Align::Center:
      return YGAlignCenter;
    case Align::End:
      return YGAlignFlexEnd;
    case Align::Stretch:
      return YGAlignStretch;
    case Align::Baseline:
      return YGAlignBaseline;
  }
  return YGAlignAuto;
}

YGJustify toYogaJustify(Justify j) {
  switch (j) {
    case Justify::Start:
      return YGJustifyFlexStart;
    case Justify::Center:
      return YGJustifyCenter;
    case Justify::End:
      return YGJustifyFlexEnd;
    case Justify::SpaceBetween:
      return YGJustifySpaceBetween;
    case Justify::SpaceAround:
      return YGJustifySpaceAround;
    case Justify::SpaceEvenly:
      return YGJustifySpaceEvenly;
  }
  return YGJustifyFlexStart;
}

void applyDim(YGNodeRef node, const Dimension& d,
              void (*setPx)(YGNodeRef, float),
              void (*setPct)(YGNodeRef, float)) {
  switch (d.unit) {
    case Dimension::Unit::Px:
      setPx(node, d.value);
      break;
    case Dimension::Unit::Pct:
      setPct(node, d.value);
      break;
    case Dimension::Unit::Auto:
      // Patch reuses the yoga node, so a dim REMOVED from the description
      // must be written back as YGUndefined rather than skipped. Skipping
      // leaves the previous describe's value in the style set, where it
      // sticks for the life of the instance.
      setPx(node, YGUndefined);
      break;
  }
}

}  // namespace

void Composer::Impl::applyLayoutProps(Instance& inst) {
  if (!inst.yoga)
    return;  // positioned subtree: instanceRect() reads the properties directly
  const LayoutProps& l = inst.description->layout;
  YGNodeRef n = inst.yoga;

  YGNodeStyleSetFlexDirection(
      n, l.row ? YGFlexDirectionRow : YGFlexDirectionColumn);
  YGNodeStyleSetFlexWrap(n, l.wrap ? YGWrapWrap : YGWrapNoWrap);
  YGNodeStyleSetGap(n, YGGutterAll, l.gap);
  YGNodeStyleSetPadding(n, YGEdgeLeft, l.padding.left);
  YGNodeStyleSetPadding(n, YGEdgeTop, l.padding.top);
  YGNodeStyleSetPadding(n, YGEdgeRight, l.padding.right);
  YGNodeStyleSetPadding(n, YGEdgeBottom, l.padding.bottom);
  YGNodeStyleSetMargin(n, YGEdgeLeft, l.margin.left);
  YGNodeStyleSetMargin(n, YGEdgeTop, l.margin.top);
  YGNodeStyleSetMargin(n, YGEdgeRight, l.margin.right);
  YGNodeStyleSetMargin(n, YGEdgeBottom, l.margin.bottom);

  // Auto-sized layout() containers: applyCustomLayouts writes the placed
  // extent as explicit W/H onto auto-dim absolute containers — releasing
  // those here would zero the container every re-describe and feed
  // place() a degenerate input for a pass.
  const bool autoSized = inst.description->deriveData &&
                         inst.description->deriveData->placeFn && l.absolute;
  if (!autoSized || l.width.unit != Dimension::Unit::Auto)
    applyDim(n, l.width, &YGNodeStyleSetWidth, &YGNodeStyleSetWidthPercent);
  if (!autoSized || l.height.unit != Dimension::Unit::Auto)
    applyDim(n, l.height, &YGNodeStyleSetHeight, &YGNodeStyleSetHeightPercent);
  applyDim(n, l.minWidth, &YGNodeStyleSetMinWidth,
           &YGNodeStyleSetMinWidthPercent);
  applyDim(n, l.maxWidth, &YGNodeStyleSetMaxWidth,
           &YGNodeStyleSetMaxWidthPercent);
  applyDim(n, l.minHeight, &YGNodeStyleSetMinHeight,
           &YGNodeStyleSetMinHeightPercent);
  applyDim(n, l.maxHeight, &YGNodeStyleSetMaxHeight,
           &YGNodeStyleSetMaxHeightPercent);
  YGNodeStyleSetAspectRatio(n, l.aspect > 0 ? l.aspect : YGUndefined);
  YGNodeStyleSetFlexGrow(n, l.grow);
  YGNodeStyleSetFlexShrink(n, l.shrink);
  applyDim(n, l.basis, &YGNodeStyleSetFlexBasis,
           &YGNodeStyleSetFlexBasisPercent);
  YGNodeStyleSetAlignItems(n, toYogaAlign(l.alignItems));
  // Measured text must not stretch on the cross axis: a stretched text leaf
  // is re-measured against a width it did not ask for, and the box stops
  // fitting the type. Demote a RESOLVED Stretch to Start for text leaves,
  // while letting any explicit alignment — the node's own or inherited from
  // the parent — through untouched.
  Align self = l.alignSelf;
  if (inst.description->kind == Kind::Text) {
    const Align resolved =
        self != Align::Auto ? self
                            : (inst.parent && inst.parent->description
                                   ? inst.parent->description->layout.alignItems
                                   : Align::Stretch);
    if (resolved == Align::Stretch) self = Align::Start;
  }
  YGNodeStyleSetAlignSelf(n, toYogaAlign(self));
  YGNodeStyleSetJustifyContent(n, toYogaJustify(l.justify));

  // The node's OWN position type. A stack child's is overwritten right
  // after this: every child of a stack is placed absolutely whatever it
  // asked for, which is what makes a stack a stack.
  YGNodeStyleSetPositionType(
      n, l.absolute ? YGPositionTypeAbsolute : YGPositionTypeRelative);
  if (l.hasInsets) {
    // Per-side Dims: Auto leaves the side UNPINNED (YGUndefined), so
    // `.top(12).right(12)` pins a corner badge without stretching it.
    // Always write all four — patch() reuses the yoga node, and a side
    // that was pinned last describe must actually release.
    auto applyInset = [n](YGEdge edge, const Dimension& d) {
      switch (d.unit) {
        case Dimension::Unit::Px:
          YGNodeStyleSetPosition(n, edge, d.value);
          break;
        case Dimension::Unit::Pct:
          YGNodeStyleSetPositionPercent(n, edge, d.value);
          break;
        case Dimension::Unit::Auto:
          YGNodeStyleSetPosition(n, edge, YGUndefined);
          break;
      }
    };
    applyInset(YGEdgeLeft, l.insets.left);
    applyInset(YGEdgeTop, l.insets.top);
    applyInset(YGEdgeRight, l.insets.right);
    applyInset(YGEdgeBottom, l.insets.bottom);
  } else {
    // Insets REMOVED since the last describe must release too.
    YGNodeStyleSetPosition(n, YGEdgeLeft, YGUndefined);
    YGNodeStyleSetPosition(n, YGEdgeTop, YGUndefined);
    YGNodeStyleSetPosition(n, YGEdgeRight, YGUndefined);
    YGNodeStyleSetPosition(n, YGEdgeBottom, YGUndefined);
  }
}

}  // namespace sigil::compose
