#pragma once

/** @file
 * Internal to the kernel — the two blocks of property fields a node
 * declares and its computed style carries: the layout block and the
 * paint block, with the per-edge storage the first is written in.
 */

#include <include/core/SkBlendMode.h>
#include <include/core/SkPoint.h>
#include <sigilmotion/values/Animated.h>

#include <optional>

#include "sigilcompose/core/Layout.h"
#include "sigilcompose/core/Paint.h"
#include "sigilcompose/core/PaintAnchor.h"

namespace sigil::compose::detail {

/** Per-edge Dims: for absolute insets Auto is a side left unpinned; for
 *  padding and margin every side is a length, zero by default.
 *
 *  THE DECLARATION ORDER HERE IS NOT THE PUBLIC ONE. Storage runs left,
 *  top, right, bottom; the public `Edges` and every verb that takes one
 *  run in CSS's order, top, right, bottom, left. Assigning an `Edges`
 *  field by field means permuting it, never copying it across. */
struct EdgeDims {
  Dimension left, top, right, bottom;
  bool operator==(const EdgeDims&) const = default;
};

/** THE SAME FOUR EDGES ONCE THEY ARE PIXELS — what a percent, an em or a
 *  custom property came to for one node, which is the form the layout and
 *  the paint read them in. */
struct Insets {
  float left = 0, top = 0, right = 0, bottom = 0;
  float across() const { return left + right; }
  float down() const { return top + bottom; }
  bool any() const {
    return left != 0 || top != 0 || right != 0 || bottom != 0;
  }
};

struct LayoutProps {
  FlexDirection direction = FlexDirection::Column;
  FlexWrap wrap = FlexWrap::NoWrap;
  Display display = Display::Flex;
  BoxSizing boxSizing = BoxSizing::BorderBox;
  Dimension gap = 0.0f;
  EdgeDims padding{0.0f, 0.0f, 0.0f, 0.0f}, margin{0.0f, 0.0f, 0.0f, 0.0f};
  Dimension width, height, minWidth, maxWidth, minHeight, maxHeight, basis;
  float aspect = 0;
  float grow = 0, shrink = 1;
  Align alignItems = Align::Stretch;
  Align alignSelf = Align::Auto;
  Justify justify = Justify::Start;
  bool absolute = false;
  bool hasInsets = false;
  /** Set by cover() alone: the node was taken out of the flow to fill
   *  its parent's box, and nothing else placed it. A size stated after
   *  that puts it back in the flow, since a box of its own is the one
   *  other thing a covering node can be; a pin or an inset stated after
   *  it is a placement, and stands. */
  bool covering = false;
  /** positioned() container: children (and their subtrees) get NO Yoga
   *  nodes; instanceRect() resolves their rects straight from these
   *  properties. */
  bool positioned = false;
  EdgeDims insets;
  std::optional<SkPoint> centerAt;  // absolute: center ON this point
                                    // (resolved post-measure)
  /** The cells this child claims of a layout() container's scheme, read
   *  by nothing else. Default means unspoken, and a scheme flows it.
   *  `CellSpan::area` is NOT set here — a named claim is rare and its
   *  string is in DeriveData; the layout pass merges the two. */
  CellSpan cells;
  bool operator==(const LayoutProps&) const = default;
};

/** Whether a container that runs @p direction has a HORIZONTAL main axis. */
constexpr bool mainAxisHorizontal(FlexDirection direction) {
  return direction == FlexDirection::Row ||
         direction == FlexDirection::RowReverse;
}

struct PaintProps {
  std::optional<motion::Animatable<Fill>> fill;
  motion::Animatable<float> opacity = 1.0f;
  SkBlendMode blendMode = SkBlendMode::kSrcOver;
  // fill(paint, anchor, origin): which of the box's rectangles the
  // paint's unit square begins at. Only where the paint is stretched
  // over the box; the painted AREA never moves with it. Beside the blend
  // mode because the two bytes share one word there and this struct is
  // inline in every node.
  BackgroundOrigin backgroundOrigin = BackgroundOrigin::BorderBox;
  motion::Animatable<float> translateX = 0.0f, translateY = 0.0f;
  motion::Animatable<float> rotate = 0.0f, scale = 1.0f;
  // Per-axis scale, multiplied INTO `scale`. Bars, wipes, meters,
  // cooldown sweeps and drain rings are the most common animated
  // primitive in a UI and none of them are uniform.
  motion::Animatable<float> scaleX = 1.0f, scaleY = 1.0f;
  motion::Animatable<float> skewX = 0.0f, skewY = 0.0f;  // degrees (shear)
  // The pivot: a percentage is of the node's own box, any other length is
  // node-local pixels. Its depth is DepthData::originZ.
  Dimension originX = pct(50.0f), originY = pct(50.0f);
  int zIndex = 0;
};

}  // namespace sigil::compose::detail
