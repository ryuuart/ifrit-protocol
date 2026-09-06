#pragma once

/** @file
 * The two image-domain answers a silhouette question needs: WHICH PIXELS A
 * PICTURE COVERS, and HOW FAR EVERY OTHER PIXEL IS FROM THEM.
 *
 * A coverage mask is an image's alpha compared with a tolerance — the
 * threshold above which a pixel counts as ink. A distance field is the
 * EXACT Euclidean distance from every pixel to the nearest covered one,
 * which is what makes "everything within m of this shape" a real disc
 * offset rather than a square one: dilating a mask by comparing this field
 * with m is the set of all points no further than m from the shape, corners
 * rounded, a diagonal edge standing off by m and not by m·√2.
 *
 * It is an image-domain primitive and not a shader: it answers for an
 * arbitrary raster, which an analytic distance function for a parameterised
 * shape cannot do.
 */

#include <include/core/SkImage.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkRefCnt.h>

#include <cstdint>
#include <vector>

namespace sigil::image {

/// WHICH PIXELS A PICTURE COVERS: one byte a pixel, 1 covered and 0 not,
/// in row-major order.
struct Mask {
  int width = 0;
  int height = 0;
  std::vector<uint8_t> covered;  ///< row-major, w*h, 0 or 1

  [[nodiscard]] bool empty() const { return width <= 0 || height <= 0; }
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

  [[nodiscard]] bool empty() const { return width <= 0 || height <= 0; }
  [[nodiscard]] float at(int x, int y) const {
    if (x < 0 || y < 0 || x >= width || y >= height) return kOutside;
    return distance[(size_t)y * width + x];
  }
  /// What a pixel off the raster answers: nothing covers it and nothing
  /// ever will, so it is further away than any distance the raster holds.
  static constexpr float kOutside = 1e30f;
};

/** The alpha of @p alpha thresholded into a coverage mask: a pixel is
 *  covered when its alpha is GREATER than @p threshold, a fraction of full
 *  opacity. A threshold of 0 admits every pixel the paint touched at all;
 *  one of 0.5 admits the pixels an unantialiased rasteriser would have
 *  filled, which puts the mask's edge where the drawn edge is.
 *
 *  Reads the alpha channel alone, so an 8-bit alpha raster and a full-color
 *  one answer alike. */
[[nodiscard]] Mask coverageMask(const SkPixmap& alpha, float threshold);

/** The same over an SkImage, read back to the CPU when it is not already
 *  there. An image that cannot be read answers an empty mask. */
[[nodiscard]] Mask coverageMask(const SkImage& image, float threshold);

/** THE EXACT EUCLIDEAN DISTANCE from every pixel to the nearest covered
 *  pixel of @p mask.
 *
 *  Exact, not approximate: two separable passes — a parabola-envelope
 *  transform down each row, then down each column — answer the true squared
 *  distance for every pixel in time proportional to the raster, where a
 *  chamfer pass would answer an integer approximation of it. A mask that
 *  covers nothing answers every pixel with `DistanceField::kOutside`. */
[[nodiscard]] DistanceField distanceField(const Mask& mask);

}  // namespace sigil::image
