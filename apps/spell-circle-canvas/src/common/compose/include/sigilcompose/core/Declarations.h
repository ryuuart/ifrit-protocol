#pragma once

/** @file
 * @ingroup compose-core
 *
 * What every value that DECLARES a node holds: the copy-on-write handle
 * onto that node, and the one door the verb mixins reach it through.
 *
 * A verb mixin is one class template per family of properties, inherited
 * by every value that may state that family. Its verbs return the
 * DERIVED value by reference, so a chain keeps the type it started on
 * however many families it crosses.
 */

#include <sigilcompose/core/Property.h>

#include <memory>
#include <utility>

namespace sigil::compose {

class Element;

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
  /** The node to WRITE, cloned first where another value shares it. A
   *  property field on it is written only through the writer named for
   *  the property, which marks it stated. */
  template <class T>
  static ElementNode* declarations(T& value) {
    return value.m_node.operator->();
  }
  /** The node to READ, shared as it stands. */
  template <class T>
  static const std::shared_ptr<ElementNode>& node(const T& value) {
    return value.m_node.value;
  }
  /** One more child under the value's node, by the value's own rule for
   *  taking one, so a verb that adds a child adds it where `children()`
   *  does. */
  template <class T, class Child>
  static void append(T& value, Child&& child) {
    value.append(std::forward<Child>(child));
  }
};

/** WHAT EVERY VALUE THAT DECLARES A NODE IS: the handle, and nothing
 *  else. A description stays one shared pointer wide however many verb
 *  families the value inherits, and the kinds of node that carry verbs
 *  of their own share this one statement of what they hold. */
class Declaring {
 public:
  /** @private reconciler access */
  const std::shared_ptr<ElementNode>& node() const { return m_node.value; }

 protected:
  Declaring();  ///< A fresh node of its own.
  explicit Declaring(std::shared_ptr<ElementNode> n) : m_node(std::move(n)) {}

  /** One more child at the end, which `children()` and every verb that
   *  takes a node go through. */
  void append(Element child);

  friend struct NodeAccess;

  NodeHandle m_node;
};

}  // namespace detail

}  // namespace sigil::compose
