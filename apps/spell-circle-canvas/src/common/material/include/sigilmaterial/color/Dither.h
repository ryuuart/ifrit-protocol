#pragma once

/** @file
 * THE THRESHOLD A PIXEL IS ROUNDED AGAINST: one dither value, read at a
 * pixel, answering whether a level is crossed there.
 *
 * Rounding every pixel the same way is what makes a shallow ramp band. A
 * dither varies the rounding across the picture instead, so the average
 * over a neighbourhood is the value that was asked for and the error is
 * a texture rather than a step. The two ways of varying it are the
 * `kind`: a repeating matrix, which is stable under motion and reads as a
 * weave, and a screen-space noise, which has no visible period and reads
 * as grain.
 */

#include <sigilmaterial/color/Color.h>

#include <cstdint>

namespace sigil::material {

/** WHERE THE THRESHOLD PATTERN COMES FROM. */
enum class DitherKind : uint8_t {
  /** The recursive Bayer matrix, tiled. Every threshold in the matrix
   *  appears exactly once per tile, so a flat area is exactly the right
   *  average and the pattern is the same on every frame — which is what
   *  makes it hold still under a moving picture, and what makes its
   *  cross-hatch visible on a large flat. */
  Ordered,
  /** A hash of the pixel coordinates, with no period to see. Cheaper to
   *  read than a matrix and never lines up with the picture's own grid;
   *  its flats are noisier than the matrix's for the same level count,
   *  because nothing guarantees each threshold appears once. */
  Noise,
};

/** THE DITHER: where the threshold comes from, how coarse the matrix is,
 *  how many levels the answer is rounded to, and how far the rounding is
 *  allowed to move.
 *
 *  It is read AT A PIXEL, in device pixels, and holds no state, so the
 *  same value serves a CPU pass over a bitmap, a per-instance choice and
 *  a shader that spells the same arithmetic. */
struct Dither {
  DitherKind kind = DitherKind::Ordered;
  /** The Bayer matrix side, in cells: rounded down to a power of two and
   *  held between 2 and 16. Larger resolves more levels of grey and
   *  shows a coarser weave. Not read by `Noise`. */
  int matrix = 4;
  /** How many values a channel is rounded to, at least two. Two is the
   *  one-bit picture; a 256-colour target's channel is six. */
  int levels = 2;
  /** How much of one level's step the threshold spans. 1 is the full
   *  dither, 0 is plain rounding with no pattern at all, and between
   *  them is the partial dither a subtle grain wants. */
  float amount = 1.0f;

  bool operator==(const Dither&) const = default;

  /** The threshold at a pixel, in [0, 1). Ordered and noise both answer
   *  a value whose average over an area is one half. */
  [[nodiscard]] float threshold(int x, int y) const;

  /** @p color rounded to `levels` per channel through the threshold at
   *  this pixel. Alpha is carried through untouched: a dither decides
   *  what colour a pixel takes, not whether it is there. */
  [[nodiscard]] Color at(const Color& color, int x, int y) const;

  /** THE ONE-BIT READ: whether @p value, in [0, 1], survives the
   *  threshold here. The mask a stipple, a screen-door fade and a
   *  halftone coverage are drawn from, where the answer wanted is a
   *  yes or a no rather than a colour. */
  [[nodiscard]] bool on(float value, int x, int y) const;
};

}  // namespace sigil::material
