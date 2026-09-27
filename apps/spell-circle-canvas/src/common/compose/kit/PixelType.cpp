/** @file
 * The aliased bitmap-font bake: the run rasterised on a scratch surface,
 * its coverage read back, thresholded to a 1-bit mask, and blitted with
 * nearest sampling through the pen's canvas.
 */

#include "sigilcompose/kit/PixelType.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPicture.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/Measure.h>
#include <sigildraw/Pen.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmedia/advanced/Skia.h>

namespace sigil::compose::kit {

namespace {

/** @p root baked at @p size and read back as F16 pixels, or a null bitmap
 *  when nothing could be drawn. Float rather than 8-bit because a coverage
 *  measured near the faint end of a glyph edge would otherwise quantise to
 *  a handful of levels. The wrapper carries EXPLICIT dims and an explicit
 *  canvas size: snapshot() sizes by the root's children and ignores the
 *  root's own dimensions. */
SkBitmap rasterize(Element root, sigil::weave::FontContext& fonts, int width,
                   int height) {
  SkBitmap out;
  if (width <= 0 || height <= 0) return out;
  const SkImageInfo info = SkImageInfo::Make(
      width, height, kRGBA_F16_SkColorType, kPremul_SkAlphaType);
  sk_sp<SkSurface> surface = SkSurfaces::Raster(info);
  if (!surface) return out;
  surface->getCanvas()->clear(SkColor4f{0, 0, 0, 0});
  if (sk_sp<SkPicture> picture =
          snapshot(box()
                       .width((float)width)
                       .height((float)height)
                       .children({std::move(root)}),
                   fonts, {(float)width, (float)height}))
    surface->getCanvas()->drawPicture(picture);
  out.allocPixels(info);
  if (!surface->readPixels(out.pixmap(), 0, 0)) out.reset();
  return out;
}

/** The still a bake answered, as the image the canvas draws. */
sk_sp<SkImage> pictureOf(const std::shared_ptr<const media::Image>& image) {
  return image && !image->frames().empty() ? image->frames().front().image
                                           : nullptr;
}

}  // namespace

Coverage coverage(std::u8string_view run, sigil::weave::FontContext& fonts,
                  const sigil::weave::TextStyle& style, Pad pad) {
  Coverage out;
  const std::u8string text8(run);
  const SkSize measured =
      intrinsicSize(box().children({text(text8, style)}), fonts);
  out.advance = {measured.width(), measured.height()};
  // SLACK ON THE ADVANCE, because the scratch surface CONSTRAINS the run.
  // `intrinsicSize()` answers an unconstrained layout; laid out again inside
  // exactly that width, a run can wrap its last word. A wrapped bake is
  // not a clipped glyph — it is a second LINE — and the pad retry below
  // cannot see it, because nothing touches an edge. The mask is cropped to
  // its ink afterwards, so the slack costs a larger scratch surface and
  // nothing in the output.
  const int advW = std::max(1, (int)std::ceil(measured.width()) + 8);
  const int advH = std::max(1, (int)std::ceil(measured.height()));

  for (int attempt = 0;; ++attempt) {
    const int w = advW + 2 * std::max(0, pad.x);
    const int h = advH + 2 * std::max(0, pad.y);
    // padding() rather than an absolute offset: the run is inset by `pad`
    // on EVERY side, so a negative left side-bearing has somewhere to go.
    // Growing only the surface would pad right and bottom alone and clip
    // that case no matter how large the pad got.
    const SkBitmap plane = rasterize(
        box()
            .padding((float)std::max(0, pad.y), (float)std::max(0, pad.x))
            .children({text(text8, style)}),
        fonts, w, h);
    if (plane.isNull()) return out;
    out.planeSize = {w, h};
    out.plane.assign((size_t)w * (size_t)h, glm::vec4(0.0f));
    out.pad = pad;
    int x0 = w, y0 = h, x1 = -1, y1 = -1;
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x) {
        const SkColor4f colour = plane.getColor4f(x, y);
        out.plane[(size_t)y * (size_t)w + (size_t)x] = {
            colour.fR, colour.fG, colour.fB, colour.fA};
        if (colour.fA > 0.0f) {
          x0 = std::min(x0, x);
          y0 = std::min(y0, y);
          x1 = std::max(x1, x);
          y1 = std::max(y1, y);
        }
      }
    out.ink = x1 < 0 ? geometry::path::Rect{}
                     : geometry::path::Rect{{(float)x0, (float)y0},
                                            {(float)x1 + 1, (float)y1 + 1}};
    const bool clipped =
        !out.ink.empty() && (out.ink.left() == 0 || out.ink.top() == 0 ||
                             out.ink.right() == (float)w ||
                             out.ink.bottom() == (float)h);
    if (!clipped || attempt >= kPadRetries || (pad.x <= 0 && pad.y <= 0))
      return out;
    pad.x = std::max(1, pad.x * 2);
    pad.y = std::max(1, pad.y * 2);
  }
}

