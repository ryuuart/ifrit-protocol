#pragma once

/** @file
 * @ingroup compose-core
 *
 * THE OPERATOR SEAM: a comparable value applied to a node with
 * `Element::operators`, handed a finished table of what the composer
 * measured, and answering with what it did to the node's children. An
 * arranging operator places and turns the node's direct children during
 * layout, reading each one's measured size and attributes; it runs in
 * list order, and each sees where the ones before it left every child.
 *
 * A placement scheme of the older shape — `place(const LayoutInput&)`
 * returning one rectangle per child — is an operator too: constructing
 * one from it adapts the call, so every `layouts::` value is applied by
 * the same verb.
 */

#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <include/core/SkSize.h>
#include <sigilcompose/core/Attributes.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcore/comparable/Erased.h>

#include <concepts>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace sigil::compose {

/** WHAT AN ARRANGING OPERATOR IS HANDED: the node's box, and one record
 *  per direct child that has a box — measured, with its facts, and where
 *  it stands so far. An operator writes where each child goes onto the
 *  record; the composer reads the records back once the list has run. */
struct Arrangement {
  /** ONE DIRECT CHILD as an operator sees it. `rect` is where the child
   *  stands when the operator is called — where the operator before it
   *  in the list left it, or where the flex layout put it for the first
   *  — so an operator that nudges reads it and an operator that places
   *  overwrites it. `turnDegrees` is a paint-only turn about the
   *  child's own transform origin, laid over the rotation the child
   *  states for itself; it moves no layout. */
  struct Child {
    SkSize size = SkSize::MakeEmpty();
    /// First-baseline offset from the child's top; NaN for a child with none.
    float baseline = std::numeric_limits<float>::quiet_NaN();
    /// The smallest the child can be without spilling its content, filled
    /// only for an operator that declares `readsChildMinSizes`.
    SkSize minSize = SkSize::MakeEmpty();
    CellSpan cells;
    std::string area;
    Attributes attributes;
    SkRect rect = SkRect::MakeEmpty();
    float turnDegrees = 0.0f;

    /** The fact under @p name, as the child stated it. */
    template <typename T>
    std::optional<T> attribute(std::string_view name) const {
      return attributes.get<T>(name);
    }
    /** The fact under @p name as a number, whichever numeric type it was
     *  written in — an operator placing by a lane reads it here, so an
     *  author who wrote `3` and one who wrote `3.0f` are placed alike. */
    std::optional<float> number(std::string_view name) const {
      if (auto value = attributes.get<float>(name)) return *value;
      if (auto value = attributes.get<int>(name)) return (float)*value;
      if (auto value = attributes.get<double>(name)) return (float)*value;
      if (auto value = attributes.get<unsigned int>(name)) return (float)*value;
      if (auto value = attributes.get<long>(name)) return (float)*value;
      if (auto value = attributes.get<unsigned long>(name)) return (float)*value;
      return std::nullopt;
    }
    /** Puts the child at @p where, size included. */
    void place(SkRect where) { rect = where; }
    /** Centres the child's measured size on @p centre. */
    void centreAt(SkPoint centre) {
      rect = SkRect::MakeXYWH(centre.x() - size.width() / 2,
                              centre.y() - size.height() / 2, size.width(),
                              size.height());
    }
    /** Turns the child @p degrees clockwise about its transform origin. */
    void turn(float degrees) { turnDegrees = degrees; }
  };

  /// The node's own box, at the origin: what the children are placed in.
  SkRect box = SkRect::MakeEmpty();
  std::vector<Child> children;
  /// Whether each child's `minSize` was measured — true only when an
  /// operator in the list asked, since the measure costs one text layout
  /// per text child.
  bool minSizesMeasured = false;
};

/** WHAT ARRANGES: a value with `void arrange(Arrangement&) const`. */
template <typename O>
concept Arranging = requires(const O& o, Arrangement& a) {
  { o.arrange(a) };
};

/** AN ARRANGING OPERATOR: what arranges, as a comparable value. Equality
 *  is the reconciler's question — equal values over an unchanged table
 *  did the same thing — so an operator is a VALUE whose equality is its
 *  parameters. A value that arranges but carries no equality is taken
 *  too, as the escape hatch that never prunes. */
template <typename O>
concept ArrangingOperator = Arranging<O> && std::equality_comparable<O>;

