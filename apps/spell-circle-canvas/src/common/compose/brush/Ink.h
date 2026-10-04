#pragma once

/** @file
 * THE INK A MARK IS PAINTED IN, laid on the executor's paint: a colour
 * as the paint's colour, a material as the shader it lowered to.
 * Internal to the brush tier: a mark's public face is its material.
 */

#include <include/core/SkPaint.h>
#include <sigilcompose/core/Paint.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>

#include "paint/FillLowering.h"

namespace sigil::compose::detail {

/** Lays @p resolved on @p paint. False where the ink paints nothing — no
 *  fill, or a colour with no alpha — so a mark can skip its drawing. */
inline bool layInk(SkPaint& paint, const Fill& resolved) {
  if (resolved.kind == Fill::Kind::Color) {
    if (resolved.colorValue.a <= 0.0f) return false;
    paint.setColor4f(material::skia::toSkColor(resolved.colorValue), nullptr);
    return true;
  }
  if (resolved.kind == Fill::Kind::Paint) {
    paint.setShader(material::skia::staticShader(paintOf(resolved)));
    return true;
  }
  return false;
}

}  // namespace sigil::compose::detail
