#pragma once

/** @file
 * @ingroup image-encode
 * The encode surface of SigilImage: Format, EncodeOptions, and
 * encodeImage(), which routes pixels — or named channel planes — to the
 * encoder for a format and hands back the encoded bytes. Skia's own
 * encoders cover PNG, JPEG and WebP, and an optional backend adds EXR
 * where it is built in; a format with no encoder simply fails to
 * encode. Resource ACCESS — where the bytes go, under what name,
 * through which mount — is another library's concern.
 */

/** @defgroup image-encode Encoding
 *  Pixels and named channel planes written back out as the bytes of a
 *  format, and the extension that names each format.
 *  @{ */
/** @} */

#include <include/core/SkRefCnt.h>

#include <filesystem>
#include <optional>

class SkData;
class SkImage;
class SkPixmap;

namespace sigil::image {

struct ChannelData;

/** The formats encodeImage() writes. */
enum class Format {
  Png,
  Jpeg,
  Webp,
  Exr,
};

/** Whether this build can write @p format at all. PNG, JPEG and WebP
 *  always answer true; EXR needs the optional backend both compiled in
 *  and carrying an EXR writer. It separates the two reasons
 *  encodeImage() answers null — nothing here writes that format, and an
 *  encoder that IS here refused those pixels. */
bool canEncode(Format format);

/** Options for encodes that support them. */
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

/** Encodes the pixels exactly as they are given: the colour type is the
 *  caller's choice and is carried through where the format can hold it,
 *  so F16 pixels reach a PNG encoder as sixteen bits per channel. Null
 *  when the format has no encoder in this build or the pixels are not
 *  one it can hold. */
sk_sp<SkData> encodeImage(const SkPixmap& pixels, Format format,
                          const EncodeOptions& options = {});

/** Reads @p image back to the CPU and encodes it, the readback colour
 *  type following the format: premultiplied N32 for the LDR formats,
 *  RGBA float for EXR. Null when the image cannot be read back or the
 *  format has no encoder.
 *  @trap A caller who wants another depth reads back itself and uses
 *  the pixmap overload. */
sk_sp<SkData> encodeImage(const SkImage& image, Format format,
                          const EncodeOptions& options = {});

/** EVERY CHANNEL UNDER ITS OWN NAME: the same value the decode side
 *  hands back, written out with those names kept, so a group comes back
 *  through `DecodeOptions::layer`. There is no other way to write a
 *  layer, since the pixmap doors carry four channels called R, G, B and
 *  A and nothing else. The channels are written HALF FLOAT, which is
 *  EXR's native storage.
 *  @trap ONLY EXR holds this, so any other @p format is null, as are
 *  names and planes that disagree; composite the group with
 *  `ChannelData::makeImage` first for a format that cannot. */
sk_sp<SkData> encodeImage(const ChannelData& channels, Format format,
                          const EncodeOptions& options = {});

/** The format a filename names, by extension, case-insensitively —
 *  ".jpg" and ".jpeg" are both JPEG — and nothing when the extension
 *  names none. The one place a filename is allowed to decide a format:
 *  encodeImage itself never looks at a name. */
std::optional<Format> formatForPath(const std::filesystem::path& path);

/** The conventional extension for @p format, leading dot included. */
const char* extensionFor(Format format);

}  // namespace sigil::image
