/** @file
 * A node's layout properties, as Yoga's setters: one dimension at a time,
 * each unit of Dimension to the setter that takes it, the relative units
 * measured against the font in force at the node first.
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

}  // namespace

void Composer::Impl::applyLayoutProps(Instance& inst) {
  if (!inst.yoga)
    return;  // positioned subtree: instanceRect() reads the properties directly
  const LayoutProps& l = inst.description->layout;
  YGNodeRef n = inst.yoga;

  // Whether any length below was measured against the font in force or
  // read from a custom property — the cascade pass rewrites this style
  // when either moves, and only then.
  bool relative = false;
  // A length written as a custom property, dereferenced once: the
  // property's own length, which may in turn be relative but may not be
  // another property.
  const auto deref = [&](const Dimension& d) -> Dimension {
    if (d.unit != Dimension::Unit::Var) return d;
    relative = true;
    const VarValue* value =
        inst.vars ? inst.vars->find(d.reference()) : nullptr;
    const Dimension* length = value ? std::get_if<Dimension>(value) : nullptr;
    if (!length || length->unit == Dimension::Unit::Var) {
      // Before the cascade pass has resolved this node, the properties are
      // not known yet and the pass rewrites this style once they are; a
      // miss is only a miss once they have been resolved.
      if (inst.cascadeResolved) warnNoSuchVar(d.reference(), false);
      return Dimension(0.0f);
    }
    return *length;
  };
  // Pixels for everything that is not a percent or auto.
  const auto px = [&](const Dimension& d) {
    return resolveLength(inst, d, relative);
  };
  const auto applyDim = [&](const Dimension& raw,
                            void (*setPx)(YGNodeRef, float),
                            void (*setPct)(YGNodeRef, float)) {
    const Dimension d = deref(raw);
    switch (d.unit) {
      case Dimension::Unit::Pct:
        setPct(n, d.value);
        break;
      case Dimension::Unit::Auto:
        // Patch reuses the yoga node, so a dim REMOVED from the description
        // must be written back as YGUndefined rather than skipped. Skipping
        // leaves the previous describe's value in the style set, where it
        // sticks for the life of the instance.
        setPx(n, YGUndefined);
        break;
      default:
        setPx(n, px(d));
        break;
    }
  };
  // An edge length: a percent as one, auto as nothing, the rest in pixels.
  const auto applyEdge = [&](const Dimension& raw, YGEdge edge,
                             void (*setPx)(YGNodeRef, YGEdge, float),
                             void (*setPct)(YGNodeRef, YGEdge, float)) {
    const Dimension d = deref(raw);
    switch (d.unit) {
      case Dimension::Unit::Pct:
        setPct(n, edge, d.value);
        break;
      case Dimension::Unit::Auto:
        setPx(n, edge, 0.0f);
        break;
      default:
        setPx(n, edge, px(d));
        break;
    }
  };

  YGNodeStyleSetFlexDirection(
      n, l.row ? YGFlexDirectionRow : YGFlexDirectionColumn);
  YGNodeStyleSetFlexWrap(n, l.wrap ? YGWrapWrap : YGWrapNoWrap);
  {
    const Dimension gap = deref(l.gap);
    if (gap.unit == Dimension::Unit::Pct)
      YGNodeStyleSetGapPercent(n, YGGutterAll, gap.value);
    else
      YGNodeStyleSetGap(n, YGGutterAll,
                        gap.unit == Dimension::Unit::Auto ? 0.0f : px(gap));
  }
  applyEdge(l.padding.left, YGEdgeLeft, &YGNodeStyleSetPadding,
            &YGNodeStyleSetPaddingPercent);
  applyEdge(l.padding.top, YGEdgeTop, &YGNodeStyleSetPadding,
            &YGNodeStyleSetPaddingPercent);
  applyEdge(l.padding.right, YGEdgeRight, &YGNodeStyleSetPadding,
            &YGNodeStyleSetPaddingPercent);
  applyEdge(l.padding.bottom, YGEdgeBottom, &YGNodeStyleSetPadding,
            &YGNodeStyleSetPaddingPercent);
  applyEdge(l.margin.left, YGEdgeLeft, &YGNodeStyleSetMargin,
            &YGNodeStyleSetMarginPercent);
  applyEdge(l.margin.top, YGEdgeTop, &YGNodeStyleSetMargin,
            &YGNodeStyleSetMarginPercent);
  applyEdge(l.margin.right, YGEdgeRight, &YGNodeStyleSetMargin,
            &YGNodeStyleSetMarginPercent);
  applyEdge(l.margin.bottom, YGEdgeBottom, &YGNodeStyleSetMargin,
            &YGNodeStyleSetMarginPercent);

  // Auto-sized layout() containers: applyCustomLayouts writes the placed
  // extent as explicit W/H onto auto-dim absolute containers — releasing
  // those here would zero the container every re-describe and feed
  // place() a degenerate input for a pass.
  const bool autoSized = inst.description->deriveData &&
                         inst.description->deriveData->placeFn && l.absolute;
  if (!autoSized || l.width.unit != Dimension::Unit::Auto)
    applyDim(l.width, &YGNodeStyleSetWidth, &YGNodeStyleSetWidthPercent);
  if (!autoSized || l.height.unit != Dimension::Unit::Auto)
    applyDim(l.height, &YGNodeStyleSetHeight, &YGNodeStyleSetHeightPercent);
  applyDim(l.minWidth, &YGNodeStyleSetMinWidth, &YGNodeStyleSetMinWidthPercent);
  applyDim(l.maxWidth, &YGNodeStyleSetMaxWidth, &YGNodeStyleSetMaxWidthPercent);
  applyDim(l.minHeight, &YGNodeStyleSetMinHeight,
           &YGNodeStyleSetMinHeightPercent);
  applyDim(l.maxHeight, &YGNodeStyleSetMaxHeight,
           &YGNodeStyleSetMaxHeightPercent);
  YGNodeStyleSetAspectRatio(n, l.aspect > 0 ? l.aspect : YGUndefined);
  YGNodeStyleSetFlexGrow(n, l.grow);
  YGNodeStyleSetFlexShrink(n, l.shrink);
  applyDim(l.basis, &YGNodeStyleSetFlexBasis, &YGNodeStyleSetFlexBasisPercent);
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
    auto applyInset = [&](YGEdge edge, const Dimension& raw) {
      const Dimension d = deref(raw);
      switch (d.unit) {
        case Dimension::Unit::Pct:
          YGNodeStyleSetPositionPercent(n, edge, d.value);
          break;
        case Dimension::Unit::Auto:
          YGNodeStyleSetPosition(n, edge, YGUndefined);
          break;
        default:
          YGNodeStyleSetPosition(n, edge, px(d));
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
  inst.relativeLengths = relative;
}

}  // namespace sigil::compose
