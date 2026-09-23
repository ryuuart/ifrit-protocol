#pragma once

/** @file
 * Internal to the kernel — the property fields a node declares that its
 * computed style carries, kept where only a writer that marks the
 * property as stated can reach them.
 */

#include <sigilweave/style/Keyword.h>

#include "HeapBox.h"
#include "PropertyBlocks.h"
#include "sigilcompose/core/Factories.h"
#include "sigilcompose/core/Property.h"

namespace sigil::compose::detail {

struct ElementNode;
class DeclaredFields;

/** The z-index an operator gives an element it adds, written where the
 *  element states none of its own. A starting value, not a statement: a
 *  rule that matches the element stands over it. */
void startWithOperatorZIndex(DeclaredFields& added, int zIndex);

/** THE KEY TO `DeclaredFields::defaults()`. Only the sites that give a
 *  node the values it starts with can make one: `point()` and
 *  `positioned()`, an operator's z-index for what it adds, and the memo
 *  shell's probe, which carries one block's values and states nothing.
 *  A verb cannot make one, so it cannot write a field without its bit. */
class DefaultsKey {
  // Written out rather than defaulted: a defaulted constructor lets
  // `DefaultsKey{}` through without asking whether the caller may.
  DefaultsKey() {}  // NOLINT(modernize-use-equals-default)
  friend Element compose::point();
  friend Element compose::positioned();
  friend void startWithOperatorZIndex(DeclaredFields& added, int zIndex);
  friend void warnIgnoredMemoShellProps(const ElementNode& shell);
};

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
 *  nothing, and takes a key a verb cannot make. The properties the
 *  computed style does not carry are kept on the node itself; their
 *  writers are the node's own and mark their bits the same way, but
 *  their storage is the node's and is not closed. */
class DeclaredFields {
 public:
  // ---- reading ----
  const LayoutProps& layout() const { return m_storage.layout; }
  const PaintProps& paint() const { return m_storage.paint; }
  const Corners& corners() const { return m_storage.corners; }
  /** overflow(Overflow::Clip): the node's content is cut to its shape. */
  bool clipContent() const { return m_storage.clipContent; }
  /** Which properties this node stated, by value or as a keyword. */
  const PropertyMask& declared() const { return m_storage.declared; }
  /** The properties stated as `inherit`, `initial` or `unset`; absent
   *  where none is, which is nearly every node. */
  const Box<sigil::weave::KeywordTable<Property>>& keywords() const {
    return m_storage.keywords;
  }

  // ---- the box ----
  Display& display() {
    return state(Property::Display, m_storage.layout.display);
  }
  BoxSizing& boxSizing() {
    return state(Property::BoxSizing, m_storage.layout.boxSizing);
  }
  Dimension& gap() { return state(Property::Gap, m_storage.layout.gap); }
  Dimension& paddingTop() {
    return state(Property::PaddingTop, m_storage.layout.padding.top);
  }
  Dimension& paddingRight() {
    return state(Property::PaddingRight, m_storage.layout.padding.right);
  }
  Dimension& paddingBottom() {
    return state(Property::PaddingBottom, m_storage.layout.padding.bottom);
  }
  Dimension& paddingLeft() {
    return state(Property::PaddingLeft, m_storage.layout.padding.left);
  }
  Dimension& marginTop() {
    return state(Property::MarginTop, m_storage.layout.margin.top);
  }
  Dimension& marginRight() {
    return state(Property::MarginRight, m_storage.layout.margin.right);
  }
  Dimension& marginBottom() {
    return state(Property::MarginBottom, m_storage.layout.margin.bottom);
  }
  Dimension& marginLeft() {
    return state(Property::MarginLeft, m_storage.layout.margin.left);
  }
  Dimension& width() { return state(Property::Width, m_storage.layout.width); }
  Dimension& height() {
    return state(Property::Height, m_storage.layout.height);
  }
  Dimension& minWidth() {
    return state(Property::MinWidth, m_storage.layout.minWidth);
  }
  Dimension& maxWidth() {
    return state(Property::MaxWidth, m_storage.layout.maxWidth);
  }
  Dimension& minHeight() {
    return state(Property::MinHeight, m_storage.layout.minHeight);
  }
  Dimension& maxHeight() {
    return state(Property::MaxHeight, m_storage.layout.maxHeight);
  }
  float& aspectRatio() {
    return state(Property::AspectRatio, m_storage.layout.aspect);
  }

