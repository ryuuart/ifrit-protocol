#pragma once

/** @file
 * @ingroup compose-core
 *
 * SigilCompose layout values — Dimension and its literals, the Edges that
 * name the four sides around a node, Align, Justify,
 * Echo, Cache with the `cachePolicy` that reads it as the kernel's own, the
 * CellSpan a child claims and the LayoutInput a custom LayoutScheme
 * places children from, and the ComponentProperties and ComponentFunction
 * concepts the generic entry points are constrained by.
 */

#include <include/core/SkColor.h>
#include <include/core/SkRect.h>
#include <include/core/SkSize.h>
#include <sigilcompose/core/Attributes.h>
#include <sigilcompose/core/Var.h>
#include <sigilcore/cache/Policy.h>
#include <sigilmaterial/color/Color.h>
#include <sigilweave/style/Length.h>

#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace sigil::compose {

namespace detail {
/** The counted table a `Dimension::Unit::Calc` length's sum lives in,
 *  addressed by the handle bit-cast into its `value`: a holder is counted
 *  in when it is made, out when it goes, and the entry leaves the table
 *  with its last holder. */
void retainCalc(float handle) noexcept;
void releaseCalc(float handle) noexcept;
/** Whether two handles stand for equal sums. */
[[nodiscard]] bool calcEqual(float left, float right);
/** How many distinct sums the table holds now. */
[[nodiscard]] size_t calcEntries();
}  // namespace detail

// ---------------------------------------------------------------------------
// Layout values (Yoga semantics, 1:1)

/** A length that may be absolute, relative to the parent, relative to the
 *  font in force, read from a custom property, or left for layout to
 *  decide. Constructing one from a bare float gives pixels, so the common
 *  case reads as a number.
 *
 *  THE FONT-RELATIVE UNITS. `em` is the node's own resolved font
 *  size, `rem` the root's, `lh` the node's own line height, `ch` the
 *  advance of "0" in the face in force — SigilWeave's `Length`, spelled
 *  with its `_em`, `_rem`, `_lh` and `_ch` literals, converts here — so a
 *  padding written in ems follows the type it surrounds, and a change to
 *  an ancestor's font relays out everything measured in it. A `var(...)`
 *  reads the length the nearest ancestor set under that name and resolves
 *  it where it is read. `pt` is SigilWeave's too, and absolute: four
 *  pixels to every three points.
 *
 *  THE CANVAS-RELATIVE UNITS ARE THE ROOT'S BOX, not the parent's: `pw` is
 *  a percentage of the canvas the composer renders into, `ph` a percentage
 *  of its height, wherever in the tree the node sits. `pct` is the
 *  parent's, which is Yoga's own percent; these two are resolved into
 *  pixels before Yoga sees them, because a percentage of something that is
 *  not the containing block is a thing Yoga cannot express. A poster whose
 *  margin is `6_pw` keeps its proportions at every canvas size, however
 *  many boxes deep it is written. */
struct Dimension {
  /** What `value` is measured in. */
  enum class Unit : uint8_t {
    Px,    ///< device-independent pixels
    Pct,   ///< percent of the parent's corresponding extent
    Auto,  ///< left for layout to decide; `value` unused
    Em,    ///< multiples of the node's own resolved font size
    Rem,   ///< multiples of the root's font size
    Lh,    ///< multiples of the node's own line height
    Var,   ///< a custom property, its id bit-cast into `value`
    Pw,    ///< percent of the CANVAS's width
    Ph,    ///< percent of the CANVAS's height
    Ch,    ///< multiples of the advance of "0" in the font in force
    Pt,    ///< printer's points, four pixels to every three
    Calc   ///< a sum over several units, its interned id bit-cast into `value`
  };
  Unit unit = Unit::Auto;
  /** The length, or under `Var` and `Calc` an id, bit-cast into the
   *  float and never read as one. Both fields are read freely; a `Calc`
   *  is made only by the arithmetic below, never by writing them. */
  float value = 0.0f;

