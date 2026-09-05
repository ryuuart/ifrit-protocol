#pragma once

/** @file
 * A shader whose colour says where on the canvas it was sampled, so a
 * case can tell a real shader from a flat fill that happens to match one
 * of its stops.
 */

#include <include/core/SkColor.h>
#include <include/core/SkPoint.h>
#include <include/core/SkShader.h>
#include <include/core/SkTileMode.h>
#include <include/effects/SkGradient.h>

namespace sigil::weave::test {

/// A gradient running along x from `left` to `right`, clamped past both
/// ends. Give it the extent of the text and both stops land on glyphs.
inline sk_sp<SkShader> horizontalGradient(float left, float right,
                                          SkColor from, SkColor to) {
  const SkPoint ends[2] = {{left, 0}, {right, 0}};
  const SkColor4f colors[2] = {SkColor4f::FromColor(from),
                               SkColor4f::FromColor(to)};
  return SkShaders::LinearGradient(
      ends, SkGradient(SkGradient::Colors({colors, 2}, SkTileMode::kClamp),
                       SkGradient::Interpolation()));
}

}  // namespace sigil::weave::test
