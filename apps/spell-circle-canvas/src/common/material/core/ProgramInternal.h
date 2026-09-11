#pragma once

#include <sigilmaterial/core/Program.h>

namespace sigil::material::detail {

/** Backend defaults fill an unassigned target without replacing an
 *  explicit compiler. Registration shares the registry's lock, so a
 *  concurrent explicit registration wins in either order. */
struct CompilerDefaults {
  static void registerIfAbsent(Target target, Compiler compiler);
};

}  // namespace sigil::material::detail
