#pragma once

/** @file
 * @ingroup media-image
 * PIXELS WRITTEN BACK OUT as the bytes of a still format: `media::encode`
 * over an `Image`, a picture, a pixmap or named channel planes, and
 * `EncodeOptions`. Skia's own encoders write PNG, JPEG and WebP, and an
 * optional backend adds EXR where it is built in; a format with no
 * encoder answers no bytes. Where the bytes then go — a file, a mount —
 * is a hub's: `hub.save(uri, image)` picks the format from the name.
 */

#include <cstddef>
#include <filesystem>
#include <vector>

#include "sigilmedia/core/Format.h"
#include "sigilmedia/core/Image.h"

class SkImage;
class SkPixmap;

namespace sigil::media {

struct Channels;

/** What `media::encode` is told besides the format. */
struct EncodeOptions {
  /** 0..100, honoured by the lossy formats; PNG and EXR are lossless at
   *  every setting and ignore it. For JPEG it is the quantization
   *  quality.
   *  @trap For WebP, 100 selects the format's LOSSLESS mode rather than
   *  lossy at maximum quality — two different codecs inside one
   *  container. */
  int quality = 100;

  bool operator==(const EncodeOptions&) const = default;
};

/** @p image's first frame as the bytes of @p format, read back to the
 *  CPU at the depth the format holds — premultiplied N32 for the LDR
 *  formats, RGBA float for EXR. Empty when there is no frame, the
 *  picture cannot be read back, or nothing here writes @p format — a
 *  movie is `media::Encoder`'s.
 *  @trap A frame standing on a device is read back through
 *  `deviceImage()` first; a caller wanting another depth reads back
 *  itself and uses the pixmap door. */
std::vector<std::byte> encode(const Image& image, Format format,
                              const EncodeOptions& options = {});

/** The same for a picture in hand. */
std::vector<std::byte> encode(const SkImage& picture, Format format,
                              const EncodeOptions& options = {});

/** THE PIXELS EXACTLY AS GIVEN: the colour type is the caller's choice
 *  and is carried through where the format can hold it, so F16 pixels
 *  reach a PNG encoder as sixteen bits per channel. A raster door for a
 *  caller that chose a depth or holds rows it did not decode. */
std::vector<std::byte> encode(const SkPixmap& pixels, Format format,
                              const EncodeOptions& options = {});

/** EVERY CHANNEL UNDER ITS OWN NAME: what the decode side hands back,
 *  written out with those names kept, so a group comes back through
 *  `ImageOptions::layer`. There is no other way to write a layer, since
 *  a picture carries four channels called R, G, B and A and nothing
 *  else. The channels are written HALF FLOAT, which is EXR's native
 *  storage.
 *  @trap ONLY EXR holds this, so any other @p format answers nothing, as
 *  do names and planes that disagree; composite the group with
 *  `Channels::image` first for a format that cannot. */
std::vector<std::byte> encode(const Channels& channels, Format format,
                              const EncodeOptions& options = {});

/** Whether this build can write @p format with `encode`. PNG, JPEG and
 *  WebP always can; EXR needs the optional backend both compiled in and
 *  carrying an EXR writer; a movie never can. It separates the two
 *  reasons `encode` answers nothing — nothing here writes that format,
 *  and an encoder that IS here refused those pixels. */
bool canEncode(Format format);

/** THE HOOK A HUB'S SAVE FINDS: @p image encoded in the format
 *  @p path's extension names, at each format's defaults. Empty when the
 *  name names no still format. Found by argument-dependent lookup, so a
 *  hub writes an image without knowing a format. */
std::vector<std::byte> encodeResource(const Image& image,
                                      const std::filesystem::path& path);

}  // namespace sigil::media
