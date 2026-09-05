#pragma once

/** @file
 * The one font context a test process shares.
 */

#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

namespace sigil::test {

/** THE PROCESS'S FONT CONTEXT.
 *
 *  A FontContext memoises shaping and strike lookups, so one per process
 *  is what a test wants: a second one re-shapes everything the first one
 *  already knows, and a per-case one turns a cache claim into a claim
 *  about a cold cache. Leaked on purpose — Skia's font manager outlives
 *  static destruction on some platforms, and a test process that has
 *  finished reporting has nothing left to free it for.
 *
 *  Any binary that includes this links the system font port, and every
 *  face it resolves is the machine's rather than a committed instrument:
 *  a case whose claim depends on WHICH face it got belongs behind the
 *  `fonts` label or on a committed instrument, not here.
 */
inline weave::FontContext& fonts() {
  static auto* context = new weave::FontContext(weave::ports::systemFontManager());
  return *context;
}

}  // namespace sigil::test
