#pragma once
// The harness every compose test binary shares: a composer drawn into a
// raster surface, the system font context, and the colour and text-style
// helpers. Includes only the kernel header and what the harness itself
// touches — each binary's own support header adds the extension headers
// its translation units use, so editing an extension header rebuilds
// only the tests that exercise it. Helpers sit in anonymous namespaces so
// each including TU gets its own internal-linkage copy; nothing here is
// meant to be shared across TUs at link time.

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/Compose.h>
#include <sigilweave/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <chrono>
#include <cstring>

// Everything here draws into a raster surface at a fixed size and reads
// pixels back, so the tests are deterministic and need no GPU.

using namespace sigil::compose;
using namespace std::chrono_literals;

namespace {

sigil::weave::FontContext& fonts() {
  static auto* context =
      new sigil::weave::FontContext(sigil::weave::ports::systemFontManager());
  return *context;
}

sigil::weave::TextStyle styleAt(float size) {
  sigil::weave::TextStyle s;
  s.shaping.fontSize = size;
  return s;
}

/** A composer with its own ticker, drawn into a raster surface. */
struct Host {
  sigil::motion::Ticker ticker;
  Composer composer{ticker, fonts()};
  sk_sp<SkSurface> surface;

  explicit Host(int w = 200, int h = 200) {
    composer.setSize({(float)w, (float)h});
    surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w, h));
  }

  SkColor pixel(int x, int y) {
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
    surface->readPixels(bm.pixmap(), x, y);
    return bm.getColor(0, 0);
  }

  void frame(double dt = 0.0) {
    if (dt > 0) ticker.tick(dt);
    surface->getCanvas()->clear(SK_ColorBLACK);
    composer.draw(*surface->getCanvas());
  }
};

Fill red() { return Fill::color({1, 0, 0, 1}); }
Fill green() { return Fill::color({0, 1, 0, 1}); }
Fill blue() { return Fill::color({0, 0, 1, 1}); }

sigil::weave::TextStyle whiteStyle(float size) {
  sigil::weave::TextStyle s = styleAt(size);
  s.paint.foreground.setColor(SK_ColorWHITE);
  return s;
}

bool anyWhiteIn(Host& host, SkIRect region) {
  for (int y = region.top(); y < region.bottom(); y += 2)
    for (int x = region.left(); x < region.right(); x += 2)
      if (host.pixel(x, y) == SK_ColorWHITE) return true;
  return false;
}

bool identicalPixels(Host& a, Host& b, int w, int h) {
  SkBitmap ba, bb;
  ba.allocPixels(SkImageInfo::MakeN32Premul(w, h));
  bb.allocPixels(SkImageInfo::MakeN32Premul(w, h));
  a.surface->readPixels(ba.pixmap(), 0, 0);
  b.surface->readPixels(bb.pixmap(), 0, 0);
  for (int y = 0; y < h; ++y)
    if (std::memcmp(ba.getAddr32(0, y), bb.getAddr32(0, y), (size_t)w * 4) != 0)
      return false;
  return true;
}

}  // namespace
