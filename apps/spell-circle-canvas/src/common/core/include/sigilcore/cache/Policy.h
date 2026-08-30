#pragma once

/** @file
 * The cache policy an author states on one node: the three answers a
 * proof cannot work out for itself.
 */

#include <cstdint>

namespace sigil::core {

/** WHAT AN AUTHOR ASKED OF ONE NODE'S CACHE.
 *
 *  Three values, because there are only three things an author can know
 *  that the proof cannot:
 *
 *   - **Auto** — nothing. The proof decides, and it is right whenever the
 *     host reports its volatility honestly.
 *   - **Always** — this node is worth a bake even when the proof's cost
 *     heuristics would decline one. It never overrules the proof's
 *     CORRECTNESS half: a node the proof calls volatile still paints live.
 *   - **Never** — this node draws something no declaration can see. A
 *     program that reads the clock, a source that changes behind the
 *     host's back. The proof cannot discover it, so the author states it,
 *     and the node counts as volatile forever.
 *
 *  WHAT IS DELIBERATELY NOT HERE IS THE ARTEFACT. Whether a bake is a
 *  recorded command list, a rasterized image, or a whole subtree
 *  composited into one layer is the host's own vocabulary: the three cost
 *  radically different amounts, replay under different rules, and go stale
 *  for different reasons. A host that offers several names one of these
 *  three policies and keeps its tier beside it. */
enum class Cache : uint8_t {
  Auto,    ///< the proof decides
  Always,  ///< bake whenever the proof permits one
  Never,   ///< opt out; the node is volatile forever
};

}  // namespace sigil::core
