#pragma once

/** @file
 * The one font context every weave test binary shapes with. Building a
 * system font manager enumerates the installed font set, so each process
 * constructs it exactly once and every test shares the result.
 */

#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

namespace sigil::weave::test {

/// The process-wide FontContext over the system font manager.
inline FontContext& sharedContext() {
  // systemFontManager() shares one enumerated font set process-wide.
  static auto* fontContext = new FontContext(ports::systemFontManager());
  return *fontContext;
}

}  // namespace sigil::weave::test
