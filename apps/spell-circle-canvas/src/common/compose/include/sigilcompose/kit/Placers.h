#pragma once

/** @file
 * @ingroup compose-kit
 *
 * SigilCompose kit — placers: arithmetic that fills an instancing::Pool with
 * a grid, a ring or a repeat chain. Data-level, O(count), no Yoga: each
 * writes only the lanes its parameters speak to and commits the pool, so a
 * pool filled by hand and then arranged here keeps its tints and frames.
 * The pool is the kernel's instanced leaf's
 * (<sigilcompose/core/Instances.h>); what a placer owns is which of its
 * lanes a parameter speaks to.
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
#include <sigilcompose/core/Instances.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilgeometry/path/Numeric.h>

#include <algorithm>
#include <cmath>
#include <cstddef>

/** THE ARRANGEMENTS THAT FILL AN INSTANCING POOL: a grid, a ring, a
 *  repeat chain. Each is arithmetic over the pool's lanes — O(count),
 *  no layout — and writes only the lanes its parameters speak to before
 *  committing, so a pool filled by hand and then arranged here keeps
 *  its tints and its frames.
 *
 *  Where item i of n falls on a ring, and which cell of a module grid
 *  it occupies, are SigilGeometry's arithmetic and are stepped through
 *  from here rather than spelled again: the same ring stamped as
 *  sprites and laid out as children must land on the same pixels. */
namespace sigil::compose::instancing::place {

/** Row-major grid of @p count slots @p cell in size, @p columns to a row,
 *  from @p origin and spaced by @p gap. */
inline void grid(Pool& pool, size_t count, int columns, SkSize cell,
                 SkPoint origin = {0, 0}, SkSize gap = {0, 0}) {
  pool.resize(count);
  auto positions = pool.positions();
  // An instance sits at the CENTRE of its slot; a laid-out child is given
  // the whole rect. Same cells either way.
  for (size_t i = 0; i < count; ++i)
    positions[i] = geometry::arrange::cellRect(
                       geometry::arrange::cellAt(i, columns), cell, gap, origin)
                       .center();
  pool.commit();
}

/** Evenly spaced ring of @p count slots on @p radius about @p center,
 *  begun at @p startRadians; @p faceOut rotates each instance along its
 *  spoke. */
inline void ring(Pool& pool, size_t count, SkPoint center, float radius,
                 float startRadians = 0.0f, bool faceOut = false) {
  pool.resize(count);
  auto positions = pool.positions();
  auto rotations = pool.rotations();
  // A whole turn, so the last instance stops short of the first rather
  // than doubling it.
  const float sweep = geometry::path::kTau;
  for (size_t i = 0; i < count; ++i) {
    const float a = geometry::arrange::along(startRadians, sweep, i, count,
                                             geometry::arrange::Turn::Closed);
    positions[i] = geometry::arrange::onEllipse(center, {radius, radius}, a);
    // faceOut turns each instance to look along its own spoke.
    if (faceOut) rotations[i] = a + geometry::path::kPi / 2.0f;
  }
  pool.commit();
}

/** A repeated copy chain from @p start: per-copy LINEAR @p translate and
 *  @p rotateStepRadians, and EXPONENTIAL scale (pow(@p scaleStep, i)),
 *  with an optional @p opacityFrom → @p opacityTo ramp.
 *
 *  The opacity ramp touches the `alphas()` lane — composing with an
 *  authored tint rather than overwriting it — and only when the two opacity
 *  arguments actually say something; @p frame is written only when it is
 *  non-negative. */
inline void repeat(Pool& pool, size_t count, SkPoint start, SkPoint translate,
                   float rotateStepRadians = 0.0f, float scaleStep = 1.0f,
                   float opacityFrom = 1.0f, float opacityTo = 1.0f,
                   int frame = -1) {
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
