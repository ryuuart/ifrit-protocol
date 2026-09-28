#pragma once
/** @file
 * @ingroup weave-geometry
 *
 * THE ESCAPE HATCH TO SKIA: the one header of this library that names the
 * renderer its values stand in for. Nothing else under `sigilweave/` that
 * lays text out includes it; a caller that holds Skia's own values — a
 * path to flow text around, a typeface, a colour — and wants to hand them
 * to the engine, or wants an answer as the Skia value it was computed as,
 * includes this header by name.
 *
 * The painter seam is not here: `paint`, `drawBatched` and the testing
 * plates draw ON a Skia canvas, which is what they are for, and say so in
 * their own headers. What stands here is each entrance whose Weave form
 * differs from its Skia form only in the type it takes or answers.
 */
#include <include/core/SkPath.h>

#include <memory>

#include "sigilgeometry/advanced/Skia.h"
#include "sigilmedia/advanced/Skia.h"
#include "sigilweave/layout/Flow.h"

namespace sigil::weave::flowshape {

/** A filled Skia path as a flow shape, on the terms `path(Outline)` reads
 *  an outline: an inverse fill is read as its own non-inverse self. */
[[nodiscard]] std::shared_ptr<FlowShape> path(const SkPath& path);

}  // namespace sigil::weave::flowshape
