#pragma once

/** @file
 * An image's pixels in the form a device texture takes them.
 *
 * A decoded HDR panorama lands as 32-bit float RGBA, which keeps the
 * range a sun needs and is NOT FILTERABLE on Apple GPUs: a sampler asked
 * to interpolate between two F32 texels there answers nothing. The copy
 * that makes such an image drawable is a half-float one, and it belongs
 * here rather than beside the decoder, because it is a property of the
 * hardware the pixels are going to and not of the file they came from.
 */

#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>

#include <cstdint>
#include <vector>

namespace sigil::skia {

/** Whether @p image holds float pixels — the form an HDR decode
 *  produces, and the one a device sampler may refuse to filter. */
bool isFloatImage(const sk_sp<SkImage>& image);

/** @p image's pixels as tightly packed half-float RGBA, four values a
 *  texel, row after row with no padding: what a device texture in a
 *  16-bit float format is uploaded from. Empty when the image cannot be
 *  read. Values above one survive, which is the whole point of asking
 *  for halves rather than bytes. */
std::vector<uint16_t> halfFloatPixels(const sk_sp<SkImage>& image);

/** @p image's pixels as tightly packed premultiplied 8-bit RGBA — the
 *  ordinary path, beside the float one so a caller choosing between them
 *  reads both in one place. Empty when the image cannot be read. */
std::vector<uint8_t> bytePixels(const sk_sp<SkImage>& image);

}  // namespace sigil::skia
