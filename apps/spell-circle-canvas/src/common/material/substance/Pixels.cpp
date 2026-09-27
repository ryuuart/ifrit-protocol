/** @file
 * An engine result in host memory turned into an image: every raw
 * precision the engine cooks (8-bit and 16-bit integers, half and full
 * floats), grey or colour in either channel order, tagged with the colour
 * space its output declared. Grey above 8 bits is spread to four channels,
 * because the image types that hold it alone read as red when sampled.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkColorSpace.h>
#include <include/core/SkImageInfo.h>

#include <cstdint>
#include <cstring>

#include "Internal.h"

namespace sigil::material::sbsar {

namespace {

sk_sp<SkColorSpace> colorSpaceOf(Encoding encoding) {
  switch (encoding) {
    case Encoding::Srgb:
      return SkColorSpace::MakeSRGB();
    case Encoding::Linear:
      return SkColorSpace::MakeSRGBLinear();
    case Encoding::Raw:
      break;
  }
  // Data carries no colour space, so nothing converts it on the way to a
  // surface: a normal or a roughness arrives as the numbers it was cooked.
  return nullptr;
}

/** Copies @p count grey samples into four-channel pixels, the grey in
 *  the first three channels and @p opaque in the fourth. */
template <class Sample>
void spreadGrey(const Sample* source, Sample* destination, size_t count,
                Sample opaque) {
  for (size_t index = 0; index < count; ++index) {
    destination[index * 4 + 0] = source[index];
    destination[index * 4 + 1] = source[index];
    destination[index * 4 + 2] = source[index];
    destination[index * 4 + 3] = opaque;
  }
}

/** Swaps the first and third channel of every pixel, for a result the
 *  engine handed back in BGRA order at a precision Skia holds only as
 *  RGBA. */
template <class Sample>
void swapRedBlue(Sample* pixels, size_t count) {
  for (size_t index = 0; index < count; ++index)
    std::swap(pixels[index * 4 + 0], pixels[index * 4 + 2]);
}

}  // namespace

sk_sp<SkImage> imageOf(const SubstanceTexture& texture, Encoding encoding) {
  const int width = texture.level0Width, height = texture.level0Height;
  if (!texture.buffer || width <= 0 || height <= 0) return nullptr;
  const unsigned format = texture.pixelFormat & Substance_PF_MASK;
  if ((format & Substance_PF_MASK_RAWFormat) != Substance_PF_RAW)
    return nullptr;
  const unsigned channels = format & Substance_PF_MASK_RAWChannels;
  const unsigned precision = format & Substance_PF_MASK_RAWPrecision;
  const bool grey = channels == Substance_PF_L;
  const bool opaque = channels == Substance_PF_RGBx;
  if (!grey && channels != Substance_PF_RGBA && !opaque) return nullptr;
  const bool bgra = texture.channelsOrder == Substance_ChanOrder_BGRA;
  const size_t count = (size_t)width * (size_t)height;
  const sk_sp<SkColorSpace> space = colorSpaceOf(encoding);
  const SkAlphaType alpha =
      grey || opaque ? kOpaque_SkAlphaType : kUnpremul_SkAlphaType;

  SkColorType type = kUnknown_SkColorType;
  size_t sampleBytes = 0;
  switch (precision) {
    case Substance_PF_8I:
      type = grey ? kGray_8_SkColorType
                  : (bgra ? kBGRA_8888_SkColorType : kRGBA_8888_SkColorType);
      sampleBytes = 1;
      break;
    case Substance_PF_16I:
      type = kR16G16B16A16_unorm_SkColorType;
      sampleBytes = 2;
      break;
    case Substance_PF_16F:
      type = kRGBA_F16_SkColorType;
      sampleBytes = 2;
      break;
    case Substance_PF_32F:
      type = kRGBA_F32_SkColorType;
      sampleBytes = 4;
      break;
    default:
      return nullptr;
  }
  SkBitmap bitmap;
  if (!bitmap.tryAllocPixels(
          SkImageInfo::Make(width, height, type, alpha, space)))
    return nullptr;
  const size_t sourceChannels = grey ? 1 : 4;
  const size_t sourceRow = (size_t)width * sourceChannels * sampleBytes;
  for (int row = 0; row < height; ++row) {
    const auto* source =
        static_cast<const uint8_t*>(texture.buffer) + sourceRow * (size_t)row;
    void* destination = bitmap.getAddr(0, row);
    if (!grey || precision == Substance_PF_8I) {
      std::memcpy(destination, source, sourceRow);
      continue;
    }
    switch (precision) {
      case Substance_PF_16I:
        spreadGrey(reinterpret_cast<const uint16_t*>(source),
                   static_cast<uint16_t*>(destination), (size_t)width,
                   uint16_t{0xFFFF});
        break;
      case Substance_PF_16F:
        // 0x3C00 is one in half precision.
        spreadGrey(reinterpret_cast<const uint16_t*>(source),
                   static_cast<uint16_t*>(destination), (size_t)width,
                   uint16_t{0x3C00});
        break;
      default:
        spreadGrey(reinterpret_cast<const float*>(source),
                   static_cast<float*>(destination), (size_t)width, 1.0f);
        break;
    }
  }
  if (bgra && !grey && precision != Substance_PF_8I) {
    if (sampleBytes == 2)
      swapRedBlue(static_cast<uint16_t*>(bitmap.getPixels()), count);
    else
      swapRedBlue(static_cast<float*>(bitmap.getPixels()), count);
  }
  bitmap.setImmutable();
  return bitmap.asImage();
}

}  // namespace sigil::material::sbsar
