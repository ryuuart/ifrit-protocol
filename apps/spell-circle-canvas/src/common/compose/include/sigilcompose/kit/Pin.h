#pragma once

/** @file
 * @ingroup compose-kit
 *
 * THE PINNING OPERATOR: an adding operator that hangs an element off
 * every node stating a request under a lane — a callout beside a card, a
 * tooltip over a port, a label under a station — placed by the same
 * `Tether` arithmetic a box hung by hand is placed by, with the same
 * list of places to try when the first will not fit. The node states
 * what to hang and where; the operator hangs it.
 */

#include <include/core/SkSize.h>
#include <sigilcompose/core/Derive.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Operator.h>

#include <string>
#include <string_view>
#include <vector>

namespace sigil::compose::pin {

/** WHAT A NODE ASKS TO HAVE HUNG OFF IT: the element, the box it is
 *  given, and where it hangs.
 *
 *      card.attribute("callout", pin::Request{
 *          .element = text(u8"failing health check").styleClass("callout"),
 *          .size = {140, 24},
 *          .where = {.on = {1, 0.5f}, .at = {0, 0.5f}, .offset = {8, 0},
 *                    .fallbacks = {{.on = {0, 0.5f}, .at = {1, 0.5f},
 *                                   .offset = {-8, 0}}}}});
 *
 *  `where` is read as `Element::tether` reads a `Tether`, except that
 *  its `key`, and every fallback's, is ignored: the node stating the
 *  request IS the anchor. `within` empty is the scope's box, which is
 *  what "on screen" means here. THE SIZE IS STATED, because the element
 *  is placed before it is laid out and a fit is a question about a box;
 *  what is in it is laid out inside that box.
 *
 *  Two requests compare by their tether, their size and the description
 *  of their element, so a card that states the same callout every frame
 *  prunes. */
struct Request {
  Element element;
  SkSize size = {0, 0};
  Tether where;
  bool operator==(const Request& other) const {
    return size == other.size && where == other.where &&
           sameDescription(element, other.element);
  }
};

/** AN ELEMENT HUNG OFF EVERY NODE STATING A REQUEST under `lane`. The
 *  element is attached to the scope, keyed `<node key>-pin`, so it
 *  stands among the scope's children — above the node it labels, and
 *  free to reach past that node's box. */
struct ByLane {
  std::string lane;
  bool operator==(const ByLane&) const = default;

  void add(Scope& scope) const;
};

}  // namespace sigil::compose::pin
