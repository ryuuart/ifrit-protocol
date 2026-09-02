#pragma once

/** @file
 * What a sketch declares about the surface it wants: how big, what is
 * behind it, and the moment a still of it is worth taking.
 */

#include <include/core/SkColor.h>
#include <include/core/SkSize.h>

namespace sigil::sketch {

/** THE CANVAS A HOST REALIZES for a sketch.
 *
 *  A sketch declares this from inside its own setup, the way
 *  `createCanvas` does, so it is only truthful once setup has run — a
 *  host reads it back afterwards rather than before.
 *
 *  `captureSeconds` is the scene time at which a STILL of this sketch is
 *  representative: the moment the piece is most itself. Negative means
 *  the sketch states no preference and a sweep falls back to its own
 *  derived frame, which is deterministic but lands at whatever time the
 *  sweep's phases happen to add up to — a time no sketch was authored
 *  around. Declare it on anything that cycles through visually distinct
 *  states, because a still of an animation is a claim about that
 *  animation, and a wrongly-chosen frame is indistinguishable from a
 *  wrongly-rendered one when all you have is the still.
 *
 *  `oversample` is how many device pixels of a plate the sketch asks for
 *  per canvas pixel, and a host honours the number EXACTLY rather than
 *  fitting it to a pixel budget. Zero means the sketch asks for nothing
 *  and the host picks, which is the ordinary case: a still is worth
 *  taking larger than its canvas, but only up to a width past which a
 *  plate is no longer a thing anyone can look at, so the host scales by
 *  whatever fraction that width allows.
 *
 *  A FRACTION IS WHAT A PIXEL-EXACT RECONSTRUCTION CANNOT HAVE. Such a
 *  sketch draws one pixel of its subject as a whole number of canvas
 *  pixels, and it is checked by downsampling the plate by that whole
 *  number and laying the result over the reference — which needs every
 *  source pixel to cover the same number of device pixels. At 1.875 a
 *  four-canvas-pixel square covers seven device pixels in one column and
 *  eight in the next, and no downsample recovers the reference from
 *  that. Declaring an integer is how such a sketch keeps its plate on
 *  its own grid, and it is the reason the number outranks the width. */
struct CanvasSpec {
  SkSize size = {900, 640};
  SkColor4f background = {0.043f, 0.039f, 0.078f, 1};
  double captureSeconds = -1.0;
  int oversample = 0;
};

}  // namespace sigil::sketch
