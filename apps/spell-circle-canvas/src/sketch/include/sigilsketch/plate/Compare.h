#pragma once

/** @file
 * @ingroup sketch-plate
 *
 * TWO DIRECTORIES OF PLATES, PICTURE BY PICTURE.
 *
 * A byte-identity sweep needs no decoder — two plates are the same file
 * or they are not. Comparing two RENDERERS does: the same sketch drawn
 * on the CPU and on a device is the same picture within a tolerance and
 * never the same bytes, so the question is how far apart the two are.
 * That is a decode and an arithmetic over pixels, which this library
 * already carries, so it is answered here as a value rather than in
 * whatever script asks — and the caller keeps the tolerances, because
 * deciding what is close enough is a judgement about a machine and not a
 * fact about two files.
 */

#include <cstddef>
#include <string>
#include <vector>

namespace sigil::sketch {

/** Two plate directories: the first is the reference. */
struct CompareOptions {
  std::string first;
  std::string second;
};

/** WHAT ONE PLATE NAME FOUND in the two directories. Only a `Compared`
 *  plate carries distances; the other three say why none were taken. */
enum class PlateOutcome {
  Compared,
  /** In one directory and not the other. */
  Missing,
  /** Present in both, and one of them would not decode. */
  Unreadable,
  /** Both decoded, at two sizes: a plate that changed shape is a plate
   *  whose scene changed, so no distance is taken between them. */
  Resized,
};

/** Which of the two directories a `Missing` or `Unreadable` plate is
 *  answered for: the one it is missing from, or the one that would not
 *  decode. */
enum class PlateSide { First, Second };

/** ONE PLATE OF THE COMPARISON.
 *
 *  Every distance is an absolute difference of one 8-bit channel, in
 *  0..255, over every channel of every pixel. The worst is split THREE
 *  ways over the pixels it stands on, because a caller's tolerance can
 *  depend on which it is:
 *
 *  - `worstOverClear` — the FIRST plate, which is the reference, holds
 *    transparent black there. Nothing was composited under the difference
 *    at all.
 *  - `worstOverGraze` — the difference is CONFINED TO AN ANTIALIASED EDGE
 *    BOTH plates draw: the picture varies by at least the difference
 *    within a pixel of that point in each of them, and so does every
 *    differing pixel beside it. What changed is one pixel's coverage of an
 *    edge the two agree about, which is what a mark standing a fraction of
 *    a device pixel from where the other drew it looks like.
 *    `grazingPixels` counts them, because a worst on four pixels and a
 *    worst on four hundred thousand are different facts.
 *  - `worstOverContent` — everything else: a difference that reaches a
 *    pixel no edge explains, which is what a picture that MOVED shows —
 *    pixels taken off the edges, a mark that is gone, a wash at another
 *    value.
 *
 *  `worstPerComposite` is the content figure again, divided by how many
 *  CACHED RASTERS were blitted over each pixel and rounded up, with
 *  `stackedPixels` counting the content pixels that stood under more than
 *  one. A cached raster is a composite the picture beside it did not make
 *  and every composite rounds, so a difference of four under four of them
 *  is the same fact as a difference of one under one — and a caller whose
 *  tolerance is a bound per composite reads this rather than the content
 *  figure. The counts come from a plane the second directory carries
 *  beside its plates, written by a headless sweep asked for one; without
 *  it every pixel stands under one composite and the two figures agree.
 *
 *  Which pixels are which is a fact about two files; how much each is
 *  allowed is a judgement and stays with the caller. */
struct PlateComparison {
  /** The sketch's filed name: the file is `kPlatePrefix`, this, `.png`. */
  std::string name;
  PlateOutcome outcome = PlateOutcome::Compared;
  PlateSide side = PlateSide::First;
  /** Both plates' sizes, for a `Compared` or `Resized` plate. */
  int firstWidth = 0;
  int firstHeight = 0;
  int secondWidth = 0;
  int secondHeight = 0;
  double mean = 0;
  int p99 = 0;
  int worst = 0;
  int worstOverClear = 0;
  int worstOverContent = 0;
  int worstOverGraze = 0;
  std::size_t grazingPixels = 0;
  int worstPerComposite = 0;
  std::size_t stackedPixels = 0;
};

/** EVERY PLATE THAT STANDS IN EITHER DIRECTORY, in name order, or why
 *  nothing could be compared at all. */
struct Comparison {
  std::vector<PlateComparison> plates;
  /** Non-empty when a directory cannot be read or neither holds a plate;
   *  `plates` is then empty. */
  std::string refusal;

  /** 0 when every plate was compared, 1 when any was missing, unreadable
   *  or resized, and 2 when there was nothing to compare. */
  [[nodiscard]] int status() const;
};

/** Decodes and differences every plate of two directories. */
[[nodiscard]] Comparison compare(const CompareOptions& options);

/** ONE PLATE AS A LINE, opening with the word that says what it is, and
 *  with no newline:
 *
 *      compared <name> mean <mean> p99 <p99> max <worst>
 *               clear <worstOverClear> content <worstOverContent>
 *               graze <worstOverGraze> <grazingPixels>
 *               composited <worstPerComposite> <stackedPixels>
 *      size <name> <W>x<H> <W>x<H>
 *      missing <name> first|second
 *      unreadable <name> first|second
 *
 *  A filed name can carry spaces, so a reader that splits one reads the
 *  verb from the front and the fixed-width tail from the back. */
[[nodiscard]] std::string comparisonLine(const PlateComparison& plate);

/** Every plate's line on stdout, and the refusal on stderr when there is
 *  one. Returns the comparison's status. */
int printComparison(const Comparison& comparison);

}  // namespace sigil::sketch
