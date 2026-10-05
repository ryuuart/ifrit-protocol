#pragma once

/** @file
 * @ingroup world-node
 * A GROUP: the one kind of node that holds children, placing them all
 * where it stands. Its children are its argument — a braced list that
 * may mix every kind of node, and the lists `each()` makes.
 */

#include <sigilworld/advanced/Node.h>
#include <sigilworld/node/Verbs.h>

#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>

namespace sigil::world {

/** NODES UNDER ONE PLACEMENT. Everything under a group stands where the
 *  group's placement puts it and is hidden where the group is; nothing
 *  else a group says reaches its children. */
class Group : public Node,
              public PlacementVerbs<Group>,
              public IdentityVerbs<Group> {
 public:
  Group() = default;
};

/** ONE RUN OF A GROUP'S CHILDREN: a node of any kind, or a list of them
 *  — what `each()` makes — so one braced block mixes both. */
struct Children {
  std::vector<Node> nodes;
  template <class One>
    requires std::convertible_to<One&&, Node> &&
             (!std::ranges::range<std::remove_cvref_t<One>>)
  Children(One&& one) {  // NOLINT: implicit by design (a children block)
    nodes.push_back(Node(std::forward<One>(one)));
  }
  template <class Many>
    requires std::ranges::input_range<Many> &&
             std::convertible_to<std::ranges::range_reference_t<Many>, Node>
  Children(Many&& many) {  // NOLINT: implicit by design (a children block)
    for (auto&& one : many) nodes.push_back(Node(one));
  }
};

/** A GROUP OF @p children, in order: `group({mesh(body), pointLight()})`.
 *  A key on a child is what keeps it the same node across describes; an
 *  unkeyed child is matched by its place among its siblings. */
Group group(std::initializer_list<Children> children = {});

/** …and of every node @p range holds, in the range's order. */
template <std::ranges::input_range Range>
  requires std::convertible_to<std::ranges::range_reference_t<Range>, Node>
Group group(Range&& range) {
  return group({Children(std::forward<Range>(range))});
}

/** THE CHILDREN A RANGE DESCRIBES, one per item in the range's order, for
 *  a group's braced block: `group({each(posts, post), each(4, lantern)})`.
 *  @p make takes the item, or the item and its index. */
template <std::ranges::input_range Range, class Make>
[[nodiscard]] std::vector<Node> each(Range&& range, Make&& make) {
  std::vector<Node> out;
  std::size_t index = 0;
  for (auto&& item : range) {
    if constexpr (std::is_invocable_v<Make&, decltype(item), std::size_t>)
      out.push_back(Node(make(item, index)));
    else
      out.push_back(Node(make(item)));
    ++index;
  }
  return out;
}

/** THE CHILDREN A COUNT DESCRIBES, one per index from 0 to @p count — the
 *  ring of N posts whose only difference is where it stands. */
template <class Make>
  requires std::is_invocable_v<Make&, std::size_t> || std::is_invocable_v<Make&>
[[nodiscard]] std::vector<Node> each(std::size_t count, Make&& make) {
  std::vector<Node> out;
  out.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    if constexpr (std::is_invocable_v<Make&, std::size_t>)
      out.push_back(Node(make(index)));
    else
      out.push_back(Node(make()));
  }
  return out;
}

}  // namespace sigil::world
