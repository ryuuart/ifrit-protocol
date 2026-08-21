#pragma once
// Shared support for the compose test translation units — the Host
// harness, colour and text-style helpers, and every header the tests
// need. Helpers sit in anonymous namespaces so each including TU gets its
// own internal-linkage copy; nothing here is meant to be shared across
// TUs at link time.

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkColorFilter.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkRRect.h>
#include <include/core/SkStream.h>
#include <include/core/SkString.h>
#include <include/core/SkStrokeRec.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <include/effects/SkTrimPathEffect.h>
#include <include/encode/SkPngEncoder.h>
#include <sigilcompose/Brushes.h>
#include <sigilcompose/Compose.h>
#include <sigilcompose/Debug.h>
#include <sigilcompose/Decorations.h>
#include <sigilcompose/Feed.h>
#include <sigilcompose/Instances.h>
#include <sigilcompose/LayerStyles.h>
#include <sigilcompose/Layouts.h>
#include <sigilcompose/Lines.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Ocio.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Routers.h>
#include <sigilcompose/Sdf.h>
#include <sigilcompose/Shapes.h>
#include <sigilcompose/Studio.h>
#include <sigilcompose/TextFx.h>
#include <sigilcompose/Util.h>
#include <sigilcompose/kit/Strokes.h>
#include <sigilimage/ImageAsset.h>
#include <sigilweave/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <type_traits>

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

}  // namespace

namespace {
sk_sp<SkRuntimeEffect> ukEffect() {
  auto [effect, err] = SkRuntimeEffect::MakeForShader(
      SkString("uniform float uK;"
               "half4 main(float2 p) { return half4(uK, 0, 0, 1); }"));
  return effect;
}
}  // namespace

namespace {
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
}  // namespace

namespace {
/** A 2-cell atlas: left 16x16 red, right 16x16 green. */
std::shared_ptr<sigil::image::ImageAsset> twoCellAtlas() {
  SkBitmap src;
  src.allocN32Pixels(32, 16);
  src.erase(SK_ColorRED, SkIRect::MakeXYWH(0, 0, 16, 16));
  src.erase(SK_ColorGREEN, SkIRect::MakeXYWH(16, 0, 16, 16));
  SkDynamicMemoryWStream stream;
  SkPngEncoder::Encode(&stream, src.pixmap(), {});
  return std::make_shared<sigil::image::ImageAsset>(
      *sigil::image::ImageAsset::decode(stream.detachAsData()));
}
}  // namespace

namespace {
/** Counts distinct painted runs in a vertical scan column. */
int verticalRuns(Host& host, int x, int y0, int y1, SkColor color) {
  int runs = 0;
  bool in = false;
  for (int y = y0; y <= y1; ++y) {
    const bool hit = host.pixel(x, y) == color;
    if (hit && !in) ++runs;
    in = hit;
  }
  return runs;
}
Element straightRun(Decoration style) {
  // A horizontal open path across the node, dressed by the line style.
  return box().child(box()
                         .absolute()
                         .inset(20, 80, 20, 80)
                         .shape([](SkSize s) {
                           SkPathBuilder b;
                           b.moveTo(0, s.height() / 2);
                           b.lineTo(s.width(), s.height() / 2);
                           return b.detach();
                         })
                         .stroke(std::move(style)));
}
}  // namespace

