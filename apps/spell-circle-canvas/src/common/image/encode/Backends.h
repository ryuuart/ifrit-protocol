#pragma once

/** @file
 * The encoders the routing entry point chooses between, one translation
 * unit each: Skia's own for PNG/JPEG/WebP, and the OpenImageIO writer
 * for EXR when SIGILIMAGE_HAS_OIIO_ENCODE is defined. Private to the
 * encode feature.
 */

#include <include/core/SkRefCnt.h>

#include "sigilimage/encode/Encode.h"

class SkData;
class SkPixmap;

namespace sigil::image {
struct ChannelData;
}

namespace sigil::image::backend {

/** PNG, JPEG and WebP through Skia's encoders; null for anything else
 *  and for pixels the encoder refuses. */
sk_sp<SkData> encodeWithSkia(const SkPixmap& pixels, Format format,
                             const EncodeOptions& options);

#ifdef SIGILIMAGE_HAS_OIIO_ENCODE

/** Scanline EXR written to memory, half float per channel for F16
 *  pixels and full float otherwise. Null when the pixels cannot be read
 *  as float or OIIO has no EXR writer. */
sk_sp<SkData> encodeExrWithOiio(const SkPixmap& pixels);

/** The same file with the channels the caller named rather than R, G, B
 *  and A. Null when the names and the planes disagree or OIIO has no EXR
 *  writer. */
sk_sp<SkData> encodeExrChannelsWithOiio(const ChannelData& channels);

#endif  // SIGILIMAGE_HAS_OIIO_ENCODE

}  // namespace sigil::image::backend
