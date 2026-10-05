#pragma once

/** @file
 * @ingroup compose-core
 *
 * EVERY PROPERTY A DECLARATION CAN STATE, as one name: the set a value
 * records that it stated, and the one table that says which properties a
 * node takes from its parent when nothing states them. The three wide
 * keywords a property may be written as instead of a value are
 * SigilWeave's `Keyword`, with the `KeywordTable` that records them —
 * a text style's partials are written with the same three.
 */

#include <sigilweave/style/Keyword.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace sigil::compose {

/** ONE PROPERTY. Every value a verb or a rule may state has an
 *  enumerator here, and nothing else does: the four decoration lists, the
 *  masks, the stroke passes, the key, the role, the classes, the sheets
 *  and the cache mode APPEND or are identity, and a keyword said about
 *  one would mean nothing.
 *
 *  The per-side names are the properties; `padding`, `margin` and `inset`
 *  are shorthands that state four of them at once, as they are in CSS. */
enum class Property : uint8_t {
  // The box model.
  Display,
  BoxSizing,
  Gap,
  PaddingTop,
  PaddingRight,
  PaddingBottom,
  PaddingLeft,
  MarginTop,
  MarginRight,
  MarginBottom,
  MarginLeft,
  Width,
  Height,
  MinWidth,
  MaxWidth,
  MinHeight,
  MaxHeight,
  AspectRatio,
  // The flex line.
  FlexDirection,
  FlexWrap,
  FlexBasis,
  FlexGrow,
  FlexShrink,
  AlignItems,
  AlignSelf,
  JustifyContent,
  // Placement out of the flow.
  Absolute,
  Left,
  Top,
  Right,
  Bottom,
  CenterAt,
  GridCells,
  GridCellAlign,
  GridArea,
  // The silhouette.
  BorderRadius,
  Shape,
  Overflow,
  // What fills it and how it composites.
  Fill,
  Opacity,
  BlendMode,
  ZIndex,
  // The 2D transform.
  TranslateX,
  TranslateY,
  Rotate,
  Scale,
  ScaleX,
  ScaleY,
  SkewX,
  SkewY,
  TransformOrigin,
  // The plane the node turns in.
  RotateX,
  RotateY,
  TranslateZ,
  ScaleZ,
  Perspective,
  PerspectiveOrigin,
  TransformOriginZ,
  Preserve3d,
  Backface,
  // What the decorations dress.
  DecorationOutline,
  // The cascade: the four values everything under a node inherits, and
  // the sampling an image leaf reads.
  Font,
  Paragraph,
  Ink,
  CustomProperties,
  ImageRendering,

  kCount
};

/** WHICH PROPERTIES A VALUE STATED. A field carries its type's default
 *  until a verb writes it, so the value alone cannot tell "states the
 *  same number" from "says nothing about it" — and those two are
 *  different nodes to the cascade, to a rule and to the prune.
 *
 *  A bit set beside every write is what keeps them apart, and it is
 *  compared by value. Every property has one writer, which sets its bit.
 *  @trap Only the fields the computed style carries are closed to any
 *  other write, so skipping the bit there does not compile; the
 *  properties kept on the node itself (the depth lanes, the shape, the
 *  grid area, the decoration outline and the five the cascade pass
 *  resolves) keep storage a kernel verb can still reach around its
 *  writer. */
class PropertyMask {
 public:
  constexpr void set(Property property) {
    m_words[word(property)] |= bit(property);
  }
  constexpr void clear(Property property) {
    m_words[word(property)] &= ~bit(property);
  }
  [[nodiscard]] constexpr bool has(Property property) const {
    return (m_words[word(property)] & bit(property)) != 0;
  }
  [[nodiscard]] constexpr bool empty() const {
    return m_words[0] == 0 && m_words[1] == 0;
  }
  bool operator==(const PropertyMask&) const = default;