  constexpr Dimension() = default;
  constexpr Dimension(float px)  // NOLINT: implicit by design
      : unit(Unit::Px), value(px) {}
  constexpr Dimension(sigil::weave::Length length)  // NOLINT: implicit
      : unit(length.unit == sigil::weave::Length::Unit::Em    ? Unit::Em
             : length.unit == sigil::weave::Length::Unit::Rem ? Unit::Rem
             : length.unit == sigil::weave::Length::Unit::Lh  ? Unit::Lh
             : length.unit == sigil::weave::Length::Unit::Ch  ? Unit::Ch
             : length.unit == sigil::weave::Length::Unit::Pt  ? Unit::Pt
                                                              : Unit::Px),
        value(length.value) {}
  constexpr Dimension(VarRef reference)  // NOLINT: implicit
      : unit(Unit::Var), value(std::bit_cast<float>(reference.id)) {}

  // A `Calc` length is a count on its sum's entry, so a copy counts one
  // more holder and the last to go takes the entry with it. Every other
  // unit is two plain fields, and copies as two plain fields.
  constexpr Dimension(const Dimension& other)
      : unit(other.unit), value(other.value) {
    if (unit == Unit::Calc) [[unlikely]]
      detail::retainCalc(value);
  }
  constexpr Dimension(Dimension&& other) noexcept
      : unit(other.unit), value(other.value) {
    if (unit == Unit::Calc) [[unlikely]]
      other.unit = Unit::Auto;
  }
  constexpr Dimension& operator=(const Dimension& other) {
    if (other.unit == Unit::Calc) [[unlikely]]
      detail::retainCalc(other.value);
    if (unit == Unit::Calc) [[unlikely]]
      detail::releaseCalc(value);
    unit = other.unit;
    value = other.value;
    return *this;
  }
  constexpr Dimension& operator=(Dimension&& other) noexcept {
    if (this == &other) return *this;
    if (unit == Unit::Calc) [[unlikely]]
      detail::releaseCalc(value);
    unit = other.unit;
    value = other.value;
    if (other.unit == Unit::Calc) [[unlikely]]
      other.unit = Unit::Auto;
    return *this;
  }
  constexpr ~Dimension() {
    if (unit == Unit::Calc) [[unlikely]]
      detail::releaseCalc(value);
  }

  /** Whether resolving this length needs something the number itself does
   *  not carry — the font in force, a custom property, or the canvas: it is
   *  everything but a pixel, a point, a parent-relative percent and auto. */
  [[nodiscard]] constexpr bool relative() const {
    return unit == Unit::Em || unit == Unit::Rem || unit == Unit::Lh ||
           unit == Unit::Ch || unit == Unit::Var || unit == Unit::Pw ||
           unit == Unit::Ph || unit == Unit::Calc;
  }
  /** The custom property a `Var` length reads; meaningless otherwise. */
  [[nodiscard]] constexpr VarRef reference() const {
    return {std::bit_cast<uint32_t>(value)};
  }
  /** Equal in unit and length; two sums are equal where their terms are,
   *  whichever entries hold them. */
  constexpr bool operator==(const Dimension& other) const {
    if (unit != other.unit) return false;
    if (unit == Unit::Calc) [[unlikely]]
      return detail::calcEqual(value, other.value);
    return value == other.value;
  }
};
static_assert(sizeof(Dimension) == 8);
/** @p v percent of the PARENT's corresponding extent — Yoga's own
 *  percent, and the one CSS means by `%`. */
constexpr Dimension pct(float v) {
  Dimension d;
  d.unit = Dimension::Unit::Pct;
  d.value = v;
  return d;
}
/** @p v percent of the CANVAS's width — the root's box, not the parent's. */
constexpr Dimension pw(float v) {
  Dimension d;
  d.unit = Dimension::Unit::Pw;
  d.value = v;
  return d;
}
/** @p v percent of the CANVAS's height. */
constexpr Dimension ph(float v) {
  Dimension d;
  d.unit = Dimension::Unit::Ph;
  d.value = v;
  return d;
}
/** THE CSS `auto`: this length is not stated, so layout decides it. It is
 *  what an unstated `Dimension` already is, and the name exists so a side
 *  of an `inset()` can be left unpinned in the middle of four that are
 *  stated — an unpinned side lets the node's own width or height, or the
 *  opposite inset, size it rather than stretching it across the box. */
constexpr Dimension autoDimension() { return {}; }

