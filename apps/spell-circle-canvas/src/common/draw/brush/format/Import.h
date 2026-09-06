#pragma once

/** @file
 * What every importer needs and none of them owns: encoded artwork as a
 * drawable image, and the tool an imported brush starts from. Private to
 * the brush formats.
 */

#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>
#include <sigildraw/brush/Tool.h>

#include <cstddef>
#include <span>

namespace sigil::draw::brush::format {

/** The image @p bytes encode, through SigilImage's own routing; null
 *  when they are empty or are not an image it reads. */
[[nodiscard]] sk_sp<SkImage> decodeArtwork(std::span<const std::byte> bytes);

/** A grayscale mask as an image whose ALPHA is the mask: @p coverage
 *  holds @p width by @p height bytes, row by row, and a byte of 255 is
 *  full coverage. What the sampled tips inside the two imported formats
 *  arrive as. */
[[nodiscard]] sk_sp<SkImage> coverageImage(std::span<const uint8_t> coverage,
                                           int width, int height);

/** The tool an imported brush begins as, before the file it came from is
 *  read over it: a stamped tip at full load, with the marked-up ends and
 *  the per-stroke envelope roll this library gives a tool of its own
 *  left off, because the file states an envelope of its own or none. */
[[nodiscard]] Tool importedTool();

}  // namespace sigil::draw::brush::format
