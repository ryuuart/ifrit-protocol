#pragma once

/** @file
 * @ingroup compose-core
 *
 * What a node IS rather than how it looks: the sheets and names it
 * resolves its cascade through, what it hangs off, who it is across
 * describes, what the painter may keep of it, and what is under it.
 */

#include <sigilcompose/core/Attributes.h>
#include <sigilcompose/core/Declarations.h>
#include <sigilcompose/core/Layout.h>  // Cache
#include <sigilcompose/core/Operator.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilmotion/schedule/Spread.h>
#include <sigilmotion/values/Transition.h>
#include <sigilweave/layout/StyleSheet.h>

#include <chrono>
#include <concepts>
#include <initializer_list>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::compose {

class Element;
// <sigilcompose/core/Derive.h>: what a node hangs off, declared beside
// the rest of the derive family because it is resolved by the same pass.
struct Tether;
// One run of a children({…}) block, defined at the foot of Element.h,
// where the block verb that takes it can be read beside it.
struct Children;

/** WHAT NO OTHER VALUE COULD STATE ABOUT A NODE. Everything here is
 *  about this node's place in the tree and in the cascade rather than
 *  about a property, so none of it is something a rule could restate or
 *  a descendant inherit. */
template <class Derived>
class StructureVerbs {
 public:
  /** @name The cascade a node NAMES
   *  The sheet this node and everything under it resolve their classes
   *  through, the semantic role that stands under those classes, and
   *  the classes themselves. What a node DECLARES to its descendants —
   *  the font, the block, the ink, the custom properties and the
   *  sampling — is the cascade mixin's.
   *  @{ */
  /** THE SHEET this node and everything under it resolve their classes
   *  through: rules under names, each a type half and a block half,
   *  stated on any node and inherited down the tree as the font is, a
   *  nearer sheet's rules standing over a farther one's by name. A sheet
   *  is a value on the description, so a subtree carries its own and
   *  nothing is bound around the code that builds it. */
  Derived& styleSheet(sigil::weave::StyleSheet sheet);
  /** APPLIES @p sheet to this node and everything under it: a value
   *  declared once, whose rules speak about the elements their
   *  selectors name rather than about a class by name. Calling this
   *  again applies another sheet, later in order; nothing is removed,
   *  because a tree that should stop applying one is described without
   *  it. THE SHEET SEES ONLY THIS SUBTREE: every compound of a
   *  selector — the subject, and every ancestor or sibling it names —
   *  must match this node or one below it, so a sheet that must name
   *  an outer element is applied at or above that element. This node
   *  is the root of what the sheet sees, so it matches `:root` and
   *  stands as the only child of nothing for these rules. */
  Derived& applyStyleSheet(StyleSheet sheet);
  /** A SEMANTIC ROLE with default typography. The rule's name selects a
   *  rule from the sheet where this node lands; that rule overrides these
   *  defaults, ordinary classes override the role, and the node's own
   *  font and block override both. Unstated fields inherit. A sheet need
   *  not carry the role: the defaults make a component useful on its own.
   *  A later call replaces the role and its defaults together. */
  Derived& role(sigil::weave::Rule defaults);
  /** A semantic role with no default fields, styled by the sheet in force. */
  Derived& role(std::string name);
  /** CLASSES: the partials the sheets in force register under each name
   *  in @p names — several, separated by spaces, as CSS's class attribute
   *  lists them, folded in left to right — resolved by the cascade pass
   *  where the element LANDS, and laid under the node's own `font()` and
   *  `block()`, as an inline style stands over a class. The fields a class
   *  sets then inherit down the tree. A name neither sheet in force
   *  carries warns once and sets nothing. */
  Derived& styleClass(std::string_view names);
  /** @} */

  /** @name Facts and operators
   *  What the node states about itself for whoever reads it, and what
   *  it runs over its own children.
   *  @{ */
  /** A TYPED FACT THIS NODE STATES: @p value under @p name, for whatever
   *  reads it — the scheme or operator on the parent placing this node
   *  by its tier, an operator building something from it — and inert
   *  where nothing does. The value is read back with the type it was
   *  written in; a string literal is stored as a `std::string`. A later
   *  fact under the same name replaces the earlier one. */
  template <AttributeValue T>
  Derived& attribute(std::string_view name, T value) {
    Attributes one;
    one.set(name, std::move(value));
    return attributes(std::move(one));
  }
  /** Every fact of @p facts laid over the node's own, same names replaced. */
  Derived& attributes(Attributes facts);
  /** THE OPERATORS THIS NODE RUNS OVER ITS CHILDREN, in list order: each
   *  is handed the children measured, with their facts and where the
   *  operators before it left them, and places or turns them. A scheme
   *  of the older `place(LayoutInput)` shape is an operator too. A later
   *  call appends to the list. What the operators do runs inside the
   *  layout's converging rounds, so what they place is what every pass
   *  after layout reads. */
  Derived& operators(std::vector<Operator> list);
  /** @} */

