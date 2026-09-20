#pragma once

/** @file
 * @ingroup material-core
 *
 * Which sides of a surface are drawn.
 */

#include <cstdint>

namespace sigil::material {

/** WHICH SIDES OF A SURFACE ARE DRAWN once something has turned one of
 *  them away from the viewer — a plane rotated past edge-on, a body
 *  orbited round to its back.
 *
 *  `Visible` draws either side, which is what a card whose reverse
 *  carries its own picture needs, and what a sheet, a panel or an open
 *  shell needs when the viewpoint passes behind it. `Hidden` draws only
 *  the side facing the viewer, which is a CULL rather than a paint
 *  decision: a closed body's inside is never seen, and the two faces of
 *  a flipping card must not show through each other.
 *
 *  `Visible` is zero because it is what a surface does when nothing has
 *  been said about it: a plane that has not turned has no back to hide.
 *  A consumer whose own default is the cull says `Hidden` where it
 *  declares the field. */
enum class Backface : std::uint8_t { Visible, Hidden };

}  // namespace sigil::material
