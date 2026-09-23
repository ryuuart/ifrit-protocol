#pragma once

/** @file
 * @ingroup compose-core
 *
 * THE OPERATOR SEAM: a comparable value applied to a node with
 * `Element::operators`, handed a finished table of what the composer
 * measured, and answering with what it did. An ARRANGING operator places
 * and turns the node's direct children during layout, reading each one's
 * measured size and attributes. An ADDING operator runs once layout has
 * settled, reads every node in its scope — key, facts, classes, bounds and
 * outline — and attaches new elements to one of them or to the scope. Both
 * run in list order, and each sees what the ones before it left.
 *
 * A placement scheme — `place(const LayoutInput&)`
 * returning one rectangle per child — is an operator too: constructing
 * one from it adapts the call, so every `layouts::` value is applied by
 * the same verb.
 */

#include <include/core/SkPath.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <include/core/SkSize.h>
#include <sigilcompose/core/Attributes.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcore/comparable/Erased.h>

#include <concepts>
#include <cstddef>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace sigil::compose {

class Element;

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
    std::optional<float> number(std::string_view name) const;
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

/** WHAT AN ADDING OPERATOR IS HANDED: the node's box and every node under
 *  it, settled — and the two places it may attach what it builds.
 *
 *  THE SCOPE IS CLOSED. A node under this one that applies operators of
 *  its own is ONE node here, with its own facts and bounds and nothing
 *  under it; what an operator inside it built is its own business, and
 *  what it wants read from outside it states as facts on its root. A
 *  node an operator added is not here at all. Everything is in the
 *  scope's coordinates: the node the operators were applied on, its
 *  top-left the origin.
 *
 *  An addition BELONGS TO WHAT IT IS ABOUT. About one node, it is
 *  attached to that node (`Node::attach`) — in the node's coordinates,
 *  under its transform and cascade, gone when it goes. About several, or
 *  about the scope, it is attached to the scope (`attach`). Either way it
 *  becomes an ordinary element beside the authored children, after them,
 *  out of their flow: it takes the cascade, a sheet dresses it, it
 *  hit-tests, and a later operator in the list reads its bounds. It can
 *  move nothing that was authored. */
class Scope {
 public:
  /** ONE NODE IN THE SCOPE as an adding operator sees it. */
  class Node {
   public:
    std::string key;
    Attributes attributes;
    std::vector<std::string> classes;
    /// Where the node stands, in the scope's coordinates.
    SkRect bounds = SkRect::MakeEmpty();
    /// The outline the node resolved to — its shape, a routed path, or
    /// its box — in the scope's coordinates.
    SkPath outline;

    template <typename T>
    std::optional<T> attribute(std::string_view name) const {
      return attributes.get<T>(name);
    }
    std::optional<float> number(std::string_view name) const;
    bool hasClass(std::string_view name) const;
    /** @p point in the scope's coordinates, moved into this node's own. */
    SkPoint toLocal(SkPoint point) const {
      return {point.x() - bounds.left(), point.y() - bounds.top()};
    }
    /** @p path in the scope's coordinates, moved into this node's own. */
    SkPath toLocal(const SkPath& path) const;
    /** ATTACHES @p element TO THIS NODE, placed in the node's own
     *  coordinates: what is about this node alone, drawn with it. */
    void attach(Element element) const;

   private:
    friend class Scope;
    Scope* m_scope = nullptr;
    size_t m_index = 0;
  };

  /// The scope's own box, at the origin.
  SkRect box = SkRect::MakeEmpty();