  // ---- the flex line ----
  FlexDirection& flexDirection() {
    return state(Property::FlexDirection, m_storage.layout.direction);
  }
  FlexWrap& flexWrap() {
    return state(Property::FlexWrap, m_storage.layout.wrap);
  }
  Dimension& flexBasis() {
    return state(Property::FlexBasis, m_storage.layout.basis);
  }
  float& flexGrow() { return state(Property::FlexGrow, m_storage.layout.grow); }
  float& flexShrink() {
    return state(Property::FlexShrink, m_storage.layout.shrink);
  }
  Align& alignItems() {
    return state(Property::AlignItems, m_storage.layout.alignItems);
  }
  Align& alignSelf() {
    return state(Property::AlignSelf, m_storage.layout.alignSelf);
  }
  Justify& justifyContent() {
    return state(Property::JustifyContent, m_storage.layout.justify);
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
    m_storage.layout.covering = false;
    return {m_storage.layout.absolute, m_storage.layout.hasInsets};
  }
  Dimension& left() {
    return state(Property::Left, m_storage.layout.insets.left);
  }
  Dimension& top() { return state(Property::Top, m_storage.layout.insets.top); }
  Dimension& right() {
    return state(Property::Right, m_storage.layout.insets.right);
  }
  Dimension& bottom() {
    return state(Property::Bottom, m_storage.layout.insets.bottom);
  }
  /** Out of the flow with the four insets at zero: states `Absolute` and
   *  the four insets, and notes the placement as a cover, which a size
   *  stated later reads to put the node back in the flow. */
  void cover();
  std::optional<SkPoint>& centerAt() {
    return state(Property::CenterAt, m_storage.layout.centerAt);
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
    CellSpan& cells = m_storage.layout.cells;
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
    CellSpan& cells = m_storage.layout.cells;
    return {cells.across, cells.down, cells.alignDeclared};
  }

  // ---- the silhouette ----
  Corners& borderRadius() {
    return state(Property::BorderRadius, m_storage.corners);
  }
  /** True where the content is cut to the node's shape. */
  bool& overflow() { return state(Property::Overflow, m_storage.clipContent); }

  // ---- the paint ----
  std::optional<motion::Animatable<Fill>>& fill() {
    return state(Property::Fill, m_storage.paint.fill);
  }
  motion::Animatable<float>& opacity() {
    return state(Property::Opacity, m_storage.paint.opacity);
  }
  SkBlendMode& blendMode() {
    return state(Property::BlendMode, m_storage.paint.blendMode);
  }
  BackgroundOrigin& backgroundOrigin() {
    return state(Property::BackgroundOrigin, m_storage.paint.backgroundOrigin);
  }
  int& zIndex() { return state(Property::ZIndex, m_storage.paint.zIndex); }

  // ---- the 2D transform ----
  motion::Animatable<float>& translateX() {
    return state(Property::TranslateX, m_storage.paint.translateX);
  }
  motion::Animatable<float>& translateY() {
    return state(Property::TranslateY, m_storage.paint.translateY);
  }
  motion::Animatable<float>& rotate() {
    return state(Property::Rotate, m_storage.paint.rotate);
  }
  motion::Animatable<float>& scale() {
    return state(Property::Scale, m_storage.paint.scale);
  }
  motion::Animatable<float>& scaleX() {
    return state(Property::ScaleX, m_storage.paint.scaleX);
  }
  motion::Animatable<float>& scaleY() {
    return state(Property::ScaleY, m_storage.paint.scaleY);
  }
  motion::Animatable<float>& skewX() {
    return state(Property::SkewX, m_storage.paint.skewX);
  }
  motion::Animatable<float>& skewY() {
    return state(Property::SkewY, m_storage.paint.skewY);
  }
  /** The pivot in the plane; its depth is the node's own. */
  struct TransformOrigin {
    Dimension& x;
    Dimension& y;
  };
  TransformOrigin transformOrigin() {
    state(Property::TransformOrigin);
    return {m_storage.paint.originX, m_storage.paint.originY};
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
   *  starting size and placement, the flag that makes a positioned
   *  container, the z-index an operator gives what it adds. Whatever the
   *  node or a rule states later stands over these. */
  struct Defaults {
    LayoutProps& layout;
    PaintProps& paint;
    Corners& corners;
  };
  Defaults defaults(DefaultsKey) {
    return {m_storage.layout, m_storage.paint, m_storage.corners};
  }

  /** Every part of the declared state in one aggregate, so the prune's
   *  field pin counts the parts and a new one fails it. */
  struct Storage {
    LayoutProps layout;
    PaintProps paint;
    Corners corners;
    bool clipContent = false;
    PropertyMask declared;
    Box<sigil::weave::KeywordTable<Property>> keywords;
  };

 private:
  friend struct ElementNode;
  /** The field walk's way in: it puts one part of the state on a node
   *  with nothing else, which no writer can. @trap Only the field walk
   *  defines it; a kernel file that did would open the storage. */
  friend struct DeclaredFieldsTestAccess;

  /** Marks @p property stated and ends a keyword written about it before:
   *  the later statement of the two is the one that stands. */
  void state(Property property) {
    m_storage.declared.set(property);
    if (m_storage.keywords) m_storage.keywords->clear(property);
  }
  template <class T>
  T& state(Property property, T& field) {
    state(property);
    return field;
  }

  Storage m_storage;
};

static_assert(sizeof(DeclaredFields) == sizeof(DeclaredFields::Storage),
              "DeclaredFields holds a member outside its Storage, which the "
              "prune's field pin cannot count; put it in Storage");

}  // namespace sigil::compose::detail
