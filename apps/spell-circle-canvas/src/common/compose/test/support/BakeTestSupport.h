#pragma once
// How a device bake is judged against the live paint it replaces: the
// surface read back from a host, the worst per-channel drift between two
// of them, and the two runs a case compares — the same scene painted live
// and with its root asked for a texture bake.

#include <sigildraw/Pen.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilmaterial/skia/Paint.h>

#include <functional>
#include <vector>

#include "CoreTestSupport.h"

namespace {

/** A stroke that declares what it is: a value-comparable scheme that
 *  paints only over what it covers. */
struct FlatStroke {
  float width = 22;
  bool operator==(const FlatStroke&) const = default;
  bool blends() const { return false; }
  float bleed() const { return width * 0.5f; }
  void paint(sigil::draw::Pen& pen, const PaintContext& ctx) const {
    SkCanvas& canvas = *pen.canvas();
    SkPaint p;
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeWidth(width);
    p.setColor(SK_ColorWHITE);
    p.setAntiAlias(true);
    canvas.drawPath(geometry::path::toSk(ctx.outline), p);
  }
};

/** The whole surface, row-major. */
std::vector<SkColor> surfaceOf(Host& host, int w, int h) {
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(w, h));
  host.surface->readPixels(bm.pixmap(), 0, 0);
  std::vector<SkColor> out;
  out.reserve((size_t)w * (size_t)h);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) out.push_back(bm.getColor(x, y));
  return out;
}

/** WHAT A DEVICE BAKE MAY CHANGE, as one number. A device-space bake is
 *  taken under the live matrix on the canvas's own grid, so nothing it
 *  draws may land on another pixel — but the bake is an image of eight
 *  bits a channel, premultiplied by the coverage it was drawn with, and
 *  the blit composites from that. So a shaded pixel may land one code
 *  value away and nothing may land further: worst > 1 is a picture that
 *  moved, not a picture that rounded. A pixel cut off by a bake surface
 *  too small for the ink is a whole coverage value, tens of code values. */
int worstDrift(const std::vector<SkColor>& live,
               const std::vector<SkColor>& baked, size_t* differing) {
  int worst = 0;
  for (size_t i = 0; i < live.size(); ++i) {
    if (live[i] == baked[i]) continue;
    if (differing) ++*differing;
    for (int shift : {0, 8, 16, 24})
      worst = std::max(worst, std::abs((int)((live[i] >> shift) & 0xffu) -
                                       (int)((baked[i] >> shift) & 0xffu)));
  }
  return worst;
}

/** One scene drawn under a host matrix twice — painted live, and with its
 *  root asked for `Cache::Texture` — and what the two pictures differ by.
 *  `baked` says the root really was blitted from a bake, so a bake that
 *  stopped being taken cannot pass this by comparing two live renders. */
struct BakeDrift {
  int worstChannel = 0;
  size_t differingPixels = 0;
  bool baked = false;
};

BakeDrift bakeDriftOf(const std::function<Element()>& page,
                      const SkMatrix& hostMatrix, int w, int h) {
  const auto render = [&](bool bake, bool* bakedOut) {
    Host host(w, h);
    host.composer.setProfiling(true);
    Element root = page().key("page");
    if (bake) root.cache(Cache::Texture);
    host.composer.render(profiledUnder(std::move(root)));
    for (int i = 0; i < 3; ++i) {
      SkCanvas& canvas = *host.surface->getCanvas();
      canvas.clear(SK_ColorBLACK);
      canvas.save();
      canvas.concat(hostMatrix);
      host.composer.draw(canvas);
      canvas.restore();
    }
    if (bakedOut)
      for (const Composer::NodeCost& row : host.composer.profile())
        *bakedOut |= row.label.rfind("page (", 0) == 0 &&
                     row.cacheState == Composer::CacheState::Texture;
    return surfaceOf(host, w, h);
  };
  BakeDrift out;
  const std::vector<SkColor> live = render(false, nullptr);
  const std::vector<SkColor> baked = render(true, &out.baked);
  out.worstChannel = worstDrift(live, baked, &out.differingPixels);
  return out;
}

}  // namespace
