#pragma once

/** @file
 * Procreate `.brush` files, read for their two images.
 *
 * A `.brush` is a zip. WHAT IS HONOURED: the shape artwork and the grain
 * texture inside it — any entry whose path names a shape or a grain and
 * decodes as an image — which is the part that makes the mark look like
 * itself. The shape becomes the tool's shape source and the grain its
 * grain, standing still in the pen's space.
 *
 * WHAT IS SKIPPED, and what the answer carries instead: every number the
 * brush states — spacing, jitter, scatter, the dynamics, whether the
 * grain moves, how deep it cuts — lives in `Brush.archive`, an
 * NSKeyedArchiver property list in Apple's binary encoding. Reading that
 * encoding, and then the object graph an archiver writes into it, is a
 * dependency this repository does not have and a format it does not
 * otherwise speak, so the archive is not read and every imported tool
 * takes the library's own defaults for all of them.
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