 private:
  static constexpr size_t word(Property property) {
    return static_cast<size_t>(property) >> 6u;
  }
  static constexpr uint64_t bit(Property property) {
    return uint64_t{1} << (static_cast<size_t>(property) & 63u);
  }
  uint64_t m_words[2] = {0, 0};
};

static_assert(static_cast<size_t>(Property::kCount) <= 128,
              "Property outgrew the two words PropertyMask holds. Widen "
              "the array and ElementNode's size assertion together — a "
              "mask too narrow silently drops the bits past its end, and "
              "a property whose bit is always clear reads as one no node "
              "ever stated.");

/** WHETHER A PROPERTY IS TAKEN FROM THE PARENT where nothing states it.
 *
 *  CSS's inherited set, as this library spells it: the type, the paragraph,
 *  the ink, the custom properties and the image sampling. Nothing in the box,
 *  the flex line, the placement, the fill, the silhouette, the transforms or
 *  the plane inherits — each of those is a statement about ONE box, and a box
 *  that took its parent's padding would apply it again at every depth.
 *
 *  This is the whole of the set: adding a property to it is adding one
 *  line here, and the fold walks the list this table builds. */
constexpr bool inheritsByDefault(Property property) {
  switch (property) {
    case Property::Font:
    case Property::Paragraph:
    case Property::Ink:
    case Property::CustomProperties:
    case Property::ImageRendering:
      return true;
    default:
      return false;
  }
}

namespace detail {
constexpr size_t inheritedCount() {
  size_t count = 0;
  for (size_t i = 0; i < (size_t)Property::kCount; ++i)
    if (inheritsByDefault((Property)i)) ++count;
  return count;
}
}  // namespace detail

/** THE INHERITED SET AS A LIST, built from the table above at compile
 *  time so the fold walks five entries rather than every property, and so the
 *  table above stays the only place the set is written down. */
constexpr std::array<Property, detail::inheritedCount()> kInherited = [] {
  std::array<Property, detail::inheritedCount()> list{};
  size_t at = 0;
  for (size_t i = 0; i < (size_t)Property::kCount; ++i)
    if (inheritsByDefault((Property)i)) list[at++] = (Property)i;
  return list;
}();

/** WHETHER A KEYWORD SAID ABOUT @p property IS ANSWERED BY ANYTHING.
 *
 *  A keyword needs a place to resolve: a row in `copyProperty`, which is
 *  every property the computed style carries, or the cascade pass, which
 *  resolves the inherited five against the values arriving from above.
 *  The rest — the plane a node turns in, the silhouette's generator, the
 *  grid area and the outline the decorations dress — are kept on the
 *  description, which no fold reads, so a keyword written about one of
 *  them would stand for nothing. Saying so at the verb is the only way an
 *  author learns it. */
constexpr bool answersKeyword(Property property) {
  switch (property) {
    case Property::GridArea:
    case Property::Shape:
    case Property::RotateX:
    case Property::RotateY:
    case Property::TranslateZ:
    case Property::ScaleZ:
    case Property::Perspective:
    case Property::PerspectiveOrigin:
    case Property::TransformOriginZ:
    case Property::Preserve3d:
    case Property::Backface:
    case Property::DecorationOutline:
    case Property::kCount:
      return false;
    default:
      return true;
  }
}

/** Which of the two a keyword comes to for @p property: `unset` is the
 *  only one that asks, and it asks this table. */
constexpr sigil::weave::Keyword resolveKeyword(sigil::weave::Keyword keyword,
                                               Property property) {
  using sigil::weave::Keyword;
  if (keyword != Keyword::Unset) return keyword;
  return inheritsByDefault(property) ? Keyword::Inherit : Keyword::Initial;
}

/** The property's name, as an author writes it — for a diagnostic, which
 *  is the only thing that reads one. */
[[nodiscard]] std::string_view propertyName(Property property);

}  // namespace sigil::compose
