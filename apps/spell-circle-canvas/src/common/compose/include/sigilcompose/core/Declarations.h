#pragma once

/** @file
 * @ingroup compose-core
 *
 * What every value that DECLARES a node holds: the copy-on-write handle
 * onto that node, and the one door the verb mixins reach it through.
 */

#include <memory>

namespace sigil::compose {

namespace detail {
struct ElementNode;

/** COPY-ON-WRITE HANDLE ONTO ONE NODE. A description stays a cheap
 *  value, and a fluent call on a copy can never reach the node another
 *  copy — or a description the composer retained — is still reading:
 *  writing through a handle whose node is shared clones it first, and
 *  reading never clones. */
struct NodeHandle {
  explicit NodeHandle(std::shared_ptr<ElementNode> node)
      : value(std::move(node)) {}

  ElementNode* operator->();
  const ElementNode* operator->() const;

  std::shared_ptr<ElementNode> value;
};

/** THE ONE DOOR A VERB MIXIN REACHES ITS NODE THROUGH. A mixin carries
 *  no state — the handle belongs to the value that inherits it — so a
 *  declaring value grants this access and nothing else. */
struct NodeAccess {
  /** The node to WRITE, cloned first where another value shares it. */
  template <class T>
  static ElementNode* declarations(T& value) {
    return value.m_node.operator->();
  }
  /** The node to READ, shared as it stands. */
  template <class T>
  static const std::shared_ptr<ElementNode>& node(const T& value) {
    return value.m_node.value;
  }
};

}  // namespace detail

/** THE VERB MIXINS: one class template per family of properties, each
 *  inherited by every value that may state that family. A verb returns
 *  the DERIVED value by reference, so a chain keeps the type it started
 *  on however many families it crosses, and a family is added to a value
 *  by naming its mixin rather than by copying its verbs.
 *
 *  A mixin is empty: every verb writes the node the derived value holds,
 *  reached through `detail::NodeAccess`. */

}  // namespace sigil::compose
