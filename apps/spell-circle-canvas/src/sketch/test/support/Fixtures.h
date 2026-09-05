#pragma once

/** @file
 * What every sketch test opens a session with: the one asset store a
 * process holds, beside the font context the whole tree shares.
 */

#include <sigilsketch/core/Assets.h>

#include "Fonts.h"

namespace sigil::sketch::test {

/** THE FONT CONTEXT, which is the tree's rather than this library's: a
 *  context memoises shaping and strike lookups, so one per process is
 *  what every test wants, and a sketch shapes through the same one a
 *  compose or a weave case does. */
using sigil::test::fonts;

/** The store a sketch that reaches for nothing is opened with: a root
 *  that names no directory, so every ask answers the placeholder.
 *
 *  Never destroyed. A store outlives every session opened over it, and a
 *  static torn down at exit would go while a session still standing
 *  there could reach it. */
inline Assets& assets() {
  static auto* store = new Assets("");
  return *store;
}

}  // namespace sigil::sketch::test
