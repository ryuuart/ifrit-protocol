#pragma once

/** @file
 * SigilCompose KIT — the aliased bitmap-font bake.
 *
 * Shape a run with antialiasing off, rasterise it, threshold it to a 1-bit
 * A8 mask, and present that mask at an integer scale with nearest
 * sampling. The result is pixel type: a Minecraft-style or PS1-style face
 * whose every edge lands on the grid.
 *
 * ## This is a workaround, and it is worth knowing what for
 *
 * `text()` takes a `std::u8string`, not an animatable value, so a LIVE
 * numeric readout cannot be a text node at all. Baking to a mask and
 * blitting it inside a `custom()` leaf is how a readout gets drawn from a
 * bound `Output` without re-describing anything. If `text()` ever accepts
 * an animatable, most of the reason to reach for this file goes away —
 * what would remain is the aliased look itself.
 *
 * ## The four traps
 *
 * 1. **`measure()` returns the ADVANCE, and glyph ink escapes it.** Ink
 *    overhanging the advance is normal — an italic's exit stroke, a
 *    negative left side-bearing, a swash — and a scratch surface sized to
 *    the advance clips it. The tell is that a clipped bake looks the same
 *    at every font size, where a rasterisation fault would move with the
 *    size.
 *
 *    No fixed pad is a fix; it is only a bigger guess. A boxed monospace
 *    never overhangs at all, while a script face can still reach past a
 *    generous pad, and padding only right and bottom leaves a negative
 *    left side-bearing clipping at the left where no pad value reaches it.
 *    So `Pad` is slack on **each** side, the run is drawn inset by it, and
 *    if ink still touches any edge the bake retries with the pad doubled.
 *    The mask is cropped to its lit bbox afterwards, so slack costs a
 *    larger scratch surface and nothing in the output.
 *
 * 2. **The SIZE is the control, and the threshold is inert.** Under
 *    `ShapingStyle::aliased` Skia lights a pixel iff its centre is inside
 *    the outline, so the coverage handed back is already 0 or 1 and any
 *    threshold in (0, 1] classifies it identically. What decides
 *    legibility is whether the x-height rounds up or down: at one size a
 *    lowercase `e`'s counter contains no pixel centre and every `e` prints
 *    as an `a`; one pixel larger and they all open; one larger again and
 *    an `m`'s gaps close instead. **Sweep the size, not the threshold.**
 *    The threshold becomes a real control only with `aliased = false`.
 *
 * 3. **Digits want one tabular advance,** or a rolling readout shivers as
 *    a `1` narrows the string.
 *
 * 4. **Present at an INTEGER scale with `kNearest`.** A bitmap face at a
 *    fractional scale is a blurry bitmap face. `Material::image` is the
 *    image path that takes a sampling parameter.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/typography/Typography.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <string_view>

namespace sigil::compose::kit {

namespace detail {
/** @p root baked at @p size and read back as F16 pixels, or a null bitmap
 *  when nothing could be drawn. Float rather than 8-bit because a coverage
 *  measured near the faint end of a glyph edge would otherwise quantise to
 *  a handful of levels. The wrapper carries EXPLICIT dims and an explicit
 *  canvas size: snapshot() sizes by the root's children and ignores the
 *  root's own dimensions. */
inline SkBitmap rasterize(Element root, sigil::weave::FontContext& fonts,
                          SkISize size) {
  SkBitmap out;
  if (size.isEmpty()) return out;
  const SkImageInfo info = SkImageInfo::Make(
      size.width(), size.height(), kRGBA_F16_SkColorType, kPremul_SkAlphaType);
  sk_sp<SkSurface> surface = SkSurfaces::Raster(info);
  if (!surface) return out;
  surface->getCanvas()->clear(SkColor4f{0, 0, 0, 0});
  if (sk_sp<SkPicture> picture =
          snapshot(box()
                       .width((float)size.width())
                       .height((float)size.height())
                       .child(std::move(root)),
                   fonts, {(float)size.width(), (float)size.height()}))
    surface->getCanvas()->drawPicture(picture);
  out.allocPixels(info);
  if (!surface->readPixels(out.pixmap(), 0, 0)) out.reset();
  return out;
}
}  // namespace detail