/** THE ONE PLACE A LENGTH WRITTEN AS TEXT IS READ — `"12"`, `"12px"`,
 *  `"1.5em"`, `"2rem"`, `".5lh"`, `"3ch"`, `"9pt"`, `"50%"`, `"10pw"`,
 *  `"10ph"`, `"auto"`, `var(name)` naming a custom property, and
 *  `calc(...)` over any of them with `+ - * /` and brackets — so a
 *  rule's text and a length handed in from Python are one grammar with
 *  one set of units.
 *
 *  Case does not matter and surrounding blank space is ignored. A bare
 *  number is pixels, as it is everywhere else in this library. Nothing is
 *  answered for text this grammar does not cover, including an empty
 *  string and a unit no `Dimension` carries; the caller says what an
 *  unreadable length means where it stands. */
[[nodiscard]] std::optional<Dimension> parseDimension(std::string_view text);

/** @name CSS's calc(), as arithmetic on lengths
 *  `width(2 * 1_em + 12_px)`, `padding(var("gutter") / 2)`: a sum over
 *  any of the units above, resolved to pixels by the cascade pass with
 *  the font, the custom properties and the canvas in force. Lengths in
 *  one unit stay in that unit, so `2 * 1_em` IS `2_em`. A number stands
 *  for pixels, as it does everywhere here; a length may be scaled by a
 *  number and divided by one, never by another length.
 *  @trap A PERCENTAGE MIXES WITH NOTHING: Yoga resolves a percentage of
 *  the parent itself and holds no sum, so `50_pct + 1_em` is REFUSED — it
 *  warns once and stands as `autoDimension()` — and so are arithmetic on
 *  auto and a division by zero. `pw` and `ph` measure the canvas and mix
 *  freely.
 *  @{ */
[[nodiscard]] Dimension operator+(Dimension left, Dimension right);
[[nodiscard]] Dimension operator-(Dimension left, Dimension right);
[[nodiscard]] Dimension operator-(Dimension length);
[[nodiscard]] Dimension operator*(Dimension length, float factor);
[[nodiscard]] Dimension operator*(float factor, Dimension length);
[[nodiscard]] Dimension operator/(Dimension length, float divisor);
/** @} */

/** `width(50_pct)`, `width(50_pw)`, `top(10_ph)`, `flexBasis(120_px)` — for the
 *  Dimension-valued setters; exposed by `using namespace sigil::compose` (or
 *  `using namespace sigil::compose::literals`). */
inline namespace literals {
constexpr Dimension operator""_px(long double v) { return Dimension((float)v); }
constexpr Dimension operator""_px(unsigned long long v) {
  return Dimension((float)v);
}
constexpr Dimension operator""_pct(long double v) { return pct((float)v); }
constexpr Dimension operator""_pct(unsigned long long v) {
  return pct((float)v);
}
constexpr Dimension operator""_pw(long double v) { return pw((float)v); }
constexpr Dimension operator""_pw(unsigned long long v) { return pw((float)v); }
constexpr Dimension operator""_ph(long double v) { return ph((float)v); }
constexpr Dimension operator""_ph(unsigned long long v) { return ph((float)v); }
}  // namespace literals

/** FOUR LENGTHS, ONE PER SIDE, EACH SAYING WHICH SIDE IT IS —
 *  `padding({.top = 8, .left = 12})`, `inset({.right = 0, .bottom = 0})`.
 *  The positional shorthands beside it run in CSS's order, and so do
 *  these fields, because a designated initialiser must follow the
 *  declaration order: top, right, bottom, left.
 *
 *  A SIDE LEFT UNNAMED IS UNSTATED, and each verb reads that as its own
 *  default: zero for `padding` and `margin`, unpinned for `inset`, which
 *  is what `autoDimension()` says. Auto is not a length the air around a
 *  node can take, so on those two an explicitly auto side is zero too.
 *
 *  The trap: braces with no names are ordinary aggregate initialisation,
 *  so `padding({2, 4})` is top 2 and right 4, not the vertical/horizontal
 *  pair the positional `padding(2, 4)` writes. */
struct Edges {
  Dimension top, right, bottom, left;
  bool operator==(const Edges&) const = default;
};

/** WHICH WAY A CONTAINER'S MAIN AXIS RUNS, and from which end its
 *  children are placed — CSS's `flex-direction`. */
enum class FlexDirection : uint8_t {
  Column,         ///< top to bottom; the default
  ColumnReverse,  ///< bottom to top
  Row,            ///< left to right
  RowReverse      ///< right to left
};
/** WHAT BECOMES OF CHILDREN THAT OVERFLOW the main axis — CSS's
 *  `flex-wrap`. */
