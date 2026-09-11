#pragma once

/** @file The layer and per-unit data supplied when a paint resolves a pass. */

#include <include/core/SkRefCnt.h>
#include <include/core/SkShader.h>

#include <cstdint>

namespace sigil::material::skia {

/** Inputs to a recipe-backed paint's pass over a rendered layer. The body
 *  reads the layer through `uContent`, each unit's rectangle through
 *  `uUnitRect`, and its progress and seed through `uUnitPhase`.
 *  The shader's `kUnitCount` is the number of units, with a minimum of one.
 *  Each non-null buffer contains at least that many entries. Buffers are
 *  borrowed for the resolve call; the shader copies their data. */
struct PassInputs {
  sk_sp<SkShader> content;
  /** Four floats per unit: x, y, width and height, in shader-local pixels.
   *  Null leaves the rectangles at zero. */
  const float* rects = nullptr;
  /** Two floats per unit: progress in [0, 1], then a stable seed.
   *  Null leaves the progress and seeds at zero. */
  const float* phases = nullptr;
  uint32_t units = 0;
};

}  // namespace sigil::material::skia
