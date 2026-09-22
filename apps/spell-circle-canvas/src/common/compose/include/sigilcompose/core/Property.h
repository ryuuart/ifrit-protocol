#pragma once

/** @file
 * @ingroup compose-core
 *
 * EVERY PROPERTY A DECLARATION CAN STATE, as one name: the set a value
 * records that it stated, the three wide keywords CSS lets a property be
 * written as instead of a value, and the one table that says which
 * properties a node takes from its parent when nothing states them.
 */

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

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
  GridArea,
  // The silhouette.
  BorderRadius,
  Shape,
  Overflow,
  // What fills it and how it composites.
  Fill,
  Opacity,
  BlendMode,
  BackgroundOrigin,
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
  Block,
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
 *  compared by value: a verb that writes a field without its bit makes
 *  the two descriptions equal, the node prunes for good, and nothing
 *  reports it. */
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

/** WHAT A PROPERTY MAY BE WRITTEN AS instead of a value — CSS's three
 *  wide keywords, each meaning something a number cannot say. */
enum class Keyword : uint8_t {
  /** Take the parent's computed value, whether or not this property is
   *  one that inherits on its own. */
  Inherit,
  /** Take the property's own initial value, whatever an ancestor says. */
  Initial,
  /** Whichever of the two the property's default behaviour asks for:
   *  inherit where it inherits, initial where it does not. */
  Unset
};

/** WHETHER A PROPERTY IS TAKEN FROM THE PARENT where nothing states it.
 *
 *  CSS's inherited set, as this library spells it: the type, the block,
 *  the ink, the custom properties and the image sampling. Nothing in the
 *  box, the flex line, the placement, the fill, the silhouette, the
 *  transforms or the plane inherits — each of those is a statement about
 *  ONE box, and a box that took its parent's padding would apply it
 *  again at every depth.
 *
 *  This is the whole of the set: adding a property to it is adding one
 *  line here. */
constexpr bool inheritsByDefault(Property property) {
  switch (property) {
    case Property::Font:
    case Property::Block:
    case Property::Ink:
    case Property::CustomProperties:
    case Property::ImageRendering:
      return true;
    default:
      return false;
  }
}

/** Which of the two a keyword comes to for @p property: `unset` is the
 *  only one that asks, and it asks this table. */
constexpr Keyword resolveKeyword(Keyword keyword, Property property) {
  if (keyword != Keyword::Unset) return keyword;
  return inheritsByDefault(property) ? Keyword::Inherit : Keyword::Initial;
}

/** THE PROPERTIES WRITTEN AS A KEYWORD rather than a value, in the order
 *  they were written. Rare — most descriptions carry none — so it is a
 *  small vector rather than a slot per property, and it lives out of line
 *  on whatever holds it. */
class KeywordTable {
 public:
  struct Entry {
    Property property = Property::Display;
    Keyword keyword = Keyword::Unset;
    bool operator==(const Entry&) const = default;
  };

  /** @p property is written as @p keyword, replacing whatever it was
   *  written as before, where it stands. */
  void set(Property property, Keyword keyword) {
    for (Entry& entry : m_entries)
      if (entry.property == property) {
        entry.keyword = keyword;
        return;
      }
    m_entries.push_back({property, keyword});
  }
  /** The keyword @p property was written as, or nothing. */
  [[nodiscard]] std::optional<Keyword> find(Property property) const {
    for (const Entry& entry : m_entries)
      if (entry.property == property) return entry.keyword;
    return std::nullopt;
  }
  [[nodiscard]] const std::vector<Entry>& entries() const { return m_entries; }
  [[nodiscard]] bool empty() const { return m_entries.empty(); }
  bool operator==(const KeywordTable&) const = default;

 private:
  std::vector<Entry> m_entries;
};

/** The property's name, as an author writes it — for a diagnostic, which
 *  is the only thing that reads one. */
[[nodiscard]] std::string_view propertyName(Property property);

}  // namespace sigil::compose