Mask threshold(const Coverage& cov, float threshold, bool cropToInk) {
  Mask m;
  if (!cov.valid() || cov.ink.empty()) return m;
  const int left = cropToInk ? (int)cov.ink.left() : 0;
  const int top = cropToInk ? (int)cov.ink.top() : 0;
  const int width = cropToInk ? (int)cov.ink.width() : cov.width();
  const int height = cropToInk ? (int)cov.ink.height() : cov.height();
  SkBitmap a8;
  a8.allocPixels(SkImageInfo::MakeA8(width, height));
  a8.eraseColor(SK_ColorTRANSPARENT);
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x)
      *a8.getAddr8(x, y) =
          cov.alphaAt(left + x, top + y) >= threshold ? 255 : 0;
  a8.setImmutable();
  m.image = media::Image::of(a8.asImage());
  m.w = width;
  m.h = height;
  m.inkX = left;
  m.inkY = top;
  m.advance = cov.advance.x;
  return m;
}

void draw(draw::Pen& pen, const Mask& m, glm::vec2 at, const Present& p) {
  SkCanvas* canvas = pen.canvas();
  const sk_sp<SkImage> image = pictureOf(m.image);
  if (!canvas || !image) return;
  const SkRect dst = SkRect::MakeXYWH(at.x, at.y, (float)m.w * p.scale,
                                      (float)m.h * p.scale);
  const SkSamplingOptions nearest(SkFilterMode::kNearest);
  SkPaint paint;
  paint.setAntiAlias(false);
  if (p.shadowOffset.x != 0 || p.shadowOffset.y != 0) {
    paint.setColor4f(
        {p.colour.r * p.shadowMultiplier, p.colour.g * p.shadowMultiplier,
         p.colour.b * p.shadowMultiplier, p.colour.a},
        nullptr);
    canvas->drawImageRect(image,
                          dst.makeOffset(p.shadowOffset.x, p.shadowOffset.y),
                          nearest, &paint);
  }
  paint.setColor4f(material::skia::toSkColor(p.colour), nullptr);
  canvas->drawImageRect(image, dst, nearest, &paint);
}

Element masked(const Mask& m, const Present& p) {
  if (!m.image) return box().width(0).height(0);
  return custom([m, p](draw::Pen& pen) { draw(pen, m, {0, 0}, p); })
      .width((float)m.w * p.scale)
      .height((float)m.h * p.scale);
}

PixFont bakeFont(sigil::weave::FontContext& fonts,
                 const sigil::weave::TextStyle& style, Pad pad,
                 float thresholdAt, float spaceRatio) {
  PixFont f;
  const float size = style.shaping.fontSize;
  const float condense = style.shaping.scaleX;
  for (int i = 0; i < 96; ++i) {
    const char32_t ch = (char32_t)(32 + i);
    if (ch == U' ') {
      f.cells[(size_t)i].advance =
          std::max(1, (int)std::lround(size * spaceRatio * condense));
      continue;
    }
    const char c = (char)ch;
    const std::u8string one(1, (char8_t)c);
    const Coverage cov = coverage(one, fonts, style, pad);
    const Mask m = threshold(cov, thresholdAt);
    Cell& cell = f.cells[(size_t)i];
    cell.mask = m.image;
    cell.w = m.w;
    cell.h = m.h;
    cell.advance = std::max(1, (int)std::lround(cov.advance.x));
    // Plane coordinates back to line-box ones: the run was drawn inset by
    // the pad the bake settled on, and that pad is not the same for every
    // cell — one whose ink touched an edge was baked again with a larger
    // one.
    cell.inkX = m.inkX - cov.pad.x;
    cell.inkY = m.inkY - cov.pad.y;
    f.lineHeight = std::max(f.lineHeight, cell.inkY + cell.h);
  }
  for (int d = 0; d < 10; ++d)
    f.digitAdvance =
        std::max(f.digitAdvance, f.cells[(size_t)d + ('0' - 32)].advance);
  return f;
}

float blit(draw::Pen& pen, const PixFont& f, glm::vec2 at, std::string_view s,
           material::Color colour, const Blit& b) {
  SkCanvas* canvas = pen.canvas();
  SkPaint p;
  p.setAntiAlias(false);
  p.setColor4f(material::skia::toSkColor(colour), nullptr);
  const SkSamplingOptions nearest(SkFilterMode::kNearest);
  const float x0 = detail::snapTo(at.x, b.snap);
  const float y = detail::snapTo(at.y, b.snap);
  return detail::walkRun(f, s, b, [&](const Cell& cell, float x) {
    const sk_sp<SkImage> mask = pictureOf(cell.mask);
    if (canvas && mask)
      canvas->drawImageRect(
          mask,
          SkRect::MakeXYWH(x0 + x + (float)cell.inkX, y + (float)cell.inkY,
                           (float)cell.w, (float)cell.h),
          nearest, &p);
  });
}

}  // namespace sigil::compose::kit
