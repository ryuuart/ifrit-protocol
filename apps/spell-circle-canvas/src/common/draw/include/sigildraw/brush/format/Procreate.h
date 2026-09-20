#pragma once

/** @file
 * @ingroup draw-brush
 *
 * Procreate `.brush` files, read for their two images: the shape
 * artwork and the grain texture inside the zip, which is the part that
 * makes the mark look like itself. The numbers the brush states live in
 * an NSKeyedArchiver property list this repository does not read, so an
 * imported tool takes the library's own defaults for all of them.
 */

#include <sigildraw/brush/Tool.h>

#include <cstddef>
#include <optional>
#include <span>

namespace sigil::draw::brush::format {

/** The brush a `.brush` archive describes; null when the bytes are not
 *  an archive or hold no image this library can decode. */
[[nodiscard]] std::optional<Tool> decodeProcreateBrush(
    std::span<const std::byte> bytes);

}  // namespace sigil::draw::brush::format