enum class FlexWrap : uint8_t {
  NoWrap,      ///< one line, however long; the default
  Wrap,        ///< new lines after the first, toward the cross-axis end
  WrapReverse  ///< new lines stacked toward the cross-axis start
};
/** WHETHER A NODE HAS A BOX IN THE LAYOUT AT ALL — CSS's `display`, as far
 *  as a flex tree has one. */
enum class Display : uint8_t {
  Flex,     ///< a flex item that lays its own children out; the default
  None,     ///< no box and no subtree: nothing is laid out, drawn or hit
  Contents  ///< no box of its own: its children are its parent's items
};
/** WHAT `width()` AND `height()` MEASURE — CSS's `box-sizing`. */
enum class BoxSizing : uint8_t {
  BorderBox,  ///< the whole box, padding included; the default
  ContentBox  ///< the content alone, so padding is added outside it
};
/** WHAT BECOMES OF PAINT THAT LEAVES A NODE'S SHAPE — CSS's `overflow`, as
 *  far as a tree that never scrolls has one. */
enum class Overflow : uint8_t {
  Visible,  ///< fill, content and children paint past the shape; the default
  Clip      ///< they are cut to the shape; the decorations keep their reach
};

/** WHERE A CHILD SITS ACROSS the container's main axis — down a row,
 *  across a column — which is CSS's `align-items` on the container and
 *  `align-self` on one child. */
enum class Align : uint8_t {
  Auto,     ///< take the container's `alignItems`; the child's default
  Start,    ///< against the cross-axis start edge
  Center,   ///< centred across the cross axis
  End,      ///< against the cross-axis end edge
  Stretch,  ///< sized to the container across the cross axis
  Baseline  ///< first baselines of the run share one line
};
/** HOW THE CHILDREN ARE DISTRIBUTED ALONG the container's main axis, and
 *  what becomes of the room left over — CSS's `justify-content`. */
enum class Justify : uint8_t {
  Start,         ///< packed at the main-axis start; the default
  Center,        ///< packed in the middle
  End,           ///< packed at the main-axis end
  SpaceBetween,  ///< the leftover room split between the children
  SpaceAround,   ///< a half share outside the first and last as well
  SpaceEvenly    ///< every gap equal, the outer ones included
};

/** One misprint pass: the node's own fill shape and text re-stamped at
 *  `offset` in a flat color, UNDER the real content. Repeated echoes stack
 *  in declaration order, bottom first. This is the registration-error
 *  look — offset ink under-copies, hard-edged sticker stacks — as one call
 *  rather than duplicate sibling nodes. */
struct Echo {
  SkVector offset = {3, 3};
  material::Color color = {0, 0, 0, 1};
  bool operator==(const Echo&) const = default;
};

/** Cache override.
 *
 *  - **Auto** (the default) records provably-static subtrees as pictures.
 *  - **Picture** records, and never lets the library promote the node to
 *    a pixel bake.
 *  - **Texture** rasterizes the subtree once into an image. Best for dense
 *    or effect-heavy content; wasteful for sparse regions, where the blit
 *    of a mostly-empty image costs more than the few draws it replaced.
 *    While the node holds still the bake is taken in DEVICE space and
 *    blitted without resampling, exact at any angle — under static
 *    ancestors too, whose recordings are then pinned to the matrix they
 *    were made under and remade when it changes. Under a live transform,
 *    its own or an ancestor's, the bake is held in local space and rides
 *    the motion through the blit.
 *  - **Group** is Texture for a subtree whose children ANIMATE — see
 *    below.
 *  - **None** opts a node out entirely. A per-frame paint program that
 *    reads the clock MUST declare this: nothing can see that a
 *    `PaintProgram` sampled `elapsedSeconds`, so an undeclared one is
 *    recorded on its first frame and replayed frozen thereafter.
 *
 *  **Group** is for "many small rotated or blended pieces forming one
 *  assembly that is currently still". `Cache::Texture` bakes a node's OWN
 *  paint and refuses the moment anything below it is volatile, so a
 *  fill-less container of animated strips gets no bake at all and every
 *  strip replays its shaders every frame. `Cache::Group` bakes the
 *  container AND its children into one unrotated device-space layer, so
 *  the children's rotations, bevels and mutual compositing resolve INSIDE
 *  the bake at full precision. That is why it is pixel-safe where putting
 *  `Cache::Texture` on each child is not: per-child bakes isolate the
 *  pieces and change how they composite with each other.
 *
 *  It is held by a SUBTREE VALUE MEMO rather than by a volatility verdict.
 *  The bake is taken only while every bound transform, opacity and content
 *  scalar below the node still holds the value it held last frame, and is
 *  dropped on the frame any of them ticks. So an entrance animation plays
 *  live and the settled assembly costs one blit.
 *
 *  IT REFUSES, permanently and with one line to stderr, any subtree
 *  carrying volatility a float comparison cannot see: a live material
 *  (`uTime` or a bound uniform), an animated decoration, an animated
 *  image, a bound `fill()`, a variable-font drive, a `Cache::None`
 *  descendant, or a non-srcOver blend or backdrop filter below the root
 *  (which would resolve against the bake's transparent black). It also
 *  declines per frame while its own transform animates or its device rect
 *  is moving, because a device-pinned bake remade every frame costs more
 *  than the paint it replaces. A Group node that never reports itself as
 *  held has one of the above in it. */
