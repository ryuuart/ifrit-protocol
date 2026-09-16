#pragma once

/** @file
 * A FRAME OFF THE GPU AND INTO A FILE: the one place in the receiver that
 * reads a texture back to the CPU and encodes it. Every other line here
 * leaves the pixels where they were drawn.
 */

#import <Metal/Metal.h>

#include <filesystem>

namespace receiver {

/** Reads @p texture back on @p queue and writes it to @p path as a PNG,
 *  the rows in the order the texture holds them and the channels as the
 *  texture's own format names them. False, with the reason on stderr,
 *  when the format is not one of the eight-bit four-channel ones, when
 *  the copy could not be made, or when the file could not be written. */
bool writeTexturePng(id<MTLTexture> texture, id<MTLCommandQueue> queue,
                     const std::filesystem::path& path);

}  // namespace receiver
