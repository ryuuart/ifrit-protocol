#pragma once

/** @file
 * WHAT MAKES AN ARRIVED FRAME A PICTURE, private to the frames feature:
 * the texture held past the next arrival, and the binding that wraps it
 * for a recorder or reads it back, turning it the right way up. A
 * publication's first row is its image's BOTTOM — the order every
 * application sharing textures on this machine writes and reads — and a
 * canvas draws with its first row at the top.
 */

#include <sigilmedia/advanced/Device.h>

#include <memory>

namespace sigil::io::frames::detail {

/** @p texture — the graphics API's own object — retained for as long as
 *  the answer is held; null where this build carries no frames. */
std::shared_ptr<void> retainTexture(void* texture);

/** The binding for one arrived frame, over its retained @p texture; null
 *  where this build carries no frames. */
std::shared_ptr<media::DeviceBinding> bindArrival(std::shared_ptr<void> texture);

}  // namespace sigil::io::frames::detail
