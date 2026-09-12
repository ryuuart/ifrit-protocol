/** @file
 * The text engine's registry: one process-wide slot the typography tier
 * fills as it is linked in, and the read-back queries fall through to
 * for a passage that dresses nothing.
 */

#include <sigilcompose/core/TextPainter.h>

namespace sigil::compose::detail {

namespace {
const TextPainterOperations*& textEngineSlot() {
  static const TextPainterOperations* engine = nullptr;
  return engine;
}
}  // namespace

void registerTextEngine(const TextPainterOperations* engine) {
  textEngineSlot() = engine;
}

const TextPainterOperations* registeredTextEngine() { return textEngineSlot(); }

}  // namespace sigil::compose::detail