enum class Cache : uint8_t { Auto, Picture, Texture, Group, None };

/** The POLICY half of a cache mode — what the settled-subtree proof reads,
 *  with the tier left behind.
 *
 *  Three of the five names above say the same thing to the proof: they ask
 *  for a bake, and they differ only in which artefact the painter makes.
 *  `Auto` leaves the decision to the proof, and `None` states volatility no
 *  declaration can see. The proof answers in those three terms and never
 *  learns what a picture, a texture or a group is. */
constexpr core::Cache cachePolicy(Cache c) {
  switch (c) {
    case Cache::Auto:
      return core::Cache::Auto;
    case Cache::None:
      return core::Cache::Never;
    case Cache::Picture:
    case Cache::Texture:
    case Cache::Group:
      break;
  }
  return core::Cache::Always;
}

// ---------------------------------------------------------------------------
// Custom layout (the SwiftUI Layout-protocol shape, C++20-ified)

/** WHICH CELLS OF A GRID A CHILD CLAIMS, and where it sits inside them —
 *  the one thing about a child that a placement scheme needs and cannot
 *  measure.
 *
 *  It is on the CHILD rather than in a list the scheme carries, and that
 *  is the whole point. A scheme holding a vector parallel to the children
 *  has nothing to check it against: insert or reorder one child and every
 *  entry after it silently addresses the wrong one, taking another cell's
 *  span, alignment and origin, with no error and a picture that still
 *  looks plausible.
 *
 *  `across` and `down` place the child INSIDE the cell box its span makes;
 *  `Align::Stretch` sizes it to that box instead. `Auto` and `Baseline`
 *  read as `Start` — a cell has no run of siblings to share a baseline
 *  with.
 *
 *  `declared` is false on a child that said nothing, so a scheme can tell
 *  "cell (0,0)" from "wherever you like" and flow the rest. */
struct CellSpan {
  int column = 0, row = 0;
  int columns = 1, rows = 1;
  Align across = Align::Start;
  Align down = Align::Start;
  bool declared = false;
  /** Whether `across` and `down` were STATED. A scheme that carries its
   *  own default alignment — a grid's place-items — must be able to tell
   *  "start, because that is the default" from "start, because the child
   *  asked for it", or its default could never apply to any child that
   *  named a cell. */
  bool alignDeclared = false;
  bool operator==(const CellSpan&) const = default;
};

/** WHERE THE CHILDREN THAT CLAIMED NOTHING LAND, in @p spans, over a grid
 *  @p columns wide: the ONE flow every cell-shaped scheme uses.
 *
 *  The spans a scheme resolved are its own — a name looked up in a picture
 *  of areas, or the numbers a child stated — and every one whose
 *  `declared` is true is taken as it stands. What is left flows into the
 *  cells nothing claimed, left to right and then down, each child at the
 *  span it asked for (clamped to the grid: there is no cell a child wider
 *  than the grid could ever be free at). A flowed child's resolved
 *  `column`, `row`, `columns` and `rows` are written back.
 *
 *  @p dense is the one difference between the two orders CSS names. Sparse
 *  never looks back past the last cell it filled, so the run stays in
 *  declaration order; dense starts every search at cell zero, which fills
 *  the holes a wide span left beside it and lets a later child land before
 *  an earlier one. */
