#pragma once

/** @file
 * Internal to the kernel — the pixel bakes a draw allocates: the density
 * they are taken at, the budget that keeps them from becoming an
 * out-of-memory, the margin every device bake is allocated with, and the
 * scratch surfaces device bakes are drawn into.
 */

#include <include/core/SkRefCnt.h>
#include <include/core/SkSurface.h>

#include <algorithm>
#include <cstddef>
#include <vector>

namespace sigil::compose::detail {

/** THE PIXEL BAKES OF A DRAW: one part of the composer's retained state,
 *  read off it by each field's own name. */
// fields are grouped by what they belong to, not by size
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
struct BakeSurfaces {
  /** Composer::setBakeDensity: device pixels per layout unit every pixel
   *  bake is taken at, whatever the frame's matrix says. Zero is the
   *  coarse ladder read off that matrix. */
  float bakeDensity = 0.0f;
  // Pixel bakes are pixels, and a dense scene can carry many full-canvas
  // group bakes at several megabytes each. A budget, carried from the
  // previous frame (paint order is stable, so the previous frame's total is
  // the right question to ask before adding one more), keeps a bake from
  // becoming an out-of-memory.
  size_t bakedBytes = 0;      // accumulated during the current paint
  size_t bakedBytesLast = 0;  // what the previous frame ended up holding
  static constexpr size_t kBakeBudget = size_t{192} * 1024 * 1024;
  /** THE MARGIN EVERY DEVICE BAKE IS ALLOCATED WITH, in device pixels, for
   *  a bake whose larger side is @p extent.
   *
   *  A bake surface sized to exactly what the node paints puts that paint
   *  flush against the surface's own edge, and Skia does not rasterize a
   *  path that reaches its clip the way it rasterizes one standing clear of
   *  it: the decision is taken on the path's CONTROL-POINT bounds, and the
   *  two routes do not answer the same antialiased coverage. A stroked
   *  curve baked flush moves by TENS of code values along its whole length
   *  — not the one an integer device offset costs — so the margin is what
   *  makes a device bake paint the picture its live paint paints.
   *
   *  It is a FRACTION of the bake because the reach it has to cover is one:
   *  a stroker approximates an offset curve with cubics whose control
   *  points stand outside the ink they draw, by about a hundredth of the
   *  curve's own extent. A thirty-second is that with room, and the two are
   *  the floor for a bake too small for the fraction to reach a pixel. What
   *  it costs is a frame of transparent pixels around each bake. */
  static constexpr int bakeMargin(int extent) {
    return 2 + std::max(0, extent) / 32;
  }
  /** Reusable device-bake surfaces, one per nesting depth, sized to the
   *  largest requested rect. Baking on the canvas's own grid avoids an
   *  offset in the layer matrix. Nodes retain image snapshots rather than
   *  these scratch surfaces, so later bakes can reuse them. */
  std::vector<sk_sp<SkSurface>> bakeSurfaces;
  size_t bakeDepth = 0;  ///< how many bakes are in flight above this one
};

}  // namespace sigil::compose::detail
