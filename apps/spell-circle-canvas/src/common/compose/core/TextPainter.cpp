/** @file
 * The text engine's registry: one process-wide slot the typography tier
 * fills as it is linked in, and the read-back queries fall through to
 * for a passage that dresses nothing.
 */

#include <sigilcompose/core/TextPainter.h>

namespace sigil::compose::detail {

namespace {
const TextPainterOps*& textEngineSlot() {
  static const TextPainterOps* engine = nullptr;
  return engine;
}
}  // namespace

void registerTextEngine(const TextPainterOps* engine) {
  textEngineSlot() = engine;
}

const TextPainterOps* registeredTextEngine() { return textEngineSlot(); }

}  // namespace sigil::compose::detail
