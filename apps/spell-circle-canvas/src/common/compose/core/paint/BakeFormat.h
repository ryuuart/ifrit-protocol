#pragma once

#include <include/core/SkImageInfo.h>

namespace sigil::compose::detail {

/** Isolated bakes need alpha even when the destination stores only RGB. */
inline SkImageInfo pixelBakeInfo(const SkImageInfo& destination,
                                 SkISize dimensions) {
  SkColorType type = destination.colorType();
  if (SkColorTypeIsAlwaysOpaque(type)) {
    switch (type) {
      case kRGB_F16F16F16x_SkColorType:
      case kR16_float_SkColorType:
      case kR16G16_float_SkColorType:
      case kRGB_101010x_SkColorType:
      case kBGR_101010x_SkColorType:
      case kBGR_101010x_XR_SkColorType:
        type = kRGBA_F16_SkColorType;
        break;
      case kR16_unorm_SkColorType:
      case kR16G16_unorm_SkColorType:
        type = kR16G16B16A16_unorm_SkColorType;
        break;
      default:
        type = kN32_SkColorType;
        break;
    }
  }
  if (type == kUnknown_SkColorType) type = kN32_SkColorType;
  return SkImageInfo::Make(dimensions.width(), dimensions.height(), type,
                           kPremul_SkAlphaType, destination.refColorSpace());
}

}  // namespace sigil::compose::detail
