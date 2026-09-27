/** @file
 * A fill whose material states a lit surface, shaded under the lighting
 * in force: made once from the two and kept while neither moves, so the
 * colours beneath the lighting pass are lowered once.
 */

#include <sigilmaterial/skia/Lit.h>

#include "Instance.h"

namespace sigil::compose::detail {

const material::Paint* Instance::litFillOf(const MaterialData& slot) const {
  if (!slot.surfaced || !lighting) return nullptr;
  const material::Material* from = &*slot.surfaced;
  if (!litFill || litFrom != from || litUnder != lighting.get()) {
    const material::Lighting under =
        material::skia::lightingFor(*from, *lighting);
    litFill = under ? material::skia::lit(*from, under)
                    : material::skia::paint(*from);
    litFrom = from;
    litUnder = lighting.get();
  }
  return &*litFill;
}

}  // namespace sigil::compose::detail
