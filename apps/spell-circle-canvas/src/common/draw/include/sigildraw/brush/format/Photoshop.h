#pragma once

/** @file
 * @ingroup draw-brush
 *
 * Photoshop `.abr` brush libraries, read for their sampled tips: file
 * versions 6, 7 and 10, and in them every SAMPLED brush's bitmap, raw
 * or PackBits-compressed, 8 or 16 bits deep, taken as the stamp's
 * coverage. The Photoshop DESCRIPTOR those files keep their names and
 * numbers in is not parsed, computed brushes are left out, and versions
 * 1 and 2 are not read.
 */

#include <sigildraw/brush/Tool.h>

#include <cstddef>
#include <span>
#include <vector>

namespace sigil::draw::brush::format {

/** Whether @p bytes begin with a version this reader can open. */
[[nodiscard]] bool isPhotoshopBrushes(std::span<const std::byte> bytes);

/** Every sampled tip in @p bytes as a tool, in the file's order; empty
 *  when the bytes are not an `.abr` this reader opens or carry no
 *  sampled brush. */
[[nodiscard]] std::vector<Tool> decodePhotoshopBrushes(
    std::span<const std::byte> bytes);

}  // namespace sigil::draw::brush::format
