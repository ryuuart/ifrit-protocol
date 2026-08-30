/** @file
 * The describe-time ambient stack behind `env::`: the thread-local snapshot,
 * its equality, and the scope guard that swaps it around a deferred call.
 */

#include <include/core/SkContourMeasure.h>
#include <include/core/SkImageFilter.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathUtils.h>
#include <include/core/SkShader.h>
#include <include/core/SkTypes.h>  // SkDebugf — the slot-rename diagnostic
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <include/pathops/SkPathOps.h>

#include <algorithm>
#include <cmath>   // std::isfinite — the profileOffset non-finite guard
#include <cstdio>  // std::snprintf — variationDrive's effect key
#include <set>

#include "ComposeInternal.h"
#include "sigilgeometry/path/Contour.h"

namespace sigil::compose {

namespace detail {

// ---- env: the describe-time ambient stack (see Compose.h "env") ----------

EnvSnapshot& envStack() {
  static thread_local EnvSnapshot stack;
  return stack;
}

bool envEqual(const EnvSnapshot& a, const EnvSnapshot& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (a[i].type != b[i].type) return false;
    if (a[i].value == b[i].value)
      continue;  // the same binding object: equal without asking
    if (!a[i].equal || !a[i].value || !b[i].value) return false;
    if (!a[i].equal(a[i].value.get(), b[i].value.get())) return false;
  }
  return true;
}

EnvRestore::EnvRestore(const EnvSnapshot& snapshot) {
  EnvSnapshot next = snapshot;  // copied first: `snapshot` may alias the stack
  m_saved = std::move(envStack());
  envStack() = std::move(next);
}

EnvRestore::~EnvRestore() { envStack() = std::move(m_saved); }

}  // namespace detail

}  // namespace sigil::compose
