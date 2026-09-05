#pragma once

/** @file
 * Photoshop `.abr` brush libraries, read for their sampled tips.
 *
 * WHAT IS HONOURED: file versions 6, 7 and 10, and in them every SAMPLED
 * brush in the `samp` section — its bitmap, at its own dimensions, raw
 * or PackBits-compressed, 8 or 16 bits deep, taken as the stamp's
 * coverage. One `.abr` holds a library, so the answer is a list, in the
 * file's order.
 *
 * WHAT IS SKIPPED, and what the answer carries instead: a version 6 file
 * keeps its NAMES, spacing, scattering, shape dynamics, texture, dual
 * brush, transfer and the rest in a Photoshop DESCRIPTOR, a separately
 * typed object graph this reader does not parse, so every imported tool
 * takes the library's own defaults for all of them and only its shape is
 * the file's. COMPUTED brushes — the ones with no bitmap, described by a
 * diameter, hardness and roundness — are not sampled tips and are left
 * out entirely. Versions 1 and 2 are not read.
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
