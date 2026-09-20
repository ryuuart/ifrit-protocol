#pragma once

/** @file
 * @ingroup material-pattern
 *
 * WOVEN CLOTH as one generator: a sett expanded into a threadcount, and
 * the threadcount read through a weave to say which thread is on top at
 * every crossing. The WARP runs down the loom and the WEFT across it,
 * and every check is that generator at another sett and weave. THE
 * THREADS ARE INDICES into the cloth's palette, which caps a cloth at
 * 256 shades. The reading answers a single crossing, so one thread and
 * a whole tile are read the same way.
 */

#include <include/core/SkImage.h>
#include <include/core/SkPoint.h>
#include <include/core/SkSize.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/pattern/Tile.h>

#include <compare>
#include <cstdint>
#include <vector>

namespace sigil::material::pattern {

/** ONE RUN OF A SETT: @p threads consecutive threads of one shade, named
 *  by its index into the cloth's palette. A run of no threads is skipped
 *  rather than refused, so a table may carry a zero. */
struct ThreadRun {
  int threads = 0;
  int shade = 0;

  constexpr auto operator<=>(const ThreadRun&) const = default;
};

/** WHETHER A RUN LIST IS THE WHOLE REPEAT OR HALF OF IT, since a
 *  threadcount is published both ways. `Asymmetric` expands the runs
 *  once; `Reflective` takes them as the HALF SETT and follows them with
 *  their mirror, so n threads become a repeat of 2n whose pivots are the
 *  gaps at the half's two ends.
 *  @trap Reading either as the other gives a cloth of half or twice the
 *  right size that still looks like the design, which is why the
 *  symmetry is asked for rather than inferred. */
enum class Symmetry : uint8_t { Asymmetric, Reflective };

/** The threadcount @p runs spell: one shade index per thread. */
std::vector<uint8_t> threadcount(const std::vector<ThreadRun>& runs,
                                 Symmetry symmetry = Symmetry::Asymmetric);

/** THE PIVOTS of a threadcount: every boundary the count reads the same
 *  in both directions from, taken round the repeat. A reflective sett
 *  has exactly two and they stand exactly half the repeat apart; an
 *  asymmetric one has none. Boundary b is the gap before thread b, so a
 *  pivot at 0 means the repeat mirrors about its own start. */
std::vector<int> pivots(const std::vector<uint8_t>& threads);

/** THE INTERLACING: how many ends the warp floats over, how many it
 *  passes under, and how far the pattern steps sideways from one pick to
 *  the next.
 *
 *  `plain()` is one over one with no step — the checkerboard every basic
 *  cloth is. A twill steps, and the step is what draws the diagonal:
 *  `advance` of one puts the rib on the "\" diagonal with y increasing
 *  downward, minus one puts it on "/", and a larger step lays it
 *  shallower. 2/2 is the tartan twill, 3/1 the warp-faced one a denim
 *  is, 1/3 its weft-faced reverse. */
struct Weave {
  int over = 2;     ///< warp ends the warp floats over, per pick
  int under = 2;    ///< ends it passes under
  int advance = 1;  ///< ends the interlacing steps per pick

  static constexpr Weave plain() { return {1, 1, 1}; }
  static constexpr Weave twill(int floatOver, int floatUnder, int step = 1) {
    return {floatOver, floatUnder, step};
  }
  /** Picks the interlacing repeats in, at least one. */
  constexpr int period() const {
    const int p = over + under;
    return p > 0 ? p : 1;
  }

  constexpr auto operator<=>(const Weave&) const = default;
};

/** Whether the WARP is on top where end @p end crosses pick @p pick.
 *  Negative coordinates wrap, so a window may be read anywhere. */
bool warpUp(Weave weave, int end, int pick);

/** A CLOTH: the two threadcounts, the shade card they index, the
 *  interlacing, and how far the picks where the weft is on top are
 *  darkened.
 *
 *  `rib` is that darkening as a fraction of the shade's own colour. It
 *  is what makes the interlacement legible inside a block of one colour,
 *  where warp and weft carry the same shade and the weave would
 *  otherwise be invisible; at zero the cloth is flat colour. */
struct Cloth {
  std::vector<uint8_t> warp;
  std::vector<uint8_t> weft;
  std::vector<Color> shades;
  Weave weave = Weave::twill(2, 2);
  float rib = 0.0f;

  /** The shade index on top at this crossing. Zero for an empty cloth. */
  uint8_t shadeAt(int end, int pick) const;
  /** That shade's colour, ribbed where the weft is on top. Fully
   *  transparent where the index names no shade. */
  Color at(int end, int pick) const;
};

/** THE REPEAT of @p cloth in threads. The interlacing steps sideways, so
 *  a cloth repeats along an axis only where both the threadcount and the
 *  weave's own period come round together — which is why a sett whose
 *  length is not a multiple of the weave's period tiles wider than the
 *  sett. */
SkISize clothRepeat(const Cloth& cloth);

/** A WINDOW OF THE CLOTH as pixels, one pixel per thread, taken at
 *  @p origin in thread coordinates. Null for a cloth with no threads or
 *  an empty window.
 *
 *  Magnify it by an INTEGER factor and sample it nearest: a thread
 *  narrower than a pixel, or a fractional magnification over the
 *  interlacing's own period, is a moire generator whatever the filter
 *  is. */
sk_sp<SkImage> clothImage(const Cloth& cloth, SkIPoint origin, SkISize size);

/** The cloth as a repeating tile at @p threadPx pixels per thread, one
 *  whole repeat baked, sampled nearest. */
Tile clothTile(const Cloth& cloth, float threadPx = 1.0f);

}  // namespace sigil::material::pattern
