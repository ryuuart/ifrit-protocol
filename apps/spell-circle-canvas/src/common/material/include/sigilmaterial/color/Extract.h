#pragma once

/** @file
 * THE PALETTE A PICTURE IS MADE OF: a run of pixels in, a fixed table of
 * the colours that stand for them out.
 *
 * Pixels rather than an image, because the algorithm has nothing to do
 * with how a picture was decoded and this leaf names no renderer. The
 * crossing from a decoded image is one loop over its pixels, and it lives
 * with the renderer that owns the image (`skia::palette`).
 */

#include <sigilmaterial/color/Color.h>

#include <cstdint>
#include <span>

namespace sigil::material {

/** HOW THE COLOURS ARE CHOSEN.
 *
 *  Both answer a table of the requested size and neither is random.
 *  `KMeans` moves the entries until each one is the average of the
 *  pixels closest to it, so its colours are the ones a picture actually
 *  holds and a small bright detail survives if enough pixels share it.
 *  `MedianCut` divides the colours into equal-population boxes and takes
 *  each box's average, so its entries are spread evenly over the range
 *  the picture covers whether or not the picture dwells there. Reach for
 *  the first when the table has to look like the picture and the second
 *  when it has to cover it. */
enum class PaletteMethod : uint8_t { KMeans, MedianCut };

/** WHAT A PALETTE IS ASKED FOR. */
struct PaletteOptions {
  /** How many entries the table holds. A picture with fewer distinct
   *  colours than this answers a shorter table rather than repeating
   *  one. */
  int entries = 5;
  PaletteMethod method = PaletteMethod::KMeans;
  /** How many times the entries are moved. Not read by `MedianCut`,
   *  which divides once. The walk stops early when nothing moves. */
  int iterations = 16;
  /** Read every nth pixel. A photograph's palette is the same to the eye
   *  at a stride of ten and ten times cheaper; a picture of a few flat
   *  colours must be read whole, since a stride can step over a colour
   *  entirely. */
  int stride = 1;
  /** Pixels at or below this alpha are not read at all. A cut-out's
   *  transparent surround is not one of its colours. */
  float minimumAlpha = 0.5f;
  /** Entries are answered darkest first, by OKLab lightness. An order
   *  the picture cannot decide is one the caller would have to impose
   *  anyway, and a stable one is what lets two runs be compared. */
  bool sortByLightness = true;

  bool operator==(const PaletteOptions&) const = default;
};

/** THE TABLE @p pixels IS MADE OF.
 *
 *  The work happens in OKLab, so "closest" means what an eye reports
 *  rather than what the sRGB numbers do — a table chosen by distance in
 *  stored numbers spends its entries on the dark end, where those
 *  numbers move fastest and the eye sees least.
 *
 *  DETERMINISTIC: the same pixels answer the same table, every run and
 *  every platform. The first entries are chosen by taking the pixel
 *  farthest from every entry chosen so far rather than at random, which
 *  is what removes the seed a k-means otherwise carries — and, since the
 *  starting entries are the extremes of the picture, what keeps a small
 *  vivid area from being averaged away before the first move. */
[[nodiscard]] Palette palette(std::span<const Color> pixels,
                              const PaletteOptions& options = {});

/** WHICH ENTRY OF @p palette IS CLOSEST to @p color, in OKLab. -1 for an
 *  empty palette.
 *
 *  The read an indexed picture is written through: a colour arrives, and
 *  the table says which of its entries stands for it. Pair it with a
 *  `Dither` to spread the error the choice makes across neighbouring
 *  pixels instead of banding on it. */
[[nodiscard]] int closestEntry(const Palette& palette, const Color& color);

}  // namespace sigil::material