/** Slack around the measured run, in px **on each side**. See trap 1: the
 *  defaults are a starting point, not a guarantee — `coverage()` grows
 *  them, up to a limit, until no ink touches an edge. */
struct Pad {
  int x = 8;
  int y = 4;
  bool operator==(const Pad&) const = default;
};

/** How many times `coverage()` doubles the pad before giving up. Four
 *  doublings take the default 8 px to 128 px a side. */
inline constexpr int kPadRetries = 4;

/** The raw bake: a coverage plane and the box its ink actually occupies.
 *
 *  Coverage rather than a finished mask, because the useful next step
 *  varies: thresholding to 1-bit A8 is one, quantising to a few levels and
 *  looking each up in a palette is another, and a colour-aware
 *  classification reads the plane directly. */
struct Coverage {
  /** F16 premultiplied, `pad` bigger than the measured advance. Read it
   *  with `alphaAt`, or directly for a colour-aware classification. */
  SkBitmap plane;
  /** The bbox of pixels with any coverage at all. Empty when nothing lit
   *  (a space, an unmapped codepoint). */
  SkIRect ink = SkIRect::MakeEmpty();
  /** What `measure()` reported — the ADVANCE, which is what a layout
   *  wants and is NOT the ink extent. */
  SkSize advance = {0, 0};
  /** The slack the bake actually used, which is NOT the slack asked for:
   *  a run whose ink touched an edge was baked again with the pad doubled.
   *  It is the origin of the run's own line box inside the plane, so it is
   *  what turns a plane coordinate into a typographic one. */
  Pad pad;

  bool valid() const { return !plane.isNull(); }
  int width() const { return plane.width(); }
  int height() const { return plane.height(); }
  float alphaAt(int x, int y) const {
    if (x < 0 || y < 0 || x >= plane.width() || y >= plane.height())
      return 0.0f;
    return plane.getColor4f(x, y).fA;
  }
};

/** Shape @p run in @p style, rasterise it, and hand back the coverage.
 *
 *  The pad grows by doubling until no ink touches an edge of the scratch
 *  surface. **After `kPadRetries` doublings it gives up silently and
 *  returns the clipped bake** — there is no error channel, so a face that
 *  overhangs further than that comes back cropped and looks like a
 *  rasterisation bug rather than a sizing one.
 *
 *  Built on `snapshot()`, which carries the neighbouring trap: it sizes by
 *  the root's CHILDREN, so the wrapper must carry explicit dimensions or an
 *  absolutely-placed child resolves against nothing. */