  std::span<const Node> nodes() const { return m_nodes; }
  /** The node keyed @p key, or null: an unknown key is silent, as it is
   *  across the derive family. */
  const Node* find(std::string_view key) const;
  /** Every node stating a fact under @p lane, in tree order. */
  std::vector<const Node*> having(std::string_view lane) const;
  /** Every node naming class @p name, in tree order. */
  std::vector<const Node*> withClass(std::string_view name) const;
  /** ATTACHES @p element TO THE SCOPE, placed in the scope's coordinates:
   *  what is about several nodes, or about the scope itself. */
  void attach(Element element);
  /** A COPY TO READ LATER — the nodes as they stand, and nothing attached:
   *  what a program run at paint time is handed, where attaching would
   *  be too late to mean anything, so `attach` on its nodes does nothing. */
  Scope snapshot() const;

  /** ONE ATTACHMENT, as the composer reads it back: which node it belongs
   *  to (`npos` for the scope itself) and the element. */
  struct Attachment {
    static constexpr size_t kScope = std::numeric_limits<size_t>::max();
    size_t owner = kScope;
    std::shared_ptr<void> element;  // an Element, held opaquely
  };
  /** @private the composer's side: the nodes to fill and the attachments
   *  to read back. */
  std::vector<Node>& mutableNodes();
  const std::vector<Attachment>& attachments() const { return m_attachments; }
  std::vector<Attachment>& mutableAttachments() { return m_attachments; }

 private:
  std::vector<Node> m_nodes;
  std::vector<Attachment> m_attachments;
};

/** WHAT ARRANGES: a value with `void arrange(Arrangement&) const`. */
template <typename O>
concept Arranging = requires(const O& o, Arrangement& a) {
  { o.arrange(a) };
};

/** WHAT ADDS: a value with `void add(Scope&) const`. */
template <typename O>
concept Adding = requires(const O& o, Scope& s) {
  { o.add(s) };
};

/** AN ARRANGING OPERATOR: what arranges, as a comparable value. Equality
 *  is the reconciler's question — equal values over an unchanged table
 *  did the same thing — so an operator is a VALUE whose equality is its
 *  parameters. A value that arranges but carries no equality is taken
 *  too, as the escape hatch that never prunes. */
template <typename O>
concept ArrangingOperator = Arranging<O> && std::equality_comparable<O>;

/** AN ADDING OPERATOR: what adds, as a comparable value — the same rule. */
template <typename O>
concept AddingOperator = Adding<O> && std::equality_comparable<O>;

/** Whether a scheme or an operator asks for the children's content
 *  minima, which cost a measure per text child: it says so with
 *  `static constexpr bool readsChildMinSizes = true;`. */
template <typename T>
concept ReadsChildMinSizes = requires {
  { T::readsChildMinSizes } -> std::convertible_to<bool>;
} && T::readsChildMinSizes;

namespace detail {

/** WHAT THE OPERATOR SEAM DOES, behind the erasure: one of the two
 *  calls, and which one it is. */
struct OperatorOperations {
  virtual ~OperatorOperations() = default;
  virtual bool arranges() const = 0;
  virtual void arrange(Arrangement&) const {}
  virtual void add(Scope&) const {}
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
  bool arranges() const override { return true; }
  void arrange(Arrangement& arrangement) const override {
    held.arrange(arrangement);
  }
};

/** An adding operator as those operations, under the same rule. */
template <Adding O>
struct AddingModel : OperatorOperations {
  O held;
  explicit AddingModel(O value) : held(std::move(value)) {}
  bool operator==(const AddingModel& other) const
    requires std::equality_comparable<O>
  {
    return held == other.held;
  }
  bool arranges() const override { return false; }
  void add(Scope& scope) const override { held.add(scope); }
};

/** The table a placement scheme reads, built from the arrangement's
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
  bool arranges() const override { return true; }
  void arrange(Arrangement& arrangement) const override {
    placeFromRects(arrangement, held.place(layoutInputOf(arrangement)));
  }
};

}  // namespace detail

/** THE OPERATOR AS A NODE HOLDS IT, type-erased: what `Element::operators`
 *  takes. Three constructions, one value: an arranging operator, an
 *  adding operator, or a placement scheme adapted to the arranging call.
 *  A comparable value prunes the node while the value and what it read
 *  are unchanged; a value with no equality never compares equal to a
 *  separately-built one, so its node re-runs the operator on every
 *  describe. Copies of ONE Operator share state and compare equal.
 *
 *  What an ADDING operator attaches takes the properties stated on the
 *  operator itself — `zIndex`, which is where its additions paint among
 *  the owner's children, and `styleClass`, the classes a sheet dresses
 *  them by — unless the element states its own.
 *
 *  Held as one shared immutable pointer, so a node carrying an operator
 *  list costs a pointer per entry. */
class Operator {
 public:
  Operator() = default;

