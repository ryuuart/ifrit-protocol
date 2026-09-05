#pragma once

/** @file
 * Encoded artwork to a drawable image. Private to the brush formats:
 * every one of them carries its tip as a picture, and none of them
 * decodes one itself.
 */

#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>

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

}  // namespace sigil::draw::brush::format
