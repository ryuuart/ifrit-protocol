#pragma once

/** @file
 * SigilCompose kit — placers: arithmetic that fills an instancing::Pool with
 * a grid, a ring or a repeat chain. Data-level, O(count), no Yoga: each
 * writes only the lanes its parameters speak to and commits the pool, so a
 * pool filled by hand and then arranged here keeps its tints and frames.
 * Shipped with the instances tier because every signature is spelled in
 * its Pool.
 *
 * WHAT A PLACER MAY NOT SPELL ITSELF. Where item i of n falls on a ring,
 * and which cell of a grid of modules it occupies, belong to nothing here
 * and are SigilGeometry's, in `<sigilgeometry/path/Arrange.h>`. `grid` and
 * `ring` step through those bodies, and so do the layout schemes of
 * `<sigilcompose/kit/Layouts.h>` — a ring is one ring whether its items
 * are measured children or sprite positions in a buffer. Spelling it a
 * second time here would round its own way, and the same ring stamped and
 * laid out would differ by a pixel with nothing in either file to say why.
 * What a placer owns is what a Pool is: which lanes a parameter speaks to,
 * and when a lane is left alone.
 */

#include <include/core/SkPoint.h>
#include <include/core/SkSize.h>
#include <sigilcompose/instances/Instances.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilgeometry/path/Numeric.h>

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace sigil::compose::instancing::place {

namespace arrange = sigil::geometry::arrange;

/** Row-major grid of cell-sized slots from @p origin. */
inline void grid(Pool& pool, size_t count, int columns, SkSize cell,
                 SkPoint origin = {0, 0}, SkSize gap = {0, 0});

/** Evenly spaced ring; @p faceOut rotates each instance along its spoke. */
inline void ring(Pool& pool, size_t count, SkPoint center, float radius,
                 float startRadians = 0.0f, bool faceOut = false);

/** A repeated copy chain: per-copy LINEAR translate and rotate, and
 *  EXPONENTIAL scale (pow(scaleStep, i)), with an optional start→end
 *  opacity ramp.
 *
 *  The opacity ramp touches the `alphas()` lane — composing with an
 *  authored tint rather than overwriting it — and only when the two opacity
 *  arguments actually say something; `frame` is written only when it is
 *  non-negative. */
inline void repeat(Pool& pool, size_t count, SkPoint start, SkPoint translate,
                   float rotateStepRadians = 0.0f, float scaleStep = 1.0f,
                   float opacityFrom = 1.0f, float opacityTo = 1.0f,
                   int frame = -1);

// ---------------------------------------------------------------------------

inline void grid(Pool& pool, size_t count, int columns, SkSize cell,
                 SkPoint origin, SkSize gap) {
  pool.resize(count);
  auto positions = pool.positions();
  // An instance sits at the CENTRE of its slot; a laid-out child is given
  // the whole rect. Same cells either way.
  for (size_t i = 0; i < count; ++i)
    positions[i] =
        arrange::cellRect(arrange::cellAt(i, columns), cell, gap, origin)
            .center();
  pool.commit();
}

inline void ring(Pool& pool, size_t count, SkPoint center, float radius,
                 float startRadians, bool faceOut) {
  pool.resize(count);
  auto positions = pool.positions();
  auto rotations = pool.rotations();
  // A whole turn, so the last instance stops short of the first rather
  // than doubling it.
  const float sweep = geometry::path::kTau;
  for (size_t i = 0; i < count; ++i) {
    const float a =
        arrange::along(startRadians, sweep, i, count, arrange::Turn::Closed);
    positions[i] = arrange::onEllipse(center, {radius, radius}, a);
    // faceOut turns each instance to look along its own spoke.
    if (faceOut) rotations[i] = a + geometry::path::kPi / 2.0f;
  }
  pool.commit();
}

inline void repeat(Pool& pool, size_t count, SkPoint start, SkPoint translate,
                   float rotateStepRadians, float scaleStep, float opacityFrom,
                   float opacityTo, int frame) {
  pool.resize(count);
  auto positions = pool.positions();
  auto rotations = pool.rotations();
  auto scales = pool.scales();
  for (size_t i = 0; i < count; ++i) {
    positions[i] = {start.fX + translate.fX * (float)i,
                    start.fY + translate.fY * (float)i};
    rotations[i] = rotateStepRadians * (float)i;
    scales[i] = std::pow(scaleStep, (float)i);
  }
  if (opacityFrom != 1.0f || opacityTo != 1.0f) {
    auto alphas = pool.alphas();
    for (size_t i = 0; i < count; ++i) {
      const float t = count > 1 ? (float)i / (float)(count - 1) : 0.0f;
      alphas[i] = opacityFrom + (opacityTo - opacityFrom) * t;
    }
  }
  if (frame >= 0) {
    auto frames = pool.frames();
    for (size_t i = 0; i < count; ++i) frames[i] = frame;
  }
  pool.commit();
}

}  // namespace sigil::compose::instancing::place