  template <Arranging O>
    requires(!LayoutScheme<std::remove_cvref_t<O>> &&
             !Adding<std::remove_cvref_t<O>> &&
             !std::same_as<std::remove_cvref_t<O>, Operator>)
  Operator(O operatorValue)  // NOLINT: implicit by design (operators({Ring{…}}))
      : m_held(detail::ArrangingModel<std::remove_cvref_t<O>>(
            std::move(operatorValue))),
        m_readsChildMinSizes(ReadsChildMinSizes<std::remove_cvref_t<O>>) {}

  template <Adding O>
    requires(!Arranging<std::remove_cvref_t<O>> &&
             !LayoutScheme<std::remove_cvref_t<O>> &&
             !std::same_as<std::remove_cvref_t<O>, Operator>)
  Operator(O operatorValue)  // NOLINT: implicit by design (operators({connect::ByLane{…}}))
      : m_held(detail::AddingModel<std::remove_cvref_t<O>>(
            std::move(operatorValue))) {}

  template <LayoutScheme L>
    requires(!std::same_as<std::remove_cvref_t<L>, Operator>)
  Operator(L scheme)  // NOLINT: implicit by design (operators({layouts::Grid{…}}))
      : m_held(detail::SchemeModel<std::remove_cvref_t<L>>(std::move(scheme))),
        m_readsChildMinSizes(ReadsChildMinSizes<std::remove_cvref_t<L>>) {}

  explicit operator bool() const { return (bool)m_held; }
  /** Whether this operator places children (else it adds elements). */
  bool arranges() const { return m_held && m_held->arranges(); }
  /** Whether this operator adds elements. */
  bool adds() const { return m_held && !m_held->arranges(); }
  /** Runs an arranging operator over @p arrangement. */
  void arrange(Arrangement& arrangement) const {
    if (m_held) m_held->arrange(arrangement);
  }
  /** Runs an adding operator over @p scope. */
  void add(Scope& scope) const {
    if (m_held) m_held->add(scope);
  }
  /** Whether the held value asked for the children's content minima. */
  bool readsChildMinSizes() const { return m_readsChildMinSizes; }
  /** Does this value take part in structural equality? (False for a
   *  value with no equality of its own.) */
  bool comparable() const { return m_held.comparable(); }

  /** WHERE THE ADDITIONS PAINT among the owner's children — CSS
   *  `z-index` on every element this operator attaches that states none
   *  of its own. Negative paints them behind the authored children. */
  Operator& zIndex(int z) {
    m_zIndex = z;
    return *this;
  }
  /** THE CLASSES the additions are dressed by, laid under any the
   *  element names itself. */
  Operator& styleClass(std::string_view names) {
    m_classes = std::string(names);
    return *this;
  }
  std::optional<int> zIndexStated() const { return m_zIndex; }
  const std::string& classesStated() const { return m_classes; }

  bool operator==(const Operator& other) const {
    return m_held == other.m_held && m_zIndex == other.m_zIndex &&
           m_classes == other.m_classes;
  }

 private:
  core::Erased<detail::OperatorOperations> m_held;
  bool m_readsChildMinSizes = false;
  std::optional<int> m_zIndex;
  std::string m_classes;
};

}  // namespace sigil::compose
