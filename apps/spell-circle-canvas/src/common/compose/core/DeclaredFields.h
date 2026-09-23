#pragma once

/** @file
 * Internal to the kernel — the property fields a node declares that its
 * computed style carries, kept where only a writer that marks the
 * property as stated can reach them.
 */

#include <sigilweave/style/Keyword.h>

#include "HeapBox.h"
#include "PropertyBlocks.h"
#include "sigilcompose/core/Property.h"

namespace sigil::compose::detail {

struct ElementNode;

/** THE DECLARED FIELDS OF ONE NODE: the layout block, the paint block,
 *  the corner radii and the clip, beside the mask of which properties
 *  the node stated and the table of those it stated as a keyword.
 *
 *  A field holds its type's default until something writes it, so the
 *  value alone cannot tell a node that states the default from one that
 *  says nothing; the bit beside it does. The fields are private, and the
 *  only way to write one is the writer named for its property, which
 *  sets that property's bit and ends a keyword written about it before.
 *  A write that skips the bit therefore does not compile.
 *
 *  Three kinds of write are not a writer: `keyword()` states a property
 *  as a keyword, `leaveCover()` withdraws the placement `cover()`
 *  stated, and `defaults()` writes a node's starting values, which state
 *  nothing. The properties the computed style does not carry are kept on
 *  the node itself, and their writers are the node's own. */
class DeclaredFields {
 public:
  // ---- reading ----
  const LayoutProps& layout() const { return m_layout; }
  const PaintProps& paint() const { return m_paint; }
  const Corners& corners() const { return m_corners; }
  /** overflow(Overflow::Clip): the node's content is cut to its shape. */
  bool clipContent() const { return m_clipContent; }
  /** Which properties this node stated, by value or as a keyword. */
  const PropertyMask& declared() const { return m_declared; }
  /** The properties stated as `inherit`, `initial` or `unset`; absent
   *  where none is, which is nearly every node. */
  const Box<sigil::weave::KeywordTable<Property>>& keywords() const {
    return m_keywords;
  }

  // ---- the box ----
  Display& display() { return state(Property::Display, m_layout.display); }
  BoxSizing& boxSizing() {
    return state(Property::BoxSizing, m_layout.boxSizing);
  }
  Dimension& gap() { return state(Property::Gap, m_layout.gap); }
  Dimension& paddingTop() {
    return state(Property::PaddingTop, m_layout.padding.top);
  }
  Dimension& paddingRight() {
    return state(Property::PaddingRight, m_layout.padding.right);
  }
  Dimension& paddingBottom() {
    return state(Property::PaddingBottom, m_layout.padding.bottom);
  }
  Dimension& paddingLeft() {
    return state(Property::PaddingLeft, m_layout.padding.left);
  }
  Dimension& marginTop() {
    return state(Property::MarginTop, m_layout.margin.top);
  }
  Dimension& marginRight() {
    return state(Property::MarginRight, m_layout.margin.right);
  }
  Dimension& marginBottom() {
    return state(Property::MarginBottom, m_layout.margin.bottom);
  }
  Dimension& marginLeft() {
    return state(Property::MarginLeft, m_layout.margin.left);
  }
  Dimension& width() { return state(Property::Width, m_layout.width); }
  Dimension& height() { return state(Property::Height, m_layout.height); }
  Dimension& minWidth() { return state(Property::MinWidth, m_layout.minWidth); }
  Dimension& maxWidth() { return state(Property::MaxWidth, m_layout.maxWidth); }
  Dimension& minHeight() {
    return state(Property::MinHeight, m_layout.minHeight);
  }
  Dimension& maxHeight() {
    return state(Property::MaxHeight, m_layout.maxHeight);
  }
  float& aspectRatio() { return state(Property::AspectRatio, m_layout.aspect); }

  // ---- the flex line ----
  FlexDirection& flexDirection() {
    return state(Property::FlexDirection, m_layout.direction);
  }
  FlexWrap& flexWrap() { return state(Property::FlexWrap, m_layout.wrap); }
  Dimension& flexBasis() { return state(Property::FlexBasis, m_layout.basis); }
  float& flexGrow() { return state(Property::FlexGrow, m_layout.grow); }
  float& flexShrink() { return state(Property::FlexShrink, m_layout.shrink); }
  Align& alignItems() {
    return state(Property::AlignItems, m_layout.alignItems);
  }
  Align& alignSelf() { return state(Property::AlignSelf, m_layout.alignSelf); }
  Justify& justifyContent() {
    return state(Property::JustifyContent, m_layout.justify);
  }

