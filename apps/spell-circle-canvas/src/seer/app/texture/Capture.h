#pragma once

/** @file
 * A FRAME OFF THE GPU AND INTO A FILE: the one place in the receiver that
 * reads a texture back to the CPU and encodes it. Every other line here
 * leaves the pixels where they were drawn.
 */

#import <Metal/Metal.h>

#include <filesystem>

namespace seer::texture {

/** WHICH ROW OF A TEXTURE IS THE TOP OF THE PICTURE. A canvas draws its
 *  first row at the top; the surface a publication is carried on holds
 *  its first row at the bottom, and a frame taken off one arrives that
 *  way round. A PNG is written top first whichever it was. */
enum class Rows { TopFirst, BottomFirst };

/** Reads @p texture back on @p queue and writes it to @p path as a PNG,
 *  the channels as the texture's own format names them and the rows the
 *  right way up for @p held. False, with the reason on stderr, when the
 *  format is not one of the eight-bit four-channel ones, when the copy
 *  could not be made, or when the file could not be written. */
bool writeTexturePng(id<MTLTexture> texture, id<MTLCommandQueue> queue,
                     const std::filesystem::path& path,
                     Rows held = Rows::TopFirst);

}  // namespace seer::texture
