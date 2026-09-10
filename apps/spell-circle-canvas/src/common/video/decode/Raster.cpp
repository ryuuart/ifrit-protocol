/** @file
 * The CPU executor: a decoded frame — transferred off the device first when
 * it arrived there — converted through libswscale to premultiplied sRGB and
 * handed back as an immutable raster image.
 */

#include <include/core/SkColorSpace.h>
#include <include/core/SkData.h>
#include <include/core/SkImageInfo.h>

#include <cstdint>
#include <utility>
#include <vector>

#include "DecodeInternal.h"

namespace sigil::video {
namespace {

/** The libswscale coefficient table for a stream's tagged matrix. An
 *  untagged stream is treated as ITU-R BT.601, the same reading the
 *  device executor gives an untagged native frame, so the two executors
 *  agree on every clip. */
int swscaleColorspace(AVColorSpace colorspace) {
  switch (colorspace) {
    case AVCOL_SPC_BT709:
      return SWS_CS_ITU709;
    case AVCOL_SPC_BT2020_NCL:
    case AVCOL_SPC_BT2020_CL:
      return SWS_CS_BT2020;
    case AVCOL_SPC_SMPTE240M:
      return SWS_CS_SMPTE240M;
    case AVCOL_SPC_FCC:
      return SWS_CS_FCC;
    default:
      return SWS_CS_ITU601;
  }
}

}  // namespace

sk_sp<SkImage> Video::Impl::rasterize(const SharedFrame& decoded) {
  if (!decoded) return nullptr;
  const AVFrame* source = decoded.get();
  AVFrame* transferred = nullptr;
  if (source->format == hardwarePixelFormat && hardwareActive) {
    transferred = av_frame_alloc();
    if (!transferred || av_hwframe_transfer_data(transferred, source, 0) < 0) {
      av_frame_free(&transferred);
      return nullptr;
    }
    source = transferred;
  }

  sws = sws_getCachedContext(sws, source->width, source->height,
                             static_cast<AVPixelFormat>(source->format),
                             source->width, source->height, AV_PIX_FMT_RGBA,
                             SWS_BILINEAR, nullptr, nullptr, nullptr);
  if (!sws) {
    av_frame_free(&transferred);
    return nullptr;
  }
  // The stream's own matrix and range drive the YUV→RGB conversion:
  // libswscale's default is BT.601 limited range, which mis-colours an
  // HD clip and crushes a full-range one. The details belong to one
  // context, and the cached context is remade whenever the source's
  // size or format changes, so they are re-applied whenever any of
  // those or the tags differ from the last frame; a source that is not
  // YUV ignores them.
  const SwsSource description{source->width, source->height, source->format,
                              swscaleColorspace(source->colorspace),
                              source->color_range == AVCOL_RANGE_JPEG};
  if (description != swsSource) {
    sws_setColorspaceDetails(sws, sws_getCoefficients(description.colorspace),
                             description.fullRange ? 1 : 0,
                             sws_getCoefficients(SWS_CS_DEFAULT), 1, 0, 1 << 16,
                             1 << 16);
    swsSource = description;
  }

  const int width = source->width;
  const int height = source->height;
  const size_t rowBytes = static_cast<size_t>(width) * 4;
  std::vector<uint8_t> pixels(rowBytes * height);
  uint8_t* destinations[] = {pixels.data(), nullptr, nullptr, nullptr};
  int destinationStrides[] = {static_cast<int>(rowBytes), 0, 0, 0};
  const int rows = sws_scale(sws, source->data, source->linesize, 0, height,
                             destinations, destinationStrides);
  const bool hasAlpha =
      metadata.hasAlpha ||
      pixelFormatHasAlpha(static_cast<AVPixelFormat>(source->format));
  const AVAlphaMode alphaMode = source->alpha_mode;
  av_frame_free(&transferred);
  if (rows != height) return nullptr;

  if (hasAlpha && alphaMode != AVALPHA_MODE_PREMULTIPLIED) {
    for (size_t offset = 0; offset < pixels.size(); offset += 4) {
      const unsigned alpha = pixels[offset + 3];
      pixels[offset + 0] = static_cast<uint8_t>(
          (static_cast<unsigned>(pixels[offset + 0]) * alpha + 127) / 255);
      pixels[offset + 1] = static_cast<uint8_t>(
          (static_cast<unsigned>(pixels[offset + 1]) * alpha + 127) / 255);
      pixels[offset + 2] = static_cast<uint8_t>(
          (static_cast<unsigned>(pixels[offset + 2]) * alpha + 127) / 255);
    }
  }

  sk_sp<SkData> data = SkData::MakeWithCopy(pixels.data(), pixels.size());
  const SkImageInfo info =
      SkImageInfo::Make(width, height, kRGBA_8888_SkColorType,
                        hasAlpha ? kPremul_SkAlphaType : kOpaque_SkAlphaType,
                        SkColorSpace::MakeSRGB());
  return SkImages::RasterFromData(info, std::move(data), rowBytes);
}

}  // namespace sigil::video