inline Coverage coverage(std::u8string_view run,
                         sigil::weave::FontContext& fonts,
                         const sigil::weave::TextStyle& style, Pad pad = {}) {
  Coverage out;
  const std::u8string text8(run);
  const SkSize sz = measure(box().child(text(text8, style)), fonts);
  out.advance = sz;
  // SLACK ON THE ADVANCE, because the scratch surface CONSTRAINS the run.
  // `measure()` answers an unconstrained layout; laid out again inside
  // exactly that width, a run can wrap its last word. A wrapped bake is
  // not a clipped glyph — it is a second LINE — and the pad retry below
  // cannot see it, because nothing touches an edge. The mask is cropped to
  // its ink afterwards, so the slack costs a larger scratch surface and
  // nothing in the output.
  const int advW = std::max(1, (int)std::ceil(sz.width()) + 8);
  const int advH = std::max(1, (int)std::ceil(sz.height()));

  for (int attempt = 0;; ++attempt) {
    const int w = advW + 2 * std::max(0, pad.x);
    const int h = advH + 2 * std::max(0, pad.y);
    // padding() rather than an absolute offset: the run is inset by `pad`
    // on EVERY side, so a negative left side-bearing has somewhere to go.
    // Growing only the surface would pad right and bottom alone and clip
    // that case no matter how large the pad got.
    SkBitmap plane = detail::rasterize(
        box()
            .padding((float)std::max(0, pad.x), (float)std::max(0, pad.y))
            .child(text(text8, style)),
        fonts, {w, h});
    if (plane.isNull()) return out;
    out.plane = std::move(plane);
    out.pad = pad;
    int x0 = w, y0 = h, x1 = -1, y1 = -1;
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x)
        if (out.plane.getColor4f(x, y).fA > 0.0f) {
          x0 = std::min(x0, x);
          y0 = std::min(y0, y);
          x1 = std::max(x1, x);
          y1 = std::max(y1, y);
        }
    out.ink = x1 < 0 ? SkIRect::MakeEmpty()
                     : SkIRect::MakeLTRB(x0, y0, x1 + 1, y1 + 1);
    const bool clipped =
        !out.ink.isEmpty() && (out.ink.fLeft == 0 || out.ink.fTop == 0 ||
                               out.ink.fRight == w || out.ink.fBottom == h);
    if (!clipped || attempt >= kPadRetries || (pad.x <= 0 && pad.y <= 0))
      return out;
    pad.x = std::max(1, pad.x * 2);
    pad.y = std::max(1, pad.y * 2);
  }
}

/** A baked run: a 1-bit A8 image plus the numbers a caller needs to place
 *  and advance past it. */
struct Mask {
  sk_sp<SkImage> image;
  int w = 0, h = 0;
  /** Where the ink sat inside the padded plane, before cropping — the
   *  offset to add back if you want the run on its own baseline rather
   *  than flush to its ink. */
  int inkX = 0, inkY = 0;
  /** The measured advance, for laying the next run out. */
  float advance = 0.0f;

  explicit operator bool() const { return image != nullptr; }
};

/** Threshold a `Coverage` to 1-bit A8 and (by default) crop to its ink.
 *
 *  @p threshold is in coverage units [0, 1] and is **inert under aliased
 *  shaping**, where the coverage is already binary — see trap 2. It
 *  becomes a real control only when the run was deliberately shaped
 *  antialiased and is being quantised afterwards. */
inline Mask threshold(const Coverage& cov, float threshold = 0.5f,
                      bool cropToInk = true) {
  Mask m;
  if (!cov.valid() || cov.ink.isEmpty()) return m;
  const SkIRect r =
      cropToInk ? cov.ink : SkIRect::MakeWH(cov.width(), cov.height());
  SkBitmap a8;
  a8.allocPixels(SkImageInfo::MakeA8(r.width(), r.height()));
  a8.eraseColor(SK_ColorTRANSPARENT);
  for (int y = 0; y < r.height(); ++y)
    for (int x = 0; x < r.width(); ++x)
      *a8.getAddr8(x, y) =
          cov.alphaAt(r.fLeft + x, r.fTop + y) >= threshold ? 255 : 0;
  a8.setImmutable();
  m.image = a8.asImage();
  m.w = r.width();
  m.h = r.height();
  m.inkX = r.fLeft;
  m.inkY = r.fTop;
  m.advance = cov.advance.width();
  return m;
}

/** The whole bake in one call: shape, rasterise, threshold, crop. */
inline Mask bakeRun(std::u8string_view run, sigil::weave::FontContext& fonts,
                    const sigil::weave::TextStyle& style, Pad pad = {},
                    float thresholdAt = 0.5f) {
  return threshold(coverage(run, fonts, style, pad), thresholdAt);
}

