#pragma once

/** @file
 * The encode surface of SigilImage: Format, EncodeOptions, and
 * encodeImage(), which routes pixels to the encoder for a format and
 * hands back the encoded bytes. Skia's own encoders cover PNG, JPEG and
 * WebP; the OpenImageIO backend, when built in
 * (SIGILIMAGE_HAS_OIIO_ENCODE), adds EXR. A format with no encoder in
 * the build simply fails to encode, the same way a format with no
 * decoder fails to decode.
 *
 * Resource ACCESS — where the bytes go, under what name, through which
 * mount — is SigilLoader's concern; this header only ever hands bytes
 * back.
 */

#include <include/core/SkRefCnt.h>

#include <filesystem>
#include <optional>

class SkData;
class SkImage;
class SkPixmap;

namespace sigil::image {

/** The formats encodeImage() writes. */
enum class Format {
  Png,
  Jpeg,
  Webp,
  Exr,
};

/** Options for encodes that support them. */
struct EncodeOptions {
  /** 0..100, honoured by the lossy formats. For JPEG it is the
   *  quantization quality. For WebP, 100 selects the format's LOSSLESS
   *  mode rather than lossy at maximum quality — the two are different
   *  codecs inside one container, and a caller asking for everything
   *  wants the one that keeps everything. PNG and EXR are lossless at
   *  every setting and ignore it. */
  int quality = 100;

  bool operator==(const EncodeOptions&) const = default;
};

/** Encodes the pixels exactly as they are given. The colour type is the
 *  caller's choice and is carried through where the format can hold it —
 *  F16 pixels reach a PNG encoder as sixteen bits per channel, float
 *  pixels reach EXR as float — so a caller who has prepared a depth
 *  prepares it here. Null when the format has no encoder in this build
 *  or the pixels are not one it can hold. */
sk_sp<SkData> encodeImage(const SkPixmap& pixels, Format format,
                          const EncodeOptions& options = {});

/** Reads @p image back to the CPU and encodes it. The readback colour
 *  type follows the format: premultiplied N32 for the LDR formats, RGBA
 *  float for EXR — so a float image written as PNG is tone-independent
 *  eight-bit, which is what asking for a PNG means. A caller who wants
 *  another depth reads back itself and uses the pixmap overload. Null
 *  when the image cannot be read back or the format has no encoder. */
sk_sp<SkData> encodeImage(const SkImage& image, Format format,
                          const EncodeOptions& options = {});

/** The format a filename names, by extension, case-insensitively
 *  (".jpg" and ".jpeg" are both JPEG); nothing when the extension names
 *  none. The one place a filename is allowed to decide a format —
 *  encodeImage itself never looks at a name. */
std::optional<Format> formatForPath(const std::filesystem::path& path);

/** The conventional extension for @p format, leading dot included. */
const char* extensionFor(Format format);

}  // namespace sigil::image
