#pragma once
// The probes the dressed-type suites share: an effect that returns one
// fixed deviation, the ink a frame left on the surface, and the colour,
// style and shaped-run identity a restyle is read back through. Each is in
// an anonymous namespace, so every including translation unit gets its own
// copy and nothing here is shared at link time.

#include "support/TextTestSupport.h"

namespace {

/** An effect under `key` returning one fixed deviation — the readable way
 *  to drive a single GlyphMod field from a test. */
TextEffect fixed(std::string key, GlyphMod mod) {
  return fx::effect(
      std::move(key),
      [mod](const GlyphInfo&, float, sigil::core::noise::Mix64Stream&) {
        return mod;
      },
      /*reach=*/120.0f);
}

/** The bounding box of everything that is not the cleared background. */
SkIRect inkBounds(Host& host, int w, int h) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(w, h));
  host.surface->readPixels(bitmap.pixmap(), 0, 0);
  SkIRect bounds = SkIRect::MakeEmpty();
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x)
      if (bitmap.getColor(x, y) != SK_ColorBLACK) {
        const SkIRect pixel = SkIRect::MakeXYWH(x, y, 1, 1);
        if (bounds.isEmpty())
          bounds = pixel;
        else
          bounds.join(pixel);
      }
  return bounds;
}

/** The mean y of the inked pixels on columns [left, right). */
float inkCentroidY(Host& host, int h, int left, int right) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(right - left, h));
  host.surface->readPixels(bitmap.pixmap(), left, 0);
  double sum = 0;
  int count = 0;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < right - left; ++x)
      if (bitmap.getColor(x, y) != SK_ColorBLACK) {
        sum += y;
        ++count;
      }
  return count ? (float)(sum / count) : 0.0f;
}

/** The mean x of the inked pixels on rows [top, bottom). */
float inkCentroidX(Host& host, int w, int top, int bottom) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(w, bottom - top));
  host.surface->readPixels(bitmap.pixmap(), 0, top);
  double sum = 0;
  int count = 0;
  for (int y = 0; y < bottom - top; ++y)
    for (int x = 0; x < w; ++x)
      if (bitmap.getColor(x, y) != SK_ColorBLACK) {
        sum += x;
        ++count;
      }
  return count ? (float)(sum / count) : 0.0f;
}

/** Every pixel of the host's surface, for a byte-for-byte compare. */
std::vector<uint8_t> surfaceBytes(Host& host, int w, int h) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(w, h));
  host.surface->readPixels(bitmap.pixmap(), 0, 0);
  const auto* data = (const uint8_t*)bitmap.getPixels();
  return {data, data + bitmap.computeByteSize()};
}

/** How many pixels of exactly this colour a region holds. */
int countColor(Host& host, SkIRect region, SkColor color) {
  int hits = 0;
  for (int y = region.top(); y < region.bottom(); ++y)
    for (int x = region.left(); x < region.right(); ++x)
      if (host.pixel(x, y) == color) ++hits;
  return hits;
}

/** The shaped-word identity behind every placed run. The shape cache is
 *  content-addressed, so an unchanged pointer is proof that nothing about
 *  that word's shaping inputs moved. */
std::vector<const void*> runShapes(Host& host, const char* key) {
  std::vector<const void*> out;
  const auto* layout = host.composer.paragraphLayout(key);
  if (!layout) return out;
  for (const sigil::weave::PositionedRun& run : layout->runs)
    out.push_back(run.shaped);
  return out;
}

std::vector<SkPoint> runOrigins(Host& host, const char* key) {
  std::vector<SkPoint> out;
  const auto* layout = host.composer.paragraphLayout(key);
  if (!layout) return out;
  for (const sigil::weave::PositionedRun& run : layout->runs)
    out.push_back(run.origin);
  return out;
}

sigil::weave::TextStyle coloredStyle(float size, SkColor color) {
  sigil::weave::TextStyle s = styleAt(size);
  s.paint.foreground.setColor(color);
  return s;
}

/** @p base with one variable-font axis set — the restyle that differs from
 *  the text it covers in that axis alone. */
sigil::weave::TextStyle withAxis(sigil::weave::TextStyle base,
                                 const char (&tag)[5], float value) {
  base.variation(tag, value);
  return base;
}

}  // namespace
