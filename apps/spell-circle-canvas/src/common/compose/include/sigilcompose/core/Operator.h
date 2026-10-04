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
 * run in list order. Arrangers read the preceding placements; adders
 * read the authored nodes only.
 *
 * Stock `layouts::` values use the same arranging call.
 */

#include <sigilcompose/core/Attributes.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcore/comparable/Erased.h>
#include <sigilmotion/values/Tween.h>

#include <concepts>
#include <cstddef>
#include <glm/vec2.hpp>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace sigil::compose {

class Element;

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
 *  hit-tests, and is read by no operator. It can move nothing that was
 *  authored. */
class Scope {
 public:
  Scope() = default;
  /** Copies hold the measured nodes without attachments. Their nodes
   *  cannot attach to the source scope, even after that scope is gone. */
  Scope(const Scope& other);
  Scope& operator=(const Scope& other);
  Scope(Scope&& other) noexcept;
  Scope& operator=(Scope&& other) noexcept;

  /** ONE NODE IN THE SCOPE as an adding operator sees it. */
  class Node {
   public:
    std::string key;
    Attributes attributes;
    std::vector<std::string> classes;
    /// Where the node stands, in the scope's coordinates.
    geometry::path::Rect bounds;
    /// The outline the node resolved to — its shape, a routed path, or
    /// its box — in the scope's coordinates.
    geometry::path::Outline outline;

    template <typename T>
    std::optional<T> attribute(std::string_view name) const {
      return attributes.get<T>(name);
    }
    std::optional<float> number(std::string_view name) const;
    bool hasClass(std::string_view name) const;
    /** @p point in the scope's coordinates, moved into this node's own. */
    glm::vec2 toLocal(glm::vec2 point) const { return point - bounds.min; }
    /** @p outline in the scope's coordinates, moved into this node's own. */
    geometry::path::Outline toLocal(
        const geometry::path::Outline& outline) const;
    /** ATTACHES @p element TO THIS NODE, placed in the node's own
     *  coordinates: what is about this node alone, drawn with it. */
    void attach(Element element) const;

   private:
    friend class Scope;
    Scope* m_scope = nullptr;
    size_t m_index = 0;
  };

  /// The scope's own box, at the origin.
  geometry::path::Rect box;

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
  { o.arrange(a) } -> std::same_as<void>;
};

/** WHAT ADDS: a value with `void add(Scope&) const`. */
template <typename O>
concept Adding = requires(const O& o, Scope& s) {
  { o.add(s) } -> std::same_as<void>;
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

/** Whether a scheme or an operator gives each child a box of its own in
 *  which the child's percentages resolve — a grid's cell, as CSS resolves
 *  a grid item's against its grid area: it says so with
 *  `static constexpr bool resolvesChildPercentages = true;`, reads
 *  each child's `statedSize`, and answers the percentages in the
 *  rects it places. */
template <typename T>
concept ResolvesChildPercentages = requires {
  { T::resolvesChildPercentages } -> std::convertible_to<bool>;
} && T::resolvesChildPercentages;

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

}  // namespace detail

/** THE OPERATOR AS A NODE HOLDS IT, type-erased: what `Element::operators`
 *  takes: an arranging operator or an adding operator.
 *  A comparable value prunes the node while the value and what it read
 *  are unchanged; a value with no equality never compares equal to a
 *  separately-built one, so its node re-runs the operator on every
 *  describe. Copies of ONE Operator share state and compare equal.
 *
 *  What an ADDING operator attaches takes the properties stated on the
 *  operator itself — `zIndex`, which is where its additions paint among
 *  the owner's children, and `styleClass`, the classes a sheet dresses
 *  them by — unless the element states its own; and the identity verbs
 *  `hitTestable`, `cache`, `cacheScale` and `transition`, on every
 *  element that leaves that verb at a node's default.
 *
 *  Held as one shared immutable pointer, so a node carrying an operator
 *  list costs a pointer per entry. */
class Operator {
 public:
  Operator() = default;

  template <Arranging O>
    requires(!Adding<std::remove_cvref_t<O>> &&
             !std::same_as<std::remove_cvref_t<O>, Operator>)
  Operator(
      O operatorValue)  // NOLINT: implicit by design (operators({Ring{…}}))
      : m_held(detail::ArrangingModel<std::remove_cvref_t<O>>(
            std::move(operatorValue))),
        m_readsChildMinSizes(ReadsChildMinSizes<std::remove_cvref_t<O>>),
        m_resolvesChildPercentages(
            ResolvesChildPercentages<std::remove_cvref_t<O>>) {}

  template <Adding O>
    requires(!Arranging<std::remove_cvref_t<O>> &&
             !std::same_as<std::remove_cvref_t<O>, Operator>)
  Operator(O operatorValue)  // NOLINT: implicit by design
                             // (operators({connect::ByLane{…}}))
      : m_held(detail::AddingModel<std::remove_cvref_t<O>>(
            std::move(operatorValue))) {}

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
  /** Whether the held value resolves the children's percentages against
   *  boxes of its own. */
  bool resolvesChildPercentages() const { return m_resolvesChildPercentages; }
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
  /** WHETHER THE ADDITIONS ANSWER A HIT — `Element::hitTestable` on every
   *  element this operator attaches that leaves it at the default, so
   *  `.hitTestable(false)` lets the pointer through a whole operator's
   *  marks to what they dress. */
  Operator& hitTestable(bool enabled) {
    m_hitTestable = enabled;
    return *this;
  }
  /** WHAT THE PAINTER MAY KEEP of each addition — `Element::cache` on
   *  every element this operator attaches that leaves it at `Cache::Auto`. */
  Operator& cache(Cache mode) {
    m_cache = mode;
    return *this;
  }
  /** `Element::cacheScale` on every addition that leaves its bake at full
   *  resolution. */
  Operator& cacheScale(float factor) {
    m_cacheScale = factor;
    return *this;
  }
  /** HOW THE ADDITIONS' PLAIN CONSTANTS CHANGE — `Element::transition` on
   *  every element this operator attaches that states none of its own. */
  Operator& transition(motion::Tween<float> timing) {
    m_transition =
        std::make_shared<const motion::Tween<float>>(std::move(timing));
    return *this;
  }
  Operator& transition(motion::Duration duration) {
    return transition(motion::Tween<float>{.duration = duration});
  }
  std::optional<int> zIndexStated() const { return m_zIndex; }
  const std::string& classesStated() const { return m_classes; }
  std::optional<bool> hitTestableStated() const { return m_hitTestable; }
  std::optional<Cache> cacheStated() const { return m_cache; }
  std::optional<float> cacheScaleStated() const { return m_cacheScale; }
  const motion::Tween<float>* transitionStated() const {
    return m_transition.get();
  }

  /** The held value and every property stated on the operator. */
  bool operator==(const Operator& other) const;

 private:
  core::Erased<detail::OperatorOperations> m_held;
  bool m_readsChildMinSizes = false;
  bool m_resolvesChildPercentages = false;
  std::optional<int> m_zIndex;
  std::string m_classes;
  std::optional<bool> m_hitTestable;
  std::optional<Cache> m_cache;
  std::optional<float> m_cacheScale;
  // Boxed: a tween is several times the size of the rest of the operator,
  // and most operators state none.
  std::shared_ptr<const motion::Tween<float>> m_transition;
};

}  // namespace sigil::compose