namespace {
/** Expensive per PIXEL, which is the only kind of expensive that matters
 *  here: automatic promotion thresholds on how long a node takes to paint,
 *  so cost has to come from fragment work over an area. */
sk_sp<SkRuntimeEffect> heavyEffect(bool withTime) {
  // The static variant must not so much as DECLARE uTime: Material::sksl
  // reads liveness off the declaration, not off whether anything drives it.
  SkString src;
  if (withTime) src.append("uniform float uTime;");
  src.append("half4 main(float2 p) {");
  src.append(withTime ? "  float t = uTime;" : "  float t = 0.0;");
  src.append(
      "  float v = 0.0;"
      "  for (int i = 0; i < 40; ++i) {"
      "    float f = float(i) + 1.0;"
      "    v += sin(p.x * 0.031 * f + t) *"
      "         cos(p.y * 0.027 * f - t) / f;"
      "  }"
      "  float g = clamp(v * 0.5 + 0.5, 0.0, 1.0);"
      "  return half4(half(g), half(g * 0.5), half(1.0 - g), 1.0);"
      "}");
  auto [effect, err] = SkRuntimeEffect::MakeForShader(src);
  if (!effect) ADD_FAILURE() << err.c_str();
  return effect;
}

/** A subtree the promoter will actually promote.
 *
 *  "Expensive" has to mean over the promotion time threshold. Child count
 *  alone does not get there — hundreds of thin hairline-stroked boxes are
 *  still far under the bar — so the panel carries a per-pixel shader across
 *  its whole area as well as its children. Drop the shader and every
 *  assertion about this node being promoted quietly becomes an assertion
 *  about a node that never could be. */
Element expensivePanel() {
  Element panel =
      box().width(180).height(180).fill(Material::sksl(heavyEffect(false)));
  for (int i = 0; i < 220; ++i) {
    const float t = (float)i / 220.0f;
    panel.child(
        box()
            .absolute()
            .left(4 + t * 170)
            .top(2)
            .width(2)
            .height(176)
            .fill(i % 2 ? green() : red())
            .foreground(util::stroke(0.7f, Fill::color({1, 1, 1, 0.5f}))));
  }
  return panel;
}

/** THE CACHE-TEST FIXTURE. Use it for anything that asserts on how a node
 *  was cached.
 *
 *  A static node under a CACHEABLE parent is painted exactly once, into
 *  that parent's recording, and never visited again — so it never appears
 *  in `profile()`, and any assertion written as "loop the rows, check the
 *  matches" passes vacuously over an empty match set. `Cache::None` on the
 *  wrapper keeps the subject painted every frame, which is also how these
 *  nodes sit in real scenes: under a stack() with animated siblings.
 *
 *  Pair it with `requireRow()`, and note what each half guarantees:
 *  `profiledUnder` gets the node PROFILED, `requireRow` proves it was.
 *  **Neither makes it PROMOTED** — that needs the node to be genuinely
 *  over the cost threshold, which is a property of the content, not of
 *  the fixture. See `expensivePanel`. */
Element profiledUnder(Element subject) {
  return box().cache(Cache::None).child(std::move(subject));
}

/** The profile row for `key`, failing loudly when there is none — the
 *  difference between "the library did not promote it" and "the test never
 *  looked at it", which a `for`-loop-with-an-`if` cannot tell you. */
const Composer::NodeCost* requireRow(const Composer& composer,
                                     const char* key) {
  for (const auto& row : composer.profile())
    if (row.label.rfind(key, 0) == 0) return &row;
  ADD_FAILURE() << "no profile row for '" << key
                << "' — the node was never painted, so nothing below this "
                   "line tested anything. Wrap it in profiledUnder().";
  return nullptr;
}
std::vector<SkColor> grab(Host& host) {
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(200, 200));
  host.surface->readPixels(bm.pixmap(), 0, 0);
  std::vector<SkColor> out;
  out.reserve(200 * 200);
  for (int y = 0; y < 200; ++y)
    for (int x = 0; x < 200; ++x) out.push_back(bm.getColor(x, y));
  return out;
}
}  // namespace

namespace {
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

namespace {
/** ONE effect for the whole process, and this is not tidiness.
 *
 *  `heavyEffect()` mints a fresh SkRuntimeEffect on every call, and a fresh
 *  effect pointer makes the material recipe compare unequal — so a fixture
 *  that re-describes each frame dirties the node's own paint each frame and
 *  no bake of any kind can hold. Tests that describe once and then only
 *  `frame()` never notice; a test whose whole point is that a child moves
 *  must re-describe, and then it does. Materials built from ordinary values
 *  compare fine; a raw SkSL effect pointer is the one that does not. */
sk_sp<SkRuntimeEffect> sharedHeavyEffect() {
  static sk_sp<SkRuntimeEffect> effect = heavyEffect(false);
  return effect;
}
}  // namespace

namespace {

/** A box whose whole boundary is stroked, for the trim/span comparisons. */
Element revealBox() { return box().rect(SkRect::MakeXYWH(20, 20, 100, 100)); }

/** A ring of samples around the stroked boundary above — enough of them
 *  that a window landing in the wrong place cannot hide. */
std::vector<SkColor> boundaryRing(Host& host) {
  std::vector<SkColor> out;
  for (int x = 15; x <= 125; x += 2) out.push_back(host.pixel(x, 20));
  for (int y = 15; y <= 125; y += 2) out.push_back(host.pixel(120, y));
  for (int x = 125; x >= 15; x -= 2) out.push_back(host.pixel(x, 120));
  for (int y = 125; y >= 15; y -= 2) out.push_back(host.pixel(20, y));
  return out;
}

size_t inkedCount(const std::vector<SkColor>& ring) {
  return (size_t)std::count_if(ring.begin(), ring.end(),
                               [](SkColor c) { return c != SK_ColorBLACK; });
}

}  // namespace
