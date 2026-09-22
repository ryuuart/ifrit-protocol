#pragma once

/** @file
 * @ingroup geometry-path
 *
 * The two words for what happens at the ends and at the corners of a
 * mark that has been given a width — the offset that straddles a
 * contour, the joinery that cuts a set of strips, and the stroke a
 * drawing library lays over an outline all decide the same two things.
 * They stand in their own header so a consumer that only names the
 * decision does not parse the operators.
 */

#include <cstdint>

namespace sigil::geometry::path {

/** How two pieces of a widened mark meet at a corner: an arc about the
 *  corner, the point where the two outer edges would cross (cut back
 *  past the miter limit the caller states), or the chord between them. */
enum class Join : uint8_t { Round, Miter, Bevel };

/** How a widened mark ends where the source has an end: flush with the
 *  last point, a half-width arc past it, or a half-width square. */
enum class Cap : uint8_t { Butt, Round, Square };

}  // namespace sigil::geometry::path