void flowCells(std::vector<CellSpan>& spans, int columns, bool dense = false);

/** What a custom layout sees: the container's resolved size, each child's
 *  measured size (text children measured by SigilWeave), each child's
 *  first-baseline offset from its own top (NaN for children without one) —
 *  what baseline-rhythm schemes (layouts::BaselineGrid) snap by — and the
 *  cells each child claimed with `Element::gridCells`. */
struct LayoutInput {
  SkSize container = SkSize::MakeEmpty();
  std::vector<SkSize> childSizes;
  std::vector<float> childBaselines;  ///< NaN = no baseline (non-text)
  std::vector<CellSpan> childCells;   ///< .declared = false when unspoken
  /** THE NAME OF THE REGION each child claims, when the scheme draws a
   *  picture of itself out of names (`layouts::Grid::areas`) — empty for a
   *  child that named none, which is the numeric spelling in `childCells`
   *  and what a name resolves to. A name no picture carries is silent and
   *  the child flows.
   *
   *  Beside `childCells` rather than in it because a string on the properties
   *  of every node in the tree is what the node size assertion forbids,
   *  and a named region is rare. */
  std::vector<std::string> childAreas;
  /** THE SMALLEST EACH CHILD CAN BE without its content spilling out of
   *  it — the second of the two intrinsic contributions a track-sizing
   *  rule needs, where `childSizes` is the first.
   *
   *  A text leaf's is its longest unbreakable run, measured at a nil
   *  width; its height is left at the measured one, because the height of
   *  a paragraph set one word to a line is not a minimum anybody wants.
   *  Everything else answers with its measured size; a scheme does not
   *  re-describe a box or infer a smaller intrinsic size for it. Text's
   *  wrapped extent is remeasured at the scheme's placed reading measure
   *  before content-sized tracks settle.
   *
   *  EMPTY unless the scheme asked for it, since the text minimum costs a
   *  measure per text child. A scheme asks by declaring
   *  `static constexpr bool readsChildMinSizes = true;`. */
  std::vector<SkSize> childMinSizes;
  /** THE FACTS EACH CHILD STATES (`Element::attribute`), one table per
   *  child, so a scheme places by what a child says of itself — its
   *  tier, its hour, its weight — rather than by its index alone. */
  std::vector<Attributes> childAttributes;

  /** The fact child @p index states under @p name, as a @p T, or nothing. */
  template <typename T>
  std::optional<T> attribute(size_t index, std::string_view name) const {
    if (index >= childAttributes.size()) return std::nullopt;
    return childAttributes[index].get<T>(name);
  }
};

/** A custom layout places children: one rect per child (position and
 *  size, container-relative). Runs as a bounded second layout pass. */
template <typename L>
concept LayoutScheme = requires(const L& l, const LayoutInput& in) {
  { l.place(in) } -> std::convertible_to<std::vector<SkRect>>;
};

/** A scheme that sizes tracks from the content and therefore needs
 *  `LayoutInput::childMinSizes` filled. Opt in, because the minimum costs
 *  a measure per text child and most schemes place from the container and
 *  a formula. */
template <typename L>
concept SizesFromContentMinima = LayoutScheme<L> && requires {
  { L::readsChildMinSizes } -> std::convertible_to<bool>;
} && L::readsChildMinSizes;

// ---------------------------------------------------------------------------
// Concepts (readable errors at the generic entry points)

/** WHAT A COMPONENT'S PROPERTIES MUST BE: a copyable value that compares
 *  equal to itself. Both are what the reconciler needs to tell a
 *  component's new properties from the ones it drew last time and skip
 *  the ones that did not move. */
template <typename P>
concept ComponentProperties = std::equality_comparable<P> && std::copyable<P>;

class Element;

/** WHAT A COMPONENT IS: anything callable with its properties that
 *  answers a node. There is no base class and nothing to inherit — a
 *  lambda, a free function or a struct with `operator()` all qualify. */
template <typename F, typename P>
concept ComponentFunction =
    std::invocable<F, const P&> &&
    std::convertible_to<std::invoke_result_t<F, const P&>, Element>;

}  // namespace sigil::compose
