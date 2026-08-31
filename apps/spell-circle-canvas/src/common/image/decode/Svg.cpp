/** @file
 * The SVG backend: content sniffed for an XML or svg root, the DOM
 * parsed without a font manager, and the raster size derived from the
 * intrinsic size and the options before rendering onto a surface.
 * Compiled to nothing without SIGILIMAGE_HAS_SVG.
 */

#ifdef SIGILIMAGE_HAS_SVG

#include <include/core/SkCanvas.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkStream.h>
#include <include/core/SkSurface.h>
#include <modules/svg/include/SkSVGDOM.h>
#include <modules/svg/include/SkSVGRenderContext.h>
#include <modules/svg/include/SkSVGSVG.h>

#include <algorithm>
#include <cmath>
#include <string_view>

#include "Backends.h"

namespace sigil::image::backend {

namespace {

/** Percent-sized SVGs have no intrinsic size; this is the viewport
 *  they rasterize into when the caller names no size either. */
constexpr int kSvgFallbackSize = 512;
constexpr int kSvgMaxSide = 8192;

/** Parses without a font manager — <text> nodes simply don't render,
 *  which is the supported trade for a dependency-free backend. */
sk_sp<SkSVGDOM> parseSvg(const std::byte* bytes, size_t size) {
  auto stream = SkMemoryStream::MakeDirect(bytes, size);
  return SkSVGDOM::Builder{}.make(*stream);
}

/** The root element's intrinsic size; (0, 0) when percent-sized. */
SkSize svgIntrinsicSize(const SkSVGDOM& dom) {
  return dom.getRoot()->intrinsicSize(
      SkSVGLengthContext(SkSize::Make(kSvgFallbackSize, kSvgFallbackSize)));
}

/** The raster size DecodeOptions asks for: a missing axis follows the
 *  intrinsic aspect, everything clamps to [1, kSvgMaxSide]. */
SkISize svgRasterSize(const SkSize& intrinsic, const DecodeOptions& options) {
  float w = (float)options.width;
  float h = (float)options.height;
  const float aspect = intrinsic.width() > 0 && intrinsic.height() > 0
                           ? intrinsic.width() / intrinsic.height()
                           : 1.0f;
  if (w <= 0 && h <= 0) {
    w = intrinsic.width() > 0 ? intrinsic.width() : kSvgFallbackSize;
    h = intrinsic.height() > 0 ? intrinsic.height() : kSvgFallbackSize;
  } else if (w <= 0) {
    w = h * aspect;
  } else if (h <= 0) {
    h = w / aspect;
  }
  auto clamp = [](float v) {
    return std::clamp((int)std::lround(v), 1, kSvgMaxSide);
  };
  return SkISize{clamp(w), clamp(h)};
}

}  // namespace

/** SVG has no magic number; sniff leading whitespace/BOM then "<?xml"
 *  or "<svg" (a .svg pathHint extension counts as a hint too). */
bool looksLikeSvg(const std::byte* bytes, size_t size,
                  const std::filesystem::path& pathHint) {
  if (pathHint.extension() == ".svg") return true;
  std::string_view text(reinterpret_cast<const char*>(bytes), size);
  if (text.starts_with("\xEF\xBB\xBF")) text.remove_prefix(3);
  while (!text.empty() && (text.front() == ' ' || text.front() == '\t' ||
                           text.front() == '\r' || text.front() == '\n'))
    text.remove_prefix(1);
  return text.starts_with("<?xml") || text.starts_with("<svg");
}

std::optional<ImageAsset> decodeWithSvg(const std::byte* bytes, size_t size,
                                        const DecodeOptions& options) {
  sk_sp<SkSVGDOM> dom = parseSvg(bytes, size);
  if (!dom || !dom->getRoot()) return std::nullopt;
  const SkSize intrinsic = svgIntrinsicSize(*dom);
  const SkISize raster = svgRasterSize(intrinsic, options);
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(raster));
  if (!surface) return std::nullopt;
  // Absolute-sized roots ignore the container size, so scaling to the
  // requested raster happens on the canvas; percent-sized roots take
  // the raster as their viewport directly.
  dom->setContainerSize(SkSize::Make(raster));
  SkCanvas* canvas = surface->getCanvas();
  if (intrinsic.width() > 0 && intrinsic.height() > 0)
    canvas->scale(raster.width() / intrinsic.width(),
                  raster.height() / intrinsic.height());
  dom->render(canvas);
  return ImageAsset::wrap(surface->makeImageSnapshot());
}

std::optional<ImageProbe> probeWithSvg(const std::byte* bytes, size_t size) {
  sk_sp<SkSVGDOM> dom = parseSvg(bytes, size);
  if (!dom || !dom->getRoot()) return std::nullopt;
  const SkSize intrinsic = svgIntrinsicSize(*dom);
  ImageProbe info;
  info.format = "svg";
  info.width = (int)std::lround(intrinsic.width());
  info.height = (int)std::lround(intrinsic.height());
  return info;  // channels 4, frames 1, not float: the struct defaults
}

}  // namespace sigil::image::backend

#endif  // SIGILIMAGE_HAS_SVG