/** Whether a scheme or an operator asks for the children's content
 *  minima, which cost a measure per text child: it says so with
 *  `static constexpr bool readsChildMinSizes = true;`. */
template <typename T>
concept ReadsChildMinSizes = requires {
  { T::readsChildMinSizes } -> std::convertible_to<bool>;
} && T::readsChildMinSizes;

namespace detail {

/** WHAT THE OPERATOR SEAM DOES, behind the erasure. */
struct OperatorOperations {
  virtual ~OperatorOperations() = default;
  virtual void arrange(Arrangement& arrangement) const = 0;
};

/** An arranging operator as those operations. The equality is the held
 *  value's, and stands only where the value has one — a defaulted
 *  comparison would compare the operations base too, which has none,
 *  and come out deleted for every model — so a value with no equality
 *  makes a model with none, which is the escape hatch: identity only,
 *  never pruned. */
template <Arranging O>
struct ArrangingModel : OperatorOperations {
  O held;
  explicit ArrangingModel(O value) : held(std::move(value)) {}
  bool operator==(const ArrangingModel& other) const
    requires std::equality_comparable<O>
  {
    return held == other.held;
  }
  void arrange(Arrangement& arrangement) const override {
    held.arrange(arrangement);
  }
};

/** The table an older placement scheme reads, built from the arrangement's
 *  records, and the rectangles it answers written back onto them. */
LayoutInput layoutInputOf(const Arrangement& arrangement);
void placeFromRects(Arrangement& arrangement, const std::vector<SkRect>& rects);

/** A placement scheme — `place(const LayoutInput&)` returning one rect per
 *  child — as the same operations, adapted. */
template <LayoutScheme L>
struct SchemeModel : OperatorOperations {
  L held;
  explicit SchemeModel(L value) : held(std::move(value)) {}
  bool operator==(const SchemeModel& other) const
    requires std::equality_comparable<L>
  {
    return held == other.held;
  }
  void arrange(Arrangement& arrangement) const override {
    placeFromRects(arrangement, held.place(layoutInputOf(arrangement)));
  }
};

}  // namespace detail

/** THE OPERATOR AS A NODE HOLDS IT, type-erased: what `Element::operators`
 *  takes. Two constructions, one value: an arranging operator, or a
 *  placement scheme adapted to the same call. A comparable value prunes
 *  the node while the value and what it read are unchanged; a value
 *  with no equality never compares equal to a separately-built one, so
 *  its node re-runs the operator on every describe. Copies of ONE
 *  Operator share state and compare equal.
 *
 *  Held as one shared immutable pointer, so a node carrying an operator
 *  list costs a pointer per entry. */
class Operator {
 public:
  Operator() = default;

  template <Arranging O>
    requires(!LayoutScheme<std::remove_cvref_t<O>> &&
             !std::same_as<std::remove_cvref_t<O>, Operator>)
  Operator(O operatorValue)  // NOLINT: implicit by design (operators({Ring{…}}))
      : m_held(detail::ArrangingModel<std::remove_cvref_t<O>>(
            std::move(operatorValue))),
        m_readsChildMinSizes(ReadsChildMinSizes<std::remove_cvref_t<O>>) {}

  template <LayoutScheme L>
    requires(!std::same_as<std::remove_cvref_t<L>, Operator>)
  Operator(L scheme)  // NOLINT: implicit by design (operators({layouts::Grid{…}}))
      : m_held(detail::SchemeModel<std::remove_cvref_t<L>>(std::move(scheme))),
        m_readsChildMinSizes(ReadsChildMinSizes<std::remove_cvref_t<L>>) {}

  explicit operator bool() const { return (bool)m_held; }
  /** Runs the operator over @p arrangement. */
  void arrange(Arrangement& arrangement) const {
    if (m_held) m_held->arrange(arrangement);
  }
  /** Whether the held value asked for the children's content minima. */
  bool readsChildMinSizes() const { return m_readsChildMinSizes; }
  /** Does this value take part in structural equality? (False for a
   *  value with no equality of its own.) */
  bool comparable() const { return m_held.comparable(); }

  bool operator==(const Operator& other) const {
    return m_held == other.m_held;
  }

 private:
  core::Erased<detail::OperatorOperations> m_held;
  bool m_readsChildMinSizes = false;
};

}  // namespace sigil::compose
