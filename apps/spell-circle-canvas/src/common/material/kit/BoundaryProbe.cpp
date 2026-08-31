/** @file
 * The kit boundary's NEGATIVE CONTROL. Built on purpose, expected to FAIL.
 *
 * The kit holds presets — bodies as shader text and the values that
 * parameterise them — and reaches no renderer: what compiles a body is the
 * business of whichever backend a consumer installed. Here that boundary is
 * a LINK boundary rather than an include one, because every material header
 * lives under one public include root and the core keeps no private header:
 * the Skia backend's declaration is visible from anywhere in the library and
 * only its definition is out of reach. So this TU calls the backend's
 * registration while linking the kit alone, and the link must not resolve.
 *
 * The `material_kit_boundary_probe` test builds this target and requires an
 * undefined `sigil::material::skia::install` in the output.
 */

#include <sigilmaterial/skia/SkiaCompiler.h>

int main() {
  sigil::material::skia::install();
  return 0;
}
