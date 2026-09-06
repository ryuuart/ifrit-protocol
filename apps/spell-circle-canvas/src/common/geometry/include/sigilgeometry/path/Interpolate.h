#pragma once
/** @file
 * TWO OUTLINES INTERPOLATED EXACTLY. Where two paths have the same nodes
 * in the same order — what `compatible()` answers `Yes` to — the
 * in-between is every node's own weighted average, verb for verb, curves
 * and all. Nothing is resampled and nothing is approximated.
 *
 * `path/blend` is the other interpolation and a different one: it takes
 * ANY two outlines, resamples both to a common count, matches them up
 * and walks between them, which is what a blend tool does and what a
 * pair of masters must never need. Ask here first; fall back there when
 * the answer is nothing.
 */
#include <include/core/SkPath.h>

#include <optional>

namespace sigil::geometry::path {

/** `a` at @p t of nothing and `b` at @p t of one, or nothing at all when
 *  the pair does not pair — `compatible()` says which of the reasons it
 *  was. Values outside [0, 1] extrapolate, which is what a master pair
 *  pushed past its own extremes is. */
std::optional<SkPath> interpolate(const SkPath& a, const SkPath& b, float t);

}  // namespace sigil::geometry::path