  /** HANG THIS NODE OFF A KEYED ONE, at a stated pair of points, with a
   *  list of places to try when the first will not fit. It takes the
   *  node out of the flow, and where it lands is an answer of the
   *  layout: resolved against the geometry the anchor resolved to, and
   *  re-resolved whenever that moves. A later call replaces the tether. */
  Derived& tether(Tether t);

  /** @name Identity, caching, transitions
   *  Who the node is across describes, whether it answers a hit, what
   *  the painter is allowed to keep of it, and how its plain constants
   *  change.
   *  @{ */
  /** Takes this node OUT of hit testing — CSS `pointer-events: none`.
   *  Its CHILDREN are still tested: this excludes the node's own box,
   *  not its subtree. A keyed, full-bleed shell with no fill otherwise
   *  swallows every hit in the frame, silently. */
  Derived& hitTestable(bool enabled);
  /** The author-owned identity: what the reconciler matches a child by
   *  across describes, what `Composer::bounds` and `hitTest` answer
   *  for, and what a connector, a rail or a `spans::fit` borrows
   *  geometry by. On a `slot()` it RENAMES the mount, and warns. */
  Derived& key(std::string_view k);
  /** WHAT THE PAINTER MAY KEEP OF THIS SUBTREE — record it as a picture,
   *  bake it to a texture, bake it with its children, or nothing at all.
   *  `Cache::Auto` when unstated, which records provably-static subtrees
   *  and leaves the rest alone. A per-frame paint program that reads the
   *  clock MUST state `Cache::None`, because nothing can see that it
   *  sampled the time. See `Cache` for what each mode costs and refuses.
   *
   *  @see sigil::compose::Cache */
  Derived& cache(Cache c);
  /** Texture-bake resolution multiplier, `Cache::Texture` only and
   *  clamped to 0.1–1: the bake rasterizes at @p factor times the
   *  device scale and every blit scales it back up. Almost always the
   *  wrong lever — it cheapens what happens once and taxes what happens
   *  forever. */
  Derived& cacheScale(float factor);
  /** HOW THIS NODE'S PLAIN CONSTANTS CHANGE when a later describe gives
   *  them a new value: the duration, easing and delay that every
   *  animatable lane on the node — its transforms, its opacity, its
   *  mask gates, its fx progresses, its fill — is retargeted over. None
   *  when unstated, so a new constant lands on the frame it arrives. A
   *  value that already carries its own `animate(...)` keeps that one;
   *  this is the node's default for the ones that do not. */
  Derived& transition(motion::Transition t);
  /** Container stagger: child i's subtree enters with an EXTRA
   *  order-times-each delay on every `animate()` mount transition under
   *  it, compounding through nested staggered containers. @p from picks
   *  the origin — declaration order, last child first (a bottom-up
   *  cascade that leaves the paint order alone), or outward from the
   *  centre. One call, and no per-child delay arithmetic. */
  Derived& staggerChildren(
      std::chrono::milliseconds each,
      motion::Spread::From from = motion::Spread::From::Start);
  /** @} */

  /** @name Composition
   *  What is under the node — written last, after every verb that says
   *  what is done to the node itself.
   *  @{ */
  /** THE CHILDREN, AS ONE BLOCK: what is in the node, in order. A run
   *  of the block is an element or the list `each()` made from a range,
   *  so a block mixes the two. A later call appends. */
  Derived& children(std::initializer_list<Children> runs);
  /** THE CHILDREN FROM A RANGE, appended in the range's own order — for
   *  a container whose whole content is a collection, where the braced
   *  block would hold one `each()` and nothing else. */
  template <std::ranges::input_range R>
    requires std::convertible_to<std::ranges::range_value_t<R>, Element>
  Derived& children(R&& range) {
    for (auto&& e : range)
      detail::NodeAccess::append(self(), Element(std::move(e)));
    return self();
  }
  /** @} */

 private:
  Derived& self() { return static_cast<Derived&>(*this); }
  detail::ElementNode* declarations() {
    return detail::NodeAccess::declarations(self());
  }
};

}  // namespace sigil::compose
