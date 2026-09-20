#pragma once

/** @file
 * @ingroup image-field
 * The two image-domain answers a silhouette question needs: WHICH PIXELS
 * A PICTURE COVERS, and HOW FAR EVERY OTHER PIXEL IS FROM THEM. A
 * coverage mask is an image's alpha compared with a tolerance; a
 * distance field is the EXACT Euclidean distance from every pixel to
 * the nearest covered one, which is what makes "everything within m of
 * this shape" a real disc offset rather than a square one. It is an
 * image-domain primitive and not a shader: it answers for an arbitrary
 * raster, which an analytic distance function cannot.
 */

/** @defgroup image-field Distance fields
 *  Which pixels a picture covers, and how far every other pixel is from
 *  them — the two rasters a margin, an outline or a dilation is measured
 *  against.
 *  @{ */
/** @} */

#include <include/core/SkImage.h>
#include <include/core/SkPixmap.h>

#include <cstdint>
#include <vector>

namespace sigil::image {

/// WHICH PIXELS A PICTURE COVERS: one byte a pixel, 1 covered and 0 not,
/// in row-major order.
struct Mask {
  int width = 0;
  int height = 0;
  std::vector<uint8_t> covered;  ///< row-major, w*h, 0 or 1

  /// Whether the mask covers no raster at all.
  [[nodiscard]] bool empty() const { return width <= 0 || height <= 0; }
  /// Whether the pixel at @p x, @p y is covered. A pixel off the raster
  /// is not.
  [[nodiscard]] bool at(int x, int y) const {
    return x >= 0 && y >= 0 && x < width && y < height &&
           covered[(size_t)y * width + x] != 0;
  }
};

/// HOW FAR EVERY PIXEL IS FROM THE NEAREST COVERED ONE, in pixels. Zero on
/// a covered pixel, so `at(x, y) <= m` is the mask dilated by a disc of
/// radius m.
struct DistanceField {
  int width = 0;
  int height = 0;
  std::vector<float> distance;  ///< row-major, w*h, px

  /// Whether the field covers no raster at all.
  [[nodiscard]] bool empty() const { return width <= 0 || height <= 0; }
  /// The distance in pixels at @p x, @p y, or kOutside off the raster.
  [[nodiscard]] float at(int x, int y) const {
    if (x < 0 || y < 0 || x >= width || y >= height) return kOutside;
    return distance[(size_t)y * width + x];
  }
  /// What a pixel off the raster answers: nothing covers it and nothing
  /// ever will, so it is further away than any distance the raster holds.
  static constexpr float kOutside = 1e30f;
};

/** The alpha of @p alpha thresholded into a coverage mask: a pixel is
 *  covered when its alpha is GREATER than @p threshold, a fraction of
 *  full opacity. 0 admits every pixel the paint touched at all; 0.5
 *  admits the pixels an unantialiased rasteriser would have filled,
 *  which puts the mask's edge where the drawn edge is. It reads the
 *  alpha channel alone. */
[[nodiscard]] Mask coverageMask(const SkPixmap& alpha, float threshold);

/** The same over an SkImage, read back to the CPU when it is not already
 *  there. An image that cannot be read answers an empty mask. */
[[nodiscard]] Mask coverageMask(const SkImage& image, float threshold);

/** THE EXACT EUCLIDEAN DISTANCE, in px, from every pixel to the nearest
 *  covered pixel of @p mask — two separable passes answering the true
 *  distance, where a chamfer pass would answer an integer approximation
 *  of it. A mask that covers nothing answers every pixel with
 *  `DistanceField::kOutside`. */
[[nodiscard]] DistanceField distanceField(const Mask& mask);

}  // namespace sigil::image