/** How a baked mask is presented. */
struct Present {
  SkColor4f colour = {1, 1, 1, 1};
  /** INTEGER, please — trap 4. A bitmap face at 1.5× is a blurry bitmap
   *  face. */
  float scale = 1.0f;
  /** A second pass underneath, offset by this many DESTINATION px, with
   *  the colour's RGB multiplied by `shadowMul`. The defaults follow
   *  Minecraft's own font renderer, which offsets by one GUI pixel and
   *  multiplies by a quarter. Zero offset = no shadow pass. */
  SkVector shadowOffset = {0, 0};
  float shadowMul = 0.25f;
};

/** Draw a baked mask at @p at (top-left), immediate mode.
 *
 *  An A8 image drawn through `drawImageRect` modulates the PAINT's colour,
 *  which is the reason to blit rather than fill: the mask carries only
 *  coverage, so one bake serves every tint the drawing needs. */
inline void draw(SkCanvas& canvas, const Mask& m, SkPoint at,
                 const Present& p = {}) {
  if (!m.image) return;
  const SkRect dst = SkRect::MakeXYWH(at.fX, at.fY, (float)m.w * p.scale,
                                      (float)m.h * p.scale);
  const SkSamplingOptions nearest(SkFilterMode::kNearest);
  SkPaint paint;
  paint.setAntiAlias(false);
  if (p.shadowOffset.fX != 0 || p.shadowOffset.fY != 0) {
    paint.setColor4f({p.colour.fR * p.shadowMul, p.colour.fG * p.shadowMul,
                      p.colour.fB * p.shadowMul, p.colour.fA},
                     nullptr);
    canvas.drawImageRect(m.image,
                         dst.makeOffset(p.shadowOffset.fX, p.shadowOffset.fY),
                         nearest, &paint);
  }
  paint.setColor4f(p.colour, nullptr);
  canvas.drawImageRect(m.image, dst, nearest, &paint);
}

/** The same as a retained leaf sized to the mask, for a STATIC run. The
 *  node is `m.w × scale` by `m.h × scale`; place it with `.at()`/`.rect()`
 *  like any other absolute node.
 *
 *  Note this is a `custom()` leaf, so it never records a picture and its
 *  program runs every frame it is visible — cheap here (one
 *  `drawImageRect`) but not free. A static run that never changes colour
 *  is better as `.cache(Cache::Texture)` on its parent. */
inline Element masked(const Mask& m, const Present& p = {}) {
  if (!m.image) return box().width(0).height(0);
  return custom([m, p](SkCanvas& canvas, const PaintContext&) {
           draw(canvas, m, {0, 0}, p);
         })
      .width((float)m.w * p.scale)
      .height((float)m.h * p.scale);
}

// ---------------------------------------------------------------------------
// The 96-cell font — what a LIVE readout needs.

/** One baked cell.
 *
 *  The mask is cropped to its ink, so where that ink sat inside the cell's
 *  own LINE BOX is the cell's to carry: a `T` and a `p` cropped flush to
 *  their ink and drawn at one y would stand on no common line at all.
 *  `inkX` is the left side bearing and `inkY` the drop from the top of the
 *  line box, both in px, and `blit` adds them back. */
struct Cell {
  sk_sp<SkImage> mask;
  int w = 0, h = 0;
  /** The shaped advance, rounded — NOT the ink width. */
  int advance = 0;
  /** Where this cell's ink sits inside its line box. */
  int inkX = 0, inkY = 0;
};

/** ASCII 32..127, baked once. */
struct PixFont {
  std::array<Cell, 96> cells{};
  /** How deep the cells reach below the top of their shared line box —
   *  a line box for the caller. It is NOT the tallest cell: a cell is
   *  cropped to its ink and sits at its own drop inside the box. */
  int lineHeight = 0;
  /** The widest DIGIT advance, shared by all ten — see trap 3. */
  int digitAdvance = 0;

  const Cell& cell(char c) const {
    const int i = (int)(unsigned char)c - 32;
    static const Cell empty{};
    return (i < 0 || i >= 96) ? empty : cells[(size_t)i];
  }
};

