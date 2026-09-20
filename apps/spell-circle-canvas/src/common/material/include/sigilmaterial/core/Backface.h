#pragma once

/** @file
 * @ingroup material-core
 *
 * Which sides of a surface are drawn.
 */

#include <cstdint>

namespace sigil::material {

/** WHICH SIDES OF A SURFACE ARE DRAWN once something has turned one of
 *  them away from the viewer. `Visible` draws either side and is zero,
 *  because it is what a surface does when nothing has been said about
 *  it; `Hidden` draws only the side facing the viewer, which is a CULL
 *  rather than a paint decision.
 *  @trap A consumer whose own default is the cull says `Hidden` where it
 *  declares the field. */
enum class Backface : std::uint8_t { Visible, Hidden };

}  // namespace sigil::material
