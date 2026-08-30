#pragma once

/** @file
 * The surface Ultralight's CPU renderer paints into: an SkBitmap, so
 * the pixels are readable by Skia without a conversion step. Internal:
 * it names Ultralight types, and Ultralight is private to the library.
 */

#include <Ultralight/platform/Surface.h>
#include <include/core/SkBitmap.h>

#include <cstddef>
#include <cstdint>

namespace sigil::scry {

/**
 * ultralight::Surface whose pixel store is an SkBitmap. Pixels are
 * premultiplied BGRA in sRGB — exactly what Ultralight writes and what
 * the scene pipeline draws. Rows are padded to 16 bytes, which keeps
 * Ultralight's SIMD paint paths on their fast lane.
 */
class SkiaSurface final : public ultralight::Surface {
 public:
  SkiaSurface(uint32_t width, uint32_t height) { Resize(width, height); }

  uint32_t width() const override { return m_bitmap.width(); }
  uint32_t height() const override { return m_bitmap.height(); }
  uint32_t row_bytes() const override {
    return static_cast<uint32_t>(m_bitmap.rowBytes());
  }
  size_t size() const override { return m_bitmap.computeByteSize(); }

  void* LockPixels() override { return m_bitmap.getPixels(); }
  void UnlockPixels() override {}

  /** Reallocates (transparent) at a new size; a no-op at the same size,
   *  so the pixels and their address survive. */
  void Resize(uint32_t width, uint32_t height) override;

  const SkBitmap& bitmap() const { return m_bitmap; }

 private:
  SkBitmap m_bitmap;
};

class SkiaSurfaceFactory final : public ultralight::SurfaceFactory {
 public:
  ultralight::Surface* CreateSurface(uint32_t width, uint32_t height) override {
    return new SkiaSurface(width, height);
  }
  void DestroySurface(ultralight::Surface* surface) override { delete surface; }
};

}  // namespace sigil::scry
