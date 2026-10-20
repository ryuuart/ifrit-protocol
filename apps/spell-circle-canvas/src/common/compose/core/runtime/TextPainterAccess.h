#pragma once

/** @file
 * Internal to the kernel — the text painter as the kernel reaches it: the
 * engine a text description installed, and the anchors and readings the
 * kernel resolves through it.
 */

#include "Instance.h"
#include "description/ComposeInternal.h"

namespace sigil::compose::detail {

/** THE TEXT PAINTER, AS THE KERNEL REACHES IT: the part of the composer
 *  that asks a text node's engine for what only the engine can resolve,
 *  reached through the composer by each operation's own name. */
struct TextPainterAccess {
  /** The engine a text description installed, or null for text the kernel
   *  draws at rest by itself. */
  static const TextPainterOperations* textPainterOf(const Instance& inst) {
    const ElementNode* node = inst.description.get();
    return node && node->textData ? node->textData->painter.get() : nullptr;
  }
  /** Resolves the node's textAttach() rects through its painter; a node with no
   *  painter anchors nothing. */
  void resolveTextMarks(Instance& inst) {
    if (const TextPainterOperations* painter = textPainterOf(inst))
      painter->marks(inst);
    else
      inst.textMarkRects.clear();
  }
  /** Lays out the node's textAnnotation() readings against the layout its
   * letters are drawn from. The engine answers even for a passage that dresses
   *  nothing else, because a reading IS the dressing and the base may
   *  carry no other. */
  void resolveTextAnnotations(Instance& inst) {
    inst.textAnnotations.clear();
    if (!inst.description || !inst.description->textData ||
        inst.description->textData->annotations.empty())
      return;
    const TextPainterOperations* painter = textPainterOf(inst);
    if (!painter) painter = registeredTextEngine();
    if (painter) painter->annotations(inst);
  }
};

}  // namespace sigil::compose::detail
