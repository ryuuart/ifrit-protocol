#pragma once

/** @file
 * SigilCompose CORE — the shelf a sheet of boxes is packed onto.
 *
 * Several small drawings share one image, and each one needs to know which
 * rectangle of it is its own. That arithmetic is the same whether the boxes
 * are an `Atlas`'s baked cells, a set of pixel sprites or anything else a
 * caller wants on one surface, so it is a value here rather than a private
 * loop inside whichever component happened to need it first.
 *
 * ## The rule
 *
 * Boxes are laid left to right on a SHELF whose height is the tallest box
 * placed on it. When the next box would cross `maxWidth` the shelf closes
 * and a new one opens below it. This is the packing whose result a caller
 * can predict by reading its own list, which matters more here than
 * density: the rectangles come back PARALLEL to the boxes handed in, in
 * the caller's order, so a frame index is an index into both.
 *
 * A caller that wants the denser sheet sorts its own boxes by height
 * before packing — shelves waste the difference between the tallest box on
 * a shelf and every other one — and reads the rectangles back through its
 * own ordering. Sorting here would silently break the parallel the
 * consumers rely on.
 *
 * A box wider than `maxWidth` gets a shelf of its own and makes the sheet
 * wider than asked, rather than being dropped or clipped: losing a
 * drawing is worse than an oversized sheet, and the caller can see the
 * width it got.
 */

#include <include/core/SkRect.h>
#include <include/core/SkSize.h>

#include <span>
#include <vector>

namespace sigil::compose {

/** How a sheet is packed. */
struct ShelfOptions {
  /** The width a shelf wraps at, in sheet pixels. */
  float maxWidth = 2048.0f;
  /** Transparent pixels left between neighbours and around the sheet's
   *  edge. Zero packs boxes flush, which is right when every box is
   *  sampled with nearest and drawn at its own rectangle exactly. It is
   *  wrong the moment a stamp is scaled or lands off the pixel grid: a
   *  linear tap at a box's edge then reaches into the neighbour, and one
   *  sprite bleeds into another. */
  float padding = 0.0f;
};

/** Where each box landed, and how large the sheet holding them is. */
struct Shelved {
  /** Parallel to the boxes handed in: `cells[i]` is where box `i` sits, in
   *  sheet pixels. An empty box gets an empty rectangle. */
  std::vector<SkRect> cells;
  /** The sheet the rectangles are in, rounded up to whole pixels. */
  SkISize sheet{0, 0};
};

/** Lay @p boxes onto shelves and hand each one its rectangle. */
Shelved shelve(std::span<const SkSize> boxes, const ShelfOptions& options = {});

}  // namespace sigil::compose
