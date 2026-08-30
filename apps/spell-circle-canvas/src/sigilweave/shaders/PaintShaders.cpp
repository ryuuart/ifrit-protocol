/** @file
 * The preset paints as SigilMaterial's text paint recipes: each call
 * builds the material for the run's bounds and the clock and shades it
 * through the Skia backend, so a caller can swap a paint's shader every
 * frame without touching text, shaping or layout.
 */

#include "sigilweave/shaders/PaintShaders.h"

#include <sigilmaterial/kit/TextPaint.h>
#include <sigilmaterial/skia/SkiaCompiler.h>

namespace sigil::weave::PaintShaders {

namespace {
sk_sp<SkShader> shade(const material::Material& m) {
  material::skia::install();
  return material::skia::shader(m, {});
}
}  // namespace

sk_sp<SkShader> water(const SkRect& bounds, float timeSeconds) {
  return shade(material::kit::water(bounds, timeSeconds));
}

sk_sp<SkShader> meshGradient(const SkRect& bounds, float timeSeconds) {
  return shade(material::kit::meshGradient(bounds, timeSeconds));
}

sk_sp<SkShader> sparkle(const SkRect& bounds, float timeSeconds) {
  return shade(material::kit::sparkle(bounds, timeSeconds));
}

sk_sp<SkShader> starNest(const SkRect& bounds, float timeSeconds) {
  return shade(material::kit::starNest(bounds, timeSeconds));
}

sk_sp<SkShader> clouds(const SkRect& bounds, float timeSeconds) {
  return shade(material::kit::clouds(bounds, timeSeconds));
}

sk_sp<SkShader> tunnel(const SkRect& bounds, float timeSeconds) {
  return shade(material::kit::tunnel(bounds, timeSeconds));
}

}  // namespace sigil::weave::PaintShaders
