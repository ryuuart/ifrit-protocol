/** @file
 * The coverage mask and the exact Euclidean distance transform over it: a
 * lower parabola envelope down each row and then down each column, which is
 * the whole of what makes the answer exact and linear at once.
 */

#include "sigilimage/field/DistanceField.h"

#include <include/core/SkAlphaType.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkColorType.h>
#include <include/core/SkImageInfo.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace sigil::image {

namespace {

constexpr float kInfinite = std::numeric_limits<float>::max() * 0.25f;

/** THE ONE-DIMENSIONAL TRANSFORM the two passes are made of.
 *
 *  Every sample of @p f is the vertex of an upward parabola standing at its
 *  own index; the distance at a point is the lowest of them there. So the
 *  answer is the LOWER ENVELOPE of n parabolas, which is built in one
 *  left-to-right sweep: keep the parabolas that still show, each with the
 *  point where it takes over from the one before, and pop the ones a new
 *  parabola has covered. A second sweep reads the envelope off at every
 *  index. Both are linear, and the result is the true squared distance and
 *  not an approximation of it. */
void lowerEnvelope(const float* f, int n, float* out, int* vertex,
                   float* boundary) {
  int top = 0;
  vertex[0] = 0;
  boundary[0] = -kInfinite;
  boundary[1] = kInfinite;
  for (int index = 1; index < n; ++index) {
    float crossing =
        ((f[index] + (float)index * (float)index) -
         (f[vertex[top]] + (float)vertex[top] * (float)vertex[top])) /
        (2.0f * (float)index - 2.0f * (float)vertex[top]);
    while (crossing <= boundary[top]) {
      --top;
      crossing = ((f[index] + (float)index * (float)index) -
                  (f[vertex[top]] + (float)vertex[top] * (float)vertex[top])) /
                 (2.0f * (float)index - 2.0f * (float)vertex[top]);
    }
    ++top;
    vertex[top] = index;
    boundary[top] = crossing;
    boundary[top + 1] = kInfinite;
  }
  top = 0;
  for (int index = 0; index < n; ++index) {
    while (boundary[top + 1] < (float)index) ++top;
    const float offset = (float)index - (float)vertex[top];
    out[index] = offset * offset + f[vertex[top]];
  }
}

}  // namespace

Mask coverageMask(const SkPixmap& alpha, float threshold) {
  Mask mask;
  const int width = alpha.width(), height = alpha.height();
  if (width <= 0 || height <= 0 || !alpha.addr()) return mask;
  mask.width = width;
  mask.height = height;
  mask.covered.assign((size_t)width * height, 0);
  const float cutoff = std::clamp(threshold, 0.0f, 1.0f) * 255.0f;
  const bool eightBit = alpha.colorType() == kAlpha_8_SkColorType;
  for (int y = 0; y < height; ++y) {
    uint8_t* row = mask.covered.data() + (size_t)y * width;
    if (eightBit) {
      const uint8_t* pixels = alpha.addr8(0, y);
      for (int x = 0; x < width; ++x) row[x] = pixels[x] > cutoff ? 1 : 0;
    } else {
      for (int x = 0; x < width; ++x)
        row[x] = alpha.getAlphaf(x, y) * 255.0f > cutoff ? 1 : 0;
    }
  }
  return mask;
}

Mask coverageMask(const SkImage& image, float threshold) {
  SkPixmap pixels;
  if (image.peekPixels(&pixels)) return coverageMask(pixels, threshold);
  // Not already on the CPU: one readback into an alpha raster, which is the
  // only channel the answer reads.
  SkBitmap alpha;
  if (!alpha.tryAllocPixels(SkImageInfo::MakeA8(image.width(), image.height())))
    return {};
  if (!image.readPixels(nullptr, alpha.pixmap(), 0, 0)) return {};
  return coverageMask(alpha.pixmap(), threshold);
}

DistanceField distanceField(const Mask& mask) {
  DistanceField field;
  if (mask.empty()) return field;
  const int width = mask.width, height = mask.height;
  field.width = width;
  field.height = height;
  field.distance.assign((size_t)width * height, 0.0f);

  const int longest = std::max(width, height);
  std::vector<float> line((size_t)longest), transformed((size_t)longest);
  std::vector<int> vertex((size_t)longest + 1);
  std::vector<float> boundary((size_t)longest + 2);

  // A covered pixel is zero away from ink; every other starts infinitely
  // far, and the two passes bring the real distance in.
  for (size_t index = 0; index < field.distance.size(); ++index)
    field.distance[index] = mask.covered[index] ? 0.0f : kInfinite;

  for (int y = 0; y < height; ++y) {
    float* row = field.distance.data() + (size_t)y * width;
    lowerEnvelope(row, width, transformed.data(), vertex.data(),
                  boundary.data());
    std::copy_n(transformed.data(), width, row);
  }
  for (int x = 0; x < width; ++x) {
    for (int y = 0; y < height; ++y)
      line[(size_t)y] = field.distance[(size_t)y * width + x];
    lowerEnvelope(line.data(), height, transformed.data(), vertex.data(),
                  boundary.data());
    for (int y = 0; y < height; ++y)
      field.distance[(size_t)y * width + x] = transformed[(size_t)y];
  }
  // The passes carry SQUARED distances, because the envelope is built of
  // parabolas; the answer is stated in pixels.
  for (float& distance : field.distance)
    distance = distance >= kInfinite ? DistanceField::kOutside
                                     : std::sqrt(std::max(distance, 0.0f));
  return field;
}

}  // namespace sigil::image
