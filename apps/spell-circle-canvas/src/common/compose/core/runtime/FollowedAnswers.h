#pragma once

/** @file
 * Internal to the kernel — the answers a query gives a memo's describe that
 * the describe follows: a keyed node it asked about before the node was in
 * the tree, and where a node it asked about landed. Each is a followable
 * value the composer advances when the answer would change, so the memo is
 * described again with the new answer.
 */

#include <include/core/SkRect.h>
#include <sigilcore/follow/Follow.h>

#include <cstdint>

namespace sigil::compose::detail {

/** AN ANSWER THAT CHANGES ONCE: a key a describe asked about while no node
 *  answered to it, advanced when one first does. */
struct FollowedArrival : core::Followable {
  void arrived() { advance(); }
};

/** WHERE A NODE LANDED, as a describe that asked a placement query read it:
 *  its rect in the composer's space and the revisions of the text set in
 *  it. Advanced when a layout moves the node, resizes it or sets its text
 *  again — what every rect a placement query answers is read off. */
struct FollowedPlacement : core::Followable {
  /** Takes the node's placement now, advancing when it differs from the
   *  one held. Answers whether it advanced. */
  bool update(const SkRect& rect, uint32_t contentRevision,
              uint32_t measuredRevision) {
    if (known && rect == m_rect && contentRevision == m_contentRevision &&
        measuredRevision == m_measuredRevision)
      return false;
    known = true;
    m_rect = rect;
    m_contentRevision = contentRevision;
    m_measuredRevision = measuredRevision;
    advance();
    return true;
  }

  /** Whether a placement has been taken since the query made this. */
  bool known = false;

 private:
  SkRect m_rect = SkRect::MakeEmpty();
  uint32_t m_contentRevision = 0;
  uint32_t m_measuredRevision = 0;
};

}  // namespace sigil::compose::detail