  // ---- placement ----
  /** Out of the flow, and whether an inset pinned it, which travels with
   *  the flag. @trap Stating it ends a `cover()`: the node's placement is
   *  whatever this statement says. */
  struct Absolute {
    bool& absolute;
    bool& hasInsets;
  };
  Absolute absolute() {
    state(Property::Absolute);
    m_layout.covering = false;
    return {m_layout.absolute, m_layout.hasInsets};
  }
  Dimension& left() { return state(Property::Left, m_layout.insets.left); }
  Dimension& top() { return state(Property::Top, m_layout.insets.top); }
  Dimension& right() { return state(Property::Right, m_layout.insets.right); }
  Dimension& bottom() {
    return state(Property::Bottom, m_layout.insets.bottom);
  }
  std::optional<SkPoint>& centerAt() {
    return state(Property::CenterAt, m_layout.centerAt);
  }
  /** Which cells a grid-shaped scheme gives the node, and whether they
   *  were named; where in them it sits is `gridCellAlign()`. */
  struct GridCells {
    int& column;
    int& row;
    int& columns;
    int& rows;
    bool& declared;
  };
  GridCells gridCells() {
    state(Property::GridCells);
    CellSpan& cells = m_layout.cells;
    return {cells.column, cells.row, cells.columns, cells.rows, cells.declared};
  }
  /** Where in its cells the node sits, and whether that was said. */
  struct GridCellAlign {
    Align& across;
    Align& down;
    bool& declared;
  };
  GridCellAlign gridCellAlign() {
    state(Property::GridCellAlign);
    CellSpan& cells = m_layout.cells;
    return {cells.across, cells.down, cells.alignDeclared};
  }

  // ---- the silhouette ----
  Corners& borderRadius() { return state(Property::BorderRadius, m_corners); }
  /** True where the content is cut to the node's shape. */
  bool& overflow() { return state(Property::Overflow, m_clipContent); }

  // ---- the paint ----
  std::optional<motion::Animatable<Fill>>& fill() {
    return state(Property::Fill, m_paint.fill);
  }
  motion::Animatable<float>& opacity() {
    return state(Property::Opacity, m_paint.opacity);
  }
  SkBlendMode& blendMode() {
    return state(Property::BlendMode, m_paint.blendMode);
  }
  BackgroundOrigin& backgroundOrigin() {
    return state(Property::BackgroundOrigin, m_paint.backgroundOrigin);
  }
  int& zIndex() { return state(Property::ZIndex, m_paint.zIndex); }

  // ---- the 2D transform ----
  motion::Animatable<float>& translateX() {
    return state(Property::TranslateX, m_paint.translateX);
  }
  motion::Animatable<float>& translateY() {
    return state(Property::TranslateY, m_paint.translateY);
  }
  motion::Animatable<float>& rotate() {
    return state(Property::Rotate, m_paint.rotate);
  }
  motion::Animatable<float>& scale() {
    return state(Property::Scale, m_paint.scale);
  }
  motion::Animatable<float>& scaleX() {
    return state(Property::ScaleX, m_paint.scaleX);
  }
  motion::Animatable<float>& scaleY() {
    return state(Property::ScaleY, m_paint.scaleY);
  }
  motion::Animatable<float>& skewX() {
    return state(Property::SkewX, m_paint.skewX);
  }
  motion::Animatable<float>& skewY() {
    return state(Property::SkewY, m_paint.skewY);
  }
  /** The pivot in the plane; its depth is the node's own. */
  struct TransformOrigin {
    Dimension& x;
    Dimension& y;
  };
  TransformOrigin transformOrigin() {
    state(Property::TransformOrigin);
    return {m_paint.originX, m_paint.originY};
  }

  // ---- what is not a statement of a value ----
  /** @p property stated as @p keyword: a keyword IS a declaration, and
   *  the value under it stops standing. Refused, and said once, for a
   *  property no fold answers a keyword about. */
  void keyword(Property property, sigil::weave::Keyword keyword);
  /** A covering node given a size of its own returns to the flow: the
   *  placement `cover()` stated is withdrawn, the fields and their bits
   *  together. Nothing where the node does not cover. */
  void leaveCover();
  /** The fields a node STARTS with, written without a bit: a factory's
   *  starting size, a flag that says what kind of container or placement
   *  the node is, a value an operator gives what it adds. Whatever the
   *  node or a rule states later stands over these. */
  struct Defaults {
    LayoutProps& layout;
    PaintProps& paint;
    Corners& corners;
  };
  Defaults defaults() { return {m_layout, m_paint, m_corners}; }

 private:
  friend struct ElementNode;

  /** Marks @p property stated and ends a keyword written about it before:
   *  the later statement of the two is the one that stands. */
  void state(Property property) {
    m_declared.set(property);
    if (m_keywords) m_keywords->clear(property);
  }
  template <class T>
  T& state(Property property, T& field) {
    state(property);
    return field;
  }

  LayoutProps m_layout;
  PaintProps m_paint;
  Corners m_corners;
  bool m_clipContent = false;
  PropertyMask m_declared;
  Box<sigil::weave::KeywordTable<Property>> m_keywords;
};

}  // namespace sigil::compose::detail