/** Bake every printable ASCII cell in @p style.
 *
 *  Space is special-cased: it gets an advance and no mask, which is not
 *  the same as a cell that failed to bake. A lone shaped space has no ink,
 *  so the crop-to-ink step would otherwise reduce it to nothing and every
 *  word would run together. @p spaceRatio is its advance as a fraction of
 *  the font size, scaled by the style's horizontal condense. */
inline PixFont bakeFont(sigil::weave::FontContext& fonts,
                        const sigil::weave::TextStyle& style, Pad pad = {3, 3},
                        float thresholdAt = 0.5f, float spaceRatio = 0.34f) {
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
    cell.advance = std::max(1, (int)std::lround(cov.advance.width()));
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

/** How a run is spaced and where it lands. */
struct Blit {
  /** px added after every cell. */
  float track = 1.0f;
  /** Digits take `PixFont::digitAdvance` instead of their own — trap 3.
   *  On for a readout, off for prose. */
  bool tabularDigits = true;
  /** Round every pen position to a multiple of this many px (0 = off).
   *  A bitmap face that lands off the device grid is a resampled bitmap
   *  face. Pass the pitch of whatever grid the drawing is on, or `1` for
   *  the device grid. */
  float snap = 0.0f;
};

namespace detail {
inline float snapTo(float v, float grid) {
  return grid > 0 ? std::round(v / grid) * grid : v;
}

/** THE PEN WALK, and the only one. Measuring and drawing must accumulate
 *  the same way or a snapped run is laid out to one width and drawn at
 *  another: `snap` rounds every pen step, so it changes the advance and
 *  not merely where the cells land. @p emit is handed each cell and the
 *  pen position it occupies, relative to a pen that started at 0; the
 *  return is the run's advance.
 *
 *  Starting at 0 loses nothing: a caller's own origin is snapped before
 *  the walk, and a snapped origin plus a snapped offset is already on the
 *  grid, so adding it back per cell rounds to the same place. */
template <typename Emit>
inline float walkRun(const PixFont& f, std::string_view s, const Blit& b,
                     Emit&& emit) {
  float x = 0;
  for (char raw : s) {
    const int i = (int)(unsigned char)raw - 32;
    if (i < 0 || i >= 96) continue;
    const Cell& cell = f.cells[(size_t)i];
    const bool digit = raw >= '0' && raw <= '9';
    emit(cell, x);
    x = snapTo(
        x + (float)(digit && b.tabularDigits ? f.digitAdvance : cell.advance) +
            b.track,
        b.snap);
  }
  return x;
}
}  // namespace detail

/** Advance width of @p s without drawing it — the width `blit` returns for
 *  the same run and the same options, snapping included. */
inline float widthOf(const PixFont& f, std::string_view s, const Blit& b = {}) {
  return detail::walkRun(f, s, b, [](const Cell&, float) {});
}

/** Draw @p s at @p at (top-left of the line box) and return the advance.
 *
 *  Immediate-mode, for inside a `custom()` leaf — which is the whole point:
 *  a live readout reads its bound `Output` here and draws the number, with
 *  nothing re-described and nothing reconciled. */
inline float blit(SkCanvas& canvas, const PixFont& f, SkPoint at,
                  std::string_view s, SkColor4f colour, const Blit& b = {}) {
  SkPaint p;
  p.setAntiAlias(false);
  p.setColor4f(colour, nullptr);
  const SkSamplingOptions nearest(SkFilterMode::kNearest);
  const float x0 = detail::snapTo(at.fX, b.snap);
  const float y = detail::snapTo(at.fY, b.snap);
  return detail::walkRun(f, s, b, [&](const Cell& cell, float x) {
    if (cell.mask)
      canvas.drawImageRect(cell.mask,
                           SkRect::MakeXYWH(x0 + x + (float)cell.inkX,
                                            y + (float)cell.inkY,
                                            (float)cell.w, (float)cell.h),
                           nearest, &p);
  });
}

}  // namespace sigil::compose::kit
