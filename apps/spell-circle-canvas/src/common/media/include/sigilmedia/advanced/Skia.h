#pragma once

/** @file
 * @ingroup media-core
 * THE ONE DOOR BETWEEN THIS LIBRARY AND SKIA, included by name by a caller
 * that holds Skia's own values: a `Picture` from and to Skia's image, a
 * size from and to Skia's, the binding that turns a frame standing on a
 * device into an image a Graphite recorder draws, and the raster doors
 * that take a pixmap exactly as stored. Every other header here speaks
 * `Picture`, `Frame`, `PixelSource` and glm sizes. Including this one also
 * lets Skia's image stand wherever a `Picture` or a `PixelSource` is
 * taken, and a `Picture` be assigned to Skia's image.
 *
 * Each raster door is defined by the feature that owns its question, so a
 * caller links the feature it calls: `encode` SigilMediaImageEncode,
 * `difference` SigilMediaDifference, `coverageMask` SigilMediaField,
 * `append` SigilMediaVideoEncode.
 */

#include <include/core/SkImage.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSize.h>

#include <cstddef>
#include <vector>

#include "sigilmedia/advanced/Skia.h"
#include "sigilmedia/core/Format.h"
#include "sigilmedia/core/Frame.h"
#include "sigilmedia/core/Picture.h"
#include "sigilmedia/difference/Difference.h"
#include "sigilmedia/field/DistanceField.h"
#include "sigilmedia/image/Encode.h"

namespace skgpu::graphite {
class Recorder;
}  // namespace skgpu::graphite

namespace sigil::media {

class Encoder;

/** Skia's image as a `Picture`, and back. */
template <>
struct PictureAdapter<sk_sp<SkImage>> {
  static Picture wrap(sk_sp<SkImage> native);
  static sk_sp<SkImage> unwrap(const Picture& picture);
};

/** @p picture as Skia's image; null for no picture. */
inline sk_sp<SkImage> toSk(const Picture& picture) {
  return PictureAdapter<sk_sp<SkImage>>::unwrap(picture);
}
/** Skia's image as a `Picture`; no picture for null. */
inline Picture fromSk(sk_sp<SkImage> image) {
  return PictureAdapter<sk_sp<SkImage>>::wrap(std::move(image));
}
/** A size in pixels as Skia's. */
inline SkISize toSk(glm::ivec2 size) { return SkISize::Make(size.x, size.y); }
/** Skia's size in pixels as a glm one. */
inline glm::ivec2 fromSk(SkISize size) { return {size.width(), size.height()}; }

/** HOW A FRAME STANDING ON A DEVICE BECOMES AN IMAGE: made by the source
 *  that produced the frame, which alone knows what its surface is.
 *  `image(recorder)` wraps the surface for @p recorder where it stands,
 *  and with no recorder reads it back into host memory. An
 *  implementation keeps what it made, so a frame drawn in several places
 *  is bound once per recorder. */
class DeviceBinding {
 public:
  virtual ~DeviceBinding() = default;
  /** The frame as an image @p recorder draws, or read back to the CPU
   *  when @p recorder is null; null when neither can be made. */
  virtual sk_sp<SkImage> image(skgpu::graphite::Recorder* recorder) = 0;
};

/** THE FRAME AS AN IMAGE @p recorder DRAWS: its picture when it has one,
 *  its device surface bound for @p recorder when it has that, and that
 *  surface read back to host memory when @p recorder is null. Null for an
 *  empty frame. What a leaf, a pen and a texture call; a sketch draws a
 *  frame through one of them. */
sk_sp<SkImage> deviceImage(const Frame& frame,
                           skgpu::graphite::Recorder* recorder);

/** THE PIXELS EXACTLY AS GIVEN, as the bytes of @p format: the colour type
 *  is the caller's choice and is carried through where the format can
 *  hold it, so F16 pixels reach a PNG encoder as sixteen bits per
 *  channel. A raster door for a caller that chose a depth or holds rows
 *  it did not decode. */
std::vector<std::byte> encode(const SkPixmap& pixels, Format format,
                              const EncodeOptions& options = {});

/** Two pixmaps held against each other as they are stored. Two of one
 *  colour type and alpha type are compared row by row as stored first, so
 *  identical rows cost a memory comparison. */
[[nodiscard]] PixelDifference difference(const SkPixmap& actual,
                                         const SkPixmap& expected);

/** Which pixels a pixmap covers, its alpha read as it is stored. */
[[nodiscard]] Mask coverageMask(const SkPixmap& alpha,
                                FieldOptions options = {});

/** Encodes @p pixels as one movie frame, read where they stand for the
 *  length of the call. */
bool append(Encoder& encoder, const SkPixmap& pixels);

}  // namespace sigil::media
