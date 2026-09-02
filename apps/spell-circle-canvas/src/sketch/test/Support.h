#pragma once

/** @file
 * What every sketch test opens a session with: the one font context and
 * the one asset store a process holds.
 */

#include <sigilsketch/core/Assets.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

namespace sigil::sketch::test {

/** THE ONE FONT CONTEXT THIS PROCESS HOLDS, over the system fonts.
 *
 *  It is never destroyed. A context outlives everything shaped through
 *  it, and a static torn down at exit would go while a session still
 *  standing there could reach it. */
inline weave::FontContext& fonts() {
  static auto* context =
      new weave::FontContext(weave::ports::systemFontManager());
  return *context;
}

/** The store a sketch that reaches for nothing is opened with: a root
 *  that names no directory, so every ask answers the placeholder. Never
 *  destroyed, for the same reason the font context is not. */
inline Assets& assets() {
  static auto* store = new Assets("");
  return *store;
}

}  // namespace sigil::sketch::test
