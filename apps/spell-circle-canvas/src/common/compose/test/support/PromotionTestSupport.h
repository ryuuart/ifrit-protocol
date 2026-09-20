#pragma once
// How a promotion case is judged: the scene that is worth promoting, the
// shader that makes it so, the surface read back from a host, the worst
// per-channel drift between two of them, and the two runs every case
// compares — the same frame with the promoter let go and pinned off, and
// a still taken after a warm.
//
// Judging a promotion by pixels means running the scene twice and
// differencing it, so the pair of runs is the fixture rather than
// anything a single case sets up.

#include <functional>
#include <vector>

#include "CoreTestSupport.h"

namespace {

/** A stroke that declares what it is: a value-comparable scheme that
 *  paints only over what it covers, so the node wearing it is one the
 *  promoter admits. */
struct FlatStroke {
  float width = 22;
  bool operator==(const FlatStroke&) const = default;
  bool blends() const { return false; }
  float bleed() const { return width * 0.5f; }
  void paint(SkCanvas& canvas, const PaintContext& ctx) const {
    SkPaint p;
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeWidth(width);
    p.setColor(SK_ColorWHITE);
    p.setAntiAlias(true);
    canvas.drawPath(ctx.outline, p);
  }
};

/** Expensive AND sensitive, which are two different demands on one shader.
 *  The forty-term sum is what puts a node over the promotion threshold; the
 *  cell test on the SAME coordinates is what reads back whether the bake
 *  sampled where the live paint sampled, since a cell boundary flips a
 *  whole channel where a smooth ramp moves one code value. */
sk_sp<SkRuntimeEffect> gridEffect() {
  static sk_sp<SkRuntimeEffect> fx = [] {
    auto [effect, err] = SkRuntimeEffect::MakeForShader(SkString(R"(
half4 main(float2 p) {
  float v = 0.0;
  for (int i = 0; i < 40; ++i) {
    float f = float(i) + 1.0;
    v += sin(p.x * 0.031 * f) * cos(p.y * 0.027 * f) / f;
  }
  float2 cell = floor(p / 6.0);
  float b = mod(cell.x + cell.y, 2.0);
  return half4(half(b), half(1.0 - b), half(clamp(v * 0.5 + 0.5, 0.0, 1.0)), 1.0);
}
)"));
    if (!effect) ADD_FAILURE() << err.c_str();
    return effect;
  }();
  return fx;
}

/** A page the promoter will actually promote: a per-pixel shader over the
 *  whole area, children that OVERFLOW it on every side, and a line of type.
 *  Child count alone is far under the promotion time threshold — the shader
 *  is what puts it over — and the overflow is the half the bake rect has to
 *  survive, since it is what carries the node's paint bounds outside the
 *  canvas the live paint is clipped to. */
Element promotablePage() {
  Element page = box().width(180).height(180).fill(
      material::skia::Paint::sksl(gridEffect()));
  for (int i = 0; i < 56; ++i)
    page.children({box()
                       .absolute()
                       .left(-6 + (float)i * 3.3f)
                       .top(-8)
                       .width(2)
                       .height(196)
                       .fill(i % 2 ? green() : red())});
  page.children(
      {text(u8"JAM", whiteStyle(24)).absolute().left(8).top(70).width(160)});
  return page;
}

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

/** WHAT PROMOTION IS ALLOWED TO CHANGE, as one number. A device-space bake
 *  is taken under the live matrix post-translated by an INTEGER, so nothing
 *  it draws may land on another pixel — but the matrix is inverted to find a
 *  shader's local coordinates, and at a scale whose reciprocal is inexact
 *  that integer does not cancel to the last bit. So a shaded pixel may land
 *  one code value away and nothing may land further: worst > 1 is a picture
 *  that moved, not a picture that rounded.
 *
 *  ONE VALUE IS THE BAKE OVER TRANSPARENT BLACK. A bake that lands on
 *  CONTENT carries a second: the node's own paint is composited twice
 *  where the live paint composited once — once into the bake, once when
 *  the bake is blitted — and Skia's blit of a raster image is not the
 *  arithmetic of its direct shader draw, so a partly transparent texel
 *  over a bright backdrop can settle a value further out. It is still a
 *  rounding and not a move: it appears only where the node's own alpha is
 *  between none and all, and taking the bake at higher precision does not
 *  remove it. Cases whose promoted node lands over other content say so
 *  and allow the two. */
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

/** One scene drawn thirty times under a host matrix — long enough that the
 *  promoter's warmup is over — with promotion off and then on, and what the
 *  two pictures differ by. `promoted` says a node really was baked, so a
 *  promoter that stopped firing cannot pass this by comparing two identical
 *  live renders. */
struct PromotionDrift {
  int worstChannel = 0;
  size_t differingPixels = 0;
  bool promoted = false;
};

PromotionDrift promotionDriftOf(const std::function<Element()>& page,
                                const SkMatrix& hostMatrix, int w, int h) {
  const auto render = [&](bool promotion, bool* promotedOut) {
    Host host(w, h);
    // EAGER on the promoted side, so the case is about the promoter and
    // never about the machine: the cost rule is a stopwatch, and an idle
    // runner crosses no bar and would compare two live renders.
    host.composer.setAutoTexturePromotion(promotion
                                              ? Composer::PromotionPolicy::Eager
                                              : Composer::PromotionPolicy::Off);
    host.composer.setProfiling(true);
    host.composer.render(profiledUnder(page().key("page")));
    for (int i = 0; i < 30; ++i) {
      SkCanvas& canvas = *host.surface->getCanvas();
      canvas.clear(SK_ColorBLACK);
      canvas.save();
      canvas.concat(hostMatrix);
      host.composer.draw(canvas);
      canvas.restore();
    }
    if (promotedOut)
      for (const Composer::NodeCost& row : host.composer.profile())
        *promotedOut |= row.cacheState == Composer::CacheState::Promoted;
    return surfaceOf(host, w, h);
  };
  PromotionDrift out;
  const std::vector<SkColor> live = render(false, nullptr);
  const std::vector<SkColor> baked = render(true, &out.promoted);
  out.worstChannel = worstDrift(live, baked, &out.differingPixels);
  return out;
}

PromotionDrift promotionDrift(const SkMatrix& hostMatrix, int w, int h) {
  return promotionDriftOf(promotablePage, hostMatrix, w, h);
}

/** The still a plate is photographed as: a scene warmed at one scale on one
 *  surface — long enough that the promoter has baked what it is going to —
 *  and then drawn ONCE at another, larger, fractional scale onto a surface
 *  of its own. Everything a bake was pinned to moved between the two. */
std::vector<SkColor> warmThenStillOf(const std::function<Element()>& page,
                                     bool promotion, float stillScale, int w,
                                     int h, bool* promotedOut) {
  Host host(w, h);
  host.composer.setAutoTexturePromotion(promotion
                                            ? Composer::PromotionPolicy::Eager
                                            : Composer::PromotionPolicy::Off);
  host.composer.setProfiling(true);
  host.composer.render(profiledUnder(page().key("page")));
  for (int i = 0; i < 30; ++i) host.frame();
  if (promotedOut)
    for (const Composer::NodeCost& row : host.composer.profile())
      *promotedOut |= row.cacheState == Composer::CacheState::Promoted;
  const int sw = (int)((float)w * stillScale);
  const int sh = (int)((float)h * stillScale);
  sk_sp<SkSurface> still =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(sw, sh));
  still->getCanvas()->clear(SK_ColorBLACK);
  still->getCanvas()->scale(stillScale, stillScale);
  host.composer.draw(*still->getCanvas());
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(sw, sh));
  still->readPixels(bm.pixmap(), 0, 0);
  std::vector<SkColor> out;
  out.reserve((size_t)sw * (size_t)sh);
  for (int y = 0; y < sh; ++y)
    for (int x = 0; x < sw; ++x) out.push_back(bm.getColor(x, y));
  return out;
}

std::vector<SkColor> warmThenStill(bool promotion, float stillScale, int w,
                                   int h, bool* promotedOut) {
  return warmThenStillOf(promotablePage, promotion, stillScale, w, h,
                         promotedOut);
}

}  // namespace
